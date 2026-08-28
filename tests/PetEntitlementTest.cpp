#include "PetEntitlementTest.h"

#include "catalog/PetEntitlement.h"

#include <string>
#include <vector>

using nimvlets::catalog::CanonicalizePetEntitlements;
using nimvlets::catalog::OwnsAnyVariantOfPet;
using nimvlets::catalog::OwnsIdentity;
using nimvlets::catalog::PetEntitlement;
using nimvlets::catalog::PetIdentity;

using Ents = std::vector<PetEntitlement>;

namespace nimvlets::tests {

namespace {

PetEntitlement Whole(const std::string& p) { return PetEntitlement{p, ""}; }
PetEntitlement Var(const std::string& p, const std::string& v) { return PetEntitlement{p, v}; }

// Covers(): un "pet entero" cubre cualquier variante; una autorización
// de variante concreta, solo esa.
bool TestCoversSemantics() {
    NIMVLETS_CHECK(Whole("frin").Covers(PetIdentity{"frin", "male"}));
    NIMVLETS_CHECK(Whole("frin").Covers(PetIdentity{"frin", "female"}));
    NIMVLETS_CHECK(Whole("frin").Covers(PetIdentity{"frin", ""}));
    NIMVLETS_CHECK(!Whole("frin").Covers(PetIdentity{"bunny", ""}));

    NIMVLETS_CHECK(Var("frin", "male").Covers(PetIdentity{"frin", "male"}));
    NIMVLETS_CHECK(!Var("frin", "male").Covers(PetIdentity{"frin", "female"}));
    NIMVLETS_CHECK(!Var("frin", "male").Covers(PetIdentity{"frin", ""}));

    // Pet sin variantes: identidad con variantId "".
    NIMVLETS_CHECK(Whole("bunny").Covers(PetIdentity{"bunny", ""}));
    return true;
}

// Canonicalización: descarta petId vacío, ordena, dedup, y SUBSUNCIÓN
// ((p,"") descarta (p,<var>)).
bool TestCanonicalizeSortsDedupsAndSubsumes() {
    Ents ents = {Var("frin", "female"), Whole("bunny"), Whole("bunny"), PetEntitlement{"", "x"},
                 Var("frin", "male"), Whole("frin")};
    CanonicalizePetEntitlements(ents);
    // {frin,""} subsume {frin,male}/{frin,female}; {bunny,""} dedup; ""
    // descartado; orden (bunny antes de frin).
    NIMVLETS_CHECK((ents == Ents{Whole("bunny"), Whole("frin")}));

    // Idempotente.
    Ents again = ents;
    CanonicalizePetEntitlements(again);
    NIMVLETS_CHECK(again == ents);
    return true;
}

// Sin un "pet entero" para ese petId, las variantes concretas conviven.
bool TestCanonicalizeKeepsDistinctVariants() {
    Ents ents = {Var("frin", "female"), Var("frin", "male"), Whole("bunny")};
    CanonicalizePetEntitlements(ents);
    NIMVLETS_CHECK((ents == Ents{Whole("bunny"), Var("frin", "female"), Var("frin", "male")}));
    return true;
}

bool TestOwnsIdentity() {
    const Ents ents = {Whole("bunny"), Var("frin", "male")};
    NIMVLETS_CHECK(OwnsIdentity(ents, PetIdentity{"bunny", ""}));
    NIMVLETS_CHECK(OwnsIdentity(ents, PetIdentity{"frin", "male"}));
    NIMVLETS_CHECK(!OwnsIdentity(ents, PetIdentity{"frin", "female"}));
    NIMVLETS_CHECK(!OwnsIdentity(ents, PetIdentity{"nidir", ""}));
    return true;
}

bool TestOwnsAnyVariantOfPet() {
    const Ents ents = {Whole("bunny"), Var("frin", "male")};
    NIMVLETS_CHECK(OwnsAnyVariantOfPet(ents, "bunny"));
    NIMVLETS_CHECK(OwnsAnyVariantOfPet(ents, "frin"));
    NIMVLETS_CHECK(!OwnsAnyVariantOfPet(ents, "nidir"));
    return true;
}

bool TestOrderPutsWholePetBeforeVariants() {
    NIMVLETS_CHECK(Whole("frin") < Var("frin", "male"));
    NIMVLETS_CHECK(Var("frin", "female") < Var("frin", "male"));  // "female" < "male"
    NIMVLETS_CHECK(Whole("bunny") < Whole("frin"));
    return true;
}

}  // namespace

void RegisterPetEntitlementTests(testing::TestRunner& runner) {
    runner.Add("PetEntitlement/CoversSemantics", TestCoversSemantics);
    runner.Add("PetEntitlement/CanonicalizeSortsDedupsAndSubsumes", TestCanonicalizeSortsDedupsAndSubsumes);
    runner.Add("PetEntitlement/CanonicalizeKeepsDistinctVariants", TestCanonicalizeKeepsDistinctVariants);
    runner.Add("PetEntitlement/OwnsIdentity", TestOwnsIdentity);
    runner.Add("PetEntitlement/OwnsAnyVariantOfPet", TestOwnsAnyVariantOfPet);
    runner.Add("PetEntitlement/OrderPutsWholePetBeforeVariants", TestOrderPutsWholePetBeforeVariants);
}

}  // namespace nimvlets::tests
