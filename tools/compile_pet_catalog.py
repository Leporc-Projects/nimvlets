#!/usr/bin/env python3
"""Pipeline de assets dev: compila un manifest JSON de catálogo al
formato binario runtime ".nvcat" que lee catalog::PetCatalogLoader, sin
necesitar un parser de JSON en el lado C++ (ver
src/catalog/PetCatalogLoader.cpp y docs/CATALOG.md para el layout
exacto en disco, "NVCATLG1").

Mismo patrón que tools/compile_pet_pack.py (manifest JSON -> binario
determinista, sin dependencias de terceros) — deliberadamente NO
reutiliza ese script (el catálogo no tiene frames/PNGs que decodificar,
así que no hay nada de prep_dev_sprite.py que compartir aquí).

Esquema del manifest (JSON):
    {
      "schema_version": 1,                    # opcional, default 1
      "entries": [
        {
          "pet_id": "bunny_dev",
          "variant_id": "",                   # opcional, default ""
          "display_name": "Bunny (dev fixture)",
          "pack_path": "assets/dev/bunny_pack.nvpack",
          "is_default": true                  # opcional, default false
        }
      ]
    }

`pack_path` se guarda tal cual en el binario compilado — a diferencia
de los `source` de frames en compile_pet_pack.py (resueltos relativos
al directorio del manifest, porque ESTE script necesita leer esos PNG
ahora mismo), `pack_path` es una referencia que el C++ en runtime
resolverá más tarde desde el directorio de trabajo del proceso, igual
que el path fijo que reemplaza este bloque. Por eso este script sí
verifica -- como chequeo de cordura al compilar, no como parte del
formato binario -- que `pack_path` exista relativo al directorio
actual (se asume que este script se corre desde la raíz del repo,
igual que el resto de tools/).

Uso:
    python3 tools/compile_pet_catalog.py <manifest.json> <output.nvcat>

Falla ruidosamente (salida no-cero, mensaje específico en stderr) ante:
    - cero entradas
    - un pet_id o pack_path vacío en alguna entrada
    - una identidad (pet_id, variant_id) duplicada
    - una cantidad de entradas is_default distinta de exactamente una
    - un pack_path que no apunta a un archivo existente (chequeo de
      cordura en tiempo de compilación, no una garantía del formato
      binario en sí -- ver docs/CATALOG.md)

Sin dependencias de terceros (json/struct/os son standard library).
"""

from __future__ import annotations

import json
import os
import struct
import sys

_MAGIC = b"NVCATLG1"
_CURRENT_SCHEMA_VERSION = 1


class CatalogCompileError(Exception):
    """Un fallo de compilación claro y específico -- nunca se atrapa en silencio."""


def _require(manifest: dict, key: str, context: str):
    if key not in manifest:
        raise CatalogCompileError(f"{context}: missing required field '{key}'")
    return manifest[key]


def _pack_string(s: str) -> bytes:
    encoded = s.encode("utf-8")
    return struct.pack("<I", len(encoded)) + encoded


def _compile_entry(entry_manifest: dict, index: int) -> tuple[bytes, str, str, bool]:
    context = f"entries[{index}]"
    pet_id = str(_require(entry_manifest, "pet_id", context))
    if not pet_id:
        raise CatalogCompileError(f"{context}: pet_id must not be empty")

    variant_id = str(entry_manifest.get("variant_id", ""))
    display_name = str(entry_manifest.get("display_name", pet_id))
    pack_path = str(_require(entry_manifest, "pack_path", context))
    if not pack_path:
        raise CatalogCompileError(f"{context} ('{pet_id}'): pack_path must not be empty")
    if not os.path.isfile(pack_path):
        raise CatalogCompileError(
            f"{context} ('{pet_id}'): pack_path '{pack_path}' does not exist relative to the "
            "current directory -- run this tool from the repository root"
        )

    is_default = bool(entry_manifest.get("is_default", False))

    out = bytearray()
    out += _pack_string(pet_id)
    out += _pack_string(variant_id)
    out += _pack_string(display_name)
    out += _pack_string(pack_path)
    out += struct.pack("<B", 1 if is_default else 0)
    return bytes(out), pet_id, variant_id, is_default


def compile_catalog(manifest_path: str, output_path: str) -> tuple[int, int]:
    with open(manifest_path, "r", encoding="utf-8") as f:
        manifest = json.load(f)

    schema_version = int(manifest.get("schema_version", _CURRENT_SCHEMA_VERSION))
    if schema_version != _CURRENT_SCHEMA_VERSION:
        raise CatalogCompileError(
            f"manifest: schema_version {schema_version} is not the supported version "
            f"({_CURRENT_SCHEMA_VERSION})"
        )

    entry_manifests = _require(manifest, "entries", "manifest")
    if not entry_manifests:
        raise CatalogCompileError("manifest: 'entries' must have at least one item")

    seen_identities: set[tuple[str, str]] = set()
    default_count = 0
    entry_blobs: list[bytes] = []
    for i, em in enumerate(entry_manifests):
        blob, pet_id, variant_id, is_default = _compile_entry(em, i)
        identity_key = (pet_id, variant_id)
        if identity_key in seen_identities:
            raise CatalogCompileError(f"entries[{i}]: duplicate identity ('{pet_id}', '{variant_id}')")
        seen_identities.add(identity_key)
        if is_default:
            default_count += 1
        entry_blobs.append(blob)

    if default_count != 1:
        raise CatalogCompileError(
            f"manifest: exactly one entry must have is_default=true, found {default_count}"
        )

    out = bytearray()
    out += _MAGIC
    out += struct.pack("<II", _CURRENT_SCHEMA_VERSION, len(entry_blobs))
    for blob in entry_blobs:
        out += blob

    with open(output_path, "wb") as f:
        f.write(out)

    return len(entry_blobs), len(out)


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2

    manifest_path, output_path = sys.argv[1], sys.argv[2]
    try:
        entry_count, total_bytes = compile_catalog(manifest_path, output_path)
    except (CatalogCompileError, FileNotFoundError, json.JSONDecodeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    print(f"{output_path}: compiled catalog with {entry_count} entr{'y' if entry_count == 1 else 'ies'} ({total_bytes} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
