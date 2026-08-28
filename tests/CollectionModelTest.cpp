#include "CollectionModelTest.h"

#include "catalog/CollectionModel.h"
#include "catalog/PetEntitlement.h"

#include <string>
#include <vector>

using nimvlets::catalog::BuildCollectionModel;
using nimvlets::catalog::CanActivate;
using nimvlets::catalog::CatalogEntry;
using nimvlets::catalog::CollectionModel;
using nimvlets::catalog::EnsureActiveEntitlementOwned;
using nimvlets::catalog::OwnershipStatus;
using nimvlets::catalog::PetCatalog;
using nimvlets::catalog::PetEntitlement;
using nimvlets::catalog::PetIdentity;
using nimvlets::catalog::SeedEntitlementsFromCatalog;

using Ents = std::vector<PetEntitlement>;

namespace nimvlets::tests {

namespace {

// El catálogo de dev real reconstruido a mano (sin pasar por el .nvcat):
// Bunny (default, initiallyOwned), Nidir (no owned -> locked), y las DOS
// entradas de Frin compartiendo petId (male/female, initiallyOwned).
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

// Poseer el pet entero.
PetEntitlement Whole(const std::string& petId) { return PetEntitlement{petId, ""}; }
// Poseer una variante concreta.
PetEntitlement Variant(const std::string& petId, const std::string& variantId) {
    return PetEntitlement{petId, variantId};
}

bool TestThreeOwnershipStatesDeriveCorrectly() {
    const PetCatalog catalog = MakeDevCatalog();
    const Ents owned = {Whole("bunny"), Whole("frin")};
    const CollectionModel model = BuildCollectionModel(catalog, owned, PetIdentity{"bunny", ""});

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
    const CollectionModel model = BuildCollectionModel(catalog, {Whole("frin")}, PetIdentity{"bunny", ""});

    const auto* frin = model.Find("frin");
    NIMVLETS_CHECK(frin != nullptr);
    NIMVLETS_CHECK(frin->HasVariants());
    NIMVLETS_CHECK(frin->variants.size() == 2);
    NIMVLETS_CHECK(frin->variants[0].variantId == "male");
    NIMVLETS_CHECK(frin->variants[1].variantId == "female");

    const auto* bunny = model.Find("bunny");
    NIMVLETS_CHECK(bunny != nullptr);
    NIMVLETS_CHECK(!bunny->HasVariants());
    NIMVLETS_CHECK(bunny->variants.size() == 1);
    NIMVLETS_CHECK(bunny->variants[0].variantId.empty());
    return true;
}

// Frin: macho poseído, hembra no -> el ítem es kOwnedInactive; el flag
// `owned` por variante distingue las dos. (Este es el escenario que el
// onboarding futuro produce; el owner actual, tras la migración, tiene
// las dos.)
bool TestFrinOneVariantOwned() {
    const PetCatalog catalog = MakeDevCatalog();
    const CollectionModel model =
        BuildCollectionModel(catalog, {Whole("bunny"), Variant("frin", "male")}, PetIdentity{"bunny", ""});
    const auto* frin = model.Find("frin");
    NIMVLETS_CHECK(frin != nullptr);
    NIMVLETS_CHECK(frin->status == OwnershipStatus::kOwnedInactive);
    NIMVLETS_CHECK(frin->VariantOwned("male"));
    NIMVLETS_CHECK(!frin->VariantOwned("female"));
    NIMVLETS_CHECK(!frin->AllVariantsOwned());
    return true;
}

// Frin: ninguna variante poseída -> kLocked, las dos `owned == false`.
bool TestFrinNoVariantsOwned() {
    const PetCatalog catalog = MakeDevCatalog();
    const CollectionModel model =
        BuildCollectionModel(catalog, {Whole("bunny")}, PetIdentity{"bunny", ""});
    const auto* frin = model.Find("frin");
    NIMVLETS_CHECK(frin->status == OwnershipStatus::kLocked);
    NIMVLETS_CHECK(!frin->VariantOwned("male"));
    NIMVLETS_CHECK(!frin->VariantOwned("female"));
    return true;
}

// Frin: pet entero poseído -> las dos variantes `owned == true`.
bool TestFrinBothVariantsOwnedViaWholePet() {
    const PetCatalog catalog = MakeDevCatalog();
    const CollectionModel model =
        BuildCollectionModel(catalog, {Whole("bunny"), Whole("frin")}, PetIdentity{"bunny", ""});
    const auto* frin = model.Find("frin");
    NIMVLETS_CHECK(frin->status == OwnershipStatus::kOwnedInactive);
    NIMVLETS_CHECK(frin->VariantOwned("male"));
    NIMVLETS_CHECK(frin->VariantOwned("female"));
    NIMVLETS_CHECK(frin->AllVariantsOwned());
    return true;
}

bool TestSelectedVariantFollowsActiveIdentity() {
    const PetCatalog catalog = MakeDevCatalog();

    const CollectionModel femaleActive =
        BuildCollectionModel(catalog, {Whole("bunny"), Whole("frin")}, PetIdentity{"frin", "female"});
    const auto* frinActive = femaleActive.Find("frin");
    NIMVLETS_CHECK(frinActive->status == OwnershipStatus::kActive);
    NIMVLETS_CHECK(frinActive->selectedVariantId == "female");

    const CollectionModel bunnyActive =
        BuildCollectionModel(catalog, {Whole("bunny"), Whole("frin")}, PetIdentity{"bunny", ""});
    const auto* frinInactive = bunnyActive.Find("frin");
    NIMVLETS_CHECK(frinInactive->status == OwnershipStatus::kOwnedInactive);
    NIMVLETS_CHECK(frinInactive->selectedVariantId == "male");  // primera del catálogo
    return true;
}

bool TestUnknownActiveVariantFallsBackToFirst() {
    const PetCatalog catalog = MakeDevCatalog();
    const CollectionModel model =
        BuildCollectionModel(catalog, {Whole("frin")}, PetIdentity{"frin", "nonexistent"});
    const auto* frin = model.Find("frin");
    NIMVLETS_CHECK(frin->status == OwnershipStatus::kActive);
    NIMVLETS_CHECK(frin->selectedVariantId == "male");
    return true;
}

// CanActivate: gate a nivel de pet Y de variante exacta (brief §6).
bool TestCanActivateGatesLockedAndUnownedVariant() {
    const PetCatalog catalog = MakeDevCatalog();
    const CollectionModel model = BuildCollectionModel(
        catalog, {Whole("bunny"), Variant("frin", "male")}, PetIdentity{"bunny", ""});

    NIMVLETS_CHECK(!CanActivate(model, "nidir"));            // locked
    NIMVLETS_CHECK(CanActivate(model, "bunny"));             // active
    NIMVLETS_CHECK(CanActivate(model, "frin", "male"));      // variante poseída
    NIMVLETS_CHECK(!CanActivate(model, "frin", "female"));   // variante NO poseída
    NIMVLETS_CHECK(CanActivate(model, "frin"));              // por defecto -> male, poseída
    NIMVLETS_CHECK(!CanActivate(model, "ghost"));            // no en catálogo
    return true;
}

// Con la variante por defecto (male) poseída, CanActivate(model,"frin")
// sin variante explícita usa selectedVariantId -> true.
bool TestCanActivateDefaultVariantResolves() {
    const PetCatalog catalog = MakeDevCatalog();
    const CollectionModel model =
        BuildCollectionModel(catalog, {Whole("bunny"), Whole("frin")}, PetIdentity{"bunny", ""});
    NIMVLETS_CHECK(CanActivate(model, "frin"));           // default male, poseída
    NIMVLETS_CHECK(CanActivate(model, "frin", "female"));
    return true;
}

// Invariante "el activo siempre es propio": si el pet/variante activo no
// está cubierto, se agrega la autorización EXACTA.
bool TestEnsureActiveEntitlementOwnedAddsExact() {
    Ents owned = {Whole("bunny")};
    const bool changed = EnsureActiveEntitlementOwned(owned, PetIdentity{"frin", "female"});
    NIMVLETS_CHECK(changed);
    NIMVLETS_CHECK((owned == Ents{Whole("bunny"), Variant("frin", "female")}));

    // Idempotente.
    NIMVLETS_CHECK(!EnsureActiveEntitlementOwned(owned, PetIdentity{"frin", "female"}));
    return true;
}

// Un "pet entero" ya cubre la identidad activa -> no cambia nada.
bool TestEnsureActiveEntitlementOwnedNoOpWhenWholePetCovers() {
    Ents owned = {Whole("frin")};
    NIMVLETS_CHECK(!EnsureActiveEntitlementOwned(owned, PetIdentity{"frin", "male"}));
    NIMVLETS_CHECK((owned == Ents{Whole("frin")}));

    Ents empty;
    NIMVLETS_CHECK(!EnsureActiveEntitlementOwned(empty, PetIdentity{"", ""}));  // activo vacío -> no-op
    return true;
}

// Un `owned` sin canonicalizar (desordenado / con duplicados / petId
// vacío) se reescribe canónico aunque ya cubra al activo -> cuenta como
// cambio (src/app lo persiste).
bool TestEnsureActiveEntitlementOwnedCanonicalizes() {
    Ents owned = {Variant("frin", "female"), Whole("bunny"), Whole("bunny"), PetEntitlement{"", "x"}};
    const bool changed = EnsureActiveEntitlementOwned(owned, PetIdentity{"bunny", ""});
    NIMVLETS_CHECK(changed);
    NIMVLETS_CHECK((owned == Ents{Whole("bunny"), Variant("frin", "female")}));
    return true;
}

bool TestSeedEntitlementsFromCatalog() {
    const PetCatalog catalog = MakeDevCatalog();
    const Ents seed = SeedEntitlementsFromCatalog(catalog);
    // Pet ENTERO por cada initiallyOwned -> {bunny,""} + {frin,""}; nidir no.
    NIMVLETS_CHECK((seed == Ents{Whole("bunny"), Whole("frin")}));
    return true;
}

// Tras la siembra + EnsureActiveEntitlementOwned, el modelo nunca tiene
// el pet activo como locked, y la variante activa queda válida.
bool TestActiveVariantValidAfterSeedAndInvariant() {
    const PetCatalog catalog = MakeDevCatalog();
    Ents owned = SeedEntitlementsFromCatalog(catalog);
    // El pet activo persistido es Nidir (no sembrado) con variante "".
    EnsureActiveEntitlementOwned(owned, PetIdentity{"nidir", ""});
    const CollectionModel model = BuildCollectionModel(catalog, owned, PetIdentity{"nidir", ""});
    const auto* nidir = model.Find("nidir");
    NIMVLETS_CHECK(nidir->status == OwnershipStatus::kActive);
    NIMVLETS_CHECK(model.Active() != nullptr && model.Active()->petId == "nidir");
    return true;
}

// El caso de la migración de un owner de Block 06: su Frin (pet entero)
// sobrevive con las DOS variantes activables.
bool TestMigratedFrinOwnerHasBothVariants() {
    const PetCatalog catalog = MakeDevCatalog();
    // Lo que produce la migración v3->v4: petIds -> pet entero.
    const Ents owned = {Whole("bunny"), Whole("frin")};
    const CollectionModel model = BuildCollectionModel(catalog, owned, PetIdentity{"frin", "male"});
    NIMVLETS_CHECK(CanActivate(model, "frin", "male"));
    NIMVLETS_CHECK(CanActivate(model, "frin", "female"));
    const auto* frin = model.Find("frin");
    NIMVLETS_CHECK(frin->AllVariantsOwned());
    return true;
}

}  // namespace

void RegisterCollectionModelTests(testing::TestRunner& runner) {
    runner.Add("CollectionModel/ThreeOwnershipStatesDeriveCorrectly", TestThreeOwnershipStatesDeriveCorrectly);
    runner.Add("CollectionModel/FrinCollapsesToOneItemWithTwoVariants", TestFrinCollapsesToOneItemWithTwoVariants);
    runner.Add("CollectionModel/FrinOneVariantOwned", TestFrinOneVariantOwned);
    runner.Add("CollectionModel/FrinNoVariantsOwned", TestFrinNoVariantsOwned);
    runner.Add("CollectionModel/FrinBothVariantsOwnedViaWholePet", TestFrinBothVariantsOwnedViaWholePet);
    runner.Add("CollectionModel/SelectedVariantFollowsActiveIdentity", TestSelectedVariantFollowsActiveIdentity);
    runner.Add("CollectionModel/UnknownActiveVariantFallsBackToFirst", TestUnknownActiveVariantFallsBackToFirst);
    runner.Add("CollectionModel/CanActivateGatesLockedAndUnownedVariant", TestCanActivateGatesLockedAndUnownedVariant);
    runner.Add("CollectionModel/CanActivateDefaultVariantResolves", TestCanActivateDefaultVariantResolves);
    runner.Add("CollectionModel/EnsureActiveEntitlementOwnedAddsExact", TestEnsureActiveEntitlementOwnedAddsExact);
    runner.Add("CollectionModel/EnsureActiveEntitlementOwnedNoOpWhenWholePetCovers",
               TestEnsureActiveEntitlementOwnedNoOpWhenWholePetCovers);
    runner.Add("CollectionModel/EnsureActiveEntitlementOwnedCanonicalizes",
               TestEnsureActiveEntitlementOwnedCanonicalizes);
    runner.Add("CollectionModel/SeedEntitlementsFromCatalog", TestSeedEntitlementsFromCatalog);
    runner.Add("CollectionModel/ActiveVariantValidAfterSeedAndInvariant", TestActiveVariantValidAfterSeedAndInvariant);
    runner.Add("CollectionModel/MigratedFrinOwnerHasBothVariants", TestMigratedFrinOwnerHasBothVariants);
}

}  // namespace nimvlets::tests
