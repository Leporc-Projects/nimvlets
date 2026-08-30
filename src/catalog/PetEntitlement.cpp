#include "catalog/PetEntitlement.h"

#include <algorithm>

namespace nimvlets::catalog {

void CanonicalizePetEntitlements(std::vector<PetEntitlement>& ents) {
    // 1. descartar petId vacío.
    ents.erase(std::remove_if(ents.begin(), ents.end(),
                              [](const PetEntitlement& e) { return e.petId.empty(); }),
               ents.end());
    // 2. ordenar por (petId, variantId).
    std::sort(ents.begin(), ents.end());
    // 3. duplicados exactos.
    ents.erase(std::unique(ents.begin(), ents.end()), ents.end());
    // Sin subsunción: `{frin, ""}` y `{frin, "male"}` conviven. Un
    // `{frin, ""}` legacy se expande contra el catálogo en src/app
    // ANTES de que esto lo vea (DEC-128).
}

bool OwnsIdentity(const std::vector<PetEntitlement>& ents, const PetIdentity& identity) {
    return std::any_of(ents.begin(), ents.end(),
                       [&](const PetEntitlement& e) { return e.Covers(identity); });
}

bool OwnsAnyVariantOfPet(const std::vector<PetEntitlement>& ents, const std::string& petId) {
    return std::any_of(ents.begin(), ents.end(),
                       [&](const PetEntitlement& e) { return e.petId == petId; });
}

}  // namespace nimvlets::catalog
