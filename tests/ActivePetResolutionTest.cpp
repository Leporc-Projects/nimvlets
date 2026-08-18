#include "ActivePetResolutionTest.h"

#include "catalog/ActivePetResolution.h"

#include <vector>

using nimvlets::catalog::CatalogEntry;
using nimvlets::catalog::PetCatalog;
using nimvlets::catalog::PetIdentity;
using nimvlets::catalog::ResolveActiveSelection;
using nimvlets::catalog::ResolvedSelection;

namespace nimvlets::tests {

namespace {

// Catálogo de tres entradas construido directamente (sin pasar por el
// formato binario -- eso ya lo cubre PetCatalogLoaderTest.cpp): un
// default sin variante, y dos variantes de un mismo petId (el caso
// Frin male/female).
PetCatalog MakeTestCatalog() {
    std::vector<CatalogEntry> entries;

    CatalogEntry bunny;
    bunny.identity = PetIdentity{"bunny_dev", ""};
    bunny.displayName = "Bunny (dev fixture)";
    bunny.packPath = "bunny.nvpack";
    bunny.isDefault = true;
    entries.push_back(bunny);

    CatalogEntry frinMale;
    frinMale.identity = PetIdentity{"frin", "male"};
    frinMale.displayName = "Frin (male)";
    frinMale.packPath = "frin_male.nvpack";
    frinMale.isDefault = false;
    entries.push_back(frinMale);

    CatalogEntry frinFemale;
    frinFemale.identity = PetIdentity{"frin", "female"};
    frinFemale.displayName = "Frin (female)";
    frinFemale.packPath = "frin_female.nvpack";
    frinFemale.isDefault = false;
    entries.push_back(frinFemale);

    return PetCatalog(std::move(entries));
}

bool TestPersistedValidSelectionResolves() {
    const PetCatalog catalog = MakeTestCatalog();
    const ResolvedSelection resolved = ResolveActiveSelection(catalog, PetIdentity{"bunny_dev", ""});
    NIMVLETS_CHECK(resolved.entry != nullptr);
    NIMVLETS_CHECK(resolved.entry->identity.petId == "bunny_dev");
    NIMVLETS_CHECK(!resolved.usedFallback);
    return true;
}

bool TestPersistedUnknownSelectionFallsBackToDefault() {
    const PetCatalog catalog = MakeTestCatalog();
    const ResolvedSelection resolved = ResolveActiveSelection(catalog, PetIdentity{"does_not_exist", ""});
    NIMVLETS_CHECK(resolved.entry != nullptr);
    NIMVLETS_CHECK(resolved.entry == &catalog.Default());
    NIMVLETS_CHECK(resolved.entry->identity.petId == "bunny_dev");
    NIMVLETS_CHECK(resolved.usedFallback);
    return true;
}

// Sin save aún (primera ejecución): activePetId/activeVariantId
// persistidos están vacíos -- debe resolver al default exactamente
// igual que una identidad desconocida, no crashear ni tratarlo como un
// caso especial.
bool TestEmptyPersistedIdentityFallsBackToDefault() {
    const PetCatalog catalog = MakeTestCatalog();
    const ResolvedSelection resolved = ResolveActiveSelection(catalog, PetIdentity{"", ""});
    NIMVLETS_CHECK(resolved.usedFallback);
    NIMVLETS_CHECK(resolved.entry == &catalog.Default());
    return true;
}

// El caso central de variante: "frin"/"male" y "frin"/"female" deben
// resolver a entradas DISTINTAS, ninguna de las dos cae al default.
bool TestVariantResolutionPicksTheExactVariant() {
    const PetCatalog catalog = MakeTestCatalog();

    const ResolvedSelection male = ResolveActiveSelection(catalog, PetIdentity{"frin", "male"});
    NIMVLETS_CHECK(!male.usedFallback);
    NIMVLETS_CHECK(male.entry->identity.variantId == "male");

    const ResolvedSelection female = ResolveActiveSelection(catalog, PetIdentity{"frin", "female"});
    NIMVLETS_CHECK(!female.usedFallback);
    NIMVLETS_CHECK(female.entry->identity.variantId == "female");

    NIMVLETS_CHECK(male.entry != female.entry);
    return true;
}

// petId correcto pero variantId incorrecto/desconocido no debe
// "casi calzar" con ninguna variante de ese petId -- debe caer al
// default, igual que un petId totalmente desconocido.
bool TestKnownPetIdWithUnknownVariantFallsBackToDefault() {
    const PetCatalog catalog = MakeTestCatalog();
    const ResolvedSelection resolved = ResolveActiveSelection(catalog, PetIdentity{"frin", "nonexistent_variant"});
    NIMVLETS_CHECK(resolved.usedFallback);
    NIMVLETS_CHECK(resolved.entry == &catalog.Default());
    return true;
}

}  // namespace

void RegisterActivePetResolutionTests(testing::TestRunner& runner) {
    runner.Add("ActivePetResolution/PersistedValidSelectionResolves", TestPersistedValidSelectionResolves);
    runner.Add("ActivePetResolution/PersistedUnknownSelectionFallsBackToDefault", TestPersistedUnknownSelectionFallsBackToDefault);
    runner.Add("ActivePetResolution/EmptyPersistedIdentityFallsBackToDefault", TestEmptyPersistedIdentityFallsBackToDefault);
    runner.Add("ActivePetResolution/VariantResolutionPicksTheExactVariant", TestVariantResolutionPicksTheExactVariant);
    runner.Add("ActivePetResolution/KnownPetIdWithUnknownVariantFallsBackToDefault", TestKnownPetIdWithUnknownVariantFallsBackToDefault);
}

}  // namespace nimvlets::tests
