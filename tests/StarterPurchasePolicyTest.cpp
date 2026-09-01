#include "StarterPurchasePolicyTest.h"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "catalog/StarterPurchasePolicy.h"
#include "catalog/StarterShopModel.h"

using nimvlets::catalog::BuildStarterShopModel;
using nimvlets::catalog::CatalogEntry;
using nimvlets::catalog::EvaluateStarterPurchase;
using nimvlets::catalog::PetCatalog;
using nimvlets::catalog::PetEntitlement;
using nimvlets::catalog::PetIdentity;
using nimvlets::catalog::StarterPurchaseResult;
using nimvlets::catalog::StarterRole;

using Ents = std::vector<PetEntitlement>;

namespace nimvlets::tests {

namespace {

PetEntitlement NoVar(const std::string& p) { return PetEntitlement{p, ""}; }
PetEntitlement Var(const std::string& p, const std::string& v) { return PetEntitlement{p, v}; }
PetIdentity Id(const std::string& p, const std::string& v = "") { return PetIdentity{p, v}; }

CatalogEntry MakeEntry(
    const std::string& petId, const std::string& variantId, StarterRole role,
    std::uint64_t priceClicks, bool publiclyPurchasable = false, bool isDefault = false) {
    CatalogEntry e;
    e.identity = PetIdentity{petId, variantId};
    e.displayName = petId == "frin" ? "Frin" : petId;
    e.packPath = petId + variantId + ".nvpack";
    e.isDefault = isDefault;
    e.priceClicks = priceClicks;
    e.publiclyPurchasable = publiclyPurchasable;
    e.starterRole = role;
    return e;
}

PetCatalog MakeStarterCatalog() {
    std::vector<CatalogEntry> e;
    e.push_back(MakeEntry("frin", "male", StarterRole::kSecret, 150, false, true));
    e.push_back(MakeEntry("frin", "female", StarterRole::kSecret, 150));
    e.push_back(MakeEntry("artu_dev", "", StarterRole::kNormal, 80));
    e.push_back(MakeEntry("rato_dev", "", StarterRole::kNormal, 100));
    // Un starter NORMAL sin precio: no configurado para el Starter Shop.
    e.push_back(MakeEntry("rinrin_dev", "", StarterRole::kNormal, 0));
    // Un pet no-starter público (Shop normal) — nunca comprable acá.
    e.push_back(MakeEntry("nidir", "", StarterRole::kNone, 300, true));
    return PetCatalog(std::move(e));
}

bool TestValidMissingSecretVariantSucceeds() {
    const PetCatalog cat = MakeStarterCatalog();
    const auto o = EvaluateStarterPurchase(cat, true, Id("frin", "male"), 500, {Var("frin", "female")});
    NIMVLETS_CHECK(o.result == StarterPurchaseResult::kSuccess);
    NIMVLETS_CHECK(o.price == 150);
    NIMVLETS_CHECK(o.newBalance == 350);
    NIMVLETS_CHECK((o.grantedEntitlement == Var("frin", "male")));
    // Ambas variantes ahora, EXACTAS — nunca {frin, ""}.
    NIMVLETS_CHECK((o.newEntitlements == Ents{Var("frin", "female"), Var("frin", "male")}));
    for (const auto& en : o.newEntitlements) {
        NIMVLETS_CHECK(!(en.petId == "frin" && en.variantId.empty()));
    }
    return true;
}

bool TestValidNormalStarterPurchaseSucceeds() {
    const PetCatalog cat = MakeStarterCatalog();
    const auto o = EvaluateStarterPurchase(cat, true, Id("artu_dev"), 200, {Var("frin", "male")});
    NIMVLETS_CHECK(o.result == StarterPurchaseResult::kSuccess);
    NIMVLETS_CHECK(o.price == 80);
    NIMVLETS_CHECK(o.newBalance == 120);
    NIMVLETS_CHECK((o.grantedEntitlement == NoVar("artu_dev")));
    NIMVLETS_CHECK((o.newEntitlements == Ents{NoVar("artu_dev"), Var("frin", "male")}));
    return true;
}

bool TestSecretWithoutSiblingOwnershipRejected() {
    const PetCatalog cat = MakeStarterCatalog();
    // El comprador NO posee ninguna variante de frin.
    const auto o = EvaluateStarterPurchase(cat, true, Id("frin", "male"), 100000, {NoVar("artu_dev")});
    NIMVLETS_CHECK(o.result == StarterPurchaseResult::kNotEligible);
    NIMVLETS_CHECK(o.newBalance == 100000);
    NIMVLETS_CHECK((o.newEntitlements == Ents{NoVar("artu_dev")}));
    return true;
}

bool TestLifecycleNotCompletedRejected() {
    const PetCatalog cat = MakeStarterCatalog();
    const auto o = EvaluateStarterPurchase(cat, false, Id("frin", "male"), 500, {Var("frin", "female")});
    NIMVLETS_CHECK(o.result == StarterPurchaseResult::kLifecycleNotCompleted);
    NIMVLETS_CHECK(o.newBalance == 500);
    NIMVLETS_CHECK((o.newEntitlements == Ents{Var("frin", "female")}));
    return true;
}

bool TestNotStarterRejected() {
    const PetCatalog cat = MakeStarterCatalog();
    const auto o = EvaluateStarterPurchase(cat, true, Id("nidir"), 500, {Var("frin", "female")});
    NIMVLETS_CHECK(o.result == StarterPurchaseResult::kNotEligible);
    NIMVLETS_CHECK(o.newBalance == 500);
    return true;
}

bool TestZeroPriceUnconfiguredStarterRejected() {
    const PetCatalog cat = MakeStarterCatalog();
    const auto o = EvaluateStarterPurchase(cat, true, Id("rinrin_dev"), 500, {NoVar("artu_dev")});
    NIMVLETS_CHECK(o.result == StarterPurchaseResult::kNotEligible);
    NIMVLETS_CHECK(o.newBalance == 500);
    return true;
}

bool TestAlreadyOwnedRejected() {
    const PetCatalog cat = MakeStarterCatalog();
    const auto o = EvaluateStarterPurchase(
        cat, true, Id("frin", "female"), 5000, {Var("frin", "female"), Var("frin", "male")});
    NIMVLETS_CHECK(o.result == StarterPurchaseResult::kAlreadyOwned);
    NIMVLETS_CHECK(o.newBalance == 5000);
    return true;
}

bool TestInsufficientBalanceRejected() {
    const PetCatalog cat = MakeStarterCatalog();
    const auto o = EvaluateStarterPurchase(cat, true, Id("frin", "male"), 149, {Var("frin", "female")});
    NIMVLETS_CHECK(o.result == StarterPurchaseResult::kInsufficientBalance);
    NIMVLETS_CHECK(o.price == 150);
    NIMVLETS_CHECK(o.newBalance == 149);              // sin débito
    NIMVLETS_CHECK((o.newEntitlements == Ents{Var("frin", "female")}));  // sin mutación
    return true;
}

bool TestInvalidIdentityRejected() {
    const PetCatalog cat = MakeStarterCatalog();
    NIMVLETS_CHECK(EvaluateStarterPurchase(cat, true, Id("ghost"), 9999, {Var("frin", "female")}).result ==
                   StarterPurchaseResult::kInvalidTarget);
    NIMVLETS_CHECK(EvaluateStarterPurchase(cat, true, Id("frin", ""), 9999, {Var("frin", "female")}).result ==
                   StarterPurchaseResult::kInvalidTarget);
    NIMVLETS_CHECK(EvaluateStarterPurchase(cat, true, Id("", ""), 9999, {Var("frin", "female")}).result ==
                   StarterPurchaseResult::kInvalidTarget);
    return true;
}

bool TestFailureIsZeroMutation() {
    const PetCatalog cat = MakeStarterCatalog();
    // Ya en forma canónica (ordenada) — la política canonicaliza
    // out.newEntitlements en todo camino de fallo.
    const Ents start = {NoVar("artu_dev"), Var("frin", "female")};
    for (const PetIdentity& t : {Id("frin", "male"),  // insuficiente (bajo balance)
                                 Id("nidir"),          // no starter
                                 Id("rinrin_dev"),     // precio 0
                                 Id("ghost"),          // inválido
                                 Id("frin", "")}) {    // inválido
        const auto o = EvaluateStarterPurchase(cat, true, t, 10, start);
        NIMVLETS_CHECK(o.result != StarterPurchaseResult::kSuccess);
        NIMVLETS_CHECK(o.newBalance == 10);
        NIMVLETS_CHECK((o.newEntitlements == start));
    }
    // lifecycle no completado sobre el mismo estado.
    const auto locked = EvaluateStarterPurchase(cat, false, Id("frin", "male"), 10, start);
    NIMVLETS_CHECK(locked.newBalance == 10);
    NIMVLETS_CHECK((locked.newEntitlements == start));
    return true;
}

bool TestSuccessGrantsExactIdentityToZeroBalanceNoUnderflow() {
    const PetCatalog cat = MakeStarterCatalog();
    // Balance exacto -> 0, sin underflow.
    const auto exact = EvaluateStarterPurchase(cat, true, Id("frin", "male"), 150, {Var("frin", "female")});
    NIMVLETS_CHECK(exact.result == StarterPurchaseResult::kSuccess);
    NIMVLETS_CHECK(exact.newBalance == 0);

    // Balance enorme, sin overflow hacia abajo.
    const std::uint64_t huge = std::numeric_limits<std::uint64_t>::max();
    const auto big = EvaluateStarterPurchase(cat, true, Id("frin", "male"), huge, {Var("frin", "female")});
    NIMVLETS_CHECK(big.result == StarterPurchaseResult::kSuccess);
    NIMVLETS_CHECK(big.newBalance == huge - 150);
    return true;
}

bool TestCanonicalDedupPreservedAndIdempotentAfterFirst() {
    const PetCatalog cat = MakeStarterCatalog();
    // Entrada con duplicados y orden arbitrario.
    const Ents messy = {Var("frin", "female"), Var("frin", "female"), NoVar("artu_dev")};
    const auto first = EvaluateStarterPurchase(cat, true, Id("frin", "male"), 400, messy);
    NIMVLETS_CHECK(first.result == StarterPurchaseResult::kSuccess);
    NIMVLETS_CHECK((first.newEntitlements ==
                    Ents{NoVar("artu_dev"), Var("frin", "female"), Var("frin", "male")}));

    // Confirmar de nuevo tras aplicar -> ya poseído, sin doble débito.
    const auto second =
        EvaluateStarterPurchase(cat, true, Id("frin", "male"), first.newBalance, first.newEntitlements);
    NIMVLETS_CHECK(second.result == StarterPurchaseResult::kAlreadyOwned);
    NIMVLETS_CHECK(second.newBalance == first.newBalance);
    return true;
}

// El modelo y la política de compra tienen que estar de acuerdo: con
// balance suficiente, EXACTAMENTE las ofertas del modelo evalúan a
// kSuccess, y nada fuera del modelo lo hace.
bool TestModelAndPolicyAgree() {
    const PetCatalog cat = MakeStarterCatalog();
    const Ents owned = {Var("frin", "female")};
    const auto model = BuildStarterShopModel(cat, true, owned, 1000000);
    for (const CatalogEntry& e : cat.Entries()) {
        const bool inModel = model.Find(e.identity) != nullptr;
        const auto o = EvaluateStarterPurchase(cat, true, e.identity, 1000000, owned);
        NIMVLETS_CHECK((o.result == StarterPurchaseResult::kSuccess) == inModel);
    }
    return true;
}

}  // namespace

void RegisterStarterPurchasePolicyTests(testing::TestRunner& runner) {
    runner.Add("StarterPurchasePolicy/ValidMissingSecretVariantSucceeds", TestValidMissingSecretVariantSucceeds);
    runner.Add("StarterPurchasePolicy/ValidNormalStarterPurchaseSucceeds", TestValidNormalStarterPurchaseSucceeds);
    runner.Add("StarterPurchasePolicy/SecretWithoutSiblingOwnershipRejected",
               TestSecretWithoutSiblingOwnershipRejected);
    runner.Add("StarterPurchasePolicy/LifecycleNotCompletedRejected", TestLifecycleNotCompletedRejected);
    runner.Add("StarterPurchasePolicy/NotStarterRejected", TestNotStarterRejected);
    runner.Add("StarterPurchasePolicy/ZeroPriceUnconfiguredStarterRejected",
               TestZeroPriceUnconfiguredStarterRejected);
    runner.Add("StarterPurchasePolicy/AlreadyOwnedRejected", TestAlreadyOwnedRejected);
    runner.Add("StarterPurchasePolicy/InsufficientBalanceRejected", TestInsufficientBalanceRejected);
    runner.Add("StarterPurchasePolicy/InvalidIdentityRejected", TestInvalidIdentityRejected);
    runner.Add("StarterPurchasePolicy/FailureIsZeroMutation", TestFailureIsZeroMutation);
    runner.Add("StarterPurchasePolicy/SuccessGrantsExactIdentityToZeroBalanceNoUnderflow",
               TestSuccessGrantsExactIdentityToZeroBalanceNoUnderflow);
    runner.Add("StarterPurchasePolicy/CanonicalDedupPreservedAndIdempotentAfterFirst",
               TestCanonicalDedupPreservedAndIdempotentAfterFirst);
    runner.Add("StarterPurchasePolicy/ModelAndPolicyAgree", TestModelAndPolicyAgree);
}

}  // namespace nimvlets::tests
