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
      "idle": { ...AnimationManifest... },
      "click_reaction": { ...AnimationManifest... },
      "passive_actions": [ {...AnimationManifest...}, ... ],  # optional, default []
      "idle_direction_overrides": [                            # optional, default: field absent entirely
        {"direction": "left", ...AnimationManifest...}, ...
      ]
    }

`idle_direction_overrides` (Block 04.2, see docs/NIDIR_CONTENT.md) is a
backward-compatible, purely ADDITIVE extension to the "NVPACK1" format:
if this key is absent from the manifest (as in every pack compiled
before Block 04.2, e.g. Bunny's), the compiled output is byte-for-byte
identical to before -- no existing pack needs recompiling. Each entry
names a `direction` ("right"/"left") other than the pet's canonical
`idle` above and provides a full AnimationManifest for that direction
-- see content::ResolveIdleAnimation() (src/content/AnimationDefinition.h)
for how the C++ runtime picks between `idle` and an override at
runtime, and content::PetPackLoader.cpp's ByteReader::BytesRemaining()
for how the loader tells an old-format pack (no trailing section at
all) apart from a new one (a trailing section, however small) without
needing a schema-version bump.

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


def _compile_frame(frame_manifest: dict, manifest_dir: str, context: str) -> tuple[int, int, bytes]:
    source = _require(frame_manifest, "source", context)
    path = os.path.join(manifest_dir, source)
    if not os.path.isfile(path):
        raise PackCompileError(f"{context}: source frame not found: {path}")

    width, height, pixels = prep_dev_sprite.read_png_rgba(path)
    if len(pixels) != width * height * 4:
        raise PackCompileError(f"{context}: decoded pixel data size mismatch for {path}")

    anchor_x = float(frame_manifest.get("anchor_x", width / 2.0))
    anchor_y = float(frame_manifest.get("anchor_y", height / 2.0))
    duration_ms = float(frame_manifest.get("duration_ms", 0.0))

    header = struct.pack("<IIddd", width, height, anchor_x, anchor_y, duration_ms)
    return width, height, header + pixels


def _compile_animation(anim_manifest: dict, manifest_dir: str, context: str) -> bytes:
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

    frame_blobs: list[bytes] = []
    first_dims: tuple[int, int] | None = None
    for i, fm in enumerate(frame_manifests):
        w, h, blob = _compile_frame(fm, manifest_dir, f"{full_context} frame {i}")
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

    idle_manifest = _require(manifest, "idle", "manifest")
    click_manifest = _require(manifest, "click_reaction", "manifest")
    passive_manifests = manifest.get("passive_actions", [])

    out = bytearray()
    out += b"NVPACK1\0"
    out += _pack_string(pet_id)
    out += _pack_string(display_name)
    out += _pack_string(variant_group)
    out += struct.pack("<II", canvas_width, canvas_height)
    out += struct.pack("<B", alpha_threshold)
    out += struct.pack("<d", passive_interval)
    out += _pack_string(content_version)

    out += _compile_animation(idle_manifest, manifest_dir, "idle")
    out += _compile_animation(click_manifest, manifest_dir, "click_reaction")

    out += struct.pack("<I", len(passive_manifests))
    for i, pm in enumerate(passive_manifests):
        out += _compile_animation(pm, manifest_dir, f"passive_actions[{i}]")

    # Sección final, opcional y aditiva (Block 04.2) -- solo se escribe
    # un solo byte de esta sección si el manifest la pide
    # explícitamente. Un manifest que no la menciona (todo pack
    # compilado antes de Block 04.2) produce exactamente los mismos
    # bytes que antes -- ver el docstring del módulo.
    if "idle_direction_overrides" in manifest:
        override_manifests = manifest["idle_direction_overrides"]
        out += struct.pack("<I", len(override_manifests))
        for i, om in enumerate(override_manifests):
            context = f"idle_direction_overrides[{i}]"
            direction_str = _require(om, "direction", context)
            if direction_str not in _DIRECTION_TO_BYTE:
                raise PackCompileError(f"{context}: invalid direction '{direction_str}' (expected right/left)")
            out += struct.pack("<B", _DIRECTION_TO_BYTE[direction_str])
            out += _compile_animation(om, manifest_dir, context)

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
