#!/usr/bin/env python3
"""Dev asset pipeline: compiles a JSON pack manifest + source PNG frames
into the runtime ".nvpack" binary format content::PetPackLoader reads,
with no PNG decoder and no JSON/manifest parser needed on the C++ side
(see src/content/PetPackLoader.cpp and docs/ANIMATION_RUNTIME.md for the
exact on-disk layout, "NVPACK1").

This compiles any content::PetDefinition-shaped pack, not just the
Bunny QA fixture (see assets/dev/bunny_pack/manifest.json for the one
this block actually ships) -- but the manifest format and this tool
remain *development* tooling, not the production content pipeline (see
docs/PET_CONTENT_SPEC.md, still unimplemented).

Reuses tools/prep_dev_sprite.py's PNG decoder by importing it, rather
than reimplementing PNG parsing a second time (see AGENTS.md Sec 10:
"reuse, don't rewrite working PNG tooling").

Manifest schema (JSON):
    {
      "id": "bunny_dev",
      "display_name": "Bunny (dev)",
      "variant_group": "",                     # optional, default ""
      "canvas_width": 160,
      "canvas_height": 160,
      "alpha_hit_threshold": 128,               # optional, default 128
      "passive_interval_seconds": 300.0,        # optional, default 300.0
      "content_version": "block02-dev-1",       # optional, default ""
      "runtime_max_frame_dimension": 320,       # optional, default: no downscale (see below)
      "normalize_visual_scale": true,           # optional, default false (see below)
      "idle": { ...AnimationManifest... },
      "click_reaction": { ...AnimationManifest... },
      "passive_actions": [ {...AnimationManifest...}, ... ],  # optional, default []
      "idle_direction_overrides": [                            # optional, see below
        {"direction": "left", ...AnimationManifest...}, ...
      ],
      "click_reaction_direction_overrides": [                  # optional, see below
        {"direction": "left", ...AnimationManifest...}, ...
      ],
      "passive_action_direction_overrides": [                  # optional, see below
        {"passive_action_index": 0, "direction": "left", ...AnimationManifest...}, ...
      ],
      "passive_action_weights": [0.7, 0.3]                     # optional, see below
    }

The three `*_direction_overrides` keys (Block 04.2, see
docs/NIDIR_CONTENT.md) are a backward-compatible, purely ADDITIVE
extension to the "NVPACK1" format: if NONE of the three are present in
the manifest (as in every pack compiled before Block 04.2, e.g.
Bunny's), the compiled output is byte-for-byte identical to before --
no existing pack needs recompiling. If ANY of the three is present, all
three sections are written, in this fixed order (idle, click_reaction,
passive_action), each either real content or an explicit empty section
(count 0) for whichever key the manifest didn't mention -- the reader
(src/content/PetPackLoader.cpp) always tries the three sections in that
same fixed order and only reads a section at all if bytes remain, so
the two sides must agree positionally; a manifest that only wants
`click_reaction_direction_overrides`, say, still gets an explicit empty
`idle_direction_overrides` section ahead of it, never a silently
skipped one. `passive_action_direction_overrides` entries name which
`passive_actions[]` entry they override via `passive_action_index`
(flat list, not nested under each passive action, so the "purely
additive at the very end" property holds for the whole format, not just
for idle/click). See content::ResolveIdleAnimation()/
ResolveClickReaction()/ResolvePassiveAction()
(src/content/AnimationDefinition.h) for how the C++ runtime picks
between the canonical animation and an override at runtime, and
content::PetPackLoader.cpp's ByteReader::HasMoreData() for how the
loader tells an old-format pack (no trailing section at all) apart from
a new one, without needing a schema-version bump.

`passive_action_weights` (Block 04.3, corrección post-QA -- see
docs/DECISION_LOG.md) is a FOURTH optional trailing section,
independent of the three `*_direction_overrides` sections above (not
coupled to them -- absent entirely if the manifest doesn't define this
key, regardless of whether any direction overrides are present).
One float weight per entry of `passive_actions`, same order, same
length (enforced at compile time and again at load time). Read at
runtime by content::ChooseWeightedPassiveActionIndex() to pick which
passive action to trigger, weighted-random instead of the simpler
round-robin this ran on before -- e.g. `[0.7, 0.3]` makes the first
`passive_actions` entry fire ~70% of the time, the second ~30%. Absent
(the default) means "every passive_actions entry has equal weight" --
byte-for-byte identical to every pack compiled before this feature
existed.

AnimationManifest:
    {
      "id": "click_reaction",
      "kind": "static" | "loop" | "one_shot",
      "fps": 10,                    # optional, default 0 (use each frame's duration_ms instead)
      "returns_to_idle": true,      # optional, default true
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

`runtime_max_frame_dimension` (Block 04.2's second pass, see
docs/NIDIR_CONTENT.md) is an OPTIONAL, purely compile-time downscale:
if set, every decoded frame (across idle/click/passive and every
direction override) whose longer side exceeds this value is resized
before being written into the compiled pack -- the canonical source
PNGs on disk are NEVER touched, only the bytes that end up inside the
".nvpack" file. Absent (the default) means "store frames at their
native decoded resolution," identical to every pack compiled before
this feature existed. This is independent of `canvas_width`/
`canvas_height` (the pet's LOGICAL on-screen size, in points) -- see
tools/prep_dev_sprite.py's compute_logical_canvas_size() for how a
generator script should derive that from a pet's native art
resolution and aspect ratio, and docs/NIDIR_CONTENT.md for why the two
are deliberately separate knobs (display size vs. stored pixel
resolution).

Real downscales (`runtime_max_frame_dimension`'s trigger, and a
`normalize_visual_scale` content_scale below 1.0 -- see below) use
`prep_dev_sprite.resize_rgba_area_average()` (a deterministic box
filter, alpha-weighted to avoid color fringing at transparent
boundaries) instead of plain nearest-neighbor -- added in Block 04.3's
correction pass after QA manual found real visual quality loss/fine
detail dropping out during Bunny's animation playback (nearest-
neighbor can, by bad luck of exactly where its single sample point
falls, skip a thin detail -- like a 1-2px outline -- entirely; a box
filter never ignores real content, only blurs it slightly). Upscales
(content_scale > 1.0, a rarer case) still use
prep_dev_sprite.resize_rgba_nearest() -- a box filter doesn't apply to
upscaling. This is purely a texture-byte quality improvement; the
runtime hit-mask (`core::AlphaMask::FromAlphaChannel`) is untouched and
still nearest-neighbor, matching the C++ side exactly as before.

`normalize_visual_scale` (Block 04.3, see docs/NIDIR_CONTENT.md,
"clipping y tamaño visual inconsistente entre animaciones") is an
OPTIONAL, purely compile-time content-anchored normalization: if
`true`, every animation of this pet (idle/click_reaction/passive
actions and their direction overrides) is scaled and padded (never
cropped) onto ONE shared working canvas, derived entirely from each
animation's own first-frame content bounding box (see
prep_dev_sprite.compute_frame_normalization_plan()), so the character
appears at the same apparent size and position on screen regardless of
which animation is currently playing -- fixes the real bug where an
animation with more empty margin around the character in its own
native frame (e.g. a click/fire effect that needs extra room) would
render the character visibly smaller than idle once independently
stretched to fill the same fixed logical canvas. Absent (the default,
and what Bunny's manifest still uses) means "no change," identical to
every pack compiled before this feature existed. Applied BEFORE
`runtime_max_frame_dimension` -- the working-canvas-sized frame is what
gets downscaled, not the raw native frame. Canonical source PNGs on
disk are never touched, only the bytes that end up inside the
".nvpack" file, following the exact same precedent as
`runtime_max_frame_dimension`.

Fails loudly (non-zero exit, a specific message on stderr naming the
animation/frame at fault) rather than silently inventing or skipping
data, on:
    - a referenced source PNG that doesn't exist
    - an animation whose frames don't all share the same pixel dimensions
    - a required manifest field missing
    - an animation with an empty frame list
    - an out-of-range alpha_hit_threshold

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

    # Normalización de escala/encuadre por contenido, opcional (Block
    # 04.3 -- ver el docstring del módulo y
    # prep_dev_sprite.compute_frame_normalization_plan()). Se aplica
    # ANTES del downscale de abajo: primero se reescala (si
    # content_scale != 1.0) y se ubica dentro del canvas de trabajo
    # compartido del pet (nunca recorta, solo agrega margen
    # transparente), y ESE resultado es lo que eventualmente se
    # downscalea a runtime_max_frame_dimension si corresponde.
    if normalization is not None:
        content_scale, working_width, working_height, offset_x, offset_y = normalization
        if abs(content_scale - 1.0) > 1e-9:
            target_w = max(1, round(width * content_scale))
            target_h = max(1, round(height * content_scale))
            # Downscale real (content_scale < 1.0) usa el box filter de
            # calidad -- ver el docstring de
            # prep_dev_sprite.resize_rgba_area_average(): nearest-
            # neighbor en un downscale puede saltarse por completo
            # detalles finos (encontrado en QA manual de Bunny, Block
            # 04.3, corrección post-QA -- ver docs/BUNNY_CONTENT.md).
            # Un upscale (content_scale > 1.0, caso raro) sigue usando
            # nearest-neighbor -- un box filter no tiene sentido ahí.
            resize_fn = prep_dev_sprite.resize_rgba_area_average if content_scale < 1.0 else prep_dev_sprite.resize_rgba_nearest
            pixels = resize_fn(width, height, pixels, target_w, target_h)
            width, height = target_w, target_h
        pixels = prep_dev_sprite.compose_on_canvas(width, height, pixels, working_width, working_height, offset_x, offset_y)
        width, height = working_width, working_height

    # Downscale opcional en tiempo de compilación (ver el docstring del
    # módulo) -- el PNG en disco nunca se toca, solo estos bytes que
    # van directo al pack compilado. anchor_x/anchor_y por defecto se
    # calculan DESPUÉS del downscale (mitad del tamaño ya reducido),
    # para que un manifest que no especifica anchor explícito siga
    # centrando correctamente sin importar si hubo downscale o no. Este
    # paso SIEMPRE es un downscale real cuando se dispara (el `if` de
    # abajo solo entra cuando el frame excede el límite) -- usa el box
    # filter de calidad, no nearest-neighbor (mismo motivo que arriba).
    if runtime_max_frame_dimension is not None and max(width, height) > runtime_max_frame_dimension:
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

    # La misma tupla de normalización (content_scale/canvas de trabajo/
    # offset) aplica a TODOS los frames de esta animación -- se calculó
    # una sola vez en el pre-pass a partir del primer frame, ver
    # _build_normalization_plan().
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


def _first_frame_pixels(anim_manifest: dict, manifest_dir: str, context: str) -> tuple[int, int, bytes]:
    """Decodifica SOLO el primer frame de una animación -- usado
    exclusivamente por el pre-pass de _build_normalization_plan() para
    conocer su bounding box de contenido nativo, sin decodificar la
    secuencia completa dos veces más de lo necesario."""
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
    docstring del módulo): recorre la MISMA estructura de manifest que
    compile_pack() ya recorre (idle/click_reaction/passive_actions y
    sus tres secciones de overrides direccionales), decodificando solo
    el primer frame de cada una, y arma los diccionarios `entries`/
    `groups` que prep_dev_sprite.compute_frame_normalization_plan()
    necesita -- las claves usadas acá ("idle", "click_reaction",
    "passive_actions[i]", "idle_direction_overrides[j]", etc.) son
    exactamente los mismos strings `context` que _compile_animation()
    ya recibe más abajo, así que el resultado se puede indexar
    directamente con ese mismo `context` en la segunda pasada real de
    compilación, sin necesidad de ningún esquema de claves paralelo."""
    entries: dict[str, tuple[int, int, bytes]] = {}
    groups: dict[str, str] = {}

    idle_manifest = _require(manifest, "idle", "manifest")
    entries["idle"] = _first_frame_pixels(idle_manifest, manifest_dir, "idle")
    groups["idle"] = "idle"

    click_manifest = _require(manifest, "click_reaction", "manifest")
    entries["click_reaction"] = _first_frame_pixels(click_manifest, manifest_dir, "click_reaction")
    groups["click_reaction"] = "click_reaction"

    passive_manifests = manifest.get("passive_actions", [])
    for i, pm in enumerate(passive_manifests):
        context = f"passive_actions[{i}]"
        entries[context] = _first_frame_pixels(pm, manifest_dir, context)
        groups[context] = context

    for i, om in enumerate(manifest.get("idle_direction_overrides", [])):
        context = f"idle_direction_overrides[{i}]"
        entries[context] = _first_frame_pixels(om, manifest_dir, context)
        groups[context] = "idle"

    for i, om in enumerate(manifest.get("click_reaction_direction_overrides", [])):
        context = f"click_reaction_direction_overrides[{i}]"
        entries[context] = _first_frame_pixels(om, manifest_dir, context)
        groups[context] = "click_reaction"

    for i, om in enumerate(manifest.get("passive_action_direction_overrides", [])):
        context = f"passive_action_direction_overrides[{i}]"
        passive_action_index = int(_require(om, "passive_action_index", context))
        entries[context] = _first_frame_pixels(om, manifest_dir, context)
        groups[context] = f"passive_actions[{passive_action_index}]"

    return prep_dev_sprite.compute_frame_normalization_plan(entries, groups, reference_group="idle")


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

    passive_interval = float(manifest.get("passive_interval_seconds", 300.0))
    content_version = str(manifest.get("content_version", ""))

    runtime_max_frame_dimension = manifest.get("runtime_max_frame_dimension")
    if runtime_max_frame_dimension is not None:
        runtime_max_frame_dimension = int(runtime_max_frame_dimension)
        if runtime_max_frame_dimension <= 0:
            raise PackCompileError(f"manifest: runtime_max_frame_dimension {runtime_max_frame_dimension} must be positive")

    idle_manifest = _require(manifest, "idle", "manifest")
    click_manifest = _require(manifest, "click_reaction", "manifest")
    passive_manifests = manifest.get("passive_actions", [])

    # Pre-pass opcional (ver el docstring del módulo): decodifica solo
    # el primer frame de cada animación para derivar el plan de
    # normalización de escala/encuadre por contenido, ANTES de la
    # compilación real de más abajo. None cuando el manifest no pide
    # esto -- comportamiento 100% sin cambios (byte-a-byte igual al de
    # antes de este feature).
    normalization_plan = _build_normalization_plan(manifest, manifest_dir) if bool(manifest.get("normalize_visual_scale", False)) else None

    out = bytearray()
    out += b"NVPACK1\0"
    out += _pack_string(pet_id)
    out += _pack_string(display_name)
    out += _pack_string(variant_group)
    out += struct.pack("<II", canvas_width, canvas_height)
    out += struct.pack("<B", alpha_threshold)
    out += struct.pack("<d", passive_interval)
    out += _pack_string(content_version)

    out += _compile_animation(idle_manifest, manifest_dir, "idle", runtime_max_frame_dimension, normalization_plan)
    out += _compile_animation(click_manifest, manifest_dir, "click_reaction", runtime_max_frame_dimension, normalization_plan)

    out += struct.pack("<I", len(passive_manifests))
    for i, pm in enumerate(passive_manifests):
        out += _compile_animation(pm, manifest_dir, f"passive_actions[{i}]", runtime_max_frame_dimension, normalization_plan)

    # Tres secciones finales, opcionales y aditivas (Block 04.2) -- ver
    # el docstring del módulo para por qué las tres se escriben juntas
    # (aunque el manifest solo mencione una) en cuanto el manifest
    # menciona cualquiera de ellas, y por qué ninguna se escribe si el
    # manifest no menciona ninguna (byte-a-byte igual que antes de este
    # bloque -- el caso de Bunny).
    direction_keys = ("idle_direction_overrides", "click_reaction_direction_overrides", "passive_action_direction_overrides")
    if any(key in manifest for key in direction_keys):
        idle_overrides = manifest.get("idle_direction_overrides", [])
        out += struct.pack("<I", len(idle_overrides))
        for i, om in enumerate(idle_overrides):
            context = f"idle_direction_overrides[{i}]"
            direction_str = _require(om, "direction", context)
            if direction_str not in _DIRECTION_TO_BYTE:
                raise PackCompileError(f"{context}: invalid direction '{direction_str}' (expected right/left)")
            out += struct.pack("<B", _DIRECTION_TO_BYTE[direction_str])
            out += _compile_animation(om, manifest_dir, context, runtime_max_frame_dimension, normalization_plan)

        click_overrides = manifest.get("click_reaction_direction_overrides", [])
        out += struct.pack("<I", len(click_overrides))
        for i, om in enumerate(click_overrides):
            context = f"click_reaction_direction_overrides[{i}]"
            direction_str = _require(om, "direction", context)
            if direction_str not in _DIRECTION_TO_BYTE:
                raise PackCompileError(f"{context}: invalid direction '{direction_str}' (expected right/left)")
            out += struct.pack("<B", _DIRECTION_TO_BYTE[direction_str])
            out += _compile_animation(om, manifest_dir, context, runtime_max_frame_dimension, normalization_plan)

        passive_overrides = manifest.get("passive_action_direction_overrides", [])
        out += struct.pack("<I", len(passive_overrides))
        for i, om in enumerate(passive_overrides):
            context = f"passive_action_direction_overrides[{i}]"
            passive_action_index = int(_require(om, "passive_action_index", context))
            if not 0 <= passive_action_index < len(passive_manifests):
                raise PackCompileError(
                    f"{context}: passive_action_index {passive_action_index} is out of range "
                    f"(manifest has {len(passive_manifests)} passive_actions entr{'y' if len(passive_manifests) == 1 else 'ies'})"
                )
            direction_str = _require(om, "direction", context)
            if direction_str not in _DIRECTION_TO_BYTE:
                raise PackCompileError(f"{context}: invalid direction '{direction_str}' (expected right/left)")
            out += struct.pack("<I", passive_action_index)
            out += struct.pack("<B", _DIRECTION_TO_BYTE[direction_str])
            out += _compile_animation(om, manifest_dir, context, runtime_max_frame_dimension, normalization_plan)

    # Cuarta sección final, opcional e INDEPENDIENTE de las tres de
    # arriba (Block 04.3, corrección post-QA -- política 70/30 de
    # selección de acción pasiva). Ausente por completo si el manifest
    # no define "passive_action_weights" -- ningún byte se escribe,
    # byte-idéntico a un pack de antes de este feature (el reader la
    # gatea con HasMoreData(), como las otras tres). Si está presente,
    # su longitud DEBE calzar con la cantidad de passive_actions.
    if "passive_action_weights" in manifest:
        weights = manifest["passive_action_weights"]
        if len(weights) != len(passive_manifests):
            raise PackCompileError(
                f"manifest: passive_action_weights has {len(weights)} entries, "
                f"but passive_actions has {len(passive_manifests)}"
            )
        out += struct.pack("<I", len(weights))
        for w in weights:
            out += struct.pack("<d", float(w))

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
