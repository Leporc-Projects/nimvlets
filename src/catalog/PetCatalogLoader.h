#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "catalog/PetCatalog.h"

namespace nimvlets::catalog {

// Versión de esquema actual del formato de catálogo binario
// "NVCATLG1" — ver docs/CATALOG.md para el layout exacto. Igual que
// persistence::AppState::kCurrentSchemaVersion, no hay lógica de
// migración en este bloque: cualquier versión distinta se rechaza.
constexpr std::uint32_t kCurrentCatalogSchemaVersion = 1;

// Parsea un catálogo compilado (ver tools/compile_pet_catalog.py para
// el productor) desde un buffer de bytes en memoria. Parseo puro — sin
// I/O de archivos — así que es directamente testeable con buffers
// sintéticos pequeños (ver tests/PetCatalogLoaderTest.cpp), el mismo
// patrón que content::PetPackLoader estableció en Block 02.
//
// Falla ruidosamente: retorna false y un mensaje legible en `outError`
// ante cualquier problema estructural — magic inválido, datos
// truncados, schema version que no coincide, cero entradas, un pet
// id o pack path vacío, un (petId, variantId) duplicado, o una
// cantidad de entradas marcadas is_default distinta de exactamente
// una. Nunca inventa, recorta ni adivina datos. `outCatalog` queda en
// un estado no especificado si falla; siempre verificar el valor de
// retorno.
//
// Deliberadamente NO valida que cada `packPath` apunte a un archivo
// que realmente existe — eso requeriría tocar el filesystem desde un
// parser que de otro modo es puro, y cargar cada pack para verificarlo
// violaría "el runtime no debe cargar todos los packs al arranque".
// Un pack faltante o corrupto se descubre recién cuando algo
// efectivamente intenta cargarlo (ver LoadPetForIdentity en
// ActivePetResolution.h, que reutiliza el mismo camino
// content::LoadPetPackFromFile que ya falla claramente en ese caso).
bool LoadCatalogFromMemory(const std::uint8_t* data, std::size_t size, PetCatalog& outCatalog, std::string& outError);

// Lee `path` a memoria y llama a LoadCatalogFromMemory(). Retorna
// false (con `outError` seteado) si el archivo no se puede abrir/leer
// en absoluto, distinto de — pero reportado de la misma forma que —
// un fallo de contenido malformado; el llamador solo necesita revisar
// el booleano de todos modos.
bool LoadCatalogFromFile(const std::string& path, PetCatalog& outCatalog, std::string& outError);

}  // namespace nimvlets::catalog
