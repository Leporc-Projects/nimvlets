#include "catalog/PetEntitlement.h"

#include <algorithm>

namespace nimvlets::catalog {

void CanonicalizePetEntitlements(std::vector<PetEntitlement>& ents) {
    // 1. descartar petId vacío.
    ents.erase(std::remove_if(ents.begin(), ents.end(),
                              [](const PetEntitlement& e) { return e.petId.empty(); }),
               ents.end());
    // 2. ordenar — operator< pone (p, "") antes que cualquier (p, <var>).
    std::sort(ents.begin(), ents.end());
    // 3. duplicados exactos.
    ents.erase(std::unique(ents.begin(), ents.end()), ents.end());
    // 4. subsunción: recorrido lineal; recordar el último petId que trajo
    //    una autorización de "pet entero" y saltar sus variantes concretas.
    std::vector<PetEntitlement> out;
    out.reserve(ents.size());
    std::string wholePetId;  // petId cuyo (p, "") ya se agregó
    bool haveWholePet = false;
    for (const PetEntitlement& e : ents) {
        if (haveWholePet && e.petId == wholePetId && !e.variantId.empty()) {
            continue;  // ya cubierta por (wholePetId, "")
        }
        if (e.variantId.empty()) {
            wholePetId = e.petId;
            haveWholePet = true;
        }
        out.push_back(e);
    }
    ents = std::move(out);
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
