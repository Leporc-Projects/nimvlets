#include "catalog/StarterPurchasePolicy.h"

namespace nimvlets::catalog {

const char* ToString(StarterPurchaseResult result) {
    switch (result) {
        case StarterPurchaseResult::kSuccess:
            return "success";
        case StarterPurchaseResult::kAlreadyOwned:
            return "already owned";
        case StarterPurchaseResult::kInsufficientBalance:
            return "insufficient balance";
        case StarterPurchaseResult::kLifecycleNotCompleted:
            return "onboarding lifecycle is not completed";
        case StarterPurchaseResult::kInvalidTarget:
            return "invalid target";
        case StarterPurchaseResult::kNotEligible:
            return "not a starter-shop offer";
    }
    return "unknown";
}

StarterPurchaseOutcome EvaluateStarterPurchase(
    const PetCatalog& catalog,
    bool lifecycleCompleted,
    const PetIdentity& target,
    std::uint64_t currentBalance,
    const std::vector<PetEntitlement>& currentEntitlements) {
    StarterPurchaseOutcome out;
    // En cualquier fallo, el estado "resultante" es el de entrada,
    // canonicalizado — nunca a medio mutar (brief §28).
    out.newBalance = currentBalance;
    out.newEntitlements = currentEntitlements;
    CanonicalizePetEntitlements(out.newEntitlements);

    // (1) Gate de lifecycle — EXACTAMENTE kCompleted (brief §5/§15).
    if (!lifecycleCompleted) {
        out.result = StarterPurchaseResult::kLifecycleNotCompleted;
        return out;
    }

    // (2) La identidad EXACTA tiene que existir en el catálogo. `{frin,
    // ""}` (comprar "todo Frin") no calza con ninguna entrada ->
    // kInvalidTarget (brief §13: nunca hay un "pet entero" que comprar).
    const CatalogEntry* entry = catalog.Find(target);
    if (entry == nullptr) {
        out.result = StarterPurchaseResult::kInvalidTarget;
        return out;
    }

    // (3) Tiene que ser parte de la política de starter y tener precio.
    if (entry->starterRole == StarterRole::kNone || entry->priceClicks == 0) {
        out.result = StarterPurchaseResult::kNotEligible;
        return out;
    }

    // (4) No-divulgación del secreto (brief §3/§15): una compra de un
    // starter SECRETO exige que el owner ya posea alguna variante hermana
    // del MISMO petId lógico. Genérico por rol + petId.
    if (entry->starterRole == StarterRole::kSecret &&
        !OwnsAnyVariantOfPet(currentEntitlements, target.petId)) {
        out.result = StarterPurchaseResult::kNotEligible;
        return out;
    }

    out.price = entry->priceClicks;
    // Lo que se otorga es la identidad de la entrada resuelta (== target
    // para una coincidencia exacta) — para Frin, la VARIANTE concreta.
    out.grantedEntitlement = PetEntitlement{entry->identity.petId, entry->identity.variantId};

    // (5) Ya poseída.
    if (OwnsIdentity(currentEntitlements, target)) {
        out.result = StarterPurchaseResult::kAlreadyOwned;
        return out;
    }
    // (6) Saldo.
    if (currentBalance < entry->priceClicks) {
        out.result = StarterPurchaseResult::kInsufficientBalance;
        return out;
    }

    // Éxito: la resta es segura (balance >= precio garantizado arriba).
    out.result = StarterPurchaseResult::kSuccess;
    out.newBalance = currentBalance - entry->priceClicks;
    out.newEntitlements.push_back(out.grantedEntitlement);
    CanonicalizePetEntitlements(out.newEntitlements);
    return out;
}

}  // namespace nimvlets::catalog
