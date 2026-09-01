#include "productui/SectionNav.h"

#include "core/Localization.h"
#include "productui/Format.h"

namespace nimvlets::productui {

using core::Language;
using core::Localized;
using core::StringKey;

namespace {

// Métricas en PUNTOS lógicos, elegidas para que el cuerpo de la
// Collection empiece casi donde empezaba en Block 06 (hero art ~112):
// la fila de pestañas ocupa el lugar del viejo "Collection / Your
// companions".
constexpr float kTitleTop = 30.0f;
constexpr float kTitleH = 24.0f;
constexpr float kTabsTop = 66.0f;
constexpr float kTabsH = 22.0f;
constexpr float kTabGap = 26.0f;       // separación entre "Collection" y "Shop"
constexpr float kTabPadX = 4.0f;       // aire clickeable a cada lado del texto
constexpr float kTabHitPadY = 7.0f;
constexpr float kBelowTabs = 22.0f;    // pestañas -> cuerpo de la sección
constexpr float kApproxCharW = 8.2f;   // ancho aprox. por carácter (la vista mide fino)

float ApproxWidth(const std::string& label) {
    return static_cast<float>(label.size()) * kApproxCharW;
}

}  // namespace

const char* SectionLabel(ProductSection section, Language lang) {
    switch (section) {
        case ProductSection::kCollection:
            return Localized(StringKey::kCollection, lang);
        case ProductSection::kShop:
            return Localized(StringKey::kShop, lang);
        case ProductSection::kSettings:
            return Localized(StringKey::kSettings, lang);
    }
    return "";
}

const char* NavFocusIdFor(ProductSection section) {
    switch (section) {
        case ProductSection::kCollection:
            return "nav:collection";
        case ProductSection::kShop:
            return "nav:shop";
        case ProductSection::kSettings:
            return "nav:settings";
    }
    return "";
}

bool NavTargetSection(const std::string& focusId, ProductSection& outSection) {
    for (const ProductSection section :
         {ProductSection::kCollection, ProductSection::kShop, ProductSection::kSettings}) {
        if (focusId == NavFocusIdFor(section)) {
            outSection = section;
            return true;
        }
    }
    return false;
}

SectionHeaderLayout BuildSectionHeaderLayout(
    float viewportW, float marginX, float scrollY, ProductSection active, Language lang,
    std::uint64_t clickBalance) {
    SectionHeaderLayout out;
    const float contentW = viewportW - 2.0f * marginX;

    out.titleAnchor = UiRect{marginX, kTitleTop - scrollY, contentW, kTitleH};
    out.clicksAnchorRight = UiRect{viewportW - marginX, kTitleTop - scrollY, 0.0f, kTitleH};
    // Formateado UNA vez, acá, a partir del balance canónico — todas las
    // secciones dibujan este mismo string.
    out.clicksText = FormatClickCount(clickBalance, lang);

    const float tabY = kTabsTop - scrollY;
    float x = marginX;
    for (const ProductSection section :
         {ProductSection::kCollection, ProductSection::kShop, ProductSection::kSettings}) {
        SectionTab tab;
        tab.section = section;
        tab.label = SectionLabel(section, lang);
        tab.active = (section == active);
        tab.focusId = NavFocusIdFor(section);

        const float w = ApproxWidth(tab.label);
        tab.labelAnchor = UiRect{x, tabY, w, kTabsH};
        tab.hitRect = UiRect{x - kTabPadX, tabY - kTabHitPadY, w + 2.0f * kTabPadX, kTabsH + 2.0f * kTabHitPadY};
        tab.underline = tab.active ? UiRect{x, tabY + kTabsH - 2.0f, w, 2.0f} : UiRect{x, tabY + kTabsH - 2.0f, 0.0f, 2.0f};
        out.tabs.push_back(tab);

        x += w + kTabGap;
    }

    out.bodyTop = kTabsTop + kTabsH + kBelowTabs - scrollY;
    return out;
}

}  // namespace nimvlets::productui
