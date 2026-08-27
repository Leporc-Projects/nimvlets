#include "CollectionModelTest.h"

#include "catalog/CollectionModel.h"

#include <string>
#include <vector>

using nimvlets::catalog::BuildCollectionModel;
using nimvlets::catalog::CanActivate;
using nimvlets::catalog::CatalogEntry;
using nimvlets::catalog::CollectionModel;
using nimvlets::catalog::EnsureActivePetOwned;
using nimvlets::catalog::OwnershipStatus;
using nimvlets::catalog::PetCatalog;
using nimvlets::catalog::PetIdentity;
using nimvlets::catalog::SeedOwnershipFromCatalog;

namespace nimvlets::tests {

namespace {

// El catálogo de dev real de Block 06 reconstruido a mano (sin pasar
// por el .nvcat — eso ya lo cubre PetCatalogLoaderTest.cpp): Bunny
// (default, initiallyOwned), Nidir (no owned -> queda locked), y las
// DOS entradas de Frin compartiendo petId (male/female, initiallyOwned).
PetCatalog MakeDevCatalog() {
    std::vector<CatalogEntry> entries;

    CatalogEntry bunny;
    bunny.identity = PetIdentity{"bunny", ""};
    bunny.displayName = "Bunny";
    bunny.packPath = "bunny.nvpack";
    bunny.isDefault = true;
    bunny.initiallyOwned = true;
    entries.push_back(bunny);

    CatalogEntry nidir;
    nidir.identity = PetIdentity{"nidir", ""};
    nidir.displayName = "Nidir";
    nidir.packPath = "nidir.nvpack";
    nidir.initiallyOwned = false;
    entries.push_back(nidir);

    CatalogEntry frinMale;
    frinMale.identity = PetIdentity{"frin", "male"};
    frinMale.displayName = "Frin";
    frinMale.packPath = "frin_male.nvpack";
    frinMale.initiallyOwned = true;
    entries.push_back(frinMale);

    CatalogEntry frinFemale;
    frinFemale.identity = PetIdentity{"frin", "female"};
    frinFemale.displayName = "Frin";
    frinFemale.packPath = "frin_female.nvpack";
    frinFemale.initiallyOwned = true;
    entries.push_back(frinFemale);

    return PetCatalog(std::move(entries));
}

bool TestThreeOwnershipStatesDeriveCorrectly() {
    const PetCatalog catalog = MakeDevCatalog();
    const std::vector<std::string> owned = {"bunny", "frin"};
    const CollectionModel model = BuildCollectionModel(catalog, owned, PetIdentity{"bunny", ""});

    // Tres filas lógicas: Bunny, Nidir, Frin (las dos entradas de Frin
    // colapsan a una).
    NIMVLETS_CHECK(model.items.size() == 3);
    NIMVLETS_CHECK(model.items[0].petId == "bunny");
    NIMVLETS_CHECK(model.items[1].petId == "nidir");
    NIMVLETS_CHECK(model.items[2].petId == "frin");

    NIMVLETS_CHECK(model.items[0].status == OwnershipStatus::kActive);         // owned + active
    NIMVLETS_CHECK(model.items[2].status == OwnershipStatus::kOwnedInactive);  // owned + inactive
    NIMVLETS_CHECK(model.items[1].status == OwnershipStatus::kLocked);         // not owned
    return true;
}

bool TestFrinCollapsesToOneItemWithTwoVariants() {
    const PetCatalog catalog = MakeDevCatalog();
    const CollectionModel model = BuildCollectionModel(catalog, {"frin"}, PetIdentity{"bunny", ""});

    const auto* frin = model.Find("frin");
    NIMVLETS_CHECK(frin != nullptr);
    NIMVLETS_CHECK(frin->HasVariants());
    NIMVLETS_CHECK(frin->variants.size() == 2);
    NIMVLETS_CHECK(frin->variants[0].variantId == "male");
    NIMVLETS_CHECK(frin->variants[1].variantId == "female");

    // Bunny no tiene variantes: exactamente una, con id vacío.
    const auto* bunny = model.Find("bunny");
    NIMVLETS_CHECK(bunny != nullptr);
    NIMVLETS_CHECK(!bunny->HasVariants());
    NIMVLETS_CHECK(bunny->variants.size() == 1);
    NIMVLETS_CHECK(bunny->variants[0].variantId.empty());
    return true;
}

// El pet activo lleva su variante activa persistida como seleccionada;
// un pet inactivo cae a la primera del catálogo.
bool TestSelectedVariantFollowsActiveIdentity() {
    const PetCatalog catalog = MakeDevCatalog();

    const CollectionModel femaleActive =
        BuildCollectionModel(catalog, {"bunny", "frin"}, PetIdentity{"frin", "female"});
    const auto* frinActive = femaleActive.Find("frin");
    NIMVLETS_CHECK(frinActive->status == OwnershipStatus::kActive);
    NIMVLETS_CHECK(frinActive->selectedVariantId == "female");

    const CollectionModel bunnyActive =
        BuildCollectionModel(catalog, {"bunny", "frin"}, PetIdentity{"bunny", ""});
    const auto* frinInactive = bunnyActive.Find("frin");
    NIMVLETS_CHECK(frinInactive->status == OwnershipStatus::kOwnedInactive);
    NIMVLETS_CHECK(frinInactive->selectedVariantId == "male");  // primera del catálogo
    return true;
}

// Un variantId activo que no existe en el catálogo no debe "casi
// calzar": cae a la primera variante, sin crashear.
bool TestUnknownActiveVariantFallsBackToFirst() {
    const PetCatalog catalog = MakeDevCatalog();
    const CollectionModel model =
        BuildCollectionModel(catalog, {"frin"}, PetIdentity{"frin", "nonexistent"});
    const auto* frin = model.Find("frin");
    NIMVLETS_CHECK(frin->status == OwnershipStatus::kActive);
    NIMVLETS_CHECK(frin->selectedVariantId == "male");
    return true;
}

bool TestLockedPetCannotActivate() {
    const PetCatalog catalog = MakeDevCatalog();
    const CollectionModel model = BuildCollectionModel(catalog, {"bunny"}, PetIdentity{"bunny", ""});

    NIMVLETS_CHECK(!CanActivate(model, "nidir"));   // locked
    NIMVLETS_CHECK(CanActivate(model, "bunny"));    // active (no-op re-activate ok)
    NIMVLETS_CHECK(!CanActivate(model, "frin"));    // owned? no -> locked here
    NIMVLETS_CHECK(!CanActivate(model, "ghost"));   // not in catalog
    return true;
}

bool TestOwnedInactiveCanActivate() {
    const PetCatalog catalog = MakeDevCatalog();
    const CollectionModel model = BuildCollectionModel(catalog, {"bunny", "frin"}, PetIdentity{"bunny", ""});
    NIMVLETS_CHECK(CanActivate(model, "frin"));
    return true;
}

// El invariante "el pet activo siempre es propio" (block brief §9): si
// el pet activo persistido no está en ownedPetIds, se agrega.
bool TestEnsureActivePetOwnedAddsMissing() {
    std::vector<std::string> owned = {"bunny"};
    const bool changed = EnsureActivePetOwned(owned, "frin");
    NIMVLETS_CHECK(changed);
    NIMVLETS_CHECK((owned == std::vector<std::string>{"bunny", "frin"}));

    // Idempotente: llamarlo de nuevo no cambia nada.
    const bool changedAgain = EnsureActivePetOwned(owned, "frin");
    NIMVLETS_CHECK(!changedAgain);
    NIMVLETS_CHECK((owned == std::vector<std::string>{"bunny", "frin"}));
    return true;
}

bool TestEnsureActivePetOwnedNormalizes() {
    std::vector<std::string> owned = {"nidir", "bunny", "bunny", ""};
    const bool changed = EnsureActivePetOwned(owned, "bunny");
    NIMVLETS_CHECK(changed);  // dedup + drop-empty cuenta como cambio
    NIMVLETS_CHECK((owned == std::vector<std::string>{"bunny", "nidir"}));

    std::vector<std::string> empty;
    NIMVLETS_CHECK(!EnsureActivePetOwned(empty, ""));  // activo vacío -> no-op
    NIMVLETS_CHECK(empty.empty());
    return true;
}

bool TestSeedOwnershipFromCatalog() {
    const PetCatalog catalog = MakeDevCatalog();
    const std::vector<std::string> seed = SeedOwnershipFromCatalog(catalog);
    // bunny + frin (las dos entradas de Frin colapsan a un petId), nidir NO.
    NIMVLETS_CHECK((seed == std::vector<std::string>{"bunny", "frin"}));
    return true;
}

// Un modelo construido tras la siembra + EnsureActivePetOwned nunca
// tiene el pet activo como locked.
bool TestActivePetIsNeverLockedAfterSeed() {
    const PetCatalog catalog = MakeDevCatalog();
    std::vector<std::string> owned = SeedOwnershipFromCatalog(catalog);
    // Simula: el pet activo persistido es Nidir, que NO está en la
    // semilla — el invariante debe repararlo.
    EnsureActivePetOwned(owned, "nidir");
    const CollectionModel model = BuildCollectionModel(catalog, owned, PetIdentity{"nidir", ""});
    const auto* nidir = model.Find("nidir");
    NIMVLETS_CHECK(nidir->status == OwnershipStatus::kActive);
    NIMVLETS_CHECK(model.Active() != nullptr);
    NIMVLETS_CHECK(model.Active()->petId == "nidir");
    return true;
}

}  // namespace

void RegisterCollectionModelTests(testing::TestRunner& runner) {
    runner.Add("CollectionModel/ThreeOwnershipStatesDeriveCorrectly", TestThreeOwnershipStatesDeriveCorrectly);
    runner.Add("CollectionModel/FrinCollapsesToOneItemWithTwoVariants", TestFrinCollapsesToOneItemWithTwoVariants);
    runner.Add("CollectionModel/SelectedVariantFollowsActiveIdentity", TestSelectedVariantFollowsActiveIdentity);
    runner.Add("CollectionModel/UnknownActiveVariantFallsBackToFirst", TestUnknownActiveVariantFallsBackToFirst);
    runner.Add("CollectionModel/LockedPetCannotActivate", TestLockedPetCannotActivate);
    runner.Add("CollectionModel/OwnedInactiveCanActivate", TestOwnedInactiveCanActivate);
    runner.Add("CollectionModel/EnsureActivePetOwnedAddsMissing", TestEnsureActivePetOwnedAddsMissing);
    runner.Add("CollectionModel/EnsureActivePetOwnedNormalizes", TestEnsureActivePetOwnedNormalizes);
    runner.Add("CollectionModel/SeedOwnershipFromCatalog", TestSeedOwnershipFromCatalog);
    runner.Add("CollectionModel/ActivePetIsNeverLockedAfterSeed", TestActivePetIsNeverLockedAfterSeed);
}

}  // namespace nimvlets::tests
