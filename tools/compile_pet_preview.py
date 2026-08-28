#!/usr/bin/env python3
"""Pipeline de assets dev: compila UNA vista previa estática y liviana
para el Product UI (la Collection) a partir de un pack de animación ya
compilado (".nvpack" / "NVPACK2").

Por qué existe (Block 06.2 §3-§5): hasta este bloque, la Collection
obtenía el arte del hero/gallery abriendo y parseando el pack COMPLETO
del pet (Frin ~76 MB) solo para quedarse con un frame de reposo. Eso no
escala al roster final de 8 pets y hace que un cambio de variante Frin
(Macho <-> Hembra) bloquee el hilo de render decenas de ms. Un
".nvprev" ("NVPREV1") contiene exactamente lo que el Product UI
necesita para dibujar una preview estática y nada más:

    magic:        8 bytes  b"NVPREV1\\0"
    version:      <I        = 1
    pet_id:       <I len + utf8   (id de catálogo)
    variant_id:   <I len + utf8   ("" si el pet no tiene variantes)
    source_pack:  <I len + utf8   (basename del .nvpack de origen; solo
                                   diagnóstico/procedencia)
    width:        <I
    height:       <I
    pixel_bytes:  <I        (= width*height*4; explícito para que el
                             lector detecte truncamiento sin ambigüedad)
    pixels:       width*height*4 bytes, RGBA8 straight-alpha, row-major,
                  top-to-bottom (idéntico contrato que un FrameDefinition
                  de NVPACK2)

Fuente de los píxeles (contrato del brief §5): el frame 0 de la pose
base (`base_animation`) del PRIMER estado del pack, resuelto en
`Direction::kRight` — exactamente el mismo frame que
`productui::PetPreviewCache` extraía antes vía
`content::ResolveAnimation(..., Direction::kRight)`. No un frame de
animación arbitrario. Determinista: mismo pack -> mismo .nvprev byte a
byte.

Los frames dentro de un pack ya vienen acotados por
`runtime_max_frame_dimension` (320 px hoy para todos los packs), así
que en la práctica no se reescala nada — `--max-edge` es solo una
salvaguarda para un pack futuro con frames más grandes. No se recorta
el margen transparente ni se toca el arte fuente: los píxeles del
.nvprev son idénticos a los del frame de reposo del pack.

Uso:
    python3 tools/compile_pet_preview.py --pack <pack.nvpack> \\
        --pet-id <id> --variant-id <vid> --out <out.nvprev> \\
        [--max-edge 320]

Sin dependencias de terceros (usa el decodificador/reescalador puro de
tools/prep_dev_sprite.py y el lector de tools/read_pet_pack.py).
"""

from __future__ import annotations

import argparse
import os
import struct
import sys

import prep_dev_sprite  # noqa: E402  (reescalado por área, si hiciera falta)
import read_pet_pack  # noqa: E402  (lector NVPACK2 + resolución de dirección)

_MAGIC = b"NVPREV1\0"
_VERSION = 1
_DEFAULT_MAX_EDGE = 320


class PreviewCompileError(Exception):
    """Fallo de compilación claro y específico — nunca se traga en silencio."""


def _pack_string(s: str) -> bytes:
    encoded = s.encode("utf-8")
    return struct.pack("<I", len(encoded)) + encoded


def _rest_frame(pack: dict) -> dict:
    """El frame de reposo canónico: frame 0 de la pose base del primer
    estado, en Direction::kRight. Misma regla que
    productui::PetPreviewCache::RestFrameOf en C++."""
    states = pack.get("states") or []
    if not states:
        raise PreviewCompileError("pack has zero states")
    state = states[0]
    anim = read_pet_pack.resolve_animation(
        state["base_animation"], state["base_animation_direction_overrides"], "right"
    )
    frames = anim.get("frames") or []
    if not frames:
        raise PreviewCompileError(f"state '{state['id']}' base animation has zero frames")
    return frames[0]


def _fit_within(width: int, height: int, max_edge: int) -> tuple[int, int]:
    longest = max(width, height)
    if longest <= max_edge:
        return width, height
    k = max_edge / longest
    return max(1, round(width * k)), max(1, round(height * k))


def compile_preview(
    pack_path: str, pet_id: str, variant_id: str, out_path: str, max_edge: int = _DEFAULT_MAX_EDGE
) -> tuple[int, int, int]:
    if not pet_id:
        raise PreviewCompileError("--pet-id must not be empty")
    if not os.path.isfile(pack_path):
        raise PreviewCompileError(f"pack '{pack_path}' does not exist relative to the current directory")

    pack = read_pet_pack.read_pack(pack_path)
    frame = _rest_frame(pack)
    width, height, pixels = frame["width"], frame["height"], frame["pixels"]
    if width <= 0 or height <= 0:
        raise PreviewCompileError(f"rest frame has non-positive dimensions {width}x{height}")
    if len(pixels) != width * height * 4:
        raise PreviewCompileError(
            f"rest frame pixel buffer is {len(pixels)} bytes, expected {width * height * 4}"
        )

    target_w, target_h = _fit_within(width, height, max_edge)
    if (target_w, target_h) != (width, height):
        pixels = prep_dev_sprite.resize_rgba_area_average(width, height, pixels, target_w, target_h)
        width, height = target_w, target_h

    body = bytearray()
    body += _MAGIC
    body += struct.pack("<I", _VERSION)
    body += _pack_string(pet_id)
    body += _pack_string(variant_id)
    body += _pack_string(os.path.basename(pack_path))
    body += struct.pack("<III", width, height, len(pixels))
    body += pixels

    with open(out_path, "wb") as f:
        f.write(body)
    return width, height, len(body)


def main() -> int:
    parser = argparse.ArgumentParser(description="Compile one NVPREV1 Product UI preview from a .nvpack")
    parser.add_argument("--pack", required=True, help="compiled .nvpack to derive the preview from")
    parser.add_argument("--pet-id", required=True, help="catalog pet id (e.g. 'frin')")
    parser.add_argument("--variant-id", default="", help="catalog variant id (e.g. 'male'); empty if none")
    parser.add_argument("--out", required=True, help="output .nvprev path")
    parser.add_argument("--max-edge", type=int, default=_DEFAULT_MAX_EDGE, help="clamp longest edge (guard)")
    args = parser.parse_args()

    try:
        w, h, total = compile_preview(args.pack, args.pet_id, args.variant_id, args.out, args.max_edge)
    except (PreviewCompileError, read_pet_pack.PackReadError, FileNotFoundError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    print(f"{args.out}: {w}x{h} preview for '{args.pet_id}/{args.variant_id}' ({total} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
