#include "catalog/StarterShopModel.h"

namespace nimvlets::catalog {

const char* ToString(StarterShopOfferStatus status) {
    switch (status) {
        case StarterShopOfferStatus::kAffordable:
            return "affordable";
        case StarterShopOfferStatus::kInsufficientBalance:
            return "insufficient balance";
    }
    return "unknown";
}

const StarterShopOffer* StarterShopModel::Find(const PetIdentity& target) const {
    for (const StarterShopOffer& offer : offers) {
        if (offer.target == target) {
            return &offer;
        }
    }
    return nullptr;
}

bool IsStarterShopEligible(
    const PetCatalog& catalog,
    bool lifecycleCompleted,
    const std::vector<PetEntitlement>& owned,
    const PetIdentity& identity) {
    // (1) Gate de lifecycle — EXACTAMENTE kCompleted. Un usuario
    // legacy/dev (kLegacyComplete) o uno que nunca pasó por la elección
    // de starter (kPending) NUNCA ve el Starter Shop (brief §5): esto
    // evita exponer retroactivamente una economía de starters nueva a
    // quien no pasó por el contrato real de elección.
    if (!lifecycleCompleted) {
        return false;
    }

    // (7) + (2) Estructura: tiene que existir una entrada de catálogo con
    // esta identidad EXACTA, y ser parte de la política de starter.
    const CatalogEntry* entry = catalog.Find(identity);
    if (entry == nullptr || entry->starterRole == StarterRole::kNone) {
        return false;
    }

    // (3) Precio de la identidad de starter (brief §6). `priceClicks` es
    // el precio asociado a ESTA identidad exacta; `publiclyPurchasable`
    // (que acá NO se mira) es solo la elegibilidad para el Shop PÚBLICO.
    if (entry->priceClicks == 0) {
        return false;
    }

    // (4) La identidad EXACTA no está ya poseída. Para Frin, poseer
    // {frin, "female"} NO cubre {frin, "male"} (Covers exacto) — así la
    // variante no elegida sí se ofrece.
    if (OwnsIdentity(owned, identity)) {
        return false;
    }

    // (5) No-divulgación del secreto (brief §3): una oferta de un starter
    // SECRETO es elegible SOLO si el owner ya posee alguna autorización
    // EXACTA del MISMO petId lógico. Genérico por rol + petId — sin
    // ninguna rama `if (petId == "frin")`. Un starter normal se salta
    // este chequeo (brief §4).
    if (entry->starterRole == StarterRole::kSecret &&
        !OwnsAnyVariantOfPet(owned, identity.petId)) {
        return false;
    }

    return true;
}

StarterShopModel BuildStarterShopModel(
    const PetCatalog& catalog,
    bool lifecycleCompleted,
    const std::vector<PetEntitlement>& owned,
    std::uint64_t clickBalance) {
    StarterShopModel model;
    if (!lifecycleCompleted) {
        return model;  // brief §5 — el Starter Shop no existe fuera de kCompleted
    }

    for (const CatalogEntry& entry : catalog.Entries()) {
        if (!IsStarterShopEligible(catalog, lifecycleCompleted, owned, entry.identity)) {
            continue;
        }

        StarterShopOffer offer;
        offer.target = entry.identity;
        offer.displayName = entry.displayName;
        offer.role = entry.starterRole;
        offer.priceClicks = entry.priceClicks;
        offer.entitlementTarget = PetEntitlement{entry.identity.petId, entry.identity.variantId};
        if (clickBalance >= entry.priceClicks) {
            offer.status = StarterShopOfferStatus::kAffordable;
        } else {
            offer.status = StarterShopOfferStatus::kInsufficientBalance;
            offer.clicksShort = entry.priceClicks - clickBalance;
        }
        model.offers.push_back(std::move(offer));
    }

    return model;
}

}  // namespace nimvlets::catalog
