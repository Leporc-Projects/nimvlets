#include "catalog/CollectionModel.h"

#include <algorithm>

namespace nimvlets::catalog {

bool CollectionItem::AllVariantsOwned() const {
    return std::all_of(variants.begin(), variants.end(),
                       [](const CollectionVariant& v) { return v.owned; });
}

bool CollectionItem::VariantOwned(const std::string& variantId) const {
    for (const CollectionVariant& v : variants) {
        if (v.variantId == variantId) {
            return v.owned;
        }
    }
    return false;
}

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
    const std::vector<PetEntitlement>& owned,
    const PetIdentity& activeId) {
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
        CollectionVariant variant;
        variant.variantId = entry.identity.variantId;
        variant.displayName = entry.displayName;
        variant.owned = OwnsIdentity(owned, PetIdentity{entry.identity.petId, entry.identity.variantId});
        item->variants.push_back(variant);
    }

    for (CollectionItem& item : model.items) {
        const bool anyOwned =
            std::any_of(item.variants.begin(), item.variants.end(),
                        [](const CollectionVariant& v) { return v.owned; });
        // Estado a nivel de pet lógico. "Activo" gana sobre todo: el
        // invariante garantiza que la variante activa es propia.
        if (item.petId == activeId.petId) {
            item.status = OwnershipStatus::kActive;
        } else if (anyOwned) {
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

bool CanActivate(const CollectionModel& model, const std::string& petId, const std::string& variantId) {
    const CollectionItem* item = model.Find(petId);
    if (item == nullptr) {
        return false;
    }
    if (item->status != OwnershipStatus::kActive && item->status != OwnershipStatus::kOwnedInactive) {
        return false;  // locked -> nunca (sin compra desde acá; el Shop es aparte)
    }
    // Gate a nivel de variante: la variante exacta a poner en el
    // escritorio tiene que estar poseída (brief §6). Un variantId vacío
    // en un pet CON variantes se resuelve contra la variante por
    // defecto del ítem.
    std::string wanted = variantId;
    if (wanted.empty() && item->HasVariants()) {
        wanted = item->selectedVariantId;
    }
    if (item->HasVariants()) {
        return item->VariantOwned(wanted);
    }
    // Pet sin variantes: su única "variante" (id vacío) lleva el flag.
    return item->variants.front().owned || item->status == OwnershipStatus::kActive;
}

namespace {

// Mapeo HISTÓRICO CONGELADO (DEC-129): qué autorizaciones EXPLÍCITAS
// otorgaba, bajo el modelo de propiedad "por pet lógico" de los schemas
// v1..v3, poseer un petId capaz de variantes. Deliberadamente
// independiente del catálogo ACTUAL — una variante agregada al catálogo
// DESPUÉS del schema v3 nunca formó parte de lo que "poseer <pet>"
// significaba, así que migrar un estado viejo NO debe otorgarla.
//
// Solo Frin califica: es el único Nimvlet capaz de variantes que
// existió bajo el modelo por-pet-lógico (Block 05/06), y sus dos
// variantes históricas son macho y hembra. Cualquier otro petId legacy
// era sin variantes y mapea natural a `{petId, ""}` -> devuelve
// nullptr. Si un bloque futuro AGREGA un Nimvlet capaz de variantes que
// alguna vez estuvo bajo el modelo viejo, se agrega una fila acá con la
// lista EXACTA de ese momento histórico — nunca se deriva del catálogo.
const std::vector<std::string>* HistoricalLegacyVariants(const std::string& petId) {
    static const std::vector<std::string> kFrinHistoricalVariants = {"female", "male"};  // orden canónico
    if (petId == "frin") {
        return &kFrinHistoricalVariants;
    }
    return nullptr;
}

}  // namespace

PetIdentity ResolveOwnedActiveIdentity(
    const std::vector<PetEntitlement>& owned,
    const PetCatalog& catalog,
    const PetIdentity& wanted,
    bool& outFellBack) {
    outFellBack = false;
    if (OwnsIdentity(owned, wanted)) {
        return wanted;
    }
    // No autorizado -> elegir una identidad que SÍ lo esté. Nunca se
    // otorga nada.
    const PetIdentity defaultId = catalog.Default().identity;
    if (OwnsIdentity(owned, defaultId)) {
        outFellBack = (defaultId != wanted);
        return defaultId;
    }
    for (const CatalogEntry& entry : catalog.Entries()) {
        if (OwnsIdentity(owned, entry.identity)) {
            outFellBack = (entry.identity != wanted);
            return entry.identity;
        }
    }
    // Degenerado: nada autorizado (solo alcanzable antes de la siembra).
    // src/app siembra en ese caso; devolver `wanted` sin marcar fallback.
    return wanted;
}

bool ExpandHistoricalWholePetEntitlements(std::vector<PetEntitlement>& ents) {
    const std::vector<PetEntitlement> before = ents;

    std::vector<PetEntitlement> out;
    out.reserve(ents.size());
    for (const PetEntitlement& e : ents) {
        const std::vector<std::string>* historical =
            e.variantId.empty() ? HistoricalLegacyVariants(e.petId) : nullptr;
        if (historical == nullptr) {
            out.push_back(e);  // ya es explícita, o era un pet legacy sin variantes
            continue;
        }
        // `{p, ""}` de un pet que HISTÓRICAMENTE tenía variantes ->
        // expandir al conjunto CONGELADO de ese momento. NO se re-agrega
        // el `{p, ""}`, y el catálogo actual no interviene.
        for (const std::string& variantId : *historical) {
            out.push_back(PetEntitlement{e.petId, variantId});
        }
    }
    CanonicalizePetEntitlements(out);
    ents = std::move(out);
    return ents != before;
}

std::vector<PetEntitlement> SeedEntitlementsFromCatalog(const PetCatalog& catalog) {
    std::vector<PetEntitlement> seed;
    for (const CatalogEntry& entry : catalog.Entries()) {
        if (entry.initiallyOwned) {
            // La autorización EXPLÍCITA de la entrada. Las dos entradas
            // de Frin marcadas initiallyOwned siembran {frin, "male"} y
            // {frin, "female"} — nunca un {frin, ""} de "todo Frin".
            seed.push_back(PetEntitlement{entry.identity.petId, entry.identity.variantId});
        }
    }
    CanonicalizePetEntitlements(seed);
    return seed;
}

}  // namespace nimvlets::catalog
