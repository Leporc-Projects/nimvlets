#include "OnboardingLayoutTest.h"

#include "catalog/OnboardingPolicy.h"
#include "catalog/PetCatalog.h"
#include "core/Localization.h"
#include "productui/OnboardingLayout.h"

#include <string>
#include <vector>

using nimvlets::catalog::BuildOnboardingOffer;
using nimvlets::catalog::CatalogEntry;
using nimvlets::catalog::OnboardingOffer;
using nimvlets::catalog::PetCatalog;
using nimvlets::catalog::PetIdentity;
using nimvlets::catalog::StarterRole;
using nimvlets::core::Language;
using nimvlets::productui::BuildOnboardingLayout;
using nimvlets::productui::OnboardingLayout;
using nimvlets::productui::OnboardingLayoutInput;
using nimvlets::productui::OnboardingStage;

namespace nimvlets::tests {

namespace {

bool Has(const std::vector<std::string>& v, const std::string& s) {
    for (const auto& x : v) {
        if (x == s) return true;
    }
    return false;
}

CatalogEntry Entry(const std::string& petId, const std::string& variantId, const std::string& name,
                   StarterRole role, bool isDefault = false) {
    CatalogEntry e;
    e.identity = PetIdentity{petId, variantId};
    e.displayName = name;
    e.packPath = petId + ".nvpack";
    e.isDefault = isDefault;
    e.starterRole = role;
    return e;
}

OnboardingOffer MakeOffer(bool revealed) {
    std::vector<CatalogEntry> e;
    e.push_back(Entry("artu", "", "Artu", StarterRole::kNormal, /*isDefault=*/true));
    e.push_back(Entry("rato", "", "Rato", StarterRole::kNormal));
    e.push_back(Entry("rinrin", "", "Rin Rin", StarterRole::kNormal));
    e.push_back(Entry("frin", "male", "Frin", StarterRole::kSecret));
    e.push_back(Entry("frin", "female", "Frin", StarterRole::kSecret));
    OnboardingOffer o = BuildOnboardingOffer(PetCatalog(std::move(e)));
    o.secretRevealed = revealed;
    return o;
}

OnboardingLayout Layout(OnboardingStage stage, bool revealed, Language lang = Language::kEn,
                        const PetIdentity& pending = {}, const std::string& pendingName = {}) {
    OnboardingLayoutInput in;
    in.language = lang;
    in.offer = MakeOffer(revealed);
    in.stage = stage;
    in.pendingSelection = pending;
    in.pendingDisplayName = pendingName;
    return BuildOnboardingLayout(in);
}

// --- kBrowse: los 3 normales; el secreto oculto hasta el reveal ----

bool TestBrowseShowsThreeNormalsSecretHidden() {
    const OnboardingLayout l = Layout(OnboardingStage::kBrowse, /*revealed=*/false);
    NIMVLETS_CHECK(l.heading == "Choose your first Nimvlet");
    NIMVLETS_CHECK(l.candidates.size() == 3);
    NIMVLETS_CHECK(l.candidates[0].focusId == "cand:artu");
    NIMVLETS_CHECK(l.candidates[1].focusId == "cand:rato");
    NIMVLETS_CHECK(l.candidates[2].focusId == "cand:rinrin");
    NIMVLETS_CHECK(!Has(l.focusOrder, "cand:frin"));
    NIMVLETS_CHECK(l.focusOrder.size() == 3);
    NIMVLETS_CHECK(l.confirm.visible == false);
    return true;
}

bool TestRevealAddsFrinAtTheEnd() {
    const OnboardingLayout l = Layout(OnboardingStage::kBrowse, /*revealed=*/true);
    NIMVLETS_CHECK(l.candidates.size() == 4);
    // Frin SIEMPRE al final -> no reordena ni arrebata el foco (brief §21).
    NIMVLETS_CHECK(l.candidates[3].focusId == "cand:frin");
    NIMVLETS_CHECK(l.candidates[3].identity == (PetIdentity{"frin", ""}));
    NIMVLETS_CHECK(l.candidates[3].displayName == "Frin");
    NIMVLETS_CHECK(l.focusOrder.size() == 4 && l.focusOrder[3] == "cand:frin");
    // Los 3 primeros no se movieron.
    NIMVLETS_CHECK(l.candidates[0].focusId == "cand:artu");
    return true;
}

bool TestBrowseHeadingLocalized() {
    NIMVLETS_CHECK(Layout(OnboardingStage::kBrowse, false, Language::kEs).heading ==
                   "Elige tu primer Nimvlet");
    return true;
}

bool TestBrowseHitTest() {
    const OnboardingLayout l = Layout(OnboardingStage::kBrowse, /*revealed=*/true);
    for (const auto& c : l.candidates) {
        NIMVLETS_CHECK(l.HitTest(c.cell.CenterX(), c.cell.CenterY()) == c.focusId);
    }
    // Fuera de cualquier tarjeta -> "".
    NIMVLETS_CHECK(l.HitTest(2.0f, 2.0f).empty());
    return true;
}

// --- kFrinVariant: la sub-elección macho/hembra -------------------

bool TestFrinVariantStage() {
    const OnboardingLayout en = Layout(OnboardingStage::kFrinVariant, /*revealed=*/true);
    NIMVLETS_CHECK(en.heading == "Which Frin?");
    NIMVLETS_CHECK(en.candidates.size() == 2);
    NIMVLETS_CHECK(en.candidates[0].focusId == "var:male");
    NIMVLETS_CHECK(en.candidates[0].speciesText == "Male");
    NIMVLETS_CHECK(en.candidates[1].focusId == "var:female");
    NIMVLETS_CHECK(en.candidates[1].speciesText == "Female");
    NIMVLETS_CHECK((en.focusOrder == std::vector<std::string>{"var:male", "var:female"}));

    const OnboardingLayout es = Layout(OnboardingStage::kFrinVariant, true, Language::kEs);
    NIMVLETS_CHECK(es.heading == "¿Qué Frin?");
    NIMVLETS_CHECK(es.candidates[0].speciesText == "Macho");
    NIMVLETS_CHECK(es.candidates[1].speciesText == "Hembra");
    return true;
}

// --- kConfirm: prompt + Cancel / Choose <name> -------------------

bool TestConfirmStageEn() {
    const OnboardingLayout l = Layout(OnboardingStage::kConfirm, /*revealed=*/true,
                                      Language::kEn, PetIdentity{"artu", ""}, "Artu");
    NIMVLETS_CHECK(l.candidates.empty());
    NIMVLETS_CHECK(l.confirm.visible);
    NIMVLETS_CHECK(l.confirm.prompt == "Make Artu your first Nimvlet?");
    NIMVLETS_CHECK(l.confirm.cancelLabel == "Cancel");
    NIMVLETS_CHECK(l.confirm.cancelFocusId == "onb:cancel");
    NIMVLETS_CHECK(l.confirm.chooseLabel == "Choose Artu");
    NIMVLETS_CHECK(l.confirm.chooseFocusId == "onb:choose");
    // El foco ARRANCA en Cancel (brief §23): un click perdido no completa.
    NIMVLETS_CHECK((l.focusOrder == std::vector<std::string>{"onb:cancel", "onb:choose"}));
    // Hit-test de los botones.
    NIMVLETS_CHECK(
        l.HitTest(l.confirm.cancelButton.CenterX(), l.confirm.cancelButton.CenterY()) == "onb:cancel");
    NIMVLETS_CHECK(
        l.HitTest(l.confirm.chooseButton.CenterX(), l.confirm.chooseButton.CenterY()) == "onb:choose");
    return true;
}

bool TestConfirmStageEs() {
    const OnboardingLayout l = Layout(OnboardingStage::kConfirm, /*revealed=*/true,
                                      Language::kEs, PetIdentity{"rato", ""}, "Rato");
    NIMVLETS_CHECK(l.confirm.prompt == "¿Quieres que Rato sea tu primer Nimvlet?");
    NIMVLETS_CHECK(l.confirm.cancelLabel == "Cancelar");
    NIMVLETS_CHECK(l.confirm.chooseLabel == "Elegir a Rato");
    return true;
}

bool TestConfirmStageFrinVariantName() {
    // Para Frin la identidad confirmada incluye la variante elegida
    // (brief §23) — el caller pasa un pendingDisplayName ya formateado.
    const OnboardingLayout l = Layout(OnboardingStage::kConfirm, /*revealed=*/true, Language::kEn,
                                      PetIdentity{"frin", "male"}, "Frin (Male)");
    NIMVLETS_CHECK(l.confirm.prompt == "Make Frin (Male) your first Nimvlet?");
    NIMVLETS_CHECK(l.confirm.chooseLabel == "Choose Frin (Male)");
    return true;
}

// --- Layout: cabe sin scroll -----------------------------------

bool TestFitsWithoutScroll() {
    NIMVLETS_CHECK(Layout(OnboardingStage::kBrowse, true).contentHeight <= 560.0f + 1.0f);
    NIMVLETS_CHECK(Layout(OnboardingStage::kBrowse, true, Language::kEs).contentHeight <= 560.0f + 1.0f);
    NIMVLETS_CHECK(Layout(OnboardingStage::kFrinVariant, true).contentHeight <= 560.0f + 1.0f);
    NIMVLETS_CHECK(Layout(OnboardingStage::kConfirm, true, Language::kEs, PetIdentity{"rinrin", ""},
                          "Rin Rin")
                       .contentHeight <= 560.0f + 1.0f);
    return true;
}

// La fila de tarjetas cabe HORIZONTALMENTE con 3 y con 4 candidatos (el
// secreto revelado no la desborda — brief §21). No hay solape.
bool TestCardRowFitsHorizontallyForThreeAndFour() {
    for (bool revealed : {false, true}) {
        const OnboardingLayout l = Layout(OnboardingStage::kBrowse, revealed);
        NIMVLETS_CHECK(!l.candidates.empty());
        NIMVLETS_CHECK(l.candidates.front().cell.x >= 0.0f);
        NIMVLETS_CHECK(l.candidates.back().cell.Right() <= 800.0f);
        for (std::size_t i = 1; i < l.candidates.size(); ++i) {
            NIMVLETS_CHECK(l.candidates[i].cell.x >= l.candidates[i - 1].cell.Right());
        }
    }
    return true;
}

// No hay cabecera de navegación de secciones — onboarding NO es una
// sección (brief §19).
bool TestNoSectionNavInFocusOrder() {
    const OnboardingLayout l = Layout(OnboardingStage::kBrowse, /*revealed=*/true);
    NIMVLETS_CHECK(!Has(l.focusOrder, "nav:collection"));
    NIMVLETS_CHECK(!Has(l.focusOrder, "nav:shop"));
    NIMVLETS_CHECK(!Has(l.focusOrder, "nav:settings"));
    return true;
}

}  // namespace

void RegisterOnboardingLayoutTests(testing::TestRunner& runner) {
    runner.Add("OnboardingLayout/BrowseShowsThreeNormalsSecretHidden", TestBrowseShowsThreeNormalsSecretHidden);
    runner.Add("OnboardingLayout/RevealAddsFrinAtTheEnd", TestRevealAddsFrinAtTheEnd);
    runner.Add("OnboardingLayout/BrowseHeadingLocalized", TestBrowseHeadingLocalized);
    runner.Add("OnboardingLayout/BrowseHitTest", TestBrowseHitTest);
    runner.Add("OnboardingLayout/FrinVariantStage", TestFrinVariantStage);
    runner.Add("OnboardingLayout/ConfirmStageEn", TestConfirmStageEn);
    runner.Add("OnboardingLayout/ConfirmStageEs", TestConfirmStageEs);
    runner.Add("OnboardingLayout/ConfirmStageFrinVariantName", TestConfirmStageFrinVariantName);
    runner.Add("OnboardingLayout/FitsWithoutScroll", TestFitsWithoutScroll);
    runner.Add("OnboardingLayout/CardRowFitsHorizontallyForThreeAndFour",
               TestCardRowFitsHorizontallyForThreeAndFour);
    runner.Add("OnboardingLayout/NoSectionNavInFocusOrder", TestNoSectionNavInFocusOrder);
}

}  // namespace nimvlets::tests
