#!/usr/bin/env python3
"""Pipeline de assets dev: compila las vistas previas ".nvprev" de TODAS
las entradas del manifest del catálogo, de una sola pasada.

Para cada entrada de `pet_catalog_manifest.json` toma su `pack_path` y
escribe la preview liviana en la ruta hermana por CONVENCIÓN: el mismo
path con la extensión `.nvpack` cambiada por `.nvprev`
(`assets/dev/frin_male_pack.nvpack` -> `assets/dev/frin_male_pack.nvprev`).
Esa es la misma convención que `productui::PetPreviewCache` usa en
runtime para encontrar la preview de una entrada de catálogo — no hace
falta ningún campo nuevo en el formato binario del catálogo ni ninguna
migración de esquema (Block 06.2 §5).

Uso:
    python3 tools/compile_pet_previews.py [manifest.json]
        (default: assets/dev/pet_catalog_manifest.json)

Correr desde la raíz del repo. Sin dependencias de terceros.
"""

from __future__ import annotations

import json
import os
import sys

import compile_pet_preview  # noqa: E402

_DEFAULT_MANIFEST = "assets/dev/pet_catalog_manifest.json"


def preview_path_for_pack(pack_path: str) -> str:
    """La MISMA regla que productui::PreviewPathForPack en C++: cambia un
    sufijo `.nvpack` por `.nvprev`; si no termina en `.nvpack`, agrega
    `.nvprev`."""
    if pack_path.endswith(".nvpack"):
        return pack_path[: -len(".nvpack")] + ".nvprev"
    return pack_path + ".nvprev"


def compile_all(manifest_path: str) -> list[tuple[str, int, int, int]]:
    with open(manifest_path, "r", encoding="utf-8") as f:
        manifest = json.load(f)

    entries = manifest.get("entries") or []
    if not entries:
        raise SystemExit("error: manifest has zero entries")

    results: list[tuple[str, int, int, int]] = []
    for i, entry in enumerate(entries):
        pet_id = str(entry.get("pet_id", ""))
        variant_id = str(entry.get("variant_id", ""))
        pack_path = str(entry.get("pack_path", ""))
        if not pet_id or not pack_path:
            raise SystemExit(f"error: entries[{i}] missing pet_id or pack_path")
        out_path = preview_path_for_pack(pack_path)
        w, h, total = compile_pet_preview.compile_preview(pack_path, pet_id, variant_id, out_path)
        results.append((out_path, w, h, total))
    return results


def main() -> int:
    manifest_path = sys.argv[1] if len(sys.argv) > 1 else _DEFAULT_MANIFEST
    if not os.path.isfile(manifest_path):
        print(f"error: manifest '{manifest_path}' not found (run from the repository root)", file=sys.stderr)
        return 1

    try:
        results = compile_all(manifest_path)
    except (compile_pet_preview.PreviewCompileError, ValueError, KeyError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    total_bytes = sum(r[3] for r in results)
    for out_path, w, h, total in results:
        print(f"  {out_path}: {w}x{h} ({total} bytes)")
    print(f"compiled {len(results)} preview(s), {total_bytes} bytes total")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
