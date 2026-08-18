#pragma once

#include <string>

#include "catalog/PetCatalog.h"
#include "catalog/PetIdentity.h"
#include "content/AnimationDefinition.h"

namespace nimvlets::catalog {

// Resultado de resolver una identidad persistida contra un catálogo.
struct ResolvedSelection {
    // Nunca null si `catalog` se cargó con éxito (invariante
    // garantizada por PetCatalogLoader: siempre hay al menos una
    // entrada y exactamente un default).
    const CatalogEntry* entry = nullptr;

    // true si `persistedIdentity` no calzó con ninguna entrada y se
    // usó el default del catálogo en su lugar.
    bool usedFallback = false;
};

// Busca `persistedIdentity` en `catalog`. Si existe una entrada cuya
// identidad calza exactamente, la usa; si no — incluyendo el caso de
// una identidad vacía (primera ejecución, sin save aún) — cae al
// default del catálogo. Determinista y puro: la misma entrada de
// entrada siempre produce el mismo resultado, sin tocar el filesystem.
// No verifica que el pack de la entrada resuelta realmente cargue —
// ver LoadPetForIdentity más abajo para eso.
ResolvedSelection ResolveActiveSelection(const PetCatalog& catalog, const PetIdentity& persistedIdentity);

// Busca `identity` en `catalog` y, si existe, intenta cargar su pack
// vía content::LoadPetPackFromFile(). Falla ruidosamente (retorna
// false, `outError` con un mensaje específico) tanto si `identity` no
// está en el catálogo como si su pack no carga — y en cualquiera de
// los dos casos `outPet` queda completamente intacto (la carga ocurre
// primero sobre un PetDefinition local; solo se mueve a `outPet` tras
// confirmar el éxito). Esto es lo que le permite tanto a la resolución
// de arranque como al switching en runtime reusar exactamente la misma
// función sin arriesgar dejar `outPet` a medio reemplazar ante un
// fallo — ver docs/CATALOG.md.
bool LoadPetForIdentity(
    const PetCatalog& catalog,
    const PetIdentity& identity,
    content::PetDefinition& outPet,
    std::string& outError);

}  // namespace nimvlets::catalog
