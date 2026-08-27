#pragma once

#include <string>
#include <vector>

#include "catalog/PetIdentity.h"

namespace nimvlets::catalog {

// Una fila del catálogo: qué pet/variante es, cómo se llama, y dónde
// está su pack compilado. `packPath` se resuelve en runtime desde el
// directorio de trabajo del proceso, igual que `kPetPackPath` lo hacía
// antes de este bloque (ver docs/CATALOG.md) — no se valida como
// archivo existente al cargar el catálogo, solo como string no vacío;
// un pack faltante o corrupto se descubre y reporta claramente recién
// cuando algo intenta cargarlo de verdad (ver LoadPetForIdentity en
// ActivePetResolution.h).
struct CatalogEntry {
    PetIdentity identity;
    std::string displayName;
    std::string packPath;
    bool isDefault = false;

    // SEMILLA de propiedad para desarrollo/default (Block 06, schema
    // "NVCATLG1" v2). NO es autoridad de runtime: solo se consulta una
    // vez, cuando persistence::AppState::ownershipSeeded todavía es
    // false, para poblar `ownedPetIds`. A partir de ahí el archivo de
    // estado manda y este campo se ignora. Un bloque futuro de
    // onboarding (Block 09) reemplaza la siembra sin tocar el catálogo.
    // Ver docs/CATALOG.md §11 y docs/PRODUCT_UI.md §5.
    //
    // Para dos entradas que comparten petId (las dos variantes de
    // Frin), la propiedad es del petId: alcanza con que UNA lo marque,
    // pero el manifest de dev las marca a las dos por claridad.
    bool initiallyOwned = false;
};

// Un catálogo de pets ya validado y listo para consultarse: sin ids
// duplicados y con exactamente una entrada default. Puro — sin SDL,
// sin I/O de archivos — construido a partir de una lista de entradas
// que el llamador garantiza válida.
//
// El único punto de entrada normal para construir un PetCatalog real
// es PetCatalogLoader (ver PetCatalogLoader.h), que valida el formato
// binario "NVCATLG1" antes de llamar a este constructor — ver
// docs/CATALOG.md. Este constructor en sí NO revalida (duplicados,
// default único, campos no vacíos): es responsabilidad exclusiva del
// loader, igual que content::PetDefinition no se autovalida y confía
// en content::PetPackLoader. Construir un PetCatalog directamente con
// `entries` vacío o sin ningún `isDefault` produce un catálogo cuyo
// comportamiento en Default() no está definido — solo aceptable en
// tests que construyen fixtures ya sabidas válidas a mano.
class PetCatalog {
 public:
    // Catálogo vacío/inválido — solo existe para que PetCatalogLoader
    // pueda usar el mismo patrón de out-parameter que
    // content::LoadPetPackFromMemory (rellenar un PetCatalog& ya
    // existente al éxito, dejarlo sin especificar al fallo). Nunca es
    // el resultado de una carga exitosa.
    PetCatalog() = default;

    explicit PetCatalog(std::vector<CatalogEntry> entries);

    // nullptr si `identity` no aparece en el catálogo.
    const CatalogEntry* Find(const PetIdentity& identity) const;

    // La entrada marcada is_default. Precondición: el catálogo se
    // construyó a partir de una lista válida (ver el comentario de la
    // clase) — nunca es null en un catálogo real cargado por
    // PetCatalogLoader.
    const CatalogEntry& Default() const;

    const std::vector<CatalogEntry>& Entries() const { return entries_; }

 private:
    std::vector<CatalogEntry> entries_;
    std::size_t defaultIndex_ = 0;
};

}  // namespace nimvlets::catalog
