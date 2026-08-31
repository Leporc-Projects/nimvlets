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
    const SectionHeaderLayout header =
        BuildSectionHeaderLayout(800.0f, 40.0f, 0.0f, ProductSection::kCollection, Language::kEn);
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

}  // namespace

void RegisterSectionNavTests(testing::TestRunner& runner) {
    runner.Add("SectionNav/NavTargetSectionMapsAllThreeTabs", TestNavTargetSectionMapsAllThreeTabs);
    runner.Add("SectionNav/NavTargetSectionRejectsNonNavIds", TestNavTargetSectionRejectsNonNavIds);
    runner.Add("SectionNav/NavRoundTripThroughHeaderLayout", TestNavRoundTripThroughHeaderLayout);
    runner.Add("SectionNav/NavFocusIdForKnownSections", TestNavFocusIdForKnownSections);
}

}  // namespace nimvlets::tests
