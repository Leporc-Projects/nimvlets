#!/usr/bin/env python3
"""Dev-only asset prep: converts a source RGBA PNG into the tiny
uncompressed `.rgba` fixture format the spike executable loads at
runtime, so the C++ side never needs a PNG decoder or an SDL_image
dependency (see AGENTS.md §10 and docs/DECISION_LOG.md).

This is a one-time, offline prep step for temporary QA/dev fixtures —
not a runtime tool, not part of any content pipeline (see
docs/PET_CONTENT_SPEC.md: Block 01 does not implement one). Re-run it
by hand if the source PNG changes.

Output format (all little-endian, no compression):
    magic:  4 bytes, ASCII "NVR1"
    width:  uint32
    height: uint32
    pixels: width * height * 4 bytes, RGBA8, row-major, top-to-bottom,
            straight (non-premultiplied) alpha.

Usage:
    python3 tools/prep_dev_sprite.py <input.png> <output.rgba>

No external dependencies (standard library only — struct + zlib).
"""

from __future__ import annotations

import struct
import sys
import zlib


def read_png_rgba(path: str) -> tuple[int, int, bytes]:
    """Decodes a standard (non-interlaced, 8-bit RGBA) PNG into raw
    RGBA8 bytes. Deliberately minimal — this is a dev tool for one
    known-format source asset, not a general PNG library."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: not a PNG file")

    pos = 8
    width = height = bitdepth = colortype = None
    idat = b""
    while pos < len(data):
        length = struct.unpack(">I", data[pos : pos + 4])[0]
        ctype = data[pos + 4 : pos + 8]
        chunk = data[pos + 8 : pos + 8 + length]
        if ctype == b"IHDR":
            width, height, bitdepth, colortype = struct.unpack(">IIBB", chunk[:10])
        elif ctype == b"IDAT":
            idat += chunk
        elif ctype == b"iDOT":
            raise ValueError(
                f"{path}: contains an Apple 'iDOT' fast-load chunk (typically from "
                "screencapture/Preview) that splits image data in a way this minimal "
                "decoder does not handle. Re-export via `sips -s format png` first "
                "to normalize it."
            )
        pos += 8 + length + 4
        if ctype == b"IEND":
            break

    if bitdepth != 8 or colortype != 6:
        raise ValueError(
            f"{path}: expected 8-bit RGBA (colortype 6), got bitdepth={bitdepth} "
            f"colortype={colortype}. Re-export as 8-bit RGBA PNG first."
        )

    raw = zlib.decompress(idat)
    bpp = 4
    stride = width * bpp
    out = bytearray(height * stride)
    prev = bytearray(stride)
    p = 0
    for y in range(height):
        ftype = raw[p]
        p += 1
        line = bytearray(raw[p : p + stride])
        p += stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if ftype == 1:
                line[x] = (line[x] + a) & 0xFF
            elif ftype == 2:
                line[x] = (line[x] + b) & 0xFF
            elif ftype == 3:
                line[x] = (line[x] + (a + b) // 2) & 0xFF
            elif ftype == 4:
                pp = a + b - c
                pa, pb, pc = abs(pp - a), abs(pp - b), abs(pp - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[x] = (line[x] + pr) & 0xFF
        out[y * stride : (y + 1) * stride] = line
        prev = line

    return width, height, bytes(out)


def write_raw_rgba(path: str, width: int, height: int, pixels: bytes) -> None:
    with open(path, "wb") as f:
        f.write(b"NVR1")
        f.write(struct.pack("<II", width, height))
        f.write(pixels)


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    src, dst = sys.argv[1], sys.argv[2]
    width, height, pixels = read_png_rgba(src)
    write_raw_rgba(dst, width, height, pixels)
    print(f"{dst}: {width}x{height} RGBA8, {len(pixels)} pixel bytes ({4 + 8 + len(pixels)} total)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
