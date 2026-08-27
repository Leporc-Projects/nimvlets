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
      "first_frame_is_state_base": "seated",   # optional, BUILD-TIME ONLY (never reaches the
      "last_frame_is_state_base": "seated",    # compiled .nvpack). IDENTIDAD SEMÁNTICA DE POSE
                                     # (DEC-099): content asserting "my first / my last frame IS
                                     # that state's stable base pose". When declared, the compiler
                                     # does not MEASURE how close that endpoint is to the base and
                                     # then correct it -- it compiles that frame FROM THE BASE'S OWN
                                     # SOURCE FILE, with the base's own transform, so the compiled
                                     # frame is byte-identical to the base's. No tolerance, no
                                     # residual, no metric at the boundary.
                                     #
                                     # The source PNGs on disk are never modified: the substitution
                                     # is a compile-time reference, so provenance is preserved.
                                     #
                                     # Replaces `align_endpoint_to_target_base` (DEC-092/095/098) and
                                     # `anchor_start_to_source_base` (DEC-097), both removed. Those
                                     # tried to make an endpoint approximately match a base pose by
                                     # adjusting scale/placement; this states the identity outright,
                                     # which is what the content actually means. Direction is
                                     # resolved per-direction: a "left" override substitutes the
                                     # target state's "left" base frame.
      "stable_pose_tail_frames": 8,   # optional, default 1, BUILD-TIME ONLY. How many TERMINAL
                                     # frames represent `last_frame_is_state_base`'s pose, not just
                                     # the last one. For an export whose root motion does not close
                                     # against the target base, those trailing frames are the
                                     # character standing still IN THE WRONG PLACE, followed by a
                                     # visible jump. Declaring the run lets the compiler replace it
                                     # with the exact base frame -- no invented motion, no authored
                                     # frame touched (the last surviving one is the last one still
                                     # moving), and the clip keeps its exact frame count and
                                     # duration. See DEC-101.
      "match_aspect_to_stable_poses": false,  # optional, default false, BUILD-TIME ONLY. Derives ONE
                                     # constant (scale_x, scale_y) for the whole sequence from the
                                     # declared stable-pose correspondence above, replacing the
                                     # uniform RMS-derived scale. For an export that brings the
                                     # character at a different ASPECT RATIO than the base pose, no
                                     # uniform factor can match both axes at once and the sequence
                                     # reads as globally wider-and-shorter than the pet at rest.
                                     # Constant for every frame -- never per-frame, never
                                     # interpolated. Derived only from the two extreme endpoints:
                                     # intermediate frames change silhouette on purpose, and
                                     # measuring them would confuse animation with error. See
                                     # DEC-100.
      "match_color_to_stable_poses": false,   # optional, default false, BUILD-TIME ONLY. Derives ONE
                                     # constant per-channel RGB gain the same way, for an export
                                     # that is globally darker/lighter than the base pose. Alpha is
                                     # never touched, the gain never varies frame-to-frame (no auto
                                     # exposure), and substituted boundary frames never receive it
                                     # -- they already ARE the base pixels. See DEC-102.
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
      ],
      "terminal_rigid_translation": {   # optional, BUILD-TIME ONLY (never reaches the compiled
        "start_frame": 17,              # .nvpack). Content saying "the export's own root motion
        "dx": 23.44, "dy": -6.33        # doesn't land at the declared stable pose, but a single
      }                                 # constant nudge (dx, dy) makes it land close enough that
                                     # from `start_frame` onward the AUTHORED pixels can stay, moved
                                     # rather than replaced". Applies to frames [start_frame, end) of
                                     # THIS animation only, in compiled-frame pixels (this animation's
                                     # own resolution, after any runtime downscale) -- never touches
                                     # scale, never touches a frame already covered by
                                     # `first_frame_is_state_base`/`last_frame_is_state_base` (those
                                     # keep using their base's own exact pixels and placement,
                                     # unaffected). ONE constant vector for the whole declared range
                                     # -- never interpolated, never a function of frame index beyond
                                     # this single before/after split. This is the opposite of the
                                     # per-frame translation schedule DEC-097 removed: that computed a
                                     # different offset for every frame to force smooth root motion
                                     # the export doesn't have; this places a handful of terminal
                                     # frames — already close to the target pose — at the position
                                     # their own content says they belong, and nowhere else. See
                                     # DEC-105.

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
import math
import os
import dataclasses
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


@dataclasses.dataclass(frozen=True)
class ContentPlan:
    """Lo que el pre-pass de contenido le pasa a la compilación real.

    Un solo objeto en vez de los cinco diccionarios paralelos que se
    enhebraban antes (`last_frames`, `transition_target_entry`,
    `start_anchor_entry`, `scale_from_last_frame_entries`,
    `strict_scale_entries`) -- ver DEC-099: esos existían todos para
    aproximar una punta a una pose base, trabajo que ahora hace
    `boundary_base_frames` de forma exacta.

    `normalization`: context -> (content_scale, canvas_w, canvas_h,
    offset_x, offset_y). UNA transforma por entrada, para TODOS sus
    frames.

    `boundary_base_frames`: context -> {índice_de_frame_absoluto:
    (frame_manifest de la base, context de la base)}. El frame en ese
    índice se compila desde el archivo Y con la transforma de esa base,
    así que sale idéntico byte a byte al frame compilado de la base."""

    normalization: dict[str, tuple[tuple[float, float], int, int, int, int]]
    boundary_base_frames: dict[str, dict[int, tuple[dict, str]]]
    color_gain: dict[str, tuple[float, float, float]] = dataclasses.field(default_factory=dict)

    def normalization_for(self, context: str) -> tuple[float, int, int, int, int] | None:
        return self.normalization.get(context)

    def boundary_for(self, context: str) -> dict[int, tuple[dict, str]]:
        return self.boundary_base_frames.get(context, {})

    def color_gain_for(self, context: str) -> tuple[float, float, float] | None:
        return self.color_gain.get(context)


def _compile_frame(
    frame_manifest: dict,
    manifest_dir: str,
    context: str,
    runtime_max_frame_dimension: int | None,
    normalization: tuple[tuple[float, float], int, int, int, int] | None,
    color_gain: tuple[float, float, float] | None = None,
    extra_offset: tuple[int, int] | None = None,
) -> tuple[int, int, bytes]:
    source = _require(frame_manifest, "source", context)
    path = os.path.join(manifest_dir, source)
    if not os.path.isfile(path):
        raise PackCompileError(f"{context}: source frame not found: {path}")

    width, height, pixels = prep_dev_sprite.read_png_rgba(path)
    if len(pixels) != width * height * 4:
        raise PackCompileError(f"{context}: decoded pixel data size mismatch for {path}")

    # Ganancia de color constante de secuencia (ver
    # `match_color_to_stable_poses`), ANTES de cualquier reescalado: se
    # aplica sobre los pixeles nativos, donde todavía hay toda la
    # información. Alpha nunca se toca.
    if color_gain is not None:
        pixels = prep_dev_sprite.apply_rgb_gain(width, height, pixels, color_gain)

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
        (content_scale_x, content_scale_y), working_width, working_height, offset_x, offset_y = normalization

        runtime_ratio = 1.0
        if runtime_max_frame_dimension is not None and max(working_width, working_height) > runtime_max_frame_dimension:
            runtime_ratio = runtime_max_frame_dimension / max(working_width, working_height)

        combined_scale_x = content_scale_x * runtime_ratio
        combined_scale_y = content_scale_y * runtime_ratio
        final_working_width = max(1, round(working_width * runtime_ratio))
        final_working_height = max(1, round(working_height * runtime_ratio))
        final_offset_x = round(offset_x * runtime_ratio)
        final_offset_y = round(offset_y * runtime_ratio)

        # Traslación rígida terminal, opcional (ver
        # `terminal_rigid_translation`, DEC-105): un desplazamiento
        # CONSTANTE en pixeles del frame COMPILADO (esta misma
        # resolución, después del downscale de runtime), sumado a la
        # colocación de ESTE frame puntual. Es la ÚNICA forma en que
        # este módulo permite que dos frames de una misma animación
        # terminen en offsets distintos -- y es deliberadamente NO una
        # transforma "por frame" en el sentido que DEC-097 prohíbe: no
        # depende del índice del frame más que por un corte binario
        # (antes/después de `start_frame`), nunca interpola ni
        # progresa. No cambia la escala ni el tamaño del canvas -- las
        # mismas dimensiones compiladas de siempre, solo compuesto en
        # otro lugar dentro de él.
        if extra_offset is not None:
            final_offset_x += extra_offset[0]
            final_offset_y += extra_offset[1]

        if abs(combined_scale_x - 1.0) > 1e-9 or abs(combined_scale_y - 1.0) > 1e-9:
            target_w = max(1, round(width * combined_scale_x))
            target_h = max(1, round(height * combined_scale_y))
            # El filtro se elige por el eje que MÁS reduce: un area-average
            # preserva mucho mejor el contenido cuando se achica, y no
            # perjudica al otro eje si ese apenas cambia.
            shrinking = combined_scale_x < 1.0 or combined_scale_y < 1.0
            resize_fn = prep_dev_sprite.resize_rgba_area_average if shrinking else prep_dev_sprite.resize_rgba_nearest
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
    content_plan: ContentPlan | None,
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

    normalization = content_plan.normalization_for(context) if content_plan is not None else None
    boundary = content_plan.boundary_for(context) if content_plan is not None else {}
    color_gain = content_plan.color_gain_for(context) if content_plan is not None else None

    # UNA sola `normalization` (escala + canvas + offset) para TODOS los
    # frames de esta animación -- nunca por-frame. Ver DEC-097: la
    # variante por-frame que existió acá se retiró porque producía
    # root-motion artificial visible.
    #
    # La ÚNICA excepción es una punta que el contenido declara como "esta
    # ES la pose base estable del estado X" (`first_frame_is_state_base`/
    # `last_frame_is_state_base`, DEC-099): ese frame se compila desde el
    # ARCHIVO de esa base y con la transforma de esa base, así que sale
    # idéntico byte a byte al frame que el runtime muestra cuando está
    # quieto en ese estado. No es una transforma por-frame inventada por
    # el compilador -- es literalmente el mismo frame, referenciado.
    # Traslación rígida terminal, opcional (ver `terminal_rigid_translation`
    # en el docstring del módulo y DEC-105): UN desplazamiento constante
    # (dx, dy), aplicado a todos los frames desde `start_frame` en
    # adelante -- salvo los que ya estén cubiertos por una sustitución
    # de punta (`boundary`), que siguen usando los pixeles Y la
    # colocación exactos de su propia base, sin tocar. Es contenido de
    # ESTA animación puntual (no un mecanismo genérico del compilador
    # que otras animaciones activen sin declararlo).
    terminal = anim_manifest.get("terminal_rigid_translation")
    terminal_start = int(terminal["start_frame"]) if terminal is not None else None
    terminal_offset = (round(terminal["dx"]), round(terminal["dy"])) if terminal is not None else None

    frame_blobs: list[bytes] = []
    first_dims: tuple[int, int] | None = None
    last_index = len(frame_manifests) - 1
    for i, fm in enumerate(frame_manifests):
        substitution = boundary.get(i)
        frame_normalization = normalization
        frame_color_gain = color_gain
        frame_extra_offset = (
            terminal_offset if terminal_start is not None and i >= terminal_start and substitution is None
            else None
        )
        if substitution is not None:
            base_frame_manifest, base_context = substitution
            # Se conserva el `duration_ms` autorado por ESTA animación
            # (el contrato de timing es suyo); solo los pixeles y su
            # encuadre vienen de la base.
            fm = {**base_frame_manifest, "duration_ms": fm.get("duration_ms", 0.0)}
            frame_normalization = (
                content_plan.normalization_for(base_context) if content_plan is not None else None
            )
            # Una punta sustituida ES la pose base: sus pixeles ya son
            # los correctos, así que NUNCA se le aplica la ganancia de
            # color de la secuencia -- aplicarla la sacaría del color de
            # la base, que es justamente lo que la ganancia corrige.
            frame_color_gain = None
        w, h, blob = _compile_frame(
            fm, manifest_dir, f"{full_context} frame {i}", runtime_max_frame_dimension,
            frame_normalization, frame_color_gain, frame_extra_offset)
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
    content_plan: ContentPlan | None,
) -> bytes:
    out = bytearray()
    out += struct.pack("<I", len(override_manifests))
    for i, om in enumerate(override_manifests):
        override_context = f"{context}_direction_overrides[{i}]"
        direction_str = _require(om, "direction", override_context)
        if direction_str not in _DIRECTION_TO_BYTE:
            raise PackCompileError(f"{override_context}: invalid direction '{direction_str}' (expected right/left)")
        out += struct.pack("<B", _DIRECTION_TO_BYTE[direction_str])
        out += _compile_animation(om, manifest_dir, override_context, runtime_max_frame_dimension, content_plan)
    return bytes(out)


def _weighted_action_context(state_id: str, trigger_name: str, action_id: str) -> str:
    """La ÚNICA fuente de verdad para cómo se nombra (como `context`,
    la clave del plan de normalización) una entrada compilable de tipo
    WeightedAction -- usada TANTO por `_build_content_plan()`
    (para construir las claves del plan) COMO por
    `_compile_weighted_actions()` (para buscarlas), así las dos pasadas
    SIEMPRE calzan por construcción.

    Bug real corregido acá (ver docs/DECISION_LOG.md): antes de esto,
    `_build_content_plan()` guardaba cada acción bajo
    `f"state[{id}].{trigger}[{action_id}]"`, pero
    `_compile_weighted_action()` la buscaba bajo
    `f"state[{id}].{trigger} ('{action_id}')"` -- un formato DISTINTO
    escrito a mano en dos lugares que nunca coincidía. El resultado:
    el lookup del plan devolvía `None` para TODA acción
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
    content_plan: ContentPlan | None,
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
    out += _compile_animation(action_manifest, manifest_dir, context, runtime_max_frame_dimension, content_plan)
    out += _compile_direction_overrides(
        action_manifest.get("direction_overrides", []), manifest_dir, context, runtime_max_frame_dimension,
        content_plan
    )
    return bytes(out)


def _compile_weighted_actions(
    action_manifests: list[dict],
    manifest_dir: str,
    state_id: str,
    trigger_name: str,
    runtime_max_frame_dimension: int | None,
    content_plan: ContentPlan | None,
) -> bytes:
    out = bytearray()
    out += struct.pack("<I", len(action_manifests))
    for am in action_manifests:
        action_id = am.get("id", "?")
        context = _weighted_action_context(state_id, trigger_name, action_id)
        out += _compile_weighted_action(am, manifest_dir, context, runtime_max_frame_dimension, content_plan)
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


def _frame_pixels_at(anim_manifest: dict, manifest_dir: str, context: str, index: int) -> tuple[int, int, bytes]:
    """Decodifica UN frame puntual por índice. Usado solo para DERIVAR
    las correcciones que el contenido declara (aspecto de export y
    ganancia de color) a partir de la correspondencia de poses estables.

    Esto NO es el `_nth_frame_pixels()` que DEC-099 eliminó: aquel leía
    el último frame para MEDIR qué tan lejos había quedado de una base y
    corregir esa distancia. Acá el contenido ya afirma que ese frame ES
    esa pose; lo único que se mide es en qué sistema de proporciones (o
    de exposición) la trae su export."""
    frame_manifests = _require(anim_manifest, "frames", context)
    if not frame_manifests:
        raise PackCompileError(f"{context}: must have at least one frame")
    resolved = index if index >= 0 else len(frame_manifests) + index
    source = _require(frame_manifests[resolved], "source", f"{context} frame {resolved}")
    path = os.path.join(manifest_dir, source)
    if not os.path.isfile(path):
        raise PackCompileError(f"{context} frame {resolved}: source frame not found: {path}")
    return prep_dev_sprite.read_png_rgba(path)


def _first_frame_pixels(anim_manifest: dict, manifest_dir: str, context: str) -> tuple[int, int, bytes]:
    """Decodifica el PRIMER frame de una animación -- usado
    exclusivamente por el pre-pass de _build_content_plan(), que nunca
    necesita la secuencia entera ni ningún otro frame: el frame 0 es el
    ancla de escala/colocación de cada entrada.

    Hasta DEC-099 existía una variante `_nth_frame_pixels()` porque el
    pre-pass también decodificaba el ÚLTIMO frame de una transición
    para medirlo contra su base (DEC-087). Esa medición ya no existe
    -- la punta se compila desde el archivo de la base, no se compara
    contra él."""
    frame_manifests = _require(anim_manifest, "frames", context)
    if not frame_manifests:
        raise PackCompileError(f"{context}: must have at least one frame")
    source = _require(frame_manifests[0], "source", f"{context} frame 0")
    path = os.path.join(manifest_dir, source)
    if not os.path.isfile(path):
        raise PackCompileError(f"{context} frame 0: source frame not found: {path}")
    return prep_dev_sprite.read_png_rgba(path)


def _grow_working_canvas_for_runtime_rounding(
    plan: dict[str, tuple[float, int, int, int, int]],
    native_sizes: dict[str, tuple[int, int]],
    runtime_max_frame_dimension: int,
) -> dict[str, tuple[float, int, int, int, int]]:
    """Agranda el canvas de trabajo compartido lo mínimo necesario para
    que el redondeo del downscale de runtime no deje ningún frame
    colgando 1px fuera.

    Por qué hace falta (Block 05, pasada de estabilización): el plan
    garantiza que cada frame entra en el canvas EN UNIDADES NATIVAS,
    pero `_compile_frame()` redondea TRES cosas por separado al pasar a
    espacio final -- el canvas (`round(working * ratio)`), el offset
    (`round(offset * ratio)`) y el tamaño del frame
    (`round(nativo * content_scale * ratio)`). Tres redondeos
    independientes pueden sumar hasta ~1px de deriva, y entonces
    `offset_final + ancho_final` supera al canvas final por 1 aunque en
    float entrara perfecto. Medido en el `lie_to_sit` real de Frin
    hembra: 4 + 294 = 298 contra un canvas de 297.

    Antes esto no aparecía por suerte aritmética, no por diseño -- el
    guard ruidoso de `_compile_frame()` existe precisamente porque el
    dimensionado nunca fue exacto. Esta función lo vuelve exacto en vez
    de dejarlo al azar: replica la MISMA aritmética que
    `_compile_frame()` va a usar, mide cuánto falta de verdad, y crece
    el canvas ese mínimo. Si nada falta -- el caso de Bunny/Nidir hoy --
    devuelve el plan sin tocar un solo valor.

    El ratio depende de `max(working_width, working_height)`, así que
    crecer puede cambiarlo; se itera hasta punto fijo, con un tope duro
    para no depender de que converja."""
    if not plan:
        return plan

    def needed(working_width: int, working_height: int) -> tuple[int, int]:
        ratio = 1.0
        if max(working_width, working_height) > runtime_max_frame_dimension:
            ratio = runtime_max_frame_dimension / max(working_width, working_height)
        need_w = 0
        need_h = 0
        for entry_key, ((content_scale_x, content_scale_y), _w, _h, offset_x, offset_y) in plan.items():
            native_w, native_h = native_sizes[entry_key]
            combined_x = content_scale_x * ratio
            combined_y = content_scale_y * ratio
            frame_w = native_w if abs(combined_x - 1.0) <= 1e-9 else max(1, round(native_w * combined_x))
            frame_h = native_h if abs(combined_y - 1.0) <= 1e-9 else max(1, round(native_h * combined_y))
            need_w = max(need_w, round(offset_x * ratio) + frame_w)
            need_h = max(need_h, round(offset_y * ratio) + frame_h)
        have_w = max(1, round(working_width * ratio))
        have_h = max(1, round(working_height * ratio))
        return need_w - have_w, need_h - have_h

    _first = next(iter(plan.values()))
    working_width, working_height = _first[1], _first[2]
    for _ in range(16):
        deficit_w, deficit_h = needed(working_width, working_height)
        if deficit_w <= 0 and deficit_h <= 0:
            break
        # El déficit se mide en espacio FINAL; se traduce a nativo con
        # el ratio vigente y se redondea hacia arriba.
        ratio = 1.0
        if max(working_width, working_height) > runtime_max_frame_dimension:
            ratio = runtime_max_frame_dimension / max(working_width, working_height)
        if deficit_w > 0:
            working_width += max(1, math.ceil(deficit_w / ratio))
        if deficit_h > 0:
            working_height += max(1, math.ceil(deficit_h / ratio))
    else:
        raise PackCompileError(
            "no se pudo dimensionar el canvas de trabajo compartido de forma consistente con el "
            "downscale de runtime tras 16 iteraciones -- esto no debería pasar; revisar "
            "_grow_working_canvas_for_runtime_rounding()"
        )

    if (working_width, working_height) == (_first[1], _first[2]):
        return plan
    return {
        key: (content_scale, working_width, working_height, offset_x, offset_y)
        for key, (content_scale, _w, _h, offset_x, offset_y) in plan.items()
    }


def _build_content_plan(manifest: dict, manifest_dir: str) -> ContentPlan:
    """Pre-pass de contenido: recorre TODA la estructura del grafo de
    comportamiento (cada state.base_animation + sus direction_overrides,
    y cada ambient/hover/click action + los suyos) UNA vez y arma las
    dos cosas que la compilación real necesita.

    1. `boundary_base_frames` -- IDENTIDAD SEMÁNTICA DE POSE (DEC-099).
       Resuelve `first_frame_is_state_base`/`last_frame_is_state_base`
       al `base_animation` del estado nombrado, en la MISMA dirección
       (cayendo a la entrada canónica si ese estado no define override
       para esa dirección). Es puramente estructural: no decodifica
       ningún pixel ni mide nada. Que la punta "sea" la pose base es
       una afirmación del CONTENIDO, no una conclusión geométrica.

    2. `normalization` -- el plan de escala/encuadre de siempre (feature
       opcional `normalize_visual_scale`, ver el docstring del módulo).
       Las claves son exactamente los mismos strings `context` que
       _compile_animation()/_compile_weighted_action() reciben más
       abajo, así que la segunda pasada las indexa directamente.
       `group_frame_paths` lleva TODAS las rutas de frame de cada grupo
       (no solo la primera) para que
       prep_dev_sprite.compute_frame_normalization_plan() pueda
       detectar cuándo dos grupos comparten un archivo real -- p. ej.
       cuando el `base_animation` de un estado ES literalmente el frame
       final de la transición de otro estado. Todas las entradas de
       TODOS los estados comparten un único canvas de trabajo, así que
       un pet con estados nunca "salta" de tamaño al transicionar.

    Cada entrada recibe UNA sola transforma (escala + offset) para
    todos sus frames -- nunca una por frame (DEC-097).

    Lo que este pre-pass YA NO hace (DEC-099): registrar puntas para
    anclarlas contra una base por medición (`transition_target_entry`,
    `start_anchor_entry`), ni derivar escalas de un último frame
    (`scale_from_last_frame_entries`), ni saltear la tolerancia de
    escala en una frontera (`strict_scale_entries`). Todo eso aproximaba
    lo que ahora es exacto por sustitución de frame canónico."""
    normalize = bool(manifest.get("normalize_visual_scale", False))

    entries: dict[str, tuple[int, int, bytes]] = {}
    groups: dict[str, str] = {}
    group_frame_paths: dict[str, list[str]] = {}
    entry_frame_paths: dict[str, list[str]] = {}
    state_of_group: dict[str, str] = {}
    base_group_of_state: dict[str, str] = {}
    # (state_id, direction|None) -> entry_key del base_animation de ese
    # estado para ESA dirección, y el manifest de su primer frame: lo
    # que una punta declarada como "esta ES la pose base de ese estado"
    # necesita para compilarse desde el MISMO archivo y con la MISMA
    # transforma que esa base.
    base_entry_of_state_direction: dict[tuple[str, str | None], str] = {}
    base_frame_manifest_of_entry: dict[str, dict] = {}
    # entry_key -> {índice de frame absoluto: (state_id, direction)}
    pending_boundary: dict[str, dict[int, tuple[str, str | None]]] = {}
    # Acciones que declararon querer una corrección derivada de su
    # correspondencia de poses estables. El aspecto es por GRUPO (las
    # dos direcciones son espejo, así que comparten sigmas y deben
    # compartir escala); el color es por ENTRADA, porque se aplica
    # frame a frame en la compilación.
    aspect_requests: list[tuple[str, str, dict]] = []
    color_requests: list[tuple[str, dict]] = []

    states = _require(manifest, "states", "manifest")
    if not states:
        raise PackCompileError("manifest: 'states' must have at least one entry")

    reference_group = f"state[{states[0]['id']}].base_animation"

    def add_animation(anim_manifest: dict, context: str, group: str, state_id: str) -> None:
        if normalize:
            entries[context] = _first_frame_pixels(anim_manifest, manifest_dir, context)
        groups[context] = group
        state_of_group.setdefault(group, state_id)
        paths = group_frame_paths.setdefault(group, [])
        own: list[str] = []
        for frame_manifest in anim_manifest.get("frames", []):
            source = frame_manifest.get("source")
            if source is not None:
                resolved = os.path.realpath(os.path.join(manifest_dir, source))
                paths.append(resolved)
                own.append(resolved)
        entry_frame_paths[context] = own

    def mark_boundaries(anim_manifest: dict, context: str, first_base: str | None,
                        last_base: str | None, direction: str | None, tail_count: int = 1) -> None:
        """`first_frame_is_state_base`/`last_frame_is_state_base` son
        propiedades de la ACCIÓN (misma familia que `target_state_id`,
        que tampoco se re-lee por-dirección), pero se RESUELVEN por
        dirección: el override "left" sustituye por el frame base
        "left" del estado nombrado, no por el canónico."""
        frame_manifests = anim_manifest.get("frames", [])
        if not frame_manifests:
            return
        marks = pending_boundary.setdefault(context, {})
        if first_base is not None:
            marks[0] = (first_base, direction)
        if last_base is not None:
            # `stable_pose_tail_frames` (default 1): cuántos frames
            # FINALES representan esa pose estable, no solo el último.
            # Existe porque un export puede quedarse quieto en la pose
            # de llegada durante varios frames -- si su root-motion no
            # cierra contra la base, esos frames son el personaje
            # INMÓVIL EN EL LUGAR EQUIVOCADO, y después un salto. Que el
            # contenido declare cuántos son deja que el compilador los
            # reemplace por la base exacta: sin inventar movimiento y
            # sin tocar un solo frame de la animación real. Ver DEC-101.
            first_tail = max(1, len(frame_manifests) - tail_count)
            for index in range(first_tail, len(frame_manifests)):
                marks[index] = (last_base, direction)

    def add_actions(action_manifests: list[dict], state_id: str, trigger_name: str) -> None:
        for am in action_manifests:
            action_id = am.get("id", "?")
            context = _weighted_action_context(state_id, trigger_name, action_id)
            group = f"state[{state_id}].{trigger_name}.{action_id}"
            add_animation(am, context, group, state_id)
            first_base = am.get("first_frame_is_state_base")
            last_base = am.get("last_frame_is_state_base")
            tail_count = int(am.get("stable_pose_tail_frames", 1))
            if tail_count < 1:
                raise PackCompileError(f"{context}: stable_pose_tail_frames must be >= 1")
            wants_aspect = bool(am.get("match_aspect_to_stable_poses", False))
            wants_color = bool(am.get("match_color_to_stable_poses", False))
            mark_boundaries(am, context, first_base, last_base, None, tail_count)
            if wants_aspect:
                aspect_requests.append((context, group, am))
            if wants_color:
                color_requests.append((context, am))
            for i, om in enumerate(am.get("direction_overrides", [])):
                override_context = f"{context}_direction_overrides[{i}]"
                add_animation(om, override_context, group, state_id)  # right/left share content_scale
                mark_boundaries(om, override_context, first_base, last_base, om.get("direction"), tail_count)
                if wants_color:
                    color_requests.append((override_context, om))

    for state in states:
        state_id = state["id"]
        base_context = f"state[{state_id}].base_animation"
        base_group = f"state[{state_id}].base_animation"
        base_group_of_state[state_id] = base_group
        base_manifest = _require(state, "base_animation", f"state '{state_id}'")
        add_animation(base_manifest, base_context, base_group, state_id)
        base_entry_of_state_direction[(state_id, None)] = base_context
        base_frame_manifest_of_entry[base_context] = _require(
            base_manifest, "frames", f"state '{state_id}'.base_animation")[0]
        for i, om in enumerate(state.get("base_animation_direction_overrides", [])):
            override_context = f"{base_context}_direction_overrides[{i}]"
            add_animation(om, override_context, base_group, state_id)
            base_entry_of_state_direction[(state_id, om.get("direction"))] = override_context
            base_frame_manifest_of_entry[override_context] = _require(
                om, "frames", f"state '{state_id}'.base_animation_direction_overrides[{i}]")[0]

        add_actions(state.get("ambient_actions", []), state_id, "ambient_actions")
        if not bool(state.get("hover_uses_ambient_actions", True)):
            add_actions(state.get("hover_actions", []), state_id, "hover_actions")
        add_actions(state.get("click_actions", []), state_id, "click_actions")

    boundary_base_frames: dict[str, dict[int, tuple[dict, str]]] = {}
    # entry_key -> entry_key de la base del estado en el que el clip
    # ARRANCA. Se deriva de la MISMA declaración que la sustitución de
    # punta (`first_frame_is_state_base`), nunca de un flag propio: si
    # el contenido dice "mi frame 0 ES la pose base de tal estado",
    # entonces dónde vive esa base es dónde tiene que arrancar el clip.
    # Ver `start_base_entry` en compute_frame_normalization_plan().
    start_base_entry: dict[str, str] = {}
    for entry_key, marks in pending_boundary.items():
        for index, (target_state_id, direction) in marks.items():
            base_entry = base_entry_of_state_direction.get((target_state_id, direction))
            if base_entry is None:
                base_entry = base_entry_of_state_direction.get((target_state_id, None))
            if base_entry is None:
                raise PackCompileError(
                    f"{entry_key}: first/last_frame_is_state_base names state "
                    f"'{target_state_id}', which has no base_animation"
                )
            boundary_base_frames.setdefault(entry_key, {})[index] = (
                base_frame_manifest_of_entry[base_entry], base_entry)
            # Solo el ARRANQUE define el sistema de coordenadas, y solo
            # si no hay ya un archivo compartido que lo ate (containment
            # es exacto por construcción y tiene prioridad).
            if index == 0 and not (set(entry_frame_paths.get(entry_key, ()))
                                   & set(entry_frame_paths.get(base_entry, ()))):
                start_base_entry[entry_key] = base_entry

    def _base_pixels(frame_manifest: dict, context: str) -> tuple[int, int, bytes]:
        source = _require(frame_manifest, "source", context)
        path = os.path.join(manifest_dir, source)
        if not os.path.isfile(path):
            raise PackCompileError(f"{context}: source frame not found: {path}")
        return prep_dev_sprite.read_png_rgba(path)

    def _stable_correspondences(entry_key: str, anim_manifest: dict):
        """Los pares (frame autorado de ESTA secuencia, frame de la pose
        base que ese frame DECLARA ser) en las dos puntas extremas.

        Solo las puntas: los frames de en medio cambian de silueta y de
        sombreado a propósito, y derivar una corrección de ellos sería
        confundir animación con error -- medido en `tail_greet` de Frin,
        cuya aspecto por frame oscila hasta 21% mientras sus dos puntas
        coinciden con la base dentro del 0.2%."""
        marks = boundary_base_frames.get(entry_key, {})
        frame_count = len(anim_manifest.get("frames", []))
        out = []
        for index in (0, frame_count - 1):
            if index not in marks:
                continue
            base_frame_manifest, _base_context = marks[index]
            out.append((_frame_pixels_at(anim_manifest, manifest_dir, entry_key, index),
                        _base_pixels(base_frame_manifest, f"{entry_key} stable-pose base for frame {index}")))
        return out

    # --- Corrección de ASPECTO de export (DEC-100) --------------------
    aspect_scale_by_group: dict[str, tuple[float, float]] = {}
    for entry_key, group_key, anim_manifest in aspect_requests:
        pairs = _stable_correspondences(entry_key, anim_manifest)
        if not pairs:
            raise PackCompileError(
                f"{entry_key}: match_aspect_to_stable_poses needs at least one declared stable-pose "
                "endpoint (first_frame_is_state_base / last_frame_is_state_base)")
        ratios_x: list[float] = []
        ratios_y: list[float] = []
        for (aw, ah, apx), (bw, bh, bpx) in pairs:
            a_sigma = prep_dev_sprite.alpha_weighted_sigma(aw, ah, apx)
            b_sigma = prep_dev_sprite.alpha_weighted_sigma(bw, bh, bpx)
            if a_sigma[0] <= 0.0 or a_sigma[1] <= 0.0:
                continue
            ratios_x.append(b_sigma[0] / a_sigma[0])
            ratios_y.append(b_sigma[1] / a_sigma[1])
        if ratios_x:
            aspect_scale_by_group[group_key] = (sum(ratios_x) / len(ratios_x),
                                                sum(ratios_y) / len(ratios_y))

    # --- Ganancia de COLOR de secuencia (DEC-102) ---------------------
    color_gain_by_entry: dict[str, tuple[float, float, float]] = {}
    for entry_key, anim_manifest in color_requests:
        pairs = _stable_correspondences(entry_key, anim_manifest)
        if not pairs:
            raise PackCompileError(
                f"{entry_key}: match_color_to_stable_poses needs at least one declared stable-pose "
                "endpoint (first_frame_is_state_base / last_frame_is_state_base)")
        channels: list[tuple[float, float, float]] = []
        for (aw, ah, apx), (bw, bh, bpx) in pairs:
            a_rgb = prep_dev_sprite.interior_mean_rgb(aw, ah, apx)
            b_rgb = prep_dev_sprite.interior_mean_rgb(bw, bh, bpx)
            if a_rgb is None or b_rgb is None or min(a_rgb) <= 0.0:
                continue
            channels.append((b_rgb[0] / a_rgb[0], b_rgb[1] / a_rgb[1], b_rgb[2] / a_rgb[2]))
        if channels:
            color_gain_by_entry[entry_key] = tuple(
                sum(c[i] for c in channels) / len(channels) for i in range(3))

    plan: dict[str, tuple[float, int, int, int, int]] = {}
    if normalize:
        plan = prep_dev_sprite.compute_frame_normalization_plan(
            entries,
            groups,
            reference_group=reference_group,
            group_frame_paths=group_frame_paths,
            state_of_group=state_of_group,
            base_group_of_state=base_group_of_state,
            entry_frame_paths=entry_frame_paths,
            start_base_entry=start_base_entry,
            aspect_scale_by_group=aspect_scale_by_group,
        )
        runtime_max_frame_dimension = manifest.get("runtime_max_frame_dimension")
        if runtime_max_frame_dimension is not None:
            plan = _grow_working_canvas_for_runtime_rounding(
                plan, {key: (w, h) for key, (w, h, _px) in entries.items()}, int(runtime_max_frame_dimension)
            )

    return ContentPlan(normalization=plan, boundary_base_frames=boundary_base_frames,
                       color_gain=color_gain_by_entry)


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

    content_plan = _build_content_plan(manifest, manifest_dir)

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
        out += _compile_animation(base_manifest, manifest_dir, base_context, runtime_max_frame_dimension, content_plan)
        out += _compile_direction_overrides(
            state.get("base_animation_direction_overrides", []), manifest_dir, base_context, runtime_max_frame_dimension,
            content_plan
        )

        out += struct.pack("<d", float(state.get("ambient_interval_seconds", 300.0)))
        out += _compile_weighted_actions(
            state.get("ambient_actions", []), manifest_dir, state_id, "ambient_actions", runtime_max_frame_dimension,
            content_plan
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
            hover_actions, manifest_dir, state_id, "hover_actions", runtime_max_frame_dimension, content_plan
        )

        out += _compile_weighted_actions(
            state.get("click_actions", []), manifest_dir, state_id, "click_actions", runtime_max_frame_dimension,
            content_plan
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
