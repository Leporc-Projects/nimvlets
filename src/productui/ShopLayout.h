#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "catalog/ShopModel.h"
#include "core/Localization.h"
#include "productui/PetAccent.h"
#include "productui/SectionNav.h"
#include "productui/UiColor.h"
#include "productui/UiGeometry.h"

namespace nimvlets::productui {

// El layout PURO del Shop (Block 07): convierte un catalog::ShopModel +
// tamaño de viewport + scroll + selección + estado de confirmación en
// widgets posicionados, el orden de tabulación, y un hit-test por punto.
// Métricas en PUNTOS lógicos. Determinista: mismas entradas -> mismo
// resultado, sin SDL, sin medición de texto real.
//
// Comparte la CABECERA (título + balance + pestañas Collection/Shop) con
// la Collection vía SectionNav — así navegar entre secciones se ve
// idéntico. Reusa la misma composición hero + gallery, el mismo acento
// de identidad por pet, y las mismas previews `.nvprev` (la vista las
// resuelve). El Shop "se siente como conocer a otro Nimvlet", no una
// plantilla de tienda (brief §8): sin card chrome pesado, sin grid.

// --- Gallery: los otros pets del Shop (no el hero) -------------------

struct ShopGalleryItem {
    std::string petId;
    std::string displayName;        // nombre propio — nunca traducido
    catalog::ShopItemStatus status = catalog::ShopItemStatus::kAffordable;
    std::string secondaryText;      // localizado: precio, o "In your collection"
    UiColor accentLine;
    UiColor pedestalTint;

    UiRect cell;     // zona clickeable + wash de hover/foco
    UiRect art;      // caja del arte (chica)
    UiRect name;     // ancla del nombre (centrada en x)
    UiRect secondary_;  // ancla de la línea secundaria (centrada en x)

    std::string focusId;  // "shopitem:<petId>"
};

// --- Confirmación de compra inline (brief §12) ----------------------

struct PurchaseConfirm {
    bool visible = false;
    std::string prompt;   // localizado: "Spend 300 clicks to add Nidir to your collection?"
    UiRect promptAnchor;  // ancla IZQUIERDA del texto (puede envolver en 2 líneas)

    UiRect cancelButton;
    std::string cancelLabel;   // "Cancel" / "Cancelar"
    std::string cancelFocusId;  // "purchase:cancel"

    UiRect confirmButton;
    std::string confirmLabel;   // "Confirm" / "Confirmar"
    std::string confirmFocusId;  // "purchase:confirm"
};

// --- Hero: el Nimvlet del Shop seleccionado ------------------------

struct ShopHero {
    std::string petId;
    std::string displayName;      // nombre propio — nunca traducido
    std::string speciesText;      // etiqueta de especie, "" si no hay
    std::string descriptionText;  // línea(s) de personalidad, "" si no hay
    catalog::ShopItemStatus status = catalog::ShopItemStatus::kAffordable;
    PetAccent accent;

    std::uint64_t priceClicks = 0;
    std::string priceText;   // "300 clicks" / "300 clics" (siempre visible salvo kOwned)

    // Línea de estado: "In your collection" (kOwned) o "Need N more
    // clicks" (kInsufficientBalance). Vacía para kAffordable (solo el
    // botón "Get <pet>"). status/botón mutuamente excluyentes con la
    // confirmación (ver `confirm`).
    std::string statusText;
    bool showStatusLine = false;

    // Acción primaria. Solo visible/habilitada si kAffordable y NO se
    // está confirmando.
    UiRect actionButton;
    std::string actionLabel;   // "Get Nidir" / "Obtener Nidir"
    bool actionEnabled = false;
    std::string actionFocusId;  // "get"

    PurchaseConfirm confirm;

    // Hero stage (idéntico concepto que la Collection).
    UiRect stagePrimary;
    UiRect stageSecondary;
    UiRect art;
    UiRect nameRule;

    UiRect nameAnchor;
    UiRect speciesAnchor;
    UiRect descriptionAnchor;  // .w = ancho de envoltura; .h = alto reservado
    UiRect priceAnchor;
    UiRect statusAnchor;
};

struct ShopLayout {
    UiRect viewport;
    SectionHeaderLayout header;

    ShopHero hero;
    std::vector<ShopGalleryItem> gallery;

    UiRect dividerRect;
    UiRect galleryShelf;

    // Orden de tabulación: pestañas de nav, luego los controles del hero
    // (confirmar: cancel/confirm; si no: "get" si está habilitado),
    // luego "shopitem:<petId>" por cada entrada de la gallery.
    std::vector<std::string> focusOrder;

    float contentHeight = 0.0f;

    // true si el Shop no tiene NINGÚN pet público (no debería pasar con
    // el catálogo real, pero la vista lo maneja con un mensaje).
    bool empty = true;

    const ShopGalleryItem* FindGalleryItem(const std::string& petId) const;

    // focusId del widget accionable en (x, y), o "" si ninguno. Incluye
    // las pestañas de navegación de la cabecera.
    std::string HitTest(float x, float y) const;
};

struct ShopLayoutInput {
    float viewportW = 800.0f;
    float viewportH = 560.0f;
    float scrollY = 0.0f;
    core::Language language = core::Language::kEn;

    // "" => el hero es el primer pet del Shop.
    std::string selectedPetId;

    // petId de la entrada de gallery bajo el mouse — micro-lift de 2pt.
    std::string hoverPetId;

    // true => el hero muestra la confirmación inline en vez del botón
    // "Get <pet>" (brief §12). Solo tiene efecto si el hero es
    // kAffordable.
    bool confirming = false;
};

// Construye el layout del Shop. Puro y determinista. Todo el texto
// traducible ya viene localizado.
ShopLayout BuildShopLayout(const catalog::ShopModel& model, const ShopLayoutInput& in);

// Acota `scrollY` a [0, max(0, contentHeight - viewportH)] (idéntico a
// productui::ClampScroll de la Collection — se reexpone acá para no
// obligar a la vista del Shop a incluir CollectionLayout.h).
float ClampShopScroll(float scrollY, float contentHeight, float viewportH);

}  // namespace nimvlets::productui
