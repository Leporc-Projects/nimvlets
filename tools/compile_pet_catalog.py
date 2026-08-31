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
      "schema_version": 4,                     # opcional, default 4
      "production_onboarding_ready": false,    # opcional, default false (Block 09A)
      "entries": [
        {
          "pet_id": "bunny",
          "variant_id": "",                    # opcional, default ""
          "display_name": "Bunny",
          "pack_path": "assets/dev/bunny_pack.nvpack",
          "is_default": true,                  # opcional, default false
          "initially_owned": true,             # opcional, default false
          "price_clicks": 120,                # opcional, default 0
          "publicly_purchasable": true,        # opcional, default false
          "starter_role": "none"              # opcional: none|normal|secret (Block 09A)
        }
      ]
    }

`initially_owned` (Block 06, schema v2) es la SEMILLA de propiedad de
desarrollo/default: el runtime la usa una sola vez, cuando el archivo
de estado todavía no pasó por la inicialización de propiedad, para
sembrar las autorizaciones de propiedad. No es autoridad de runtime —
ver docs/CATALOG.md §11 y docs/PRODUCT_UI.md §5.

`price_clicks` / `publicly_purchasable` (Block 07, schema v3) son el
precio en clics y la elegibilidad para el Shop público, como DATO
(nunca una rama por pet en el runtime/UI — brief §10). Una entrada con
`publicly_purchasable: true` DEBE tener `price_clicks > 0` (precio cero
no soportado — brief §26). Frin queda `publicly_purchasable: false` en
las dos variantes: su obtención es onboarding + shop oculto, trabajo
futuro que NO se implementa acá (brief §11). Ver docs/CATALOG.md §12.

`starter_role` / `production_onboarding_ready` (Block 09A, schema v4)
son el rol de cada entrada en el onboarding de primer arranque
(none/normal/secret) y el datum EXPLÍCITO que arma el onboarding de
PRODUCCIÓN. Ver docs/ONBOARDING.md y DEC-132. `production_onboarding_ready`
solo se puede compilar en `true` si el manifest declara al menos
`REQUIRED_NORMAL_STARTERS` (3) entradas `starter_role: "normal"` Y el
contenido de cada una (el `pack_path` y su `.nvprev` hermano) existe en
disco — así un onboarding roto (assets de Artu/Rato/Rin Rin ausentes)
NUNCA puede quedar armado en producción (brief §8/§30/§31). La metadata
del secreto (Frin) sola nunca cuenta para eso.

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
    - un price_clicks negativo, o publicly_purchasable=true con
      price_clicks == 0
    - un starter_role desconocido
    - production_onboarding_ready=true con menos de 3 starters normales,
      o con un starter normal cuyo pack / .nvprev no existe en disco

Sin dependencias de terceros (json/struct/os son standard library).
"""

from __future__ import annotations

import json
import os
import struct
import sys

_MAGIC = b"NVCATLG1"
_CURRENT_SCHEMA_VERSION = 4

_STARTER_ROLES = {"none": 0, "normal": 1, "secret": 2}
REQUIRED_NORMAL_STARTERS = 3


class CatalogCompileError(Exception):
    """Un fallo de compilación claro y específico -- nunca se atrapa en silencio."""


def _require(manifest: dict, key: str, context: str):
    if key not in manifest:
        raise CatalogCompileError(f"{context}: missing required field '{key}'")
    return manifest[key]


def _pack_string(s: str) -> bytes:
    encoded = s.encode("utf-8")
    return struct.pack("<I", len(encoded)) + encoded


def _preview_path_for_pack(pack_path: str) -> str:
    # Misma convención que productui::PreviewPathForPack: reemplaza la
    # extensión .nvpack por .nvprev (o le agrega .nvprev si no termina en
    # .nvpack).
    if pack_path.endswith(".nvpack"):
        return pack_path[: -len(".nvpack")] + ".nvprev"
    return pack_path + ".nvprev"


def _compile_entry(entry_manifest: dict, index: int) -> tuple[bytes, dict]:
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
    initially_owned = bool(entry_manifest.get("initially_owned", False))

    price_clicks = int(entry_manifest.get("price_clicks", 0))
    if price_clicks < 0:
        raise CatalogCompileError(f"{context} ('{pet_id}'): price_clicks must not be negative")
    publicly_purchasable = bool(entry_manifest.get("publicly_purchasable", False))
    if publicly_purchasable and price_clicks == 0:
        raise CatalogCompileError(
            f"{context} ('{pet_id}'): publicly_purchasable is true but price_clicks is 0 "
            "(zero-price purchases are not supported)"
        )

    starter_role_name = str(entry_manifest.get("starter_role", "none"))
    if starter_role_name not in _STARTER_ROLES:
        raise CatalogCompileError(
            f"{context} ('{pet_id}'): unknown starter_role '{starter_role_name}' "
            f"(expected one of {sorted(_STARTER_ROLES)})"
        )
    starter_role = _STARTER_ROLES[starter_role_name]

    out = bytearray()
    out += _pack_string(pet_id)
    out += _pack_string(variant_id)
    out += _pack_string(display_name)
    out += _pack_string(pack_path)
    out += struct.pack("<B", 1 if is_default else 0)
    out += struct.pack("<B", 1 if initially_owned else 0)
    out += struct.pack("<Q", price_clicks)
    out += struct.pack("<B", 1 if publicly_purchasable else 0)
    out += struct.pack("<B", starter_role)
    meta = {
        "pet_id": pet_id,
        "variant_id": variant_id,
        "is_default": is_default,
        "starter_role": starter_role_name,
        "pack_path": pack_path,
    }
    return bytes(out), meta


def compile_catalog(manifest_path: str, output_path: str) -> tuple[int, int]:
    with open(manifest_path, "r", encoding="utf-8") as f:
        manifest = json.load(f)

    schema_version = int(manifest.get("schema_version", _CURRENT_SCHEMA_VERSION))
    if schema_version != _CURRENT_SCHEMA_VERSION:
        raise CatalogCompileError(
            f"manifest: schema_version {schema_version} is not the supported version "
            f"({_CURRENT_SCHEMA_VERSION})"
        )

    production_onboarding_ready = bool(manifest.get("production_onboarding_ready", False))

    entry_manifests = _require(manifest, "entries", "manifest")
    if not entry_manifests:
        raise CatalogCompileError("manifest: 'entries' must have at least one item")

    seen_identities: set[tuple[str, str]] = set()
    default_count = 0
    entry_blobs: list[bytes] = []
    normal_starters: list[dict] = []
    for i, em in enumerate(entry_manifests):
        blob, meta = _compile_entry(em, i)
        identity_key = (meta["pet_id"], meta["variant_id"])
        if identity_key in seen_identities:
            raise CatalogCompileError(
                f"entries[{i}]: duplicate identity ('{meta['pet_id']}', '{meta['variant_id']}')"
            )
        seen_identities.add(identity_key)
        if meta["is_default"]:
            default_count += 1
        if meta["starter_role"] == "normal":
            normal_starters.append(meta)
        entry_blobs.append(blob)

    if default_count != 1:
        raise CatalogCompileError(
            f"manifest: exactly one entry must have is_default=true, found {default_count}"
        )

    # Gate de contenido listo para producción (brief §8/§30/§31): un
    # onboarding roto NUNCA puede quedar armado.
    if production_onboarding_ready:
        if len(normal_starters) < REQUIRED_NORMAL_STARTERS:
            raise CatalogCompileError(
                f"manifest: production_onboarding_ready is true but there are only "
                f"{len(normal_starters)} entr{'y' if len(normal_starters) == 1 else 'ies'} with "
                f"starter_role 'normal'; need {REQUIRED_NORMAL_STARTERS} (Artu / Rato / Rin Rin)"
            )
        for meta in normal_starters:
            preview_path = _preview_path_for_pack(meta["pack_path"])
            if not os.path.isfile(preview_path):
                raise CatalogCompileError(
                    f"manifest: production_onboarding_ready is true but normal starter "
                    f"'{meta['pet_id']}' has no preview asset at '{preview_path}' "
                    "(run tools/compile_pet_previews.py after generating its pack)"
                )

    out = bytearray()
    out += _MAGIC
    out += struct.pack("<II", _CURRENT_SCHEMA_VERSION, len(entry_blobs))
    out += struct.pack("<B", 1 if production_onboarding_ready else 0)
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
