#pragma once

#include <string>
#include <vector>

#include "catalog/PetIdentity.h"
#include "catalog/StarterShopModel.h"
#include "core/Localization.h"
#include "productui/SectionNav.h"
#include "productui/ShopLayout.h"  // ShopTile / ShopHero / helpers de geometría compartidos
#include "productui/UiGeometry.h"

namespace nimvlets::productui {

// El layout PURO del SHOP OCULTO DE STARTERS (Block 10) — el SUBMODO
// contextual de la sección Shop, no una cuarta pestaña de navegación.
//
// Misma jerarquía visual que el Shop browse-first: rejilla de ofertas ->
// hover/foco revela precio/estado -> seleccionar promueve a hero grande
// -> Get -> Cancel / Confirm. REUSA la geometría del Shop
// (ComputeBrowseGrid / LayoutBrowseGrid / LayoutShopRail / LayoutShopHero
// en ShopLayout.h) y su pintado (ShopPaint.h) — sin una segunda copia de
// esa lógica (brief §12).
//
// La diferencia con el Shop público: opera en IDENTIDADES EXACTAS
// ({petId, variantId}) — dos "Frin" (male / female) conviven como
// ofertas distintas —, la cabecera compartida sigue marcando "Shop", y
// hay un back affordance quieto "← Shop" (brief §11). Un poco más
// quieto / intencional que el storefront público.

enum class StarterShopPresentation {
    kBrowse,     // rejilla de ofertas
    kSelected,   // una oferta elegida: hero + rail
    kEmpty,      // se compró la última oferta: estado quieto + "← Shop" (brief §19)
};

struct StarterShopLayout {
    UiRect viewport;
    SectionHeaderLayout header;  // pestaña "Shop" activa (brief §11)
    StarterShopPresentation presentation = StarterShopPresentation::kBrowse;

    // Back affordance "← Shop" (brief §11). focusId "starter:back".
    std::string backText;
    UiRect backAnchor;

    // Encabezado quieto "Starter choices".
    std::string heading;
    UiRect headingAnchor;

    // Estado vacío (todas las ofertas compradas).
    std::string emptyText;
    UiRect emptyAnchor;

    std::vector<ShopTile> tiles;  // kBrowse
    ShopHero hero;                // kSelected
    UiRect dividerRect;           // kSelected
    std::vector<ShopTile> rail;   // kSelected
    UiRect shelfBackground;       // kSelected

    // Orden de tabulación:
    //   kBrowse   -> nav tabs, "starter:back", "starteritem:<id>" por oferta.
    //   kSelected -> nav tabs, "starter:back", controles del hero
    //                (confirmando: "purchase:cancel"/"purchase:confirm";
    //                si no: "get" si asequible), "starteritem:<id>" del rail.
    //   kEmpty    -> nav tabs, "starter:back".
    std::vector<std::string> focusOrder;
    float contentHeight = 0.0f;

    const ShopTile* FindTileByFocusId(const std::string& focusId) const;

    // focusId accionable en (x, y), o "" si ninguno. Incluye las pestañas
    // de nav y "starter:back".
    std::string HitTest(float x, float y) const;
};

struct StarterShopLayoutInput {
    float viewportW = 800.0f;
    float viewportH = 560.0f;
    float scrollY = 0.0f;
    core::Language language = core::Language::kEn;

    // "" => modo BROWSE. Un focusId de oferta
    // ("starteritem:<petId>/<variantId>") que sigue en el modelo => modo
    // SELECTED con esa oferta como hero.
    std::string selectedFocusId;

    // El focusId de la tarjeta bajo el mouse / foco de teclado — revela
    // su info liviana (precio). NO cambia la selección.
    std::string hoverFocusId;

    // true => el hero muestra la confirmación inline. Solo en kSelected +
    // asequible.
    bool confirming = false;
};

// focusId estable de una oferta: "starteritem:<petId>/<variantId>" (para
// un starter normal, "<petId>/"). Disambigua las dos variantes de Frin,
// que el Shop público (keyed por petId) no podría.
std::string StarterOfferFocusId(const catalog::PetIdentity& target);

// Parsea un "starteritem:<petId>/<variantId>" de vuelta a la identidad
// EXACTA. Devuelve {} (petId vacío) si `focusId` no tiene ese prefijo.
catalog::PetIdentity StarterOfferIdentityFromFocusId(const std::string& focusId);

// Etiqueta de variante localizada para el nombre compuesto ("Male" /
// "Female" via kMale/kFemale). "" para un starter normal (variantId
// vacío) o una variante desconocida.
std::string StarterVariantLabel(const std::string& variantId, core::Language lang);

// Construye el layout. Puro y determinista. Todo el texto traducible ya
// viene localizado.
StarterShopLayout BuildStarterShopLayout(
    const catalog::StarterShopModel& model, const StarterShopLayoutInput& in);

}  // namespace nimvlets::productui
