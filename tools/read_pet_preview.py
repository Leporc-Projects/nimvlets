#!/usr/bin/env python3
"""Lector del formato ".nvprev" ("NVPREV1") que compila
`tools/compile_pet_preview.py` — el lado de LECTURA del mismo contrato
binario que `src/productui/PreviewArtifact.cpp` implementa en C++ (ver
el docstring de compile_pet_preview.py para el layout exacto).

Por qué existe como módulo aparte (Block 09A, pasada de endurecimiento
del gate de contenido): `tools/compile_pet_catalog.py` necesita
verificar que la preview de un starter de producción REALMENTE
pertenece a esa identidad de catálogo (mismo `pet_id` / `variant_id`) y
fue derivada del pack declarado (`source_pack`). Antes solo se
verificaba que el archivo EXISTIERA. `tools/test_asset_pipeline.py` ya
tenía un lector mínimo inline; esto lo hace compartible sin
duplicarlo.

No decodifica nada ni valida: solo parsea. Sin dependencias de
terceros.

Uso como script (inspección rápida, sin volcar píxeles):
    python3 tools/read_pet_preview.py assets/dev/frin_male_pack.nvprev
"""

from __future__ import annotations

import struct
import sys

MAGIC = b"NVPREV1\0"


class PreviewReadError(Exception):
    pass


def read_preview_header(path: str) -> dict:
    """Todo menos los píxeles: version, pet_id, variant_id, source_pack,
    width, height, pixel_bytes. Suficiente para validar identidad y
    procedencia."""
    with open(path, "rb") as f:
        # La cabecera son pocas decenas de bytes + los 3 strings; 4 KiB
        # sobra para cualquier basename razonable.
        data = f.read(4096)

    if data[:8] != MAGIC:
        raise PreviewReadError(f"{path}: not an NVPREV1 file (magic was {data[:8]!r})")

    pos = 8

    def u32() -> int:
        nonlocal pos
        if pos + 4 > len(data):
            raise PreviewReadError(f"{path}: truncated preview header")
        v = struct.unpack_from("<I", data, pos)[0]
        pos += 4
        return v

    def s() -> str:
        nonlocal pos
        n = u32()
        if pos + n > len(data):
            raise PreviewReadError(f"{path}: truncated preview string")
        v = data[pos : pos + n].decode("utf-8")
        pos += n
        return v

    version = u32()
    pet_id = s()
    variant_id = s()
    source_pack = s()
    width = u32()
    height = u32()
    pixel_bytes = u32()
    return {
        "version": version,
        "pet_id": pet_id,
        "variant_id": variant_id,
        "source_pack": source_pack,
        "width": width,
        "height": height,
        "pixel_bytes": pixel_bytes,
    }


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    h = read_preview_header(sys.argv[1])
    print(
        f"preview v{h['version']} pet_id='{h['pet_id']}' variant_id='{h['variant_id']}' "
        f"source_pack='{h['source_pack']}' {h['width']}x{h['height']} ({h['pixel_bytes']} px bytes)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
