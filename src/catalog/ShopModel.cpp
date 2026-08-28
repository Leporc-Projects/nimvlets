#include "catalog/ShopModel.h"

namespace nimvlets::catalog {

const ShopItem* ShopModel::Find(const std::string& petId) const {
    for (const ShopItem& item : items) {
        if (item.petId == petId) {
            return &item;
        }
    }
    return nullptr;
}

ShopModel BuildShopModel(
    const PetCatalog& catalog,
    std::uint64_t clickBalance,
    const std::vector<PetEntitlement>& ownedEntitlements) {
    ShopModel model;

    for (const CatalogEntry& entry : catalog.Entries()) {
        if (!entry.publiclyPurchasable) {
            continue;  // Frin y cualquier entrada no pública nunca se listan
        }
        // Una fila por pet lógico: si ya se agregó este petId (otra
        // variante pública, caso futuro), no se duplica.
        bool already = false;
        for (const ShopItem& existing : model.items) {
            if (existing.petId == entry.identity.petId) {
                already = true;
                break;
            }
        }
        if (already) {
            continue;
        }

        ShopItem item;
        item.petId = entry.identity.petId;
        item.displayName = entry.displayName;
        item.priceClicks = entry.priceClicks;
        item.entitlementTarget = PetEntitlement{entry.identity.petId, entry.identity.variantId};

        const bool owned = OwnsIdentity(
            ownedEntitlements, PetIdentity{entry.identity.petId, entry.identity.variantId});
        if (owned) {
            item.status = ShopItemStatus::kOwned;
        } else if (clickBalance >= entry.priceClicks) {
            item.status = ShopItemStatus::kAffordable;
        } else {
            item.status = ShopItemStatus::kInsufficientBalance;
            item.clicksShort = entry.priceClicks - clickBalance;
        }

        model.items.push_back(std::move(item));
    }

    return model;
}

}  // namespace nimvlets::catalog
