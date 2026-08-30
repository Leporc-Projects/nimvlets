#include "CollectionModelTest.h"

#include "catalog/CollectionModel.h"
#include "catalog/PetEntitlement.h"

#include <string>
#include <vector>

using nimvlets::catalog::BuildCollectionModel;
using nimvlets::catalog::CanActivate;
using nimvlets::catalog::CatalogEntry;
using nimvlets::catalog::CollectionModel;
using nimvlets::catalog::ExpandHistoricalWholePetEntitlements;
using nimvlets::catalog::OwnershipStatus;
using nimvlets::catalog::PetCatalog;
using nimvlets::catalog::PetEntitlement;
using nimvlets::catalog::PetIdentity;
using nimvlets::catalog::ResolveOwnedActiveIdentity;
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

PetEntitlement NoVar(const std::string& p) { return PetEntitlement{p, ""}; }
PetEntitlement Var(const std::string& p, const std::string& v) { return PetEntitlement{p, v}; }
// Frin como lo tiene el owner tras la migración/siembra: las DOS
// variantes explícitas, NO un {frin, ""}.
Ents FrinBoth() { return {Var("frin", "female"), Var("frin", "male")}; }
Ents MigratedOwner() { return {NoVar("bunny"), Var("frin", "female"), Var("frin", "male")}; }

bool TestThreeOwnershipStatesDeriveCorrectly() {
    const PetCatalog catalog = MakeDevCatalog();
    const CollectionModel model = BuildCollectionModel(catalog, MigratedOwner(), PetIdentity{"bunny", ""});

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
    const CollectionModel model = BuildCollectionModel(catalog, FrinBoth(), PetIdentity{"bunny", ""});

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
// `owned` por variante distingue las dos. (El escenario que el
// onboarding futuro produce.)
bool TestFrinOneVariantOwned() {
    const PetCatalog catalog = MakeDevCatalog();
    const CollectionModel model =
        BuildCollectionModel(catalog, {NoVar("bunny"), Var("frin", "male")}, PetIdentity{"bunny", ""});
    const auto* frin = model.Find("frin");
    NIMVLETS_CHECK(frin != nullptr);
    NIMVLETS_CHECK(frin->status == OwnershipStatus::kOwnedInactive);
    NIMVLETS_CHECK(frin->VariantOwned("male"));
    NIMVLETS_CHECK(!frin->VariantOwned("female"));
    NIMVLETS_CHECK(!frin->AllVariantsOwned());
    return true;
}

// Frin: ninguna variante poseída -> kLocked, las dos `owned == false`.
// Un {frin, ""} suelto NO cuenta como poseer ninguna variante.
bool TestFrinNoVariantsOwned() {
    const PetCatalog catalog = MakeDevCatalog();
    for (const Ents& owned : {Ents{NoVar("bunny")}, Ents{NoVar("bunny"), NoVar("frin")}}) {
        const CollectionModel model = BuildCollectionModel(catalog, owned, PetIdentity{"bunny", ""});
        const auto* frin = model.Find("frin");
        NIMVLETS_CHECK(frin->status == OwnershipStatus::kLocked);
        NIMVLETS_CHECK(!frin->VariantOwned("male"));
        NIMVLETS_CHECK(!frin->VariantOwned("female"));
    }
    return true;
}

// Frin: las dos variantes explícitas -> ambas `owned == true`.
bool TestFrinBothVariantsOwned() {
    const PetCatalog catalog = MakeDevCatalog();
    const CollectionModel model =
        BuildCollectionModel(catalog, MigratedOwner(), PetIdentity{"bunny", ""});
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
        BuildCollectionModel(catalog, MigratedOwner(), PetIdentity{"frin", "female"});
    const auto* frinActive = femaleActive.Find("frin");
    NIMVLETS_CHECK(frinActive->status == OwnershipStatus::kActive);
    NIMVLETS_CHECK(frinActive->selectedVariantId == "female");

    const CollectionModel bunnyActive =
        BuildCollectionModel(catalog, MigratedOwner(), PetIdentity{"bunny", ""});
    const auto* frinInactive = bunnyActive.Find("frin");
    NIMVLETS_CHECK(frinInactive->status == OwnershipStatus::kOwnedInactive);
    NIMVLETS_CHECK(frinInactive->selectedVariantId == "male");  // primera del catálogo
    return true;
}

bool TestUnknownActiveVariantFallsBackToFirst() {
    const PetCatalog catalog = MakeDevCatalog();
    const CollectionModel model =
        BuildCollectionModel(catalog, FrinBoth(), PetIdentity{"frin", "nonexistent"});
    const auto* frin = model.Find("frin");
    NIMVLETS_CHECK(frin->status == OwnershipStatus::kActive);
    NIMVLETS_CHECK(frin->selectedVariantId == "male");
    return true;
}

// CanActivate: gate a nivel de pet Y de variante exacta (brief §6).
bool TestCanActivateGatesLockedAndUnownedVariant() {
    const PetCatalog catalog = MakeDevCatalog();
    const CollectionModel model = BuildCollectionModel(
        catalog, {NoVar("bunny"), Var("frin", "male")}, PetIdentity{"bunny", ""});

    NIMVLETS_CHECK(!CanActivate(model, "nidir"));            // locked
    NIMVLETS_CHECK(CanActivate(model, "bunny"));             // active
    NIMVLETS_CHECK(CanActivate(model, "frin", "male"));      // variante poseída
    NIMVLETS_CHECK(!CanActivate(model, "frin", "female"));   // variante NO poseída
    NIMVLETS_CHECK(CanActivate(model, "frin"));              // por defecto -> male, poseída
    NIMVLETS_CHECK(!CanActivate(model, "ghost"));            // no en catálogo
    return true;
}

bool TestCanActivateDefaultVariantResolves() {
    const PetCatalog catalog = MakeDevCatalog();
    const CollectionModel model =
        BuildCollectionModel(catalog, MigratedOwner(), PetIdentity{"bunny", ""});
    NIMVLETS_CHECK(CanActivate(model, "frin"));           // default male, poseída
    NIMVLETS_CHECK(CanActivate(model, "frin", "female"));
    return true;
}

// --- SeedEntitlementsFromCatalog: variantes EXPLÍCITAS ----------------

bool TestSeedGrantsExplicitVariants() {
    const PetCatalog catalog = MakeDevCatalog();
    const Ents seed = SeedEntitlementsFromCatalog(catalog);
    // La autorización EXPLÍCITA de cada entrada initiallyOwned: bunny sin
    // variante + las DOS de Frin. NUNCA un {frin, ""}.
    NIMVLETS_CHECK((seed == Ents{NoVar("bunny"), Var("frin", "female"), Var("frin", "male")}));
    for (const auto& e : seed) {
        NIMVLETS_CHECK(!(e.petId == "frin" && e.variantId.empty()));
    }
    return true;
}

// --- ExpandHistoricalWholePetEntitlements ---------------------------

// Un {frin, ""} legacy se expande a las variantes del catálogo; un
// {bunny, ""} (pet sin variantes) se deja igual.
bool TestExpandFrinWholePetToBothVariants() {
    const PetCatalog catalog = MakeDevCatalog();
    Ents ents = {NoVar("bunny"), NoVar("frin")};
    const bool changed = ExpandHistoricalWholePetEntitlements(ents, catalog);
    NIMVLETS_CHECK(changed);
    NIMVLETS_CHECK((ents == Ents{NoVar("bunny"), Var("frin", "female"), Var("frin", "male")}));
    // Y no quedó ningún {frin, ""}.
    for (const auto& e : ents) {
        NIMVLETS_CHECK(!(e.petId == "frin" && e.variantId.empty()));
    }
    return true;
}

bool TestExpandIsIdempotentAndLeavesExplicitAlone() {
    const PetCatalog catalog = MakeDevCatalog();
    Ents ents = MigratedOwner();  // ya explícito
    NIMVLETS_CHECK(!ExpandHistoricalWholePetEntitlements(ents, catalog));
    NIMVLETS_CHECK((ents == MigratedOwner()));

    // Un {bunny, ""} solo (pet sin variantes) tampoco cambia.
    Ents onlyBunny = {NoVar("bunny")};
    NIMVLETS_CHECK(!ExpandHistoricalWholePetEntitlements(onlyBunny, catalog));
    NIMVLETS_CHECK((onlyBunny == Ents{NoVar("bunny")}));
    return true;
}

// Migrar el Frin legacy NO autoriza una tercera variante hipotética.
bool TestMigratedFrinDoesNotCoverHypotheticalThirdVariant() {
    const PetCatalog catalog = MakeDevCatalog();  // solo male/female
    Ents ents = {NoVar("frin")};
    ExpandHistoricalWholePetEntitlements(ents, catalog);
    NIMVLETS_CHECK((ents == FrinBoth()));
    NIMVLETS_CHECK(!nimvlets::catalog::OwnsIdentity(ents, PetIdentity{"frin", "spirit"}));
    NIMVLETS_CHECK(!nimvlets::catalog::OwnsIdentity(ents, PetIdentity{"frin", ""}));
    return true;
}

// Un owner migrado (Frin expandido a las dos variantes) puede activar
// Macho y Hembra.
bool TestMigratedFrinOwnerActivatesBothVariants() {
    const PetCatalog catalog = MakeDevCatalog();
    Ents owned = {NoVar("bunny"), NoVar("frin")};
    ExpandHistoricalWholePetEntitlements(owned, catalog);
    const CollectionModel model = BuildCollectionModel(catalog, owned, PetIdentity{"frin", "male"});
    NIMVLETS_CHECK(CanActivate(model, "frin", "male"));
    NIMVLETS_CHECK(CanActivate(model, "frin", "female"));
    NIMVLETS_CHECK(model.Find("frin")->AllVariantsOwned());
    return true;
}

// --- ResolveOwnedActiveIdentity: NUNCA otorga ---------------------

// Identidad activa autorizada -> se devuelve sin cambios.
bool TestResolveOwnedActiveKeepsOwnedWanted() {
    const PetCatalog catalog = MakeDevCatalog();
    bool fellBack = true;
    const PetIdentity r =
        ResolveOwnedActiveIdentity(MigratedOwner(), catalog, PetIdentity{"frin", "female"}, fellBack);
    NIMVLETS_CHECK(!fellBack);
    NIMVLETS_CHECK((r == PetIdentity{"frin", "female"}));
    return true;
}

// Identidad activa NO autorizada (estado corrupto: active=nidir, owned
// solo bunny) -> cae al default poseído (bunny). NO se otorga nidir.
bool TestResolveOwnedActiveFallsBackWithoutGranting() {
    const PetCatalog catalog = MakeDevCatalog();
    const Ents owned = {NoVar("bunny")};
    bool fellBack = false;
    const PetIdentity r =
        ResolveOwnedActiveIdentity(owned, catalog, PetIdentity{"nidir", ""}, fellBack);
    NIMVLETS_CHECK(fellBack);
    NIMVLETS_CHECK((r == PetIdentity{"bunny", ""}));
    // `owned` es const: la función no puede haber otorgado nada. Y nidir
    // sigue sin estar autorizado.
    NIMVLETS_CHECK(!nimvlets::catalog::OwnsIdentity(owned, PetIdentity{"nidir", ""}));
    return true;
}

// Si el default tampoco está autorizado, cae a la primera entrada del
// catálogo que sí lo esté.
bool TestResolveOwnedActivePicksFirstOwnedWhenDefaultNotOwned() {
    const PetCatalog catalog = MakeDevCatalog();  // default = bunny
    const Ents owned = {Var("frin", "male"), Var("frin", "female")};  // bunny NO poseído
    bool fellBack = false;
    const PetIdentity r =
        ResolveOwnedActiveIdentity(owned, catalog, PetIdentity{"nidir", ""}, fellBack);
    NIMVLETS_CHECK(fellBack);
    NIMVLETS_CHECK((r == PetIdentity{"frin", "male"}));  // primera entrada poseída en orden de catálogo
    return true;
}

// Estado degenerado (nada autorizado) -> devuelve `wanted` sin marcar
// fallback (src/app siembra en ese caso).
bool TestResolveOwnedActiveDegenerateNothingOwned() {
    const PetCatalog catalog = MakeDevCatalog();
    bool fellBack = true;
    const PetIdentity r = ResolveOwnedActiveIdentity({}, catalog, PetIdentity{"nidir", ""}, fellBack);
    NIMVLETS_CHECK(!fellBack);
    NIMVLETS_CHECK((r == PetIdentity{"nidir", ""}));
    return true;
}

}  // namespace

void RegisterCollectionModelTests(testing::TestRunner& runner) {
    runner.Add("CollectionModel/ThreeOwnershipStatesDeriveCorrectly", TestThreeOwnershipStatesDeriveCorrectly);
    runner.Add("CollectionModel/FrinCollapsesToOneItemWithTwoVariants", TestFrinCollapsesToOneItemWithTwoVariants);
    runner.Add("CollectionModel/FrinOneVariantOwned", TestFrinOneVariantOwned);
    runner.Add("CollectionModel/FrinNoVariantsOwned", TestFrinNoVariantsOwned);
    runner.Add("CollectionModel/FrinBothVariantsOwned", TestFrinBothVariantsOwned);
    runner.Add("CollectionModel/SelectedVariantFollowsActiveIdentity", TestSelectedVariantFollowsActiveIdentity);
    runner.Add("CollectionModel/UnknownActiveVariantFallsBackToFirst", TestUnknownActiveVariantFallsBackToFirst);
    runner.Add("CollectionModel/CanActivateGatesLockedAndUnownedVariant", TestCanActivateGatesLockedAndUnownedVariant);
    runner.Add("CollectionModel/CanActivateDefaultVariantResolves", TestCanActivateDefaultVariantResolves);
    runner.Add("CollectionModel/SeedGrantsExplicitVariants", TestSeedGrantsExplicitVariants);
    runner.Add("CollectionModel/ExpandFrinWholePetToBothVariants", TestExpandFrinWholePetToBothVariants);
    runner.Add("CollectionModel/ExpandIsIdempotentAndLeavesExplicitAlone", TestExpandIsIdempotentAndLeavesExplicitAlone);
    runner.Add("CollectionModel/MigratedFrinDoesNotCoverHypotheticalThirdVariant",
               TestMigratedFrinDoesNotCoverHypotheticalThirdVariant);
    runner.Add("CollectionModel/MigratedFrinOwnerActivatesBothVariants", TestMigratedFrinOwnerActivatesBothVariants);
    runner.Add("CollectionModel/ResolveOwnedActiveKeepsOwnedWanted", TestResolveOwnedActiveKeepsOwnedWanted);
    runner.Add("CollectionModel/ResolveOwnedActiveFallsBackWithoutGranting", TestResolveOwnedActiveFallsBackWithoutGranting);
    runner.Add("CollectionModel/ResolveOwnedActivePicksFirstOwnedWhenDefaultNotOwned",
               TestResolveOwnedActivePicksFirstOwnedWhenDefaultNotOwned);
    runner.Add("CollectionModel/ResolveOwnedActiveDegenerateNothingOwned", TestResolveOwnedActiveDegenerateNothingOwned);
}

}  // namespace nimvlets::tests
