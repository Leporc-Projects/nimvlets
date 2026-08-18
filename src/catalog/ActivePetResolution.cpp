#include "catalog/ActivePetResolution.h"

#include "content/PetPackLoader.h"

#include <utility>

namespace nimvlets::catalog {

ResolvedSelection ResolveActiveSelection(const PetCatalog& catalog, const PetIdentity& persistedIdentity) {
    if (const CatalogEntry* found = catalog.Find(persistedIdentity)) {
        return ResolvedSelection{found, false};
    }
    return ResolvedSelection{&catalog.Default(), true};
}

bool LoadPetForIdentity(
    const PetCatalog& catalog,
    const PetIdentity& identity,
    content::PetDefinition& outPet,
    std::string& outError) {
    const CatalogEntry* entry = catalog.Find(identity);
    if (entry == nullptr) {
        outError = "identity not found in catalog: '" + identity.petId + "'" +
                   (identity.variantId.empty() ? "" : ("/" + identity.variantId));
        return false;
    }

    content::PetDefinition loaded;
    if (!content::LoadPetPackFromFile(entry->packPath, loaded, outError)) {
        return false;  // outPet intacto -- el pet activo anterior sigue usable
    }

    outPet = std::move(loaded);
    return true;
}

}  // namespace nimvlets::catalog
