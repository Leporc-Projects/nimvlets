#include "OnboardingPolicyTest.h"

#include "catalog/OnboardingPolicy.h"
#include "catalog/PetCatalog.h"

#include <string>
#include <vector>

using nimvlets::catalog::BuildOnboardingOffer;
using nimvlets::catalog::CatalogEntry;
using nimvlets::catalog::CountNormalStarters;
using nimvlets::catalog::EvaluateCatalogOnboardingReadiness;
using nimvlets::catalog::EvaluateOnboardingReadiness;
using nimvlets::catalog::EvaluateOnboardingSelection;
using nimvlets::catalog::kRequiredNormalStarterCount;
using nimvlets::catalog::kSecretRevealDwellMs;
using nimvlets::catalog::OnboardingGrant;
using nimvlets::catalog::OnboardingOffer;
using nimvlets::catalog::OnboardingSelectionResult;
using nimvlets::catalog::OnboardingStarter;
using nimvlets::catalog::PetCatalog;
using nimvlets::catalog::PetIdentity;
using nimvlets::catalog::SecretRevealDeadlineMs;
using nimvlets::catalog::SecretRevealedAfterDwell;
using nimvlets::catalog::StarterRole;

namespace nimvlets::tests {

namespace {

CatalogEntry Entry(
    const std::string& petId, const std::string& variantId, StarterRole role, bool isDefault = false) {
    CatalogEntry e;
    e.identity = PetIdentity{petId, variantId};
    e.displayName = petId;
    e.packPath = petId + ".nvpack";
    e.isDefault = isDefault;
    e.starterRole = role;
    return e;
}

// Un catálogo sintético: 3 starters normales + Frin secreto (2 variantes)
// + un pet que no participa del onboarding.
PetCatalog MakeOnboardingCatalog(bool productionReady = true) {
    std::vector<CatalogEntry> e;
    e.push_back(Entry("artu", "", StarterRole::kNormal, /*isDefault=*/true));
    e.push_back(Entry("rato", "", StarterRole::kNormal));
    e.push_back(Entry("rinrin", "", StarterRole::kNormal));
    e.push_back(Entry("frin", "male", StarterRole::kSecret));
    e.push_back(Entry("frin", "female", StarterRole::kSecret));
    e.push_back(Entry("bunny", "", StarterRole::kNone));
    return PetCatalog(std::move(e), productionReady);
}

OnboardingOffer OfferFrom(const PetCatalog& catalog, bool revealed) {
    OnboardingOffer o = BuildOnboardingOffer(catalog);
    o.secretRevealed = revealed;
    return o;
}

// ================= §28 — política de selección =================

bool TestBuildOffer() {
    const OnboardingOffer o = BuildOnboardingOffer(MakeOnboardingCatalog());
    NIMVLETS_CHECK(o.normal.size() == 3);
    NIMVLETS_CHECK(o.normal[0].identity == (PetIdentity{"artu", ""}));
    NIMVLETS_CHECK(o.normal[1].identity == (PetIdentity{"rato", ""}));
    NIMVLETS_CHECK(o.normal[2].identity == (PetIdentity{"rinrin", ""}));
    NIMVLETS_CHECK(o.secret.has_value());
    NIMVLETS_CHECK(o.secret->identity == (PetIdentity{"frin", ""}));  // identidad lógica
    NIMVLETS_CHECK(o.secret->variants.size() == 2);
    NIMVLETS_CHECK(o.secret->variants[0] == (PetIdentity{"frin", "male"}));
    NIMVLETS_CHECK(o.secret->variants[1] == (PetIdentity{"frin", "female"}));
    NIMVLETS_CHECK(!o.secretRevealed);
    return true;
}

bool TestNormalStarterAccepted() {
    const OnboardingOffer o = OfferFrom(MakeOnboardingCatalog(), /*revealed=*/false);
    const OnboardingGrant g = EvaluateOnboardingSelection(o, PetIdentity{"rato", ""}, false);
    NIMVLETS_CHECK(g.result == OnboardingSelectionResult::kOk);
    NIMVLETS_CHECK(g.entitlement.petId == "rato" && g.entitlement.variantId.empty());
    NIMVLETS_CHECK(g.activeIdentity == (PetIdentity{"rato", ""}));
    NIMVLETS_CHECK(g.newBalance == 0);  // §16: usuario nuevo arranca sin clics
    return true;
}

bool TestUnknownStarterRejected() {
    const OnboardingOffer o = OfferFrom(MakeOnboardingCatalog(), /*revealed=*/true);
    for (const PetIdentity& bogus :
         {PetIdentity{"ghost", ""}, PetIdentity{"bunny", ""}, PetIdentity{"frin", "spirit"},
          PetIdentity{"rato", "x"}}) {
        const OnboardingGrant g = EvaluateOnboardingSelection(o, bogus, false);
        NIMVLETS_CHECK(g.result == OnboardingSelectionResult::kUnknownStarter);
        // §28: target inválido -> CERO mutación (grant en default).
        NIMVLETS_CHECK(g.entitlement.petId.empty());
        NIMVLETS_CHECK(g.activeIdentity.petId.empty());
        NIMVLETS_CHECK(g.newBalance == 0);
    }
    return true;
}

bool TestSecretRejectedBeforeReveal() {
    const OnboardingOffer o = OfferFrom(MakeOnboardingCatalog(), /*revealed=*/false);
    NIMVLETS_CHECK(EvaluateOnboardingSelection(o, PetIdentity{"frin", ""}, false).result ==
                   OnboardingSelectionResult::kSecretNotYetRevealed);
    NIMVLETS_CHECK(EvaluateOnboardingSelection(o, PetIdentity{"frin", "male"}, false).result ==
                   OnboardingSelectionResult::kSecretNotYetRevealed);
    return true;
}

bool TestSecretAcceptedAfterReveal() {
    const OnboardingOffer o = OfferFrom(MakeOnboardingCatalog(), /*revealed=*/true);
    // La identidad lógica {frin,""} -> el UI todavía debe pedir variante.
    NIMVLETS_CHECK(EvaluateOnboardingSelection(o, PetIdentity{"frin", ""}, false).result ==
                   OnboardingSelectionResult::kSecretNeedsVariant);
    // Una variante concreta -> kOk con esa variante EXACTA.
    const OnboardingGrant male = EvaluateOnboardingSelection(o, PetIdentity{"frin", "male"}, false);
    NIMVLETS_CHECK(male.result == OnboardingSelectionResult::kOk);
    NIMVLETS_CHECK(male.entitlement.petId == "frin" && male.entitlement.variantId == "male");
    NIMVLETS_CHECK(male.activeIdentity == (PetIdentity{"frin", "male"}));
    NIMVLETS_CHECK(male.newBalance == 0);
    return true;
}

bool TestExactFrinVariantGrant_MaleAndFemale_NeitherGrantsTheOther() {
    const OnboardingOffer o = OfferFrom(MakeOnboardingCatalog(), /*revealed=*/true);

    const OnboardingGrant male = EvaluateOnboardingSelection(o, PetIdentity{"frin", "male"}, false);
    NIMVLETS_CHECK(male.result == OnboardingSelectionResult::kOk);
    NIMVLETS_CHECK(male.entitlement.variantId == "male");
    NIMVLETS_CHECK(male.entitlement.variantId != "female");  // la hembra NO se otorga

    const OnboardingGrant female = EvaluateOnboardingSelection(o, PetIdentity{"frin", "female"}, false);
    NIMVLETS_CHECK(female.result == OnboardingSelectionResult::kOk);
    NIMVLETS_CHECK(female.entitlement.variantId == "female");
    NIMVLETS_CHECK(female.entitlement.variantId != "male");
    return true;
}

// §15 — idempotencia: un onboarding YA completo rechaza toda selección
// sin mutar nada.
bool TestCompletedUserCannotSelectAgain() {
    const OnboardingOffer o = OfferFrom(MakeOnboardingCatalog(), /*revealed=*/true);
    for (const PetIdentity& sel :
         {PetIdentity{"artu", ""}, PetIdentity{"frin", "male"}, PetIdentity{"rato", ""}}) {
        const OnboardingGrant g = EvaluateOnboardingSelection(o, sel, /*alreadyCompleted=*/true);
        NIMVLETS_CHECK(g.result == OnboardingSelectionResult::kAlreadyCompleted);
        NIMVLETS_CHECK(g.entitlement.petId.empty());  // CERO mutación
        NIMVLETS_CHECK(g.activeIdentity.petId.empty());
    }
    return true;
}

// Repetir la MISMA evaluación exitosa da el MISMO grant (determinista) —
// aplicarla una vez y luego rechazar es responsabilidad de src/app
// (alreadyCompleted), pero la política en sí es pura y estable.
bool TestRepeatedEvaluationIsDeterministic() {
    const OnboardingOffer o = OfferFrom(MakeOnboardingCatalog(), /*revealed=*/true);
    const OnboardingGrant a = EvaluateOnboardingSelection(o, PetIdentity{"artu", ""}, false);
    const OnboardingGrant b = EvaluateOnboardingSelection(o, PetIdentity{"artu", ""}, false);
    NIMVLETS_CHECK(a.result == b.result && a.result == OnboardingSelectionResult::kOk);
    NIMVLETS_CHECK(a.entitlement.petId == b.entitlement.petId);
    NIMVLETS_CHECK(a.activeIdentity == b.activeIdentity);
    NIMVLETS_CHECK(a.newBalance == 0 && b.newBalance == 0);
    return true;
}

// ================= §29 — deadline de los 44 segundos =================

bool TestSecretRevealDeadlineBoundaries() {
    NIMVLETS_CHECK(kSecretRevealDwellMs == 44000.0);
    NIMVLETS_CHECK(!SecretRevealedAfterDwell(0.0));         // t = 0: oculto
    NIMVLETS_CHECK(!SecretRevealedAfterDwell(43999.0));     // t < 44 s: oculto
    NIMVLETS_CHECK(!SecretRevealedAfterDwell(43999.999));
    NIMVLETS_CHECK(SecretRevealedAfterDwell(44000.0));      // EXACTAMENTE 44 s: revelado
    NIMVLETS_CHECK(SecretRevealedAfterDwell(44000.001));
    NIMVLETS_CHECK(SecretRevealedAfterDwell(120000.0));     // mucho después: revelado

    // El deadline monotónico = instante de activación + 44 s.
    NIMVLETS_CHECK(SecretRevealDeadlineMs(0.0) == 44000.0);
    NIMVLETS_CHECK(SecretRevealDeadlineMs(10000.0) == 54000.0);
    // Una nueva sesión arranca un dwell FRESCO: el deadline se recalcula
    // desde el nuevo instante de activación, no hay acumulación.
    NIMVLETS_CHECK(SecretRevealDeadlineMs(999999.0) == 999999.0 + kSecretRevealDwellMs);
    return true;
}

// ================= §30 — gate de contenido listo =================

bool TestReadinessGate() {
    // Incompleto (no marcado ready) -> deshabilitado, con razón.
    {
        const auto r = EvaluateOnboardingReadiness(/*manifestProductionReady=*/false, 3);
        NIMVLETS_CHECK(!r.armed);
        NIMVLETS_CHECK(!r.reason.empty());
    }
    // Marcado ready pero sin la tríada -> deshabilitado.
    {
        const auto r = EvaluateOnboardingReadiness(/*manifestProductionReady=*/true, 2);
        NIMVLETS_CHECK(!r.armed);
        NIMVLETS_CHECK(r.reason.find("need") != std::string::npos);
    }
    // Marcado ready + tríada completa -> puede armarse.
    {
        const auto r = EvaluateOnboardingReadiness(/*manifestProductionReady=*/true, 3);
        NIMVLETS_CHECK(r.armed);
        NIMVLETS_CHECK(r.reason.empty());
    }
    NIMVLETS_CHECK(kRequiredNormalStarterCount == 3);
    return true;
}

bool TestReadinessFromCatalog() {
    // Catálogo completo + marcado ready -> armado.
    NIMVLETS_CHECK(EvaluateCatalogOnboardingReadiness(MakeOnboardingCatalog(/*productionReady=*/true)).armed);
    NIMVLETS_CHECK(CountNormalStarters(MakeOnboardingCatalog()) == 3);

    // MISMO catálogo pero NO marcado ready -> NO armado (el datum
    // explícito manda).
    NIMVLETS_CHECK(
        !EvaluateCatalogOnboardingReadiness(MakeOnboardingCatalog(/*productionReady=*/false)).armed);

    // §30: la metadata del SECRETO sola no cuenta. Un catálogo con SOLO
    // el secreto (Frin) marcado, sin starters normales, aunque diga
    // ready, no se arma.
    {
        std::vector<CatalogEntry> e;
        e.push_back(Entry("bunny", "", StarterRole::kNone, /*isDefault=*/true));
        e.push_back(Entry("frin", "male", StarterRole::kSecret));
        e.push_back(Entry("frin", "female", StarterRole::kSecret));
        const PetCatalog secretOnly(std::move(e), /*productionOnboardingReady=*/true);
        NIMVLETS_CHECK(CountNormalStarters(secretOnly) == 0);
        NIMVLETS_CHECK(!EvaluateCatalogOnboardingReadiness(secretOnly).armed);
    }
    return true;
}

// DEC-133: CountNormalStarters cuenta IDENTIDADES LÓGICAS distintas, no
// filas — así filas duplicadas o "variantes" de un mismo Nimvlet no
// inflan la tríada de readiness. (El loader/compilador rechazan esas
// formas de raíz; acá se prueba que aunque una llegara a un PetCatalog
// construido a mano, la política no la cuenta de más.)
bool TestNormalStarterCountIsDistinctLogical() {
    std::vector<CatalogEntry> e;
    e.push_back(Entry("artu", "", StarterRole::kNormal, /*isDefault=*/true));
    e.push_back(Entry("artu", "", StarterRole::kNormal));       // fila duplicada
    e.push_back(Entry("artu", "gold", StarterRole::kNormal));   // "variante" del mismo pet
    e.push_back(Entry("rato", "", StarterRole::kNormal));
    const PetCatalog catalog(std::move(e), /*productionOnboardingReady=*/true);

    // 4 filas kNormal, pero solo 2 identidades lógicas (artu, rato).
    NIMVLETS_CHECK(CountNormalStarters(catalog) == 2);
    NIMVLETS_CHECK(!EvaluateCatalogOnboardingReadiness(catalog).armed);
    return true;
}

// El catálogo de dev REAL (bunny/nidir/frin, sin roles de starter, no
// marcado ready) nunca arma onboarding de producción — brief §31.
bool TestCurrentDevCatalogDoesNotArmOnboarding() {
    std::vector<CatalogEntry> e;
    e.push_back(Entry("bunny", "", StarterRole::kNone, /*isDefault=*/true));
    e.push_back(Entry("nidir", "", StarterRole::kNone));
    e.push_back(Entry("frin", "male", StarterRole::kNone));
    e.push_back(Entry("frin", "female", StarterRole::kNone));
    const PetCatalog dev(std::move(e));  // productionOnboardingReady default false
    const auto r = EvaluateCatalogOnboardingReadiness(dev);
    NIMVLETS_CHECK(!r.armed);
    NIMVLETS_CHECK(BuildOnboardingOffer(dev).normal.empty());
    NIMVLETS_CHECK(!BuildOnboardingOffer(dev).secret.has_value());
    return true;
}

}  // namespace

void RegisterOnboardingPolicyTests(testing::TestRunner& runner) {
    runner.Add("OnboardingPolicy/BuildOffer", TestBuildOffer);
    runner.Add("OnboardingPolicy/NormalStarterAccepted", TestNormalStarterAccepted);
    runner.Add("OnboardingPolicy/UnknownStarterRejected", TestUnknownStarterRejected);
    runner.Add("OnboardingPolicy/SecretRejectedBeforeReveal", TestSecretRejectedBeforeReveal);
    runner.Add("OnboardingPolicy/SecretAcceptedAfterReveal", TestSecretAcceptedAfterReveal);
    runner.Add("OnboardingPolicy/ExactFrinVariantGrantNeitherGrantsTheOther",
               TestExactFrinVariantGrant_MaleAndFemale_NeitherGrantsTheOther);
    runner.Add("OnboardingPolicy/CompletedUserCannotSelectAgain", TestCompletedUserCannotSelectAgain);
    runner.Add("OnboardingPolicy/RepeatedEvaluationIsDeterministic", TestRepeatedEvaluationIsDeterministic);
    runner.Add("OnboardingPolicy/SecretRevealDeadlineBoundaries", TestSecretRevealDeadlineBoundaries);
    runner.Add("OnboardingPolicy/ReadinessGate", TestReadinessGate);
    runner.Add("OnboardingPolicy/ReadinessFromCatalog", TestReadinessFromCatalog);
    runner.Add("OnboardingPolicy/NormalStarterCountIsDistinctLogical",
               TestNormalStarterCountIsDistinctLogical);
    runner.Add("OnboardingPolicy/CurrentDevCatalogDoesNotArmOnboarding",
               TestCurrentDevCatalogDoesNotArmOnboarding);
}

}  // namespace nimvlets::tests
