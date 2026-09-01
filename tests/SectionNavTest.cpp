#include "SectionNavTest.h"

#include <string>

#include "core/Localization.h"
#include "productui/SectionNav.h"

using nimvlets::core::Language;
using nimvlets::productui::BuildSectionHeaderLayout;
using nimvlets::productui::NavFocusIdFor;
using nimvlets::productui::NavTargetSection;
using nimvlets::productui::ProductSection;
using nimvlets::productui::SectionHeaderLayout;
using nimvlets::productui::SectionTab;

namespace nimvlets::tests {

namespace {

// La tabla de secciones y sus consumidores no pueden divergir: TODA
// vista que incruste la cabecera compartida rutea sus tres pestañas por
// NavTargetSection. El bug de Block 09A-QA fue que CollectionView /
// ShopView solo conocían Collection/Shop (forma de Block 07), así que la
// pestaña "Settings" no hacía NADA desde las dos secciones donde el
// owner realmente arranca — y ninguna captura de QA lo vio porque todas
// forzaban la sección con ShowSectionForQA (que saltea la pestaña).

bool TestNavTargetSectionMapsAllThreeTabs() {
    ProductSection s = ProductSection::kCollection;

    NIMVLETS_CHECK(NavTargetSection("nav:collection", s));
    NIMVLETS_CHECK(s == ProductSection::kCollection);

    s = ProductSection::kCollection;
    NIMVLETS_CHECK(NavTargetSection("nav:shop", s));
    NIMVLETS_CHECK(s == ProductSection::kShop);

    // La que faltaba: "Settings" DEBE rutear a kSettings.
    s = ProductSection::kCollection;
    NIMVLETS_CHECK(NavTargetSection("nav:settings", s));
    NIMVLETS_CHECK(s == ProductSection::kSettings);
    return true;
}

bool TestNavTargetSectionRejectsNonNavIds() {
    ProductSection s = ProductSection::kShop;  // valor centinela: no debe cambiar
    NIMVLETS_CHECK(!NavTargetSection("", s));
    NIMVLETS_CHECK(!NavTargetSection("item:artu", s));
    NIMVLETS_CHECK(!NavTargetSection("opt:size:large", s));
    NIMVLETS_CHECK(!NavTargetSection("nav:", s));
    NIMVLETS_CHECK(!NavTargetSection("nav:onboarding", s));
    NIMVLETS_CHECK(!NavTargetSection("use", s));
    NIMVLETS_CHECK(s == ProductSection::kShop);  // intacto
    return true;
}

// Ida y vuelta: el focusId de cada pestaña que dibuja la cabecera
// resuelve de vuelta a esa misma sección. Así una pestaña nueva en
// SectionNav nunca queda "inerte" para las vistas.
bool TestNavRoundTripThroughHeaderLayout() {
    const SectionHeaderLayout header = BuildSectionHeaderLayout(
        800.0f, 40.0f, 0.0f, ProductSection::kCollection, Language::kEn, /*clickBalance=*/0);
    NIMVLETS_CHECK(header.tabs.size() == 3);
    for (const SectionTab& tab : header.tabs) {
        NIMVLETS_CHECK(!tab.focusId.empty());
        NIMVLETS_CHECK(tab.focusId == NavFocusIdFor(tab.section));
        ProductSection routed = ProductSection::kCollection;
        NIMVLETS_CHECK(NavTargetSection(tab.focusId, routed));
        NIMVLETS_CHECK(routed == tab.section);
    }
    return true;
}

bool TestNavFocusIdForKnownSections() {
    NIMVLETS_CHECK(std::string(NavFocusIdFor(ProductSection::kCollection)) == "nav:collection");
    NIMVLETS_CHECK(std::string(NavFocusIdFor(ProductSection::kShop)) == "nav:shop");
    NIMVLETS_CHECK(std::string(NavFocusIdFor(ProductSection::kSettings)) == "nav:settings");
    return true;
}

// El balance de clics se FORMATEA en la cabecera pura, a partir del
// valor canónico que pasa el caller — así NINGUNA sección puede elegir
// su propio número. (Corrección de QA del owner, Block 10: Settings
// mostraba "0 clicks" hard-codeado en su Render mientras Collection /
// Shop mostraban el balance real.) `DrawSectionHeader` ahora solo dibuja
// este string, sin re-formatear ni recibir un balance suelto.
bool TestHeaderFormatsCanonicalBalance() {
    for (const ProductSection section :
         {ProductSection::kCollection, ProductSection::kShop, ProductSection::kSettings}) {
        NIMVLETS_CHECK(BuildSectionHeaderLayout(800.0f, 40.0f, 0.0f, section, Language::kEn, 0)
                           .clicksText == "0 clicks");
        NIMVLETS_CHECK(BuildSectionHeaderLayout(800.0f, 40.0f, 0.0f, section, Language::kEn, 500)
                           .clicksText == "500 clicks");
        NIMVLETS_CHECK(BuildSectionHeaderLayout(800.0f, 40.0f, 0.0f, section, Language::kEn, 1)
                           .clicksText == "1 click");  // singular
        NIMVLETS_CHECK(BuildSectionHeaderLayout(800.0f, 40.0f, 0.0f, section, Language::kEs, 350)
                           .clicksText == "350 clics");
        NIMVLETS_CHECK(BuildSectionHeaderLayout(800.0f, 40.0f, 0.0f, section, Language::kEs, 1)
                           .clicksText == "1 clic");
    }
    // Agrupación de dígitos: "1 248 clicks" (mismo formateo que el resto
    // del Product UI).
    NIMVLETS_CHECK(BuildSectionHeaderLayout(800.0f, 40.0f, 0.0f, ProductSection::kSettings,
                                            Language::kEn, 1248)
                       .clicksText == "1 248 clicks");
    return true;
}

}  // namespace

void RegisterSectionNavTests(testing::TestRunner& runner) {
    runner.Add("SectionNav/NavTargetSectionMapsAllThreeTabs", TestNavTargetSectionMapsAllThreeTabs);
    runner.Add("SectionNav/NavTargetSectionRejectsNonNavIds", TestNavTargetSectionRejectsNonNavIds);
    runner.Add("SectionNav/NavRoundTripThroughHeaderLayout", TestNavRoundTripThroughHeaderLayout);
    runner.Add("SectionNav/NavFocusIdForKnownSections", TestNavFocusIdForKnownSections);
    runner.Add("SectionNav/HeaderFormatsCanonicalBalance", TestHeaderFormatsCanonicalBalance);
}

}  // namespace nimvlets::tests
