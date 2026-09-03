#include "SectionNavTest.h"

#include <cmath>
#include <string>

#include "core/Localization.h"
#include "productui/SectionNav.h"

using nimvlets::core::Language;
using nimvlets::productui::BuildSectionHeaderLayout;
using nimvlets::productui::ComputeWalletPill;
using nimvlets::productui::NavFocusIdFor;
using nimvlets::productui::NavTargetSection;
using nimvlets::productui::ProductSection;
using nimvlets::productui::SectionHeaderLayout;
using nimvlets::productui::SectionTab;
using nimvlets::productui::WalletPillMetrics;

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

// Block 12A: la geometría de la pill del wallet es pura y monótona en el
// ancho del texto — la vista y el test consumen la MISMA función, no
// pueden divergir. spark y texto quedan DENTRO de la pill, con aire.
bool TestWalletPillMetricsAreSaneAndMonotonic() {
    const WalletPillMetrics zero = ComputeWalletPill(0.0f);
    const WalletPillMetrics small = ComputeWalletPill(40.0f);
    const WalletPillMetrics big = ComputeWalletPill(180.0f);  // ~"1 000 000 clics"

    NIMVLETS_CHECK(zero.height > 0.0f);
    NIMVLETS_CHECK(zero.width > 0.0f);
    // El centro del spark y el ancla del texto están dentro de la pill,
    // en orden, con aire a la izquierda.
    NIMVLETS_CHECK(zero.sparkCenterX > 0.0f && zero.sparkCenterX < zero.textLeftX);
    NIMVLETS_CHECK(zero.textLeftX < zero.width);
    // Monótona y "ancho crece 1:1 con el texto".
    NIMVLETS_CHECK(small.width > zero.width);
    NIMVLETS_CHECK(big.width > small.width);
    NIMVLETS_CHECK(std::abs((big.width - small.width) - (180.0f - 40.0f)) < 1e-3f);
    // El hueco a la derecha del texto (padding) es igual para cualquier
    // largo: width - textLeftX - textWidth == padX constante.
    NIMVLETS_CHECK(std::abs((small.width - small.textLeftX - 40.0f) -
                            (big.width - big.textLeftX - 180.0f)) < 1e-3f);
    return true;
}

// La cabecera compartida no rompe su layout con un balance grande en ES
// ni en el mínimo / un viewport ancho (brief §32): el ancla derecha del
// balance queda dentro del contenido y las pestañas caben a su
// izquierda.
bool TestHeaderHoldsAtResizeAndLargeBalance() {
    for (const float w : {600.0f, 800.0f, 1200.0f}) {
        for (const Language lang : {Language::kEn, Language::kEs}) {
            const SectionHeaderLayout h = BuildSectionHeaderLayout(
                w, 40.0f, 0.0f, ProductSection::kShop, lang, /*clickBalance=*/1000000);
            NIMVLETS_CHECK(h.tabs.size() == 3);
            // El ancla derecha del balance está en el margen derecho.
            NIMVLETS_CHECK(std::abs(h.clicksAnchorRight.x - (w - 40.0f)) < 1e-3f);
            // La última pestaña termina bien a la izquierda del margen
            // derecho — hay lugar para la pill.
            const SectionTab& last = h.tabs.back();
            NIMVLETS_CHECK(last.hitRect.Right() < w - 40.0f);
            // La pill estimada (texto ~ 12 px/char a 13 pt) entra dentro
            // del ancho de contenido y no pisa el wordmark (~120 pt).
            const WalletPillMetrics pill =
                ComputeWalletPill(static_cast<float>(h.clicksText.size()) * 9.0f);
            NIMVLETS_CHECK(pill.width < (w - 2.0f * 40.0f));
            NIMVLETS_CHECK((h.clicksAnchorRight.x - pill.width) > (40.0f + 120.0f));
        }
    }
    return true;
}

// Convergencia DEC-147: ReflowNavTabs coloca las pestañas con anchos
// MEDIDOS (serif) — separadores y regla del indicador activo precisos —
// y centra el bloque de nav si entra. Con `w = {0,0,0}` es un no-op
// (sigue valiendo la aproximación).
bool TestReflowNavTabsNoOpWithoutMeasuredWidths() {
    const SectionHeaderLayout base = BuildSectionHeaderLayout(
        800.0f, 40.0f, 0.0f, ProductSection::kShop, Language::kEn, 0);
    SectionHeaderLayout h = base;
    const float zeros[3] = {0.0f, 0.0f, 0.0f};
    ReflowNavTabs(h, zeros, 800.0f, 40.0f);
    for (std::size_t i = 0; i < 3; ++i) {
        NIMVLETS_CHECK(std::abs(h.tabs[i].labelAnchor.x - base.tabs[i].labelAnchor.x) < 1e-3f);
        NIMVLETS_CHECK(std::abs(h.tabs[i].hitRect.w - base.tabs[i].hitRect.w) < 1e-3f);
    }
    return true;
}

bool TestReflowNavTabsUsesMeasuredWidthsAndCenters() {
    SectionHeaderLayout h = BuildSectionHeaderLayout(
        800.0f, 40.0f, 0.0f, ProductSection::kShop, Language::kEn, 0);
    const float w[3] = {92.0f, 46.0f, 68.0f};  // anchos medidos ficticios
    ReflowNavTabs(h, w, 800.0f, 40.0f);

    // Cada pestaña adopta su ancho medido.
    for (std::size_t i = 0; i < 3; ++i) {
        NIMVLETS_CHECK(std::abs(h.tabs[i].labelAnchor.w - w[i]) < 1e-3f);
    }
    // El bloque (w0 + w1 + w2 + 2*gap) queda centrado en el ancho de
    // contenido: el centro del bloque coincide con el centro del área.
    const float blockLeft = h.tabs[0].labelAnchor.x;
    const float blockRight = h.tabs[2].labelAnchor.Right();
    const float areaCenter = 40.0f + (800.0f - 80.0f) * 0.5f;
    NIMVLETS_CHECK(std::abs(0.5f * (blockLeft + blockRight) - areaCenter) < 1.0f);
    // Las pestañas no se solapan y van en orden.
    NIMVLETS_CHECK(h.tabs[0].labelAnchor.Right() < h.tabs[1].labelAnchor.x);
    NIMVLETS_CHECK(h.tabs[1].labelAnchor.Right() < h.tabs[2].labelAnchor.x);
    // El indicador activo (Shop) sigue el ancho medido de esa pestaña.
    NIMVLETS_CHECK(std::abs(h.tabs[1].underline.w - w[1]) < 1e-3f);
    NIMVLETS_CHECK(h.tabs[0].underline.w == 0.0f && h.tabs[2].underline.w == 0.0f);
    // Ida y vuelta del ruteo intacto tras el reflow.
    for (const SectionTab& tab : h.tabs) {
        ProductSection routed = ProductSection::kCollection;
        NIMVLETS_CHECK(NavTargetSection(tab.focusId, routed) && routed == tab.section);
    }
    return true;
}

bool TestReflowNavTabsClampsToMarginWhenTooWide() {
    SectionHeaderLayout h = BuildSectionHeaderLayout(
        520.0f, 40.0f, 0.0f, ProductSection::kCollection, Language::kEs, 0);
    const float w[3] = {150.0f, 120.0f, 130.0f};  // no entra centrado a 520
    ReflowNavTabs(h, w, 520.0f, 40.0f);
    // Anclado al margen izquierdo (no negativo, no fuera de pantalla).
    NIMVLETS_CHECK(std::abs(h.tabs[0].labelAnchor.x - 40.0f) < 1e-3f);
    return true;
}

}  // namespace

void RegisterSectionNavTests(testing::TestRunner& runner) {
    runner.Add("SectionNav/NavTargetSectionMapsAllThreeTabs", TestNavTargetSectionMapsAllThreeTabs);
    runner.Add("SectionNav/NavTargetSectionRejectsNonNavIds", TestNavTargetSectionRejectsNonNavIds);
    runner.Add("SectionNav/NavRoundTripThroughHeaderLayout", TestNavRoundTripThroughHeaderLayout);
    runner.Add("SectionNav/NavFocusIdForKnownSections", TestNavFocusIdForKnownSections);
    runner.Add("SectionNav/HeaderFormatsCanonicalBalance", TestHeaderFormatsCanonicalBalance);
    runner.Add("SectionNav/WalletPillMetricsAreSaneAndMonotonic",
               TestWalletPillMetricsAreSaneAndMonotonic);
    runner.Add("SectionNav/ReflowNavTabsNoOpWithoutMeasuredWidths",
               TestReflowNavTabsNoOpWithoutMeasuredWidths);
    runner.Add("SectionNav/ReflowNavTabsUsesMeasuredWidthsAndCenters",
               TestReflowNavTabsUsesMeasuredWidthsAndCenters);
    runner.Add("SectionNav/ReflowNavTabsClampsToMarginWhenTooWide",
               TestReflowNavTabsClampsToMarginWhenTooWide);
    runner.Add("SectionNav/HeaderHoldsAtResizeAndLargeBalance",
               TestHeaderHoldsAtResizeAndLargeBalance);
}

}  // namespace nimvlets::tests
