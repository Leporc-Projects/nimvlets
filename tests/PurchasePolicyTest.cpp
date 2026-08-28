#include "PurchasePolicyTest.h"

#include "catalog/PurchasePolicy.h"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

using nimvlets::catalog::CatalogEntry;
using nimvlets::catalog::EvaluatePurchase;
using nimvlets::catalog::PetCatalog;
using nimvlets::catalog::PetEntitlement;
using nimvlets::catalog::PetIdentity;
using nimvlets::catalog::PurchaseOutcome;
using nimvlets::catalog::PurchaseResult;

using Ents = std::vector<PetEntitlement>;

namespace nimvlets::tests {

namespace {

// Catálogo estilo dev: Bunny (público, 120), Nidir (público, 300), Frin
// male/female (NO público, precio 0).
PetCatalog MakeShopCatalog() {
    std::vector<CatalogEntry> e;

    CatalogEntry bunny;
    bunny.identity = PetIdentity{"bunny", ""};
    bunny.displayName = "Bunny";
    bunny.packPath = "b.nvpack";
    bunny.isDefault = true;
    bunny.priceClicks = 120;
    bunny.publiclyPurchasable = true;
    e.push_back(bunny);

    CatalogEntry nidir;
    nidir.identity = PetIdentity{"nidir", ""};
    nidir.displayName = "Nidir";
    nidir.packPath = "n.nvpack";
    nidir.priceClicks = 300;
    nidir.publiclyPurchasable = true;
    e.push_back(nidir);

    CatalogEntry fm;
    fm.identity = PetIdentity{"frin", "male"};
    fm.displayName = "Frin";
    fm.packPath = "fm.nvpack";
    e.push_back(fm);

    CatalogEntry ff;
    ff.identity = PetIdentity{"frin", "female"};
    ff.displayName = "Frin";
    ff.packPath = "ff.nvpack";
    e.push_back(ff);

    return PetCatalog(std::move(e));
}

PetEntitlement Whole(const std::string& p) { return PetEntitlement{p, ""}; }

bool TestSuccessDebitsExactPriceAndGrantsOnce() {
    const PetCatalog cat = MakeShopCatalog();
    const PurchaseOutcome o = EvaluatePurchase(cat, "nidir", 500, {Whole("bunny")});
    NIMVLETS_CHECK(o.result == PurchaseResult::kSuccess);
    NIMVLETS_CHECK(o.price == 300);
    NIMVLETS_CHECK(o.newBalance == 200);  // 500 - 300 exacto
    NIMVLETS_CHECK((o.newEntitlements == Ents{Whole("bunny"), Whole("nidir")}));
    NIMVLETS_CHECK((o.grantedEntitlement == Whole("nidir")));
    return true;
}

bool TestExactPriceBalanceSucceedsToZero() {
    const PetCatalog cat = MakeShopCatalog();
    const PurchaseOutcome o = EvaluatePurchase(cat, "nidir", 300, {});
    NIMVLETS_CHECK(o.result == PurchaseResult::kSuccess);
    NIMVLETS_CHECK(o.newBalance == 0);
    NIMVLETS_CHECK((o.newEntitlements == Ents{Whole("nidir")}));
    return true;
}

bool TestOneClickShortIsInsufficient() {
    const PetCatalog cat = MakeShopCatalog();
    const PurchaseOutcome o = EvaluatePurchase(cat, "nidir", 299, {});
    NIMVLETS_CHECK(o.result == PurchaseResult::kInsufficientBalance);
    NIMVLETS_CHECK(o.price == 300);
    NIMVLETS_CHECK(o.newBalance == 299);          // sin débito
    NIMVLETS_CHECK(o.newEntitlements.empty());    // sin mutación
    return true;
}

bool TestZeroBalanceIsInsufficient() {
    const PetCatalog cat = MakeShopCatalog();
    const PurchaseOutcome o = EvaluatePurchase(cat, "bunny", 0, {});
    NIMVLETS_CHECK(o.result == PurchaseResult::kInsufficientBalance);
    NIMVLETS_CHECK(o.newBalance == 0);
    return true;
}

bool TestVeryLargeBalanceSucceedsWithoutOverflow() {
    const PetCatalog cat = MakeShopCatalog();
    const std::uint64_t huge = std::numeric_limits<std::uint64_t>::max();
    const PurchaseOutcome o = EvaluatePurchase(cat, "nidir", huge, {});
    NIMVLETS_CHECK(o.result == PurchaseResult::kSuccess);
    NIMVLETS_CHECK(o.newBalance == huge - 300);
    return true;
}

bool TestAlreadyOwnedDoesNotDebit() {
    const PetCatalog cat = MakeShopCatalog();
    const PurchaseOutcome o = EvaluatePurchase(cat, "bunny", 1000, {Whole("bunny")});
    NIMVLETS_CHECK(o.result == PurchaseResult::kAlreadyOwned);
    NIMVLETS_CHECK(o.newBalance == 1000);
    NIMVLETS_CHECK((o.newEntitlements == Ents{Whole("bunny")}));
    return true;
}

// Confirmar dos veces: la primera compra tiene éxito; aplicada al
// estado, la segunda evaluación da kAlreadyOwned sin débito.
bool TestRepeatedPurchaseIsIdempotentAfterFirst() {
    const PetCatalog cat = MakeShopCatalog();
    const PurchaseOutcome first = EvaluatePurchase(cat, "nidir", 400, {});
    NIMVLETS_CHECK(first.result == PurchaseResult::kSuccess);

    const PurchaseOutcome second =
        EvaluatePurchase(cat, "nidir", first.newBalance, first.newEntitlements);
    NIMVLETS_CHECK(second.result == PurchaseResult::kAlreadyOwned);
    NIMVLETS_CHECK(second.newBalance == first.newBalance);  // sin doble débito
    return true;
}

bool TestFrinIsNotPubliclyPurchasable() {
    const PetCatalog cat = MakeShopCatalog();
    const PurchaseOutcome o = EvaluatePurchase(cat, "frin", 100000, {});
    NIMVLETS_CHECK(o.result == PurchaseResult::kNotPurchasable);
    NIMVLETS_CHECK(o.newBalance == 100000);
    NIMVLETS_CHECK(o.newEntitlements.empty());
    return true;
}

bool TestUnknownTargetIsInvalid() {
    const PetCatalog cat = MakeShopCatalog();
    const PurchaseOutcome o = EvaluatePurchase(cat, "ghost", 100000, {});
    NIMVLETS_CHECK(o.result == PurchaseResult::kInvalidTarget);
    return true;
}

// Precio 0 en una entrada pública nunca llega desde el catálogo (el
// loader lo rechaza), pero la política también se protege: precio 0 ->
// no comprable.
bool TestZeroPriceIsRejected() {
    std::vector<CatalogEntry> e;
    CatalogEntry p;
    p.identity = PetIdentity{"freebie", ""};
    p.displayName = "Freebie";
    p.packPath = "f.nvpack";
    p.isDefault = true;
    p.priceClicks = 0;
    p.publiclyPurchasable = true;  // (estado imposible vía el loader — defensa en profundidad)
    e.push_back(p);
    const PetCatalog cat(std::move(e));

    const PurchaseOutcome o = EvaluatePurchase(cat, "freebie", 10, {});
    NIMVLETS_CHECK(o.result == PurchaseResult::kNotPurchasable);
    return true;
}

// Ningún camino de fallo muta el estado (ni parcialmente).
bool TestNoPartialMutationOnAnyFailure() {
    const PetCatalog cat = MakeShopCatalog();
    const Ents start = {Whole("bunny")};
    for (const char* pet : {"nidir", "frin", "ghost", "bunny"}) {
        // nidir: insuficiente; frin: no público; ghost: inválido; bunny: ya poseído.
        const std::uint64_t bal = (std::string(pet) == "nidir") ? 10 : 10;
        const PurchaseOutcome o = EvaluatePurchase(cat, pet, bal, start);
        NIMVLETS_CHECK(o.result != PurchaseResult::kSuccess);
        NIMVLETS_CHECK(o.newBalance == bal);
        NIMVLETS_CHECK((o.newEntitlements == start));
    }
    return true;
}

}  // namespace

void RegisterPurchasePolicyTests(testing::TestRunner& runner) {
    runner.Add("PurchasePolicy/SuccessDebitsExactPriceAndGrantsOnce", TestSuccessDebitsExactPriceAndGrantsOnce);
    runner.Add("PurchasePolicy/ExactPriceBalanceSucceedsToZero", TestExactPriceBalanceSucceedsToZero);
    runner.Add("PurchasePolicy/OneClickShortIsInsufficient", TestOneClickShortIsInsufficient);
    runner.Add("PurchasePolicy/ZeroBalanceIsInsufficient", TestZeroBalanceIsInsufficient);
    runner.Add("PurchasePolicy/VeryLargeBalanceSucceedsWithoutOverflow", TestVeryLargeBalanceSucceedsWithoutOverflow);
    runner.Add("PurchasePolicy/AlreadyOwnedDoesNotDebit", TestAlreadyOwnedDoesNotDebit);
    runner.Add("PurchasePolicy/RepeatedPurchaseIsIdempotentAfterFirst", TestRepeatedPurchaseIsIdempotentAfterFirst);
    runner.Add("PurchasePolicy/FrinIsNotPubliclyPurchasable", TestFrinIsNotPubliclyPurchasable);
    runner.Add("PurchasePolicy/UnknownTargetIsInvalid", TestUnknownTargetIsInvalid);
    runner.Add("PurchasePolicy/ZeroPriceIsRejected", TestZeroPriceIsRejected);
    runner.Add("PurchasePolicy/NoPartialMutationOnAnyFailure", TestNoPartialMutationOnAnyFailure);
}

}  // namespace nimvlets::tests
