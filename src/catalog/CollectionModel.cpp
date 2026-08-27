#include "catalog/CollectionModel.h"

#include <algorithm>
#include <unordered_set>

namespace nimvlets::catalog {

namespace {

// sort + unique + descartar el string vacío, en el lugar. Mismo
// resultado canónico que persistence::NormalizeOwnedPetIds, pero
// src/catalog no puede ver src/persistence (y no debe: la conexión
// catálogo<->persistencia la hace src/app) — así que este idiom de 3
// líneas se repite acá a propósito.
void CanonicalizeIds(std::vector<std::string>& ids) {
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    ids.erase(std::remove(ids.begin(), ids.end(), std::string()), ids.end());
}

}  // namespace

const CollectionItem* CollectionModel::Find(const std::string& petId) const {
    for (const CollectionItem& item : items) {
        if (item.petId == petId) {
            return &item;
        }
    }
    return nullptr;
}

const CollectionItem* CollectionModel::Active() const {
    for (const CollectionItem& item : items) {
        if (item.status == OwnershipStatus::kActive) {
            return &item;
        }
    }
    return nullptr;
}

CollectionModel BuildCollectionModel(
    const PetCatalog& catalog,
    const std::vector<std::string>& ownedPetIds,
    const PetIdentity& activeId) {
    const std::unordered_set<std::string> owned(ownedPetIds.begin(), ownedPetIds.end());

    CollectionModel model;
    model.activePetId = activeId.petId;
    model.activeVariantId = activeId.variantId;

    // Agrupa por petId preservando el orden de primera aparición en el
    // catálogo (y el orden de variantes dentro de cada grupo). Lineal:
    // los catálogos reales tienen un puñado de entradas.
    for (const CatalogEntry& entry : catalog.Entries()) {
        CollectionItem* item = nullptr;
        for (CollectionItem& candidate : model.items) {
            if (candidate.petId == entry.identity.petId) {
                item = &candidate;
                break;
            }
        }
        if (item == nullptr) {
            model.items.push_back(CollectionItem{});
            item = &model.items.back();
            item->petId = entry.identity.petId;
            item->displayName = entry.displayName;
        }
        item->variants.push_back(CollectionVariant{entry.identity.variantId, entry.displayName});
    }

    for (CollectionItem& item : model.items) {
        // Estado de propiedad — por petId, nunca por variante.
        if (item.petId == activeId.petId) {
            item.status = OwnershipStatus::kActive;
        } else if (owned.count(item.petId) != 0) {
            item.status = OwnershipStatus::kOwnedInactive;
        } else {
            item.status = OwnershipStatus::kLocked;
        }

        // Variante seleccionada por defecto. Un pet locked no muestra
        // detalle accionable, pero igual se le da un valor coherente.
        const bool activePet = item.status == OwnershipStatus::kActive;
        std::string wanted = activePet ? activeId.variantId : std::string();
        const bool wantedExists =
            std::any_of(item.variants.begin(), item.variants.end(),
                        [&](const CollectionVariant& v) { return v.variantId == wanted; });
        if (wantedExists) {
            item.selectedVariantId = wanted;
        } else {
            // Primera variante del catálogo (variantId vacío para un pet
            // sin variantes).
            item.selectedVariantId = item.variants.front().variantId;
        }
    }

    return model;
}

bool CanActivate(const CollectionModel& model, const std::string& petId) {
    const CollectionItem* item = model.Find(petId);
    if (item == nullptr) {
        return false;
    }
    return item->status == OwnershipStatus::kActive || item->status == OwnershipStatus::kOwnedInactive;
}

bool EnsureActivePetOwned(std::vector<std::string>& ownedPetIds, const std::string& activePetId) {
    if (activePetId.empty()) {
        return false;
    }
    // Se compara contra el valor CRUDO de entrada, no contra su forma
    // canónica: si lo guardado venía sin ordenar / con duplicados / con
    // un string vacío, reescribirlo canónico también es un cambio que
    // src/app debe persistir.
    const std::vector<std::string> before = ownedPetIds;

    std::vector<std::string> after = ownedPetIds;
    if (std::find(after.begin(), after.end(), activePetId) == after.end()) {
        after.push_back(activePetId);
    }
    CanonicalizeIds(after);

    ownedPetIds = after;
    return after != before;
}

std::vector<std::string> SeedOwnershipFromCatalog(const PetCatalog& catalog) {
    std::vector<std::string> seed;
    for (const CatalogEntry& entry : catalog.Entries()) {
        if (entry.initiallyOwned) {
            seed.push_back(entry.identity.petId);
        }
    }
    CanonicalizeIds(seed);
    return seed;
}

}  // namespace nimvlets::catalog
