#include "catalog/PurchasePolicy.h"

namespace nimvlets::catalog {

const char* ToString(PurchaseResult result) {
    switch (result) {
        case PurchaseResult::kSuccess:
            return "success";
        case PurchaseResult::kAlreadyOwned:
            return "already owned";
        case PurchaseResult::kInsufficientBalance:
            return "insufficient balance";
        case PurchaseResult::kNotPurchasable:
            return "not publicly purchasable";
        case PurchaseResult::kInvalidTarget:
            return "invalid target";
    }
    return "unknown";
}

namespace {

// La entrada de catálogo pública de `petId`, o nullptr si el pet no
// existe / no tiene ninguna entrada pública. `outAnyEntryForPet` se
// marca si el petId aparece en el catálogo aunque sea sin ser público
// — así el caller distingue "no comprable" de "no existe".
const CatalogEntry* PublicEntryFor(
    const PetCatalog& catalog, const std::string& petId, bool& outAnyEntryForPet) {
    outAnyEntryForPet = false;
    for (const CatalogEntry& entry : catalog.Entries()) {
        if (entry.identity.petId != petId) {
            continue;
        }
        outAnyEntryForPet = true;
        if (entry.publiclyPurchasable) {
            return &entry;
        }
    }
    return nullptr;
}

}  // namespace

PurchaseOutcome EvaluatePurchase(
    const PetCatalog& catalog,
    const std::string& petId,
    std::uint64_t currentBalance,
    const std::vector<PetEntitlement>& currentEntitlements) {
    PurchaseOutcome out;
    // En cualquier fallo, el estado "resultante" es el de entrada,
    // canonicalizado — nunca a medio mutar.
    out.newBalance = currentBalance;
    out.newEntitlements = currentEntitlements;
    CanonicalizePetEntitlements(out.newEntitlements);

    bool anyEntry = false;
    const CatalogEntry* entry = PublicEntryFor(catalog, petId, anyEntry);
    if (!anyEntry) {
        out.result = PurchaseResult::kInvalidTarget;
        return out;
    }
    if (entry == nullptr || entry->priceClicks == 0) {
        // Sin entrada pública, o precio cero (no soportado — brief §26).
        out.result = PurchaseResult::kNotPurchasable;
        return out;
    }

    out.price = entry->priceClicks;
    out.grantedEntitlement = PetEntitlement{entry->identity.petId, entry->identity.variantId};

    if (OwnsIdentity(currentEntitlements,
                     PetIdentity{entry->identity.petId, entry->identity.variantId})) {
        out.result = PurchaseResult::kAlreadyOwned;
        return out;
    }
    if (currentBalance < entry->priceClicks) {
        out.result = PurchaseResult::kInsufficientBalance;
        return out;
    }

    // Éxito: la resta es segura (balance >= precio garantizado arriba),
    // así que newBalance nunca puede desbordar hacia abajo.
    out.result = PurchaseResult::kSuccess;
    out.newBalance = currentBalance - entry->priceClicks;
    out.newEntitlements.push_back(out.grantedEntitlement);
    CanonicalizePetEntitlements(out.newEntitlements);
    return out;
}

}  // namespace nimvlets::catalog
