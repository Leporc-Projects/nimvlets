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

// SINTÉTICO (no producto): un catálogo donde Frin/male SÍ es
// públicamente comprable — para probar que la política procesa un
// objetivo de variante concreta SIN ninguna rama `if (pet == "frin")`.
// Frin NUNCA es público en el catálogo real de Block 07.
PetCatalog MakeVariantShopCatalog() {
    std::vector<CatalogEntry> e;

    CatalogEntry bunny;
    bunny.identity = PetIdentity{"bunny", ""};
    bunny.displayName = "Bunny";
    bunny.packPath = "b.nvpack";
    bunny.isDefault = true;
    bunny.priceClicks = 120;
    bunny.publiclyPurchasable = true;
    e.push_back(bunny);

    CatalogEntry fm;
    fm.identity = PetIdentity{"frin", "male"};
    fm.displayName = "Frin";
    fm.packPath = "fm.nvpack";
    fm.priceClicks = 200;
    fm.publiclyPurchasable = true;  // SOLO en este catálogo sintético
    e.push_back(fm);

    CatalogEntry ff;
    ff.identity = PetIdentity{"frin", "female"};
    ff.displayName = "Frin";
    ff.packPath = "ff.nvpack";
    e.push_back(ff);  // hembra sigue NO pública

    return PetCatalog(std::move(e));
}

PetEntitlement NoVar(const std::string& p) { return PetEntitlement{p, ""}; }
PetEntitlement Var(const std::string& p, const std::string& v) { return PetEntitlement{p, v}; }
PetIdentity Id(const std::string& p, const std::string& v = "") { return PetIdentity{p, v}; }

bool TestSuccessDebitsExactPriceAndGrantsCorrectTarget() {
    const PetCatalog cat = MakeShopCatalog();
    const PurchaseOutcome o = EvaluatePurchase(cat, Id("nidir"), 500, {NoVar("bunny")});
    NIMVLETS_CHECK(o.result == PurchaseResult::kSuccess);
    NIMVLETS_CHECK(o.price == 300);
    NIMVLETS_CHECK(o.newBalance == 200);  // 500 - 300 exacto
    NIMVLETS_CHECK((o.grantedEntitlement == NoVar("nidir")));
    NIMVLETS_CHECK((o.newEntitlements == Ents{NoVar("bunny"), NoVar("nidir")}));
    return true;
}

bool TestBunnyGrantTargetIsCorrect() {
    const PetCatalog cat = MakeShopCatalog();
    const PurchaseOutcome o = EvaluatePurchase(cat, Id("bunny"), 1000, {});
    NIMVLETS_CHECK(o.result == PurchaseResult::kSuccess);
    NIMVLETS_CHECK((o.grantedEntitlement == NoVar("bunny")));
    NIMVLETS_CHECK((o.newEntitlements == Ents{NoVar("bunny")}));
    return true;
}

bool TestExactPriceBalanceSucceedsToZero() {
    const PetCatalog cat = MakeShopCatalog();
    const PurchaseOutcome o = EvaluatePurchase(cat, Id("nidir"), 300, {});
    NIMVLETS_CHECK(o.result == PurchaseResult::kSuccess);
    NIMVLETS_CHECK(o.newBalance == 0);
    NIMVLETS_CHECK((o.newEntitlements == Ents{NoVar("nidir")}));
    return true;
}

bool TestOneClickShortIsInsufficient() {
    const PetCatalog cat = MakeShopCatalog();
    const PurchaseOutcome o = EvaluatePurchase(cat, Id("nidir"), 299, {});
    NIMVLETS_CHECK(o.result == PurchaseResult::kInsufficientBalance);
    NIMVLETS_CHECK(o.price == 300);
    NIMVLETS_CHECK(o.newBalance == 299);          // sin débito
    NIMVLETS_CHECK(o.newEntitlements.empty());    // sin mutación
    return true;
}

bool TestZeroBalanceIsInsufficient() {
    const PetCatalog cat = MakeShopCatalog();
    const PurchaseOutcome o = EvaluatePurchase(cat, Id("bunny"), 0, {});
    NIMVLETS_CHECK(o.result == PurchaseResult::kInsufficientBalance);
    NIMVLETS_CHECK(o.newBalance == 0);
    return true;
}

bool TestVeryLargeBalanceSucceedsWithoutOverflow() {
    const PetCatalog cat = MakeShopCatalog();
    const std::uint64_t huge = std::numeric_limits<std::uint64_t>::max();
    const PurchaseOutcome o = EvaluatePurchase(cat, Id("nidir"), huge, {});
    NIMVLETS_CHECK(o.result == PurchaseResult::kSuccess);
    NIMVLETS_CHECK(o.newBalance == huge - 300);
    return true;
}

bool TestAlreadyOwnedDoesNotDebit() {
    const PetCatalog cat = MakeShopCatalog();
    const PurchaseOutcome o = EvaluatePurchase(cat, Id("bunny"), 1000, {NoVar("bunny")});
    NIMVLETS_CHECK(o.result == PurchaseResult::kAlreadyOwned);
    NIMVLETS_CHECK(o.newBalance == 1000);
    NIMVLETS_CHECK((o.newEntitlements == Ents{NoVar("bunny")}));
    return true;
}

// Confirmar dos veces: la primera compra tiene éxito; aplicada al
// estado, la segunda evaluación da kAlreadyOwned sin débito.
bool TestRepeatedPurchaseIsIdempotentAfterFirst() {
    const PetCatalog cat = MakeShopCatalog();
    const PurchaseOutcome first = EvaluatePurchase(cat, Id("nidir"), 400, {});
    NIMVLETS_CHECK(first.result == PurchaseResult::kSuccess);

    const PurchaseOutcome second =
        EvaluatePurchase(cat, Id("nidir"), first.newBalance, first.newEntitlements);
    NIMVLETS_CHECK(second.result == PurchaseResult::kAlreadyOwned);
    NIMVLETS_CHECK(second.newBalance == first.newBalance);  // sin doble débito
    return true;
}

// Frin no es público en el catálogo real: la entrada {frin, "male"}
// existe -> kNotPurchasable (no kInvalidTarget). Y {frin, ""} (comprar
// "todo Frin") no calza con ninguna entrada -> kInvalidTarget.
bool TestFrinTargetsRejected() {
    const PetCatalog cat = MakeShopCatalog();

    const PurchaseOutcome maleReal = EvaluatePurchase(cat, Id("frin", "male"), 100000, {});
    NIMVLETS_CHECK(maleReal.result == PurchaseResult::kNotPurchasable);
    NIMVLETS_CHECK(maleReal.newBalance == 100000);
    NIMVLETS_CHECK(maleReal.newEntitlements.empty());

    const PurchaseOutcome bareWhole = EvaluatePurchase(cat, Id("frin", ""), 100000, {});
    NIMVLETS_CHECK(bareWhole.result == PurchaseResult::kInvalidTarget);
    NIMVLETS_CHECK(bareWhole.newBalance == 100000);
    return true;
}

// El seam data-driven: en un catálogo SINTÉTICO donde Frin/male SÍ es
// público, la MISMA política lo compra y otorga {frin, "male"} — sin
// tocar {frin, "female"} ni {frin, ""}. Ninguna rama por pet.
bool TestVariantSpecificTargetInSyntheticCatalog() {
    const PetCatalog cat = MakeVariantShopCatalog();
    const PurchaseOutcome o = EvaluatePurchase(cat, Id("frin", "male"), 500, {NoVar("bunny")});
    NIMVLETS_CHECK(o.result == PurchaseResult::kSuccess);
    NIMVLETS_CHECK(o.price == 200);
    NIMVLETS_CHECK(o.newBalance == 300);
    NIMVLETS_CHECK((o.grantedEntitlement == Var("frin", "male")));
    NIMVLETS_CHECK((o.newEntitlements == Ents{NoVar("bunny"), Var("frin", "male")}));
    // Hembra NO se otorgó, y NO hay un {frin, ""}.
    for (const auto& en : o.newEntitlements) {
        NIMVLETS_CHECK(!(en.petId == "frin" && en.variantId == "female"));
        NIMVLETS_CHECK(!(en.petId == "frin" && en.variantId.empty()));
    }

    // Repetir tras aplicar -> kAlreadyOwned, sin débito.
    const PurchaseOutcome again =
        EvaluatePurchase(cat, Id("frin", "male"), o.newBalance, o.newEntitlements);
    NIMVLETS_CHECK(again.result == PurchaseResult::kAlreadyOwned);
    NIMVLETS_CHECK(again.newBalance == o.newBalance);

    // La hembra sigue sin ser comprable (su entrada no es pública).
    const PurchaseOutcome female =
        EvaluatePurchase(cat, Id("frin", "female"), 999, o.newEntitlements);
    NIMVLETS_CHECK(female.result == PurchaseResult::kNotPurchasable);
    return true;
}

bool TestUnknownTargetIsInvalid() {
    const PetCatalog cat = MakeShopCatalog();
    NIMVLETS_CHECK(EvaluatePurchase(cat, Id("ghost"), 100000, {}).result == PurchaseResult::kInvalidTarget);
    // Malformado: identidad con petId vacío.
    NIMVLETS_CHECK(EvaluatePurchase(cat, Id("", ""), 100000, {}).result == PurchaseResult::kInvalidTarget);
    // petId real pero variante inexistente.
    NIMVLETS_CHECK(EvaluatePurchase(cat, Id("nidir", "ghost"), 100000, {}).result ==
                   PurchaseResult::kInvalidTarget);
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

    const PurchaseOutcome o = EvaluatePurchase(cat, Id("freebie"), 10, {});
    NIMVLETS_CHECK(o.result == PurchaseResult::kNotPurchasable);
    return true;
}

// Ningún camino de fallo muta el estado (ni parcialmente).
bool TestNoPartialMutationOnAnyFailure() {
    const PetCatalog cat = MakeShopCatalog();
    const Ents start = {NoVar("bunny")};
    // nidir: insuficiente; frin/male: no público; ghost: inválido;
    // frin/"": inválido; bunny: ya poseído.
    for (const PetIdentity& t :
         {Id("nidir"), Id("frin", "male"), Id("ghost"), Id("frin", ""), Id("bunny")}) {
        const PurchaseOutcome o = EvaluatePurchase(cat, t, 10, start);
        NIMVLETS_CHECK(o.result != PurchaseResult::kSuccess);
        NIMVLETS_CHECK(o.newBalance == 10);
        NIMVLETS_CHECK((o.newEntitlements == start));
    }
    return true;
}

}  // namespace

void RegisterPurchasePolicyTests(testing::TestRunner& runner) {
    runner.Add("PurchasePolicy/SuccessDebitsExactPriceAndGrantsCorrectTarget",
               TestSuccessDebitsExactPriceAndGrantsCorrectTarget);
    runner.Add("PurchasePolicy/BunnyGrantTargetIsCorrect", TestBunnyGrantTargetIsCorrect);
    runner.Add("PurchasePolicy/ExactPriceBalanceSucceedsToZero", TestExactPriceBalanceSucceedsToZero);
    runner.Add("PurchasePolicy/OneClickShortIsInsufficient", TestOneClickShortIsInsufficient);
    runner.Add("PurchasePolicy/ZeroBalanceIsInsufficient", TestZeroBalanceIsInsufficient);
    runner.Add("PurchasePolicy/VeryLargeBalanceSucceedsWithoutOverflow", TestVeryLargeBalanceSucceedsWithoutOverflow);
    runner.Add("PurchasePolicy/AlreadyOwnedDoesNotDebit", TestAlreadyOwnedDoesNotDebit);
    runner.Add("PurchasePolicy/RepeatedPurchaseIsIdempotentAfterFirst", TestRepeatedPurchaseIsIdempotentAfterFirst);
    runner.Add("PurchasePolicy/FrinTargetsRejected", TestFrinTargetsRejected);
    runner.Add("PurchasePolicy/VariantSpecificTargetInSyntheticCatalog", TestVariantSpecificTargetInSyntheticCatalog);
    runner.Add("PurchasePolicy/UnknownTargetIsInvalid", TestUnknownTargetIsInvalid);
    runner.Add("PurchasePolicy/ZeroPriceIsRejected", TestZeroPriceIsRejected);
    runner.Add("PurchasePolicy/NoPartialMutationOnAnyFailure", TestNoPartialMutationOnAnyFailure);
}

}  // namespace nimvlets::tests
