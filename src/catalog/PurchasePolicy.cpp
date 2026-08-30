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

PurchaseOutcome EvaluatePurchase(
    const PetCatalog& catalog,
    const PetIdentity& target,
    std::uint64_t currentBalance,
    const std::vector<PetEntitlement>& currentEntitlements) {
    PurchaseOutcome out;
    // En cualquier fallo, el estado "resultante" es el de entrada,
    // canonicalizado — nunca a medio mutar.
    out.newBalance = currentBalance;
    out.newEntitlements = currentEntitlements;
    CanonicalizePetEntitlements(out.newEntitlements);

    // El objetivo se resuelve contra una entrada EXACTA del catálogo
    // ({petId, variantId}). `{frin, ""}` (comprar "todo Frin") no calza
    // con ninguna entrada -> kInvalidTarget, no hay un pet entero que
    // comprar.
    const CatalogEntry* entry = catalog.Find(target);
    if (entry == nullptr) {
        out.result = PurchaseResult::kInvalidTarget;
        return out;
    }
    if (!entry->publiclyPurchasable || entry->priceClicks == 0) {
        // La entrada existe pero no se vende en el Shop público, o su
        // precio es 0 (no soportado — brief §26).
        out.result = PurchaseResult::kNotPurchasable;
        return out;
    }

    out.price = entry->priceClicks;
    // Lo que se otorga es la identidad de la entrada resuelta (== target
    // para una coincidencia exacta).
    out.grantedEntitlement = PetEntitlement{entry->identity.petId, entry->identity.variantId};

    if (OwnsIdentity(currentEntitlements, target)) {
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
