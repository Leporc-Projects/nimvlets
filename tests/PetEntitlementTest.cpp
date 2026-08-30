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

// `NoVar(p)` = la autorización de un Nimvlet SIN variantes ({p, ""}).
// `Var(p,v)` = la autorización de una variante concreta.
PetEntitlement NoVar(const std::string& p) { return PetEntitlement{p, ""}; }
PetEntitlement Var(const std::string& p, const std::string& v) { return PetEntitlement{p, v}; }

// Covers() es coincidencia EXACTA en los dos campos (DEC-128): no hay
// "pet entero" que cubra variantes.
bool TestCoversIsExactMatch() {
    // {frin, ""} NO cubre ninguna variante concreta de Frin.
    NIMVLETS_CHECK(!NoVar("frin").Covers(PetIdentity{"frin", "male"}));
    NIMVLETS_CHECK(!NoVar("frin").Covers(PetIdentity{"frin", "female"}));
    NIMVLETS_CHECK(NoVar("frin").Covers(PetIdentity{"frin", ""}));  // exacta

    // Una autorización de variante cubre SOLO esa variante.
    NIMVLETS_CHECK(Var("frin", "male").Covers(PetIdentity{"frin", "male"}));
    NIMVLETS_CHECK(!Var("frin", "male").Covers(PetIdentity{"frin", "female"}));
    NIMVLETS_CHECK(!Var("frin", "male").Covers(PetIdentity{"frin", ""}));
    // ...ni una tercera variante hipotética futura.
    NIMVLETS_CHECK(!Var("frin", "male").Covers(PetIdentity{"frin", "spirit"}));

    // Nimvlet sin variantes: identidad con variantId "".
    NIMVLETS_CHECK(NoVar("bunny").Covers(PetIdentity{"bunny", ""}));
    NIMVLETS_CHECK(!NoVar("bunny").Covers(PetIdentity{"nidir", ""}));
    return true;
}

// Canonicalización: descarta petId vacío, ordena, dedup. SIN subsunción
// — {frin,""} y {frin,male} conviven (no hay noción de "pet entero" que
// absorba).
bool TestCanonicalizeSortsAndDedups() {
    Ents ents = {Var("frin", "female"), NoVar("bunny"), NoVar("bunny"), PetEntitlement{"", "x"},
                 Var("frin", "male"), NoVar("frin")};
    CanonicalizePetEntitlements(ents);
    // "" descartado; {bunny,""} dedup; orden (bunny < frin, y dentro de
    // frin: "" < "female" < "male"). NADA se subsume.
    NIMVLETS_CHECK((ents == Ents{NoVar("bunny"), NoVar("frin"), Var("frin", "female"), Var("frin", "male")}));

    Ents again = ents;
    CanonicalizePetEntitlements(again);
    NIMVLETS_CHECK(again == ents);  // idempotente
    return true;
}

// Las dos variantes de Frin conviven sin que ninguna absorba a la otra.
bool TestCanonicalizeKeepsDistinctVariants() {
    Ents ents = {Var("frin", "female"), Var("frin", "male"), NoVar("bunny")};
    CanonicalizePetEntitlements(ents);
    NIMVLETS_CHECK((ents == Ents{NoVar("bunny"), Var("frin", "female"), Var("frin", "male")}));
    return true;
}

bool TestOwnsIdentityIsExact() {
    const Ents ents = {NoVar("bunny"), Var("frin", "male")};
    NIMVLETS_CHECK(OwnsIdentity(ents, PetIdentity{"bunny", ""}));
    NIMVLETS_CHECK(OwnsIdentity(ents, PetIdentity{"frin", "male"}));
    NIMVLETS_CHECK(!OwnsIdentity(ents, PetIdentity{"frin", "female"}));  // NO poseída
    NIMVLETS_CHECK(!OwnsIdentity(ents, PetIdentity{"frin", ""}));        // exacta: no la tiene
    NIMVLETS_CHECK(!OwnsIdentity(ents, PetIdentity{"nidir", ""}));

    // Un {frin, ""} suelto NO autoriza ninguna variante concreta.
    const Ents bareFrin = {NoVar("frin")};
    NIMVLETS_CHECK(!OwnsIdentity(bareFrin, PetIdentity{"frin", "male"}));
    NIMVLETS_CHECK(!OwnsIdentity(bareFrin, PetIdentity{"frin", "female"}));
    return true;
}

bool TestOwnsAnyVariantOfPet() {
    const Ents ents = {NoVar("bunny"), Var("frin", "male")};
    NIMVLETS_CHECK(OwnsAnyVariantOfPet(ents, "bunny"));
    NIMVLETS_CHECK(OwnsAnyVariantOfPet(ents, "frin"));  // al menos una variante
    NIMVLETS_CHECK(!OwnsAnyVariantOfPet(ents, "nidir"));
    return true;
}

bool TestOrderIsStable() {
    NIMVLETS_CHECK(NoVar("frin") < Var("frin", "male"));
    NIMVLETS_CHECK(Var("frin", "female") < Var("frin", "male"));  // "female" < "male"
    NIMVLETS_CHECK(NoVar("bunny") < NoVar("frin"));
    return true;
}

}  // namespace

void RegisterPetEntitlementTests(testing::TestRunner& runner) {
    runner.Add("PetEntitlement/CoversIsExactMatch", TestCoversIsExactMatch);
    runner.Add("PetEntitlement/CanonicalizeSortsAndDedups", TestCanonicalizeSortsAndDedups);
    runner.Add("PetEntitlement/CanonicalizeKeepsDistinctVariants", TestCanonicalizeKeepsDistinctVariants);
    runner.Add("PetEntitlement/OwnsIdentityIsExact", TestOwnsIdentityIsExact);
    runner.Add("PetEntitlement/OwnsAnyVariantOfPet", TestOwnsAnyVariantOfPet);
    runner.Add("PetEntitlement/OrderIsStable", TestOrderIsStable);
}

}  // namespace nimvlets::tests
