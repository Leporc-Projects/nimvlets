#include "StarterShopModelTest.h"

#include <string>
#include <vector>

#include "catalog/StarterShopModel.h"

using nimvlets::catalog::BuildStarterShopModel;
using nimvlets::catalog::CatalogEntry;
using nimvlets::catalog::IsStarterShopEligible;
using nimvlets::catalog::PetCatalog;
using nimvlets::catalog::PetEntitlement;
using nimvlets::catalog::PetIdentity;
using nimvlets::catalog::StarterRole;
using nimvlets::catalog::StarterShopModel;
using nimvlets::catalog::StarterShopOfferStatus;

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

// Catálogo estilo harness-DEV de onboarding: 3 starters normales
// (artu_dev / rato_dev / rinrin_dev) con precio de QA + Frin secreto
// (male/female) con precio de QA. NINGUNO es publiclyPurchasable.
PetCatalog MakeStarterCatalog() {
    std::vector<CatalogEntry> e;
    e.push_back(MakeEntry("frin", "male", StarterRole::kSecret, 150, false, true));
    e.push_back(MakeEntry("frin", "female", StarterRole::kSecret, 150));
    e.push_back(MakeEntry("artu_dev", "", StarterRole::kNormal, 80));
    e.push_back(MakeEntry("rato_dev", "", StarterRole::kNormal, 100));
    e.push_back(MakeEntry("rinrin_dev", "", StarterRole::kNormal, 120));
    return PetCatalog(std::move(e));
}

// --- lifecycle gate --------------------------------------------------

bool TestPendingLifecycleYieldsNoOffers() {
    const PetCatalog cat = MakeStarterCatalog();
    const StarterShopModel m =
        BuildStarterShopModel(cat, /*lifecycleCompleted=*/false, {Var("frin", "female")}, 10000);
    NIMVLETS_CHECK(m.Empty());
    // Y el predicado directo también.
    NIMVLETS_CHECK(!IsStarterShopEligible(cat, false, {Var("frin", "female")}, Id("frin", "male")));
    return true;
}

bool TestLegacyCompleteYieldsNoOffers() {
    // "legacy complete" llega a esta capa como lifecycleCompleted==false
    // (solo kCompleted EXACTO habilita el Starter Shop — brief §5).
    const PetCatalog cat = MakeStarterCatalog();
    const StarterShopModel m =
        BuildStarterShopModel(cat, /*lifecycleCompleted=*/false, {Var("frin", "male")}, 10000);
    NIMVLETS_CHECK(m.Empty());
    return true;
}

bool TestCompletedLifecycleWithFrinSiblingIsEligible() {
    const PetCatalog cat = MakeStarterCatalog();
    const StarterShopModel m =
        BuildStarterShopModel(cat, /*lifecycleCompleted=*/true, {Var("frin", "female")}, 10000);
    // frin/male ofrecido; frin/female NO (poseído); los 3 normales SÍ.
    NIMVLETS_CHECK(m.Find(Id("frin", "male")) != nullptr);
    NIMVLETS_CHECK(m.Find(Id("frin", "female")) == nullptr);
    NIMVLETS_CHECK(m.Find(Id("artu_dev")) != nullptr);
    NIMVLETS_CHECK(m.offers.size() == 4);
    return true;
}

// --- ownership -----------------------------------------------------

bool TestOwnedExactIdentityExcluded() {
    const PetCatalog cat = MakeStarterCatalog();
    const StarterShopModel m = BuildStarterShopModel(
        cat, true, {Var("frin", "female"), Var("frin", "male"), NoVar("artu_dev")}, 10000);
    // Ambas variantes de Frin poseídas + artu_dev poseído -> quedan
    // rato_dev y rinrin_dev.
    NIMVLETS_CHECK(m.Find(Id("frin", "male")) == nullptr);
    NIMVLETS_CHECK(m.Find(Id("frin", "female")) == nullptr);
    NIMVLETS_CHECK(m.Find(Id("artu_dev")) == nullptr);
    NIMVLETS_CHECK(m.offers.size() == 2);
    NIMVLETS_CHECK(m.Find(Id("rato_dev")) != nullptr);
    NIMVLETS_CHECK(m.Find(Id("rinrin_dev")) != nullptr);
    return true;
}

bool TestNormalUnownedStarterEligibleWhenPriced() {
    const PetCatalog cat = MakeStarterCatalog();
    // Eligió un starter normal (artu_dev) -> los otros dos normales se
    // ofrecen.
    const StarterShopModel m = BuildStarterShopModel(cat, true, {NoVar("artu_dev")}, 10000);
    NIMVLETS_CHECK(m.Find(Id("rato_dev")) != nullptr);
    NIMVLETS_CHECK(m.Find(Id("rinrin_dev")) != nullptr);
    NIMVLETS_CHECK(m.Find(Id("artu_dev")) == nullptr);  // poseído
    return true;
}

// --- secret non-disclosure --------------------------------------------

bool TestSecretVariantEligibleOnlyWithOwnedSibling() {
    const PetCatalog cat = MakeStarterCatalog();
    // Posee frin/female -> puede comprar frin/male.
    NIMVLETS_CHECK(IsStarterShopEligible(cat, true, {Var("frin", "female")}, Id("frin", "male")));
    // Posee frin/male -> puede comprar frin/female.
    NIMVLETS_CHECK(IsStarterShopEligible(cat, true, {Var("frin", "male")}, Id("frin", "female")));
    return true;
}

bool TestSecretWithoutOwnedSiblingIsHidden() {
    const PetCatalog cat = MakeStarterCatalog();
    // Un usuario que eligió un starter NORMAL (no Frin) — no posee
    // ninguna variante de frin: Frin NUNCA aparece (brief §3).
    const StarterShopModel m = BuildStarterShopModel(cat, true, {NoVar("artu_dev")}, 10000);
    NIMVLETS_CHECK(m.Find(Id("frin", "male")) == nullptr);
    NIMVLETS_CHECK(m.Find(Id("frin", "female")) == nullptr);
    NIMVLETS_CHECK(!IsStarterShopEligible(cat, true, {NoVar("artu_dev")}, Id("frin", "male")));
    NIMVLETS_CHECK(!IsStarterShopEligible(cat, true, {NoVar("artu_dev")}, Id("frin", "female")));
    // Ni siquiera con un balance enorme.
    for (const auto& o : m.offers) {
        NIMVLETS_CHECK(o.PetId() != "frin");
    }
    return true;
}

bool TestSecretCannotLeakToNormalStarterUser() {
    // Un catálogo con SOLO un secreto priced + un normal priced. El
    // usuario del normal jamás descubre el secreto.
    std::vector<CatalogEntry> e;
    e.push_back(MakeEntry("artu_dev", "", StarterRole::kNormal, 80, false, true));
    e.push_back(MakeEntry("frin", "male", StarterRole::kSecret, 150));
    e.push_back(MakeEntry("frin", "female", StarterRole::kSecret, 150));
    const PetCatalog cat(std::move(e));

    const StarterShopModel m = BuildStarterShopModel(cat, true, {NoVar("artu_dev")}, 999999);
    NIMVLETS_CHECK(m.Empty());  // artu_dev poseído, frin oculto -> 0 ofertas
    return true;
}

// --- pricing / role gates -------------------------------------------

bool TestZeroPriceYieldsNoOffer() {
    std::vector<CatalogEntry> e;
    e.push_back(MakeEntry("artu_dev", "", StarterRole::kNormal, 0, false, true));  // sin precio
    e.push_back(MakeEntry("frin", "male", StarterRole::kSecret, 0));
    e.push_back(MakeEntry("frin", "female", StarterRole::kSecret, 0));
    const PetCatalog cat(std::move(e));
    const StarterShopModel m = BuildStarterShopModel(cat, true, {Var("frin", "female")}, 10000);
    NIMVLETS_CHECK(m.Empty());
    return true;
}

bool TestNonStarterEntryNeverOffered() {
    std::vector<CatalogEntry> e;
    CatalogEntry bunny = MakeEntry("bunny", "", StarterRole::kNone, 120, true, true);
    CatalogEntry nidir = MakeEntry("nidir", "", StarterRole::kNone, 300, true);
    e.push_back(bunny);
    e.push_back(nidir);
    e.push_back(MakeEntry("frin", "male", StarterRole::kSecret, 150));
    e.push_back(MakeEntry("frin", "female", StarterRole::kSecret, 150));
    const PetCatalog cat(std::move(e));
    // Posee bunny (no starter) y frin/female.
    const StarterShopModel m =
        BuildStarterShopModel(cat, true, {NoVar("bunny"), Var("frin", "female")}, 10000);
    // Solo frin/male — bunny/nidir NUNCA (starterRole kNone), aunque
    // tengan precio y sean publiclyPurchasable.
    NIMVLETS_CHECK(m.offers.size() == 1);
    NIMVLETS_CHECK(m.Find(Id("frin", "male")) != nullptr);
    NIMVLETS_CHECK(m.Find(Id("bunny")) == nullptr);
    NIMVLETS_CHECK(m.Find(Id("nidir")) == nullptr);
    return true;
}

// --- exact-variant identity + offer payload ------------------------

bool TestExactVariantIdentityPreservedInOffer() {
    const PetCatalog cat = MakeStarterCatalog();
    const StarterShopModel m = BuildStarterShopModel(cat, true, {Var("frin", "male")}, 10000);
    const auto* offer = m.Find(Id("frin", "female"));
    NIMVLETS_CHECK(offer != nullptr);
    NIMVLETS_CHECK(offer->target.petId == "frin");
    NIMVLETS_CHECK(offer->target.variantId == "female");
    NIMVLETS_CHECK(offer->entitlementTarget.petId == "frin");
    NIMVLETS_CHECK(offer->entitlementTarget.variantId == "female");
    NIMVLETS_CHECK(offer->displayName == "Frin");
    NIMVLETS_CHECK(offer->IsSecret());
    NIMVLETS_CHECK(offer->priceClicks == 150);
    return true;
}

bool TestAffordabilityStatusAndClicksShort() {
    const PetCatalog cat = MakeStarterCatalog();
    // Balance 100: artu_dev(80) asequible; rato_dev(100) asequible;
    // rinrin_dev(120) insuficiente por 20; frin/male(150) insuficiente
    // por 50.
    const StarterShopModel m = BuildStarterShopModel(cat, true, {Var("frin", "female")}, 100);
    NIMVLETS_CHECK(m.Find(Id("artu_dev"))->status == StarterShopOfferStatus::kAffordable);
    NIMVLETS_CHECK(m.Find(Id("rato_dev"))->status == StarterShopOfferStatus::kAffordable);
    const auto* rr = m.Find(Id("rinrin_dev"));
    NIMVLETS_CHECK(rr->status == StarterShopOfferStatus::kInsufficientBalance);
    NIMVLETS_CHECK(rr->clicksShort == 20);
    const auto* fm = m.Find(Id("frin", "male"));
    NIMVLETS_CHECK(fm->status == StarterShopOfferStatus::kInsufficientBalance);
    NIMVLETS_CHECK(fm->clicksShort == 50);
    return true;
}

bool TestDeterministicCatalogOrdering() {
    const PetCatalog cat = MakeStarterCatalog();
    const StarterShopModel a = BuildStarterShopModel(cat, true, {Var("frin", "female")}, 10000);
    const StarterShopModel b = BuildStarterShopModel(cat, true, {Var("frin", "female")}, 10000);
    NIMVLETS_CHECK(a.offers.size() == b.offers.size());
    NIMVLETS_CHECK(a.offers.size() == 4);
    // Orden de catálogo: frin/male, artu_dev, rato_dev, rinrin_dev.
    NIMVLETS_CHECK(a.offers[0].target == Id("frin", "male"));
    NIMVLETS_CHECK(a.offers[1].target == Id("artu_dev"));
    NIMVLETS_CHECK(a.offers[2].target == Id("rato_dev"));
    NIMVLETS_CHECK(a.offers[3].target == Id("rinrin_dev"));
    for (std::size_t i = 0; i < a.offers.size(); ++i) {
        NIMVLETS_CHECK(a.offers[i].target == b.offers[i].target);
    }
    return true;
}

bool TestUnknownIdentityNotEligible() {
    const PetCatalog cat = MakeStarterCatalog();
    NIMVLETS_CHECK(!IsStarterShopEligible(cat, true, {Var("frin", "female")}, Id("ghost")));
    NIMVLETS_CHECK(!IsStarterShopEligible(cat, true, {Var("frin", "female")}, Id("frin", "spirit")));
    NIMVLETS_CHECK(!IsStarterShopEligible(cat, true, {Var("frin", "female")}, Id("", "")));
    // Comprar "todo Frin" ({frin, ""}) no calza con ninguna entrada.
    NIMVLETS_CHECK(!IsStarterShopEligible(cat, true, {Var("frin", "female")}, Id("frin", "")));
    return true;
}

}  // namespace

void RegisterStarterShopModelTests(testing::TestRunner& runner) {
    runner.Add("StarterShopModel/PendingLifecycleYieldsNoOffers", TestPendingLifecycleYieldsNoOffers);
    runner.Add("StarterShopModel/LegacyCompleteYieldsNoOffers", TestLegacyCompleteYieldsNoOffers);
    runner.Add("StarterShopModel/CompletedLifecycleWithFrinSiblingIsEligible",
               TestCompletedLifecycleWithFrinSiblingIsEligible);
    runner.Add("StarterShopModel/OwnedExactIdentityExcluded", TestOwnedExactIdentityExcluded);
    runner.Add("StarterShopModel/NormalUnownedStarterEligibleWhenPriced",
               TestNormalUnownedStarterEligibleWhenPriced);
    runner.Add("StarterShopModel/SecretVariantEligibleOnlyWithOwnedSibling",
               TestSecretVariantEligibleOnlyWithOwnedSibling);
    runner.Add("StarterShopModel/SecretWithoutOwnedSiblingIsHidden", TestSecretWithoutOwnedSiblingIsHidden);
    runner.Add("StarterShopModel/SecretCannotLeakToNormalStarterUser", TestSecretCannotLeakToNormalStarterUser);
    runner.Add("StarterShopModel/ZeroPriceYieldsNoOffer", TestZeroPriceYieldsNoOffer);
    runner.Add("StarterShopModel/NonStarterEntryNeverOffered", TestNonStarterEntryNeverOffered);
    runner.Add("StarterShopModel/ExactVariantIdentityPreservedInOffer", TestExactVariantIdentityPreservedInOffer);
    runner.Add("StarterShopModel/AffordabilityStatusAndClicksShort", TestAffordabilityStatusAndClicksShort);
    runner.Add("StarterShopModel/DeterministicCatalogOrdering", TestDeterministicCatalogOrdering);
    runner.Add("StarterShopModel/UnknownIdentityNotEligible", TestUnknownIdentityNotEligible);
}

}  // namespace nimvlets::tests
