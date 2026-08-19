#!/usr/bin/env python3
"""SUPERSEDED since Block 04.3 -- see tools/generate_bunny_pack.py.

Bunny migrated from this synthetic QA fixture to the owner's real
production art (idle + click) in Block 04.3 (ver docs/BUNNY_CONTENT.md,
docs/DECISION_LOG.md). `assets/dev/bunny_pack.nvpack` -- the exact same
compiled path this script writes below -- now holds REAL content
compiled by tools/generate_bunny_pack.py, not the synthetic derivation
this script produces.

**Do NOT run this script against the current repository state**: doing
so would silently overwrite the real compiled Bunny pack with the
synthetic squash/stretch/lean placeholder content described below,
regressing Bunny back to Block 02's QA fixture. It is kept, unmodified,
purely as a historical/reference artifact (see docs/DECISION_LOG.md
DEC-018/DEC-022 for the block-01/02 context it documents) -- not
because anything in the current pipeline depends on it.

Generates the Bunny DEV animation pack (Block 02) from the existing
Block 01 Bunny QA fixture (assets/dev/bunny_source.png).

This block must not depend on finishing AI-generated animations (see
the block brief §5), so this tool derives a small, deterministic set of
extra frames from the *existing* static Bunny image via plain pixel
transforms (nearest-neighbor scale and shear — no rotation/interpolation
library, no new artistic dependency) instead of waiting on real art:

    idle             1 frame  (Bunny's existing static image, unchanged)
    click_reaction   3 frames (squash -> stretch -> settle: a "bounce")
    passive_wiggle   3 frames (lean left -> lean right -> settle)

These are clearly-derived, clearly-non-final QA/development fixtures —
see docs/PET_CONTENT_SPEC.md and docs/DECISION_LOG.md. They exist to
exercise the runtime (static idle, one-shot playback, per-frame
silhouette changes for alpha hit-mask verification), not to look good.

Writes:
    assets/dev/bunny_pack/frames/*.png   (derived + reused source frames)
    assets/dev/bunny_pack/manifest.json  (tools/compile_pet_pack.py input)
    assets/dev/bunny_pack.nvpack         (compiled runtime pack — what
                                          src/app actually loads)

Deterministic: re-running this script with the same
assets/dev/bunny_source.png produces byte-identical output every time.

Usage:
    python3 tools/generate_bunny_dev_pack.py

No third-party dependencies.
"""

from __future__ import annotations

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import compile_pet_pack  # noqa: E402
import prep_dev_sprite  # noqa: E402

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE_PNG = os.path.join(REPO_ROOT, "assets", "dev", "bunny_source.png")
PACK_DIR = os.path.join(REPO_ROOT, "assets", "dev", "bunny_pack")
FRAMES_DIR = os.path.join(PACK_DIR, "frames")
MANIFEST_PATH = os.path.join(PACK_DIR, "manifest.json")
COMPILED_PATH = os.path.join(REPO_ROOT, "assets", "dev", "bunny_pack.nvpack")

# Matches the pet's logical canvas size (src/app renders every frame
# into this same destination rect regardless of native resolution) —
# see docs/ANIMATION_RUNTIME.md. Deliberately smaller than Block 01's
# 320x320 native Bunny texture: this is a QA/dev fixture, not final
# art, and a 1:1 native-to-canvas ratio also makes the hit-mask
# nearest-neighbor sampling in core::AlphaMask::FromAlphaChannel exact
# rather than a downscale.
CANVAS_SIZE = 160


def resize_nearest(pixels: bytes, src_w: int, src_h: int, dst_w: int, dst_h: int) -> bytes:
    """Same nearest-neighbor mapping formula as
    core::AlphaMask::FromAlphaChannel, deliberately kept consistent
    across the Python tooling and the C++ runtime."""
    out = bytearray(dst_w * dst_h * 4)
    for ty in range(dst_h):
        sy = min(src_h - 1, (ty * src_h) // dst_h)
        for tx in range(dst_w):
            sx = min(src_w - 1, (tx * src_w) // dst_w)
            src_off = (sy * src_w + sx) * 4
            dst_off = (ty * dst_w + tx) * 4
            out[dst_off : dst_off + 4] = pixels[src_off : src_off + 4]
    return bytes(out)


def scale_canvas(pixels: bytes, size: int, scale_x: float, scale_y: float) -> bytes:
    """Returns a new `size` x `size` RGBA buffer with the source content
    scaled by (scale_x, scale_y), anchored at (horizontal center,
    bottom) so a "squash" looks grounded rather than floating. Inverse
    (destination -> source) nearest-neighbor mapping — no gaps, no
    interpolation library."""
    out = bytearray(size * size * 4)
    cx = size / 2.0
    bottom = float(size)
    for ty in range(size):
        sy = (ty - bottom) / scale_y + bottom
        sy_i = int(round(sy))
        if sy_i < 0 or sy_i >= size:
            continue
        for tx in range(size):
            sx = (tx - cx) / scale_x + cx
            sx_i = int(round(sx))
            if sx_i < 0 or sx_i >= size:
                continue
            src_off = (sy_i * size + sx_i) * 4
            dst_off = (ty * size + tx) * 4
            out[dst_off : dst_off + 4] = pixels[src_off : src_off + 4]
    return bytes(out)


def shear_canvas(pixels: bytes, size: int, max_offset: float) -> bytes:
    """Returns a new `size` x `size` RGBA buffer with each row shifted
    horizontally by an amount that's largest at the top row and zero at
    the bottom row — a cheap, deterministic "lean" silhouette change
    with feet planted, no rotation matrix / interpolation needed."""
    out = bytearray(size * size * 4)
    for ty in range(size):
        offset = int(round(max_offset * (1.0 - ty / float(size - 1))))
        for tx in range(size):
            sx = tx - offset
            if sx < 0 or sx >= size:
                continue
            src_off = (ty * size + sx) * 4
            dst_off = (ty * size + tx) * 4
            out[dst_off : dst_off + 4] = pixels[src_off : src_off + 4]
    return bytes(out)


def main() -> int:
    if not os.path.isfile(SOURCE_PNG):
        print(f"error: source fixture not found: {SOURCE_PNG}", file=sys.stderr)
        return 1

    os.makedirs(FRAMES_DIR, exist_ok=True)

    src_w, src_h, src_pixels = prep_dev_sprite.read_png_rgba(SOURCE_PNG)
    base = resize_nearest(src_pixels, src_w, src_h, CANVAS_SIZE, CANVAS_SIZE)

    frames = {
        "idle_00.png": base,
        "click_squash.png": scale_canvas(base, CANVAS_SIZE, scale_x=1.18, scale_y=0.72),
        "click_stretch.png": scale_canvas(base, CANVAS_SIZE, scale_x=0.88, scale_y=1.18),
        "click_settle.png": base,
        "passive_lean_left.png": shear_canvas(base, CANVAS_SIZE, max_offset=14.0),
        "passive_lean_right.png": shear_canvas(base, CANVAS_SIZE, max_offset=-14.0),
        "passive_settle.png": base,
    }
    for filename, pixels in frames.items():
        prep_dev_sprite.write_png_rgba(os.path.join(FRAMES_DIR, filename), CANVAS_SIZE, CANVAS_SIZE, pixels)

    def frame_entry(filename: str, duration_ms: float) -> dict:
        return {"source": os.path.join("frames", filename), "duration_ms": duration_ms}

    manifest = {
        "id": "bunny_dev",
        "display_name": "Bunny (dev fixture)",
        "variant_group": "",
        "canvas_width": CANVAS_SIZE,
        "canvas_height": CANVAS_SIZE,
        "alpha_hit_threshold": 128,
        "passive_interval_seconds": 300.0,
        "content_version": "block02-dev-1",
        "idle": {
            "id": "idle",
            "kind": "static",
            "fps": 0,
            "returns_to_idle": True,
            "frames": [frame_entry("idle_00.png", 0)],
        },
        "click_reaction": {
            "id": "click_reaction",
            "kind": "one_shot",
            "fps": 0,
            "returns_to_idle": True,
            "frames": [
                frame_entry("click_squash.png", 90),
                frame_entry("click_stretch.png", 90),
                frame_entry("click_settle.png", 90),
            ],
        },
        "passive_actions": [
            {
                "id": "passive_wiggle",
                "kind": "one_shot",
                "fps": 0,
                "returns_to_idle": True,
                "frames": [
                    frame_entry("passive_lean_left.png", 160),
                    frame_entry("passive_lean_right.png", 160),
                    frame_entry("passive_settle.png", 160),
                ],
            }
        ],
    }
    with open(MANIFEST_PATH, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")

    pet_id, total_bytes = compile_pet_pack.compile_pack(MANIFEST_PATH, COMPILED_PATH)
    print(f"generated {len(frames)} frame PNGs in {FRAMES_DIR}")
    print(f"wrote manifest: {MANIFEST_PATH}")
    print(f"compiled pet '{pet_id}': {COMPILED_PATH} ({total_bytes} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
