#!/usr/bin/env python3
"""Valida una carpeta de frames PNG deterministas (`frame_000.png`,
`frame_001.png`, ...) contra el contrato de normalización del block
brief 04.2 §4, antes de que cualquier frame entre a
tools/compile_pet_pack.py. Reusable para Nidir y para cualquier Nimvlet
futuro que siga la misma convención de nombrado (ver
docs/NIDIR_CONTENT.md).

Chequea:
    - al menos un frame presente
    - nombrado exactamente "frame_NNN.png" (NNN = índice decimal,
      cualquier cantidad de dígitos, sin ceros de más/de menos respecto
      al máximo índice -- ver _parse_frame_index)
    - orden determinista == orden numérico de índice
    - sin huecos (0..N-1 contiguo) ni índices duplicados
    - PNG 8-bit RGBA válido en cada frame (delegado a
      prep_dev_sprite.read_png_rgba, que ya rechaza cualquier otro
      formato)
    - mismas dimensiones en todos los frames
    - alpha "real": ningún frame es 100% opaco ni 100% transparente
      (eso indicaría un canal alpha degenerado, no una silueta real)

Reporta (no solo valida): cantidad de frames, dimensiones, rango de
fracción de pixeles transparentes entre frames, y rango del borde
inferior del bounding box de pixeles opacos entre frames (proxy de
"alineación al piso estable" -- ver el comentario de
`_opaque_bbox_bottom`: es una medida objetiva pero aproximada, el
brief mismo pide "as much as objectively measurable", no un chequeo
estricto).

Falla ruidosamente (excepción específica, nunca silenciosa) ante
cualquier violación estructural. Uso:

    python3 tools/validate_frame_sequence.py <carpeta_de_frames> [--alpha-threshold 128]

Sin dependencias de terceros.
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from dataclasses import dataclass

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import prep_dev_sprite  # noqa: E402  (reused PNG decoder, not reimplemented)

_FRAME_NAME_RE = re.compile(r"^frame_(\d+)\.png$")


class FrameSequenceError(Exception):
    """Fallo de validación claro y específico -- nunca se atrapa en silencio."""


@dataclass
class FrameSequenceReport:
    frame_count: int
    width: int
    height: int
    # Fracción de pixeles con alpha == 0 (totalmente transparentes),
    # mínimo y máximo entre todos los frames -- una silueta real
    # debería tener una fracción bien lejos de 0.0 y de 1.0 en ambos
    # extremos.
    transparent_fraction_range: tuple[float, float]
    # Coordenada Y del borde inferior del bounding box de pixeles
    # opacos (alpha >= threshold), mínimo y máximo entre todos los
    # frames -- ver _opaque_bbox_bottom() para las limitaciones de esta
    # medida.
    opaque_bbox_bottom_range: tuple[int, int]


def _parse_frame_index(filename: str) -> int:
    match = _FRAME_NAME_RE.match(filename)
    if not match:
        raise FrameSequenceError(f"nombre de archivo inesperado (se esperaba 'frame_NNN.png'): {filename}")
    return int(match.group(1))


def _transparent_fraction(width: int, height: int, pixels: bytes) -> float:
    transparent = 0
    total = width * height
    for i in range(3, len(pixels), 4):
        if pixels[i] == 0:
            transparent += 1
    return transparent / total if total else 0.0


def _opaque_bbox_bottom(width: int, height: int, pixels: bytes, threshold: int) -> int | None:
    """Y del borde inferior (fila más baja con al menos un pixel
    alpha >= threshold), o None si el frame no tiene ningún pixel
    opaco. Medida aproximada de "alineación al piso": una animación de
    idle real puede mover ligeramente este valor frame a frame (una
    respiración, un parpadeo de cola) -- este módulo lo REPORTA para
    que quien revise el import pueda juzgar si el rango es razonable,
    no lo convierte en un hard-fail, siguiendo la instrucción explícita
    del brief ("as much as objectively measurable")."""
    stride = width * 4
    for y in range(height - 1, -1, -1):
        row = pixels[y * stride : (y + 1) * stride]
        for x in range(3, len(row), 4):
            if row[x] >= threshold:
                return y
    return None


def validate_frame_sequence(frame_dir: str, alpha_hit_threshold: int = 128) -> FrameSequenceReport:
    if not os.path.isdir(frame_dir):
        raise FrameSequenceError(f"no es una carpeta: {frame_dir}")

    entries = [f for f in os.listdir(frame_dir) if f.lower().endswith(".png")]
    if not entries:
        raise FrameSequenceError(f"no hay ningún .png en {frame_dir}")

    indexed = sorted((_parse_frame_index(f), f) for f in entries)
    indices = [i for i, _ in indexed]

    seen: set[int] = set()
    for i in indices:
        if i in seen:
            raise FrameSequenceError(f"índice de frame duplicado: {i} (en {frame_dir})")
        seen.add(i)

    expected = list(range(len(indexed)))
    if indices != expected:
        raise FrameSequenceError(
            f"la secuencia de índices tiene huecos o no empieza en 0: encontrado {indices}, "
            f"se esperaba {expected} (en {frame_dir})"
        )

    width = height = None
    transparent_fractions: list[float] = []
    opaque_bottoms: list[int] = []
    for index, filename in indexed:
        path = os.path.join(frame_dir, filename)
        w, h, pixels = prep_dev_sprite.read_png_rgba(path)  # ya rechaza cualquier formato que no sea 8-bit RGBA

        if width is None:
            width, height = w, h
        elif (w, h) != (width, height):
            raise FrameSequenceError(
                f"{filename}: {w}x{h} no coincide con la dimensión de frame_000 ({width}x{height})"
            )

        frac = _transparent_fraction(w, h, pixels)
        if frac <= 0.0:
            raise FrameSequenceError(f"{filename}: 0% de pixeles transparentes -- canal alpha degenerado (frame totalmente opaco)")
        if frac >= 1.0:
            raise FrameSequenceError(f"{filename}: 100% de pixeles transparentes -- canal alpha degenerado (frame totalmente invisible)")
        transparent_fractions.append(frac)

        bottom = _opaque_bbox_bottom(w, h, pixels, alpha_hit_threshold)
        if bottom is not None:
            opaque_bottoms.append(bottom)

    assert width is not None and height is not None  # garantizado por el bucle de arriba (entries no está vacío)

    return FrameSequenceReport(
        frame_count=len(indexed),
        width=width,
        height=height,
        transparent_fraction_range=(min(transparent_fractions), max(transparent_fractions)),
        opaque_bbox_bottom_range=(min(opaque_bottoms), max(opaque_bottoms)) if opaque_bottoms else (0, 0),
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("frame_dir")
    parser.add_argument("--alpha-threshold", type=int, default=128)
    args = parser.parse_args()

    try:
        report = validate_frame_sequence(args.frame_dir, args.alpha_threshold)
    except FrameSequenceError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    print(f"{args.frame_dir}: {report.frame_count} frames, {report.width}x{report.height}")
    print(f"  fracción de pixeles transparentes: {report.transparent_fraction_range[0]:.3f} .. {report.transparent_fraction_range[1]:.3f}")
    print(f"  borde inferior del bbox opaco (alpha>={args.alpha_threshold}): fila {report.opaque_bbox_bottom_range[0]} .. {report.opaque_bbox_bottom_range[1]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
