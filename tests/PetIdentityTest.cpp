#include "PetIdentityTest.h"

#include "catalog/PetIdentity.h"

#include <algorithm>
#include <vector>

using nimvlets::catalog::PetIdentity;
using nimvlets::catalog::PetIdentityHash;

namespace nimvlets::tests {

namespace {

bool TestEqualIdentitiesCompareEqual() {
    const PetIdentity a{"bunny_dev", ""};
    const PetIdentity b{"bunny_dev", ""};
    NIMVLETS_CHECK(a == b);
    NIMVLETS_CHECK(!(a != b));
    return true;
}

bool TestDifferentPetIdsCompareUnequal() {
    const PetIdentity a{"bunny_dev", ""};
    const PetIdentity b{"frin", ""};
    NIMVLETS_CHECK(a != b);
    NIMVLETS_CHECK(!(a == b));
    return true;
}

// El caso central del concepto de variante (ver docs/PET_CONTENT_SPEC.md
// y docs/CATALOG.md): mismo petId, distinto variantId, son identidades
// DISTINTAS -- así es como male/female de un mismo Frin conviven como
// dos entradas separadas del catálogo.
bool TestSamePetIdDifferentVariantCompareUnequal() {
    const PetIdentity male{"frin", "male"};
    const PetIdentity female{"frin", "female"};
    NIMVLETS_CHECK(male != female);
    return true;
}

bool TestEmptyVariantIdMeansNoVariant() {
    const PetIdentity noVariant{"bunny_dev", ""};
    const PetIdentity alsoNoVariant{"bunny_dev", ""};
    NIMVLETS_CHECK(noVariant == alsoNoVariant);
    return true;
}

// Orden solo necesita ser estable y total (para contenedores
// ordenados) -- no tiene significado de producto. Confirma que
// std::sort con operator< no crashea ni produce un orden inconsistente
// para un conjunto pequeño con petId y variantId repetidos.
bool TestOrderingIsStableAndTotal() {
    std::vector<PetIdentity> identities = {
        {"frin", "male"},
        {"bunny_dev", ""},
        {"frin", "female"},
        {"artu", ""},
    };
    std::sort(identities.begin(), identities.end());

    // Orden lexicográfico por petId, luego por variantId: artu, bunny_dev, frin/female, frin/male.
    NIMVLETS_CHECK(identities[0].petId == "artu");
    NIMVLETS_CHECK(identities[1].petId == "bunny_dev");
    NIMVLETS_CHECK(identities[2].petId == "frin" && identities[2].variantId == "female");
    NIMVLETS_CHECK(identities[3].petId == "frin" && identities[3].variantId == "male");
    return true;
}

bool TestHashIsConsistentWithEquality() {
    const PetIdentity a{"frin", "male"};
    const PetIdentity b{"frin", "male"};
    const PetIdentityHash hasher;
    NIMVLETS_CHECK(a == b);
    NIMVLETS_CHECK(hasher(a) == hasher(b));
    return true;
}

}  // namespace

void RegisterPetIdentityTests(testing::TestRunner& runner) {
    runner.Add("PetIdentity/EqualIdentitiesCompareEqual", TestEqualIdentitiesCompareEqual);
    runner.Add("PetIdentity/DifferentPetIdsCompareUnequal", TestDifferentPetIdsCompareUnequal);
    runner.Add("PetIdentity/SamePetIdDifferentVariantCompareUnequal", TestSamePetIdDifferentVariantCompareUnequal);
    runner.Add("PetIdentity/EmptyVariantIdMeansNoVariant", TestEmptyVariantIdMeansNoVariant);
    runner.Add("PetIdentity/OrderingIsStableAndTotal", TestOrderingIsStableAndTotal);
    runner.Add("PetIdentity/HashIsConsistentWithEquality", TestHashIsConsistentWithEquality);
}

}  // namespace nimvlets::tests
