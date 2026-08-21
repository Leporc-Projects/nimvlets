#!/usr/bin/env python3
"""Dev asset pipeline: compiles a JSON pack manifest + source PNG frames
into the runtime ".nvpack" binary format content::PetPackLoader reads,
with no PNG decoder and no JSON/manifest parser needed on the C++ side
(see src/content/PetPackLoader.cpp and docs/ANIMATION_RUNTIME.md for the
exact on-disk layout, "NVPACK2").

Block 05 replaces the old flat idle/click_reaction/passive_actions
manifest shape with a named-state BEHAVIOR GRAPH: a pet is one or more
`states`, each with a base pose plus three triggers (ambient/hover/
click), each trigger a weighted list of actions that can transition to
ANY state (including the same one, a "self-loop" -- the shape every
normal single-state pet like Bunny/Nidir already reduces to). This is a
genuinely different binary layout from "NVPACK1", not an additive
extension of it -- see docs/ANIMATION_RUNTIME.md for why the magic
value changed instead of silently reinterpreting old bytes.

Manifest schema (JSON):
    {
      "id": "bunny_dev",
      "display_name": "Bunny",
      "variant_group": "",                     # optional, default ""
      "canvas_width": 160,
      "canvas_height": 160,
      "alpha_hit_threshold": 128,               # optional, default 128
      "visual_scale": 1.0,                      # optional, default 1.0 -- per-pet display-size
                                                 # multiplier, applied only at render/window/hit-mask
                                                 # time (src/app/SpikeApp.cpp) -- never touches source
                                                 # art or the pixel bytes compiled below.
      "content_version": "block05-bunny-1",     # optional, default ""
      "runtime_max_frame_dimension": 320,       # optional, default: no downscale (see below)
      "normalize_visual_scale": true,           # optional, default false (see below)
      "states": [
        {
          "id": "default",
          "base_animation": { ...AnimationManifest... },
          "base_animation_direction_overrides": [    # optional, default []
            {"direction": "left", ...AnimationManifest...}, ...
          ],
          "ambient_interval_seconds": 15.0,           # optional, default 300.0 -- meaningless if
                                                       # ambient_actions is empty (no timer is ever
                                                       # armed for a state with no ambient actions).
          "ambient_actions": [ {...WeightedActionManifest...}, ... ],   # optional, default []
          "hover_uses_ambient_actions": true,         # optional, default true -- see below
          "hover_actions": [ {...WeightedActionManifest...}, ... ],     # optional, default []
          "click_actions": [ {...WeightedActionManifest...}, ... ]      # optional, default []
        },
        ...
      ]
    }

`hover_uses_ambient_actions` (default true): if true, a hover trigger
picks from this state's OWN `ambient_actions` (same pool, no duplicated
frame data in the compiled pack) -- the default policy for a normal pet
("hover uses the same available passive-action pool unless content says
otherwise"). If false, `hover_actions` is used instead (possibly empty
-- no hover behavior at all for this state, e.g. Frin today, which has
no owner-authored hover action yet). Setting both true AND a non-empty
`hover_actions` is rejected (ambiguous) both here and by the C++ loader.

WeightedActionManifest:
    {
      "id": "howl",
      "weight": 0.7,                # relative weight among sibling actions of the SAME trigger
      "target_state_id": "seated",  # which state to enter when this one-shot finishes -- the
                                     # SAME id as the state it's authored under is a self-loop
                                     # (the normal case for a single-state pet's click/ambient)
      ...AnimationManifest...,
      "direction_overrides": [ {"direction": "left", ...AnimationManifest...}, ... ]  # optional
    }

AnimationManifest:
    {
      "id": "click_reaction",
      "kind": "static" | "loop" | "one_shot",
      "fps": 10,                    # optional, default 0 (use each frame's duration_ms instead)
      "returns_to_idle": true,      # optional, default true -- for a WeightedAction's animation,
                                     # this gates whether finishing the one-shot actually transitions
                                     # to target_state_id (true) or holds on the last frame forever
                                     # (false); meaningless for a state's base_animation (kStatic/kLoop).
      "frames": [
        {"source": "relative/or/manifest-relative/path.png",
         "duration_ms": 100,        # optional, default 0.0 (only matters if fps == 0)
         "anchor_x": 80, "anchor_y": 80}   # optional, default = frame center
      ]
    }

`source` paths are resolved relative to the manifest file's own
directory, not the current working directory.

Usage:
    python3 tools/compile_pet_pack.py <manifest.json> <output.nvpack>

`runtime_max_frame_dimension` and `normalize_visual_scale` behave
exactly as they did before Block 05 (see prep_dev_sprite.py) -- the
only thing that changed is which manifest structure they walk to find
every compilable animation (every state's base_animation plus every
ambient/hover/click action, instead of idle/click_reaction/
passive_actions). Real downscales use
`prep_dev_sprite.resize_rgba_area_average()` (a deterministic box
filter) instead of plain nearest-neighbor -- see that function's
docstring for why (Block 04.3 correction).

Fails loudly (non-zero exit, a specific message on stderr naming the
animation/frame at fault) rather than silently inventing or skipping
data, on:
    - a referenced source PNG that doesn't exist
    - an animation whose frames don't all share the same pixel dimensions
    - a required manifest field missing
    - an animation with an empty frame list
    - an out-of-range alpha_hit_threshold
    - a non-positive visual_scale
    - a WeightedAction whose target_state_id doesn't match any state
    - hover_uses_ambient_actions=true together with a non-empty hover_actions

No third-party dependencies (json/struct/os are standard library).
"""

from __future__ import annotations

import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import prep_dev_sprite  # noqa: E402  (reused for its PNG decoder, not reimplemented)

_KIND_TO_BYTE = {"static": 0, "loop": 1, "one_shot": 2}
_DIRECTION_TO_BYTE = {"right": 0, "left": 1}


class PackCompileError(Exception):
    """A clear, specific compile failure — never caught silently."""


def _require(manifest: dict, key: str, context: str):
    if key not in manifest:
        raise PackCompileError(f"{context}: missing required field '{key}'")
    return manifest[key]


def _pack_string(s: str) -> bytes:
    encoded = s.encode("utf-8")
    return struct.pack("<I", len(encoded)) + encoded


def _compile_frame(
    frame_manifest: dict,
    manifest_dir: str,
    context: str,
    runtime_max_frame_dimension: int | None,
    normalization: tuple[float, int, int, int, int] | None,
) -> tuple[int, int, bytes]:
    source = _require(frame_manifest, "source", context)
    path = os.path.join(manifest_dir, source)
    if not os.path.isfile(path):
        raise PackCompileError(f"{context}: source frame not found: {path}")

    width, height, pixels = prep_dev_sprite.read_png_rgba(path)
    if len(pixels) != width * height * 4:
        raise PackCompileError(f"{context}: decoded pixel data size mismatch for {path}")

    # Normalización de escala/encuadre por contenido, opcional (ver el
    # docstring del módulo y prep_dev_sprite.compute_frame_normalization_plan()),
    # y el downscale opcional en tiempo de compilación
    # (`runtime_max_frame_dimension`) -- COMBINADOS en un único resize
    # cuando ambos aplican, en vez de dos resizes de box-filter
    # encadenados (Block 05, segunda pasada de corrección post-QA: ver
    # docs/DECISION_LOG.md DEC-075). Dos pasadas secuenciales de
    # `resize_rgba_area_average` para el MISMO factor de reescalado neto
    # pierden más detalle fino que una sola pasada directa desde la
    # resolución nativa -- medido en este bloque: hasta ~1.75% de los
    # pixeles de alpha de un frame real de Bunny (groom) difieren
    # visiblemente (>10/255) entre ambos caminos, con un ablandamiento
    # de contorno perceptible en el de dos pasadas. El PNG en disco
    # nunca se toca de ninguna forma, solo estos bytes que van directo
    # al pack compilado.
    if normalization is not None:
        content_scale, working_width, working_height, offset_x, offset_y = normalization

        runtime_ratio = 1.0
        if runtime_max_frame_dimension is not None and max(working_width, working_height) > runtime_max_frame_dimension:
            runtime_ratio = runtime_max_frame_dimension / max(working_width, working_height)

        combined_scale = content_scale * runtime_ratio
        final_working_width = max(1, round(working_width * runtime_ratio))
        final_working_height = max(1, round(working_height * runtime_ratio))
        final_offset_x = round(offset_x * runtime_ratio)
        final_offset_y = round(offset_y * runtime_ratio)

        if abs(combined_scale - 1.0) > 1e-9:
            target_w = max(1, round(width * combined_scale))
            target_h = max(1, round(height * combined_scale))
            resize_fn = prep_dev_sprite.resize_rgba_area_average if combined_scale < 1.0 else prep_dev_sprite.resize_rgba_nearest
            pixels = resize_fn(width, height, pixels, target_w, target_h)
            width, height = target_w, target_h

        # Falla fuerte en vez de recortar contenido en silencio (Block
        # 05, segunda pasada de corrección post-QA -- ver
        # docs/DECISION_LOG.md DEC-075): el canvas de trabajo compartido
        # se dimensiona a partir del bounding box de contenido del
        # PRIMER frame de cada entrada (ver
        # compute_frame_normalization_plan()), así que un frame
        # POSTERIOR de la misma animación con una pose que se extiende
        # más lejos del ancla (p. ej. una oreja que se estira más en el
        # frame 13 que en el frame 0) podría, en principio, exceder ese
        # canvas -- `compose_on_canvas()` recortaría ese exceso en
        # silencio si no se verificara acá. Se detectó exactamente este
        # caso (por poco, <1px) en el `lie_to_sit` real de Frin durante
        # este bloque -- ver el informe. Chequea el bounding box de
        # contenido de ESTE frame específico (no el de su plan,
        # calculado solo con el frame 0) contra el canvas final.
        content_bbox = prep_dev_sprite.compute_content_bbox(width, height, pixels)
        if content_bbox is not None:
            minx, miny, maxx, maxy = content_bbox
            left, top = final_offset_x + minx, final_offset_y + miny
            right, bottom = final_offset_x + maxx + 1, final_offset_y + maxy + 1
            if left < 0 or top < 0 or right > final_working_width or bottom > final_working_height:
                raise PackCompileError(
                    f"{context}: contenido real ({right - left}x{bottom - top} en "
                    f"({left},{top})-({right},{bottom})) excede el canvas de trabajo compartido "
                    f"({final_working_width}x{final_working_height}) -- este frame perdería contenido real "
                    "si se recortara en silencio. El canvas se dimensiona a partir del frame 0 de cada "
                    "animación; si un frame posterior se extiende más lejos del ancla, el margen de "
                    "seguridad de compute_frame_normalization_plan() no alcanzó para este contenido real."
                )
        pixels = prep_dev_sprite.compose_on_canvas(width, height, pixels, final_working_width, final_working_height, final_offset_x, final_offset_y)
        width, height = final_working_width, final_working_height
    elif runtime_max_frame_dimension is not None and max(width, height) > runtime_max_frame_dimension:
        # Sin normalización de contenido activa para este pet -- el
        # downscale de tiempo de compilación de siempre, sin cambios.
        scale = runtime_max_frame_dimension / max(width, height)
        target_w = max(1, round(width * scale))
        target_h = max(1, round(height * scale))
        pixels = prep_dev_sprite.resize_rgba_area_average(width, height, pixels, target_w, target_h)
        width, height = target_w, target_h

    anchor_x = float(frame_manifest.get("anchor_x", width / 2.0))
    anchor_y = float(frame_manifest.get("anchor_y", height / 2.0))
    duration_ms = float(frame_manifest.get("duration_ms", 0.0))

    header = struct.pack("<IIddd", width, height, anchor_x, anchor_y, duration_ms)
    return width, height, header + pixels


def _compile_animation(
    anim_manifest: dict,
    manifest_dir: str,
    context: str,
    runtime_max_frame_dimension: int | None,
    normalization_plan: dict[str, tuple[float, int, int, int, int]] | None,
) -> bytes:
    anim_id = _require(anim_manifest, "id", context)
    full_context = f"{context} ('{anim_id}')"

    kind_str = _require(anim_manifest, "kind", full_context)
    if kind_str not in _KIND_TO_BYTE:
        raise PackCompileError(f"{full_context}: invalid kind '{kind_str}' (expected static/loop/one_shot)")
    kind_byte = _KIND_TO_BYTE[kind_str]

    fps = float(anim_manifest.get("fps", 0.0))
    returns_to_idle = bool(anim_manifest.get("returns_to_idle", True))

    frame_manifests = _require(anim_manifest, "frames", full_context)
    if not frame_manifests:
        raise PackCompileError(f"{full_context}: must have at least one frame")

    normalization = normalization_plan.get(context) if normalization_plan is not None else None

    frame_blobs: list[bytes] = []
    first_dims: tuple[int, int] | None = None
    for i, fm in enumerate(frame_manifests):
        w, h, blob = _compile_frame(fm, manifest_dir, f"{full_context} frame {i}", runtime_max_frame_dimension, normalization)
        if first_dims is None:
            first_dims = (w, h)
        elif (w, h) != first_dims:
            raise PackCompileError(
                f"{full_context} frame {i}: {w}x{h} does not match this "
                f"animation's first frame ({first_dims[0]}x{first_dims[1]}) — "
                "every frame in one animation must share the same dimensions."
            )
        frame_blobs.append(blob)

    out = bytearray()
    out += _pack_string(anim_id)
    out += struct.pack("<B", kind_byte)
    out += struct.pack("<d", fps)
    out += struct.pack("<B", 1 if returns_to_idle else 0)
    out += struct.pack("<I", len(frame_blobs))
    for blob in frame_blobs:
        out += blob
    return bytes(out)


def _compile_direction_overrides(
    override_manifests: list[dict],
    manifest_dir: str,
    context: str,
    runtime_max_frame_dimension: int | None,
    normalization_plan: dict[str, tuple[float, int, int, int, int]] | None,
) -> bytes:
    out = bytearray()
    out += struct.pack("<I", len(override_manifests))
    for i, om in enumerate(override_manifests):
        override_context = f"{context}_direction_overrides[{i}]"
        direction_str = _require(om, "direction", override_context)
        if direction_str not in _DIRECTION_TO_BYTE:
            raise PackCompileError(f"{override_context}: invalid direction '{direction_str}' (expected right/left)")
        out += struct.pack("<B", _DIRECTION_TO_BYTE[direction_str])
        out += _compile_animation(om, manifest_dir, override_context, runtime_max_frame_dimension, normalization_plan)
    return bytes(out)


def _weighted_action_context(state_id: str, trigger_name: str, action_id: str) -> str:
    """La ÚNICA fuente de verdad para cómo se nombra (como `context`,
    la clave del plan de normalización) una entrada compilable de tipo
    WeightedAction -- usada TANTO por `_build_normalization_plan()`
    (para construir las claves del plan) COMO por
    `_compile_weighted_actions()` (para buscarlas), así las dos pasadas
    SIEMPRE calzan por construcción.

    Bug real corregido acá (ver docs/DECISION_LOG.md): antes de esto,
    `_build_normalization_plan()` guardaba cada acción bajo
    `f"state[{id}].{trigger}[{action_id}]"`, pero
    `_compile_weighted_action()` la buscaba bajo
    `f"state[{id}].{trigger} ('{action_id}')"` -- un formato DISTINTO
    escrito a mano en dos lugares que nunca coincidía. El resultado:
    `normalization_plan.get(...)` devolvía `None` para TODA acción
    ambient/hover/click de TODO pet con `normalize_visual_scale: true`
    -- ninguna pasaba nunca por `compose_on_canvas()`, así que cada una
    terminaba compilada a su propia resolución/encuadre NATIVO en vez
    del canvas de trabajo compartido del pet. Visualmente: el personaje
    se veía a un tamaño/posición distintos (típicamente más grande,
    a veces con proporciones distorsionadas) en cualquier animación
    disparada por click/ambient/hover frente a la pose base estática
    (que SÍ usaba la clave correcta) -- exactamente el defecto
    reportado ("quieto se ve chico, animando se ve grande") y la causa
    más probable de la corrupción visual observada en Frin al quedar
    acostado (un salto de escala/posición real en la transición
    sit_to_lie -> lying). Fijar esto en una única función compartida
    hace que esta clase de bug sea estructuralmente imposible de
    reintroducir -- ninguna de las dos pasadas puede divergir por
    copiar el formato a mano en el otro lugar."""
    return f"state[{state_id}].{trigger_name}[{action_id}]"


def _compile_weighted_action(
    action_manifest: dict,
    manifest_dir: str,
    context: str,
    runtime_max_frame_dimension: int | None,
    normalization_plan: dict[str, tuple[float, int, int, int, int]] | None,
) -> bytes:
    """`context` ya es la clave COMPLETA y correcta de esta acción (ver
    `_weighted_action_context()`) -- este helper no le agrega nada."""
    action_id = _require(action_manifest, "id", context)
    weight = float(action_manifest.get("weight", 1.0))
    target_state_id = _require(action_manifest, "target_state_id", context)

    out = bytearray()
    out += _pack_string(action_id)
    out += struct.pack("<d", weight)
    out += _pack_string(target_state_id)
    out += _compile_animation(action_manifest, manifest_dir, context, runtime_max_frame_dimension, normalization_plan)
    out += _compile_direction_overrides(
        action_manifest.get("direction_overrides", []), manifest_dir, context, runtime_max_frame_dimension, normalization_plan
    )
    return bytes(out)


def _compile_weighted_actions(
    action_manifests: list[dict],
    manifest_dir: str,
    state_id: str,
    trigger_name: str,
    runtime_max_frame_dimension: int | None,
    normalization_plan: dict[str, tuple[float, int, int, int, int]] | None,
) -> bytes:
    out = bytearray()
    out += struct.pack("<I", len(action_manifests))
    for am in action_manifests:
        action_id = am.get("id", "?")
        context = _weighted_action_context(state_id, trigger_name, action_id)
        out += _compile_weighted_action(am, manifest_dir, context, runtime_max_frame_dimension, normalization_plan)
    return bytes(out)


def _validate_target_state_ids(states: list[dict]) -> None:
    state_ids = {_require(s, "id", "state") for s in states}
    for state in states:
        state_id = state["id"]
        for trigger_key in ("ambient_actions", "hover_actions", "click_actions"):
            for action in state.get(trigger_key, []):
                target = action.get("target_state_id")
                if target not in state_ids:
                    raise PackCompileError(
                        f"state '{state_id}'.{trigger_key}[{action.get('id', '?')}]: "
                        f"target_state_id '{target}' does not match any state id"
                    )


def _first_frame_pixels(anim_manifest: dict, manifest_dir: str, context: str) -> tuple[int, int, bytes]:
    """Decodifica SOLO el primer frame de una animación -- usado
    exclusivamente por el pre-pass de _build_normalization_plan()."""
    frame_manifests = _require(anim_manifest, "frames", context)
    if not frame_manifests:
        raise PackCompileError(f"{context}: must have at least one frame")
    source = _require(frame_manifests[0], "source", f"{context} frame 0")
    path = os.path.join(manifest_dir, source)
    if not os.path.isfile(path):
        raise PackCompileError(f"{context} frame 0: source frame not found: {path}")
    return prep_dev_sprite.read_png_rgba(path)


def _build_normalization_plan(manifest: dict, manifest_dir: str) -> dict[str, tuple[float, int, int, int, int]]:
    """Pre-pass del feature opcional `normalize_visual_scale` (ver el
    docstring del módulo): recorre TODA la estructura del grafo de
    comportamiento (cada state.base_animation + sus direction_overrides,
    y cada ambient/hover/click action + los suyos), decodificando solo
    el primer frame de cada una, y arma los diccionarios que
    prep_dev_sprite.compute_frame_normalization_plan() necesita --
    `entries`/`groups` (las claves usadas acá son exactamente los mismos
    strings `context` que _compile_animation()/_compile_weighted_action()
    ya reciben más abajo, así que el resultado se puede indexar
    directamente con ese mismo `context` en la segunda pasada real de
    compilación), más `group_frame_paths` (TODAS las rutas de frame de
    cada grupo, no solo la primera -- para que esa función pueda
    detectar cuándo dos grupos comparten un archivo real, p. ej. cuando
    el `base_animation` de un estado ES literalmente el frame final de
    la transición de otro estado) y `state_of_group`/
    `base_group_of_state` (qué BehaviorState autoriza cada grupo, y
    cuál es el `base_animation` de cada estado específicamente) -- ver
    el docstring de esa función y docs/DECISION_LOG.md DEC-075 para por
    qué esto ya no es una comparación de bounding box plana entre
    estados. Todas las entradas de TODOS los estados comparten un único
    canvas de trabajo (el mismo invariante que ya regía Nidir/Bunny de
    un solo estado) -- para un pet con estados reales (Frin) esto
    además evita que el personaje "salte" de tamaño/posición al
    transicionar entre estados."""
    entries: dict[str, tuple[int, int, bytes]] = {}
    groups: dict[str, str] = {}
    group_frame_paths: dict[str, list[str]] = {}
    state_of_group: dict[str, str] = {}
    base_group_of_state: dict[str, str] = {}

    states = _require(manifest, "states", "manifest")
    if not states:
        raise PackCompileError("manifest: 'states' must have at least one entry")

    reference_group = f"state[{states[0]['id']}].base_animation"

    def add_animation(anim_manifest: dict, context: str, group: str, state_id: str) -> None:
        entries[context] = _first_frame_pixels(anim_manifest, manifest_dir, context)
        groups[context] = group
        state_of_group.setdefault(group, state_id)
        paths = group_frame_paths.setdefault(group, [])
        for frame_manifest in anim_manifest.get("frames", []):
            source = frame_manifest.get("source")
            if source is not None:
                paths.append(os.path.realpath(os.path.join(manifest_dir, source)))

    def add_actions(action_manifests: list[dict], state_id: str, trigger_name: str) -> None:
        for am in action_manifests:
            action_id = am.get("id", "?")
            context = _weighted_action_context(state_id, trigger_name, action_id)
            group = f"state[{state_id}].{trigger_name}.{action_id}"
            add_animation(am, context, group, state_id)
            for i, om in enumerate(am.get("direction_overrides", [])):
                override_context = f"{context}_direction_overrides[{i}]"
                add_animation(om, override_context, group, state_id)  # right/left share content_scale

    for state in states:
        state_id = state["id"]
        base_context = f"state[{state_id}].base_animation"
        base_group = f"state[{state_id}].base_animation"
        base_group_of_state[state_id] = base_group
        base_manifest = _require(state, "base_animation", f"state '{state_id}'")
        add_animation(base_manifest, base_context, base_group, state_id)
        for i, om in enumerate(state.get("base_animation_direction_overrides", [])):
            override_context = f"{base_context}_direction_overrides[{i}]"
            add_animation(om, override_context, base_group, state_id)

        add_actions(state.get("ambient_actions", []), state_id, "ambient_actions")
        if not bool(state.get("hover_uses_ambient_actions", True)):
            add_actions(state.get("hover_actions", []), state_id, "hover_actions")
        add_actions(state.get("click_actions", []), state_id, "click_actions")

    return prep_dev_sprite.compute_frame_normalization_plan(
        entries,
        groups,
        reference_group=reference_group,
        group_frame_paths=group_frame_paths,
        state_of_group=state_of_group,
        base_group_of_state=base_group_of_state,
    )


def compile_pack(manifest_path: str, output_path: str) -> tuple[str, int]:
    manifest_dir = os.path.dirname(os.path.abspath(manifest_path))
    with open(manifest_path, "r", encoding="utf-8") as f:
        manifest = json.load(f)

    pet_id = _require(manifest, "id", "manifest")
    display_name = _require(manifest, "display_name", "manifest")
    variant_group = str(manifest.get("variant_group", ""))
    canvas_width = int(_require(manifest, "canvas_width", "manifest"))
    canvas_height = int(_require(manifest, "canvas_height", "manifest"))
    if canvas_width <= 0 or canvas_height <= 0:
        raise PackCompileError(f"manifest: canvas size {canvas_width}x{canvas_height} must be positive")

    alpha_threshold = int(manifest.get("alpha_hit_threshold", 128))
    if not 0 <= alpha_threshold <= 255:
        raise PackCompileError(f"manifest: alpha_hit_threshold {alpha_threshold} out of range 0-255")

    visual_scale = float(manifest.get("visual_scale", 1.0))
    if visual_scale <= 0.0:
        raise PackCompileError(f"manifest: visual_scale {visual_scale} must be positive")

    content_version = str(manifest.get("content_version", ""))

    runtime_max_frame_dimension = manifest.get("runtime_max_frame_dimension")
    if runtime_max_frame_dimension is not None:
        runtime_max_frame_dimension = int(runtime_max_frame_dimension)
        if runtime_max_frame_dimension <= 0:
            raise PackCompileError(f"manifest: runtime_max_frame_dimension {runtime_max_frame_dimension} must be positive")

    states = _require(manifest, "states", "manifest")
    if not states:
        raise PackCompileError("manifest: 'states' must have at least one entry")
    _validate_target_state_ids(states)

    normalization_plan = _build_normalization_plan(manifest, manifest_dir) if bool(manifest.get("normalize_visual_scale", False)) else None

    out = bytearray()
    out += b"NVPACK2\0"
    out += _pack_string(pet_id)
    out += _pack_string(display_name)
    out += _pack_string(variant_group)
    out += struct.pack("<II", canvas_width, canvas_height)
    out += struct.pack("<B", alpha_threshold)
    out += struct.pack("<d", visual_scale)
    out += _pack_string(content_version)

    out += struct.pack("<I", len(states))
    for state in states:
        state_id = _require(state, "id", "state")
        out += _pack_string(state_id)

        base_context = f"state[{state_id}].base_animation"
        base_manifest = _require(state, "base_animation", f"state '{state_id}'")
        out += _compile_animation(base_manifest, manifest_dir, base_context, runtime_max_frame_dimension, normalization_plan)
        out += _compile_direction_overrides(
            state.get("base_animation_direction_overrides", []), manifest_dir, base_context, runtime_max_frame_dimension,
            normalization_plan
        )

        out += struct.pack("<d", float(state.get("ambient_interval_seconds", 300.0)))
        out += _compile_weighted_actions(
            state.get("ambient_actions", []), manifest_dir, state_id, "ambient_actions", runtime_max_frame_dimension,
            normalization_plan
        )

        hover_uses_ambient_actions = bool(state.get("hover_uses_ambient_actions", True))
        hover_actions = state.get("hover_actions", [])
        if hover_uses_ambient_actions and hover_actions:
            raise PackCompileError(
                f"state '{state_id}': hover_uses_ambient_actions is true but hover_actions is also "
                "non-empty (ambiguous — pick one)"
            )
        out += struct.pack("<B", 1 if hover_uses_ambient_actions else 0)
        out += _compile_weighted_actions(
            hover_actions, manifest_dir, state_id, "hover_actions", runtime_max_frame_dimension, normalization_plan
        )

        out += _compile_weighted_actions(
            state.get("click_actions", []), manifest_dir, state_id, "click_actions", runtime_max_frame_dimension,
            normalization_plan
        )

    with open(output_path, "wb") as f:
        f.write(out)

    return pet_id, len(out)


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2

    manifest_path, output_path = sys.argv[1], sys.argv[2]
    try:
        pet_id, total_bytes = compile_pack(manifest_path, output_path)
    except (PackCompileError, FileNotFoundError, json.JSONDecodeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    print(f"{output_path}: compiled pet '{pet_id}' ({total_bytes} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
