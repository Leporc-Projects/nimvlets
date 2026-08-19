#!/usr/bin/env python3
"""Dev-only asset prep: converts a source RGBA PNG into the tiny
uncompressed `.rgba` fixture format Block 01's spike loaded directly at
runtime (see AGENTS.md §10 and docs/DECISION_LOG.md). That format and
the `graphics::DevSprite` loader that read it have since been retired
(Block 02, see docs/DECISION_LOG.md DEC-023) — nothing in the runtime
loads `.rgba` anymore. `write_raw_rgba()`/`main()` below are kept as a
small, harmless historical utility, not because anything depends on
them now.

This module's real ongoing role is the shared, dependency-free PNG
read/write module reused by
Block 02's asset pipeline: `read_png_rgba()` is imported by
tools/compile_pet_pack.py (per AGENTS.md §10, "reuse, don't rewrite
working PNG tooling"), and `write_png_rgba()` is used by
tools/generate_bunny_dev_pack.py to materialize its deterministically
derived frames as real PNG files, so the pipeline documented in
docs/ANIMATION_RUNTIME.md is exercised with genuine PNG input end to
end, not an in-memory shortcut.

Block 04.2 adds `mirror_rgba_horizontal()`, reused by
tools/generate_nidir_pack.py to derive Nidir's "left" idle frames from
its real "right" idle frames by deterministic horizontal flip (never
AI-regenerated) -- see docs/NIDIR_CONTENT.md.

This file's own CLI entry point (`main()`, below) is a one-time,
offline prep step for temporary QA/dev fixtures — not a runtime tool,
not part of any content pipeline (see docs/PET_CONTENT_SPEC.md, still
unimplemented). Re-run it by hand if a source PNG changes.

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


def mirror_rgba_horizontal(width: int, height: int, pixels: bytes) -> bytes:
    """Espeja horizontalmente un buffer RGBA8 (mismo layout que
    read_png_rgba/write_png_rgba: row-major, top-to-bottom, alpha
    directo) -- invierte el orden de columnas en cada fila, pixel por
    pixel completo (los 4 canales RGBA se mueven juntos, así que el
    canal alpha se preserva exactamente, nunca se recalcula ni se
    aproxima). Usado por tools/generate_nidir_pack.py (Block 04.2) para
    derivar los frames "left" a partir de los frames "right" reales de
    forma determinista, sin IA y sin ninguna otra transformación (ver
    block brief §3: "Do not use AI to regenerate the left side").

    Determinista y sin pérdida: espejar dos veces devuelve el buffer
    original byte a byte (ver tools/test_asset_pipeline.py)."""
    if len(pixels) != width * height * 4:
        raise ValueError(f"mirror_rgba_horizontal: pixel buffer is {len(pixels)} bytes, expected {width * height * 4} for {width}x{height} RGBA8")

    stride = width * 4
    out = bytearray(len(pixels))
    for y in range(height):
        row_start = y * stride
        row = pixels[row_start : row_start + stride]
        for x in range(width):
            src_off = x * 4
            dst_x = width - 1 - x
            dst_off = dst_x * 4
            out[row_start + dst_off : row_start + dst_off + 4] = row[src_off : src_off + 4]
    return bytes(out)


def write_raw_rgba(path: str, width: int, height: int, pixels: bytes) -> None:
    with open(path, "wb") as f:
        f.write(b"NVR1")
        f.write(struct.pack("<II", width, height))
        f.write(pixels)


def _png_chunk(chunk_type: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + chunk_type + data + struct.pack(">I", zlib.crc32(chunk_type + data) & 0xFFFFFFFF)


def write_png_rgba(path: str, width: int, height: int, pixels: bytes) -> None:
    """Encodes raw RGBA8 bytes (row-major, top-to-bottom, straight
    alpha — the same layout read_png_rgba() produces) as a standard,
    minimal 8-bit RGBA PNG: filter type "None" on every scanline (no
    attempt at optimal compression — these are small, deterministic dev
    fixtures, not production art) and a single IDAT chunk. Any standard
    PNG reader, and this module's own read_png_rgba(), can read the
    result back byte-for-byte.
    """
    if len(pixels) != width * height * 4:
        raise ValueError(f"write_png_rgba: pixel buffer is {len(pixels)} bytes, expected {width * height * 4} for {width}x{height} RGBA8")

    stride = width * 4
    raw = bytearray()
    for y in range(height):
        raw.append(0)  # filter type 0 ("None") for every scanline
        raw += pixels[y * stride : (y + 1) * stride]
    compressed = zlib.compress(bytes(raw), 9)

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(_png_chunk(b"IHDR", ihdr))
        f.write(_png_chunk(b"IDAT", compressed))
        f.write(_png_chunk(b"IEND", b""))


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
