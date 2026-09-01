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

// El layout PURO del Shop: convierte un catalog::ShopModel + tamaño de
// viewport + scroll + selección + estado de confirmación en widgets
// posicionados, el orden de tabulación, y un hit-test por punto.
// Métricas en PUNTOS lógicos. Determinista: mismas entradas -> mismo
// resultado, sin SDL, sin medición de texto real.
//
// Block 09C — BROWSE-FIRST (DEC-135, supersede el estado de entrada
// hero-first de DEC-127). El Shop ya NO abre mostrando un hero gigante:
// abre en modo BROWSE — "un lugar chico donde miro personajes que
// podría conseguir". Solo tras SELECCIONAR un personaje ese personaje
// se promueve a un hero grande con descripción / precio / acción de
// compra, y la estantería de browse baja a un rail compacto. La
// jerarquía es:
//
//   BROWSE (rejilla de personajes) -> HOVER (info liviana: precio /
//   propiedad) -> SELECCIÓN (hero grande + detalle + compra).
//
// hovered != selected != confirming — tres cosas distintas, nunca una
// sola variable (brief §13). Comparte la CABECERA (título + balance +
// pestañas) con la Collection vía SectionNav, y reusa las previews
// `.nvprev` (el Shop NUNCA abre un `.nvpack`). Sin card chrome pesado,
// sin sidebar, sin grid de e-commerce (brief §3).

// Dos presentaciones. `confirming` es un sub-estado de kSelected.
enum class ShopPresentation {
    kBrowse,    // ninguna selección: la estantería de personajes es todo el contenido
    kSelected,  // un personaje elegido: hero grande + rail compacto de la estantería
};

// --- Personaje en la estantería ----------------------------------------
//
// Se usa en las dos presentaciones: como tarjeta grande de la rejilla de
// browse, y como tarjeta compacta del rail bajo el hero. La vista elige
// el tamaño del arte / tipografía según `ShopLayout::presentation`.

struct ShopTile {
    std::string petId;
    std::string displayName;        // nombre propio — nunca traducido
    catalog::ShopItemStatus status = catalog::ShopItemStatus::kAffordable;
    // Info contextual liviana revelada al hacer hover / foco (brief §6):
    // precio formateado ("300 clicks" / "300 clics"), o el estado de
    // propiedad ("In your collection"). Nunca un precio falso para lo
    // ya poseído.
    std::string revealText;
    UiColor accentLine;
    UiColor pedestalTint;

    // Solo en el rail (kSelected): marca la tarjeta cuyo personaje es el
    // hero abierto ahora mismo.
    bool selected = false;

    UiRect cell;          // zona clickeable + wash de hover / anillo de foco
    UiRect art;           // caja del arte (la vista resuelve la preview `.nvprev`)
    UiRect name;          // ancla del nombre (centrada en x)
    UiRect revealAnchor;  // ancla de la línea revelada (centrada en x); alto SIEMPRE reservado

    std::string focusId;  // "shopitem:<petId>"
};

// --- Confirmación de compra inline (brief §8) --------------------------

struct PurchaseConfirm {
    bool visible = false;
    std::string prompt;   // localizado: "Spend 300 clicks to add Nidir to your collection?"
    UiRect promptAnchor;  // ancla IZQUIERDA del texto (puede envolver en 2 líneas)

    UiRect cancelButton;
    std::string cancelLabel;    // "Cancel" / "Cancelar"
    std::string cancelFocusId;  // "purchase:cancel"

    UiRect confirmButton;
    std::string confirmLabel;    // "Confirm" / "Confirmar"
    std::string confirmFocusId;  // "purchase:confirm"
};

// --- Hero: el personaje del Shop seleccionado -------------------------
//
// Solo existe en ShopPresentation::kSelected. Es el "detalle": arte
// grande, especie, descripción editorial, precio y acción de compra.
// Debe sentirse SUSTANCIALMENTE más prominente que una tarjeta de
// browse (brief §7).

struct ShopHero {
    std::string petId;
    std::string displayName;      // nombre propio — nunca traducido
    std::string speciesText;      // etiqueta de especie, "" si no hay
    std::string descriptionText;  // línea(s) de personalidad, "" si no hay
    catalog::ShopItemStatus status = catalog::ShopItemStatus::kAffordable;
    // Qué identidad de catálogo se compra (de ShopItem::entitlementTarget).
    // Para los pets sin variantes es {petId, ""}. La vista la emite como
    // PurchaseRequest; src/app se la pasa a EvaluatePurchase.
    catalog::PetEntitlement entitlementTarget;
    PetAccent accent;

    std::uint64_t priceClicks = 0;
    std::string priceText;   // "300 clicks" / "300 clics" (visible salvo kOwned)

    // Línea de estado: "In your collection" (kOwned) o "Need N more
    // clicks" (kInsufficientBalance). Vacía para kAffordable (solo el
    // botón "Get <pet>"). status/botón mutuamente excluyentes con la
    // confirmación.
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
    ShopPresentation presentation = ShopPresentation::kBrowse;

    // --- kBrowse -----------------------------------------------------
    std::string browseHeading;    // localizado: "Nimvlets you can meet"
    UiRect browseHeadingAnchor;   // ancla centrada
    std::vector<ShopTile> tiles;  // la rejilla de browse (vacía en kSelected)

    // --- kSelected -------------------------------------------------
    ShopHero hero;
    UiRect dividerRect;          // hairline entre el hero y el rail
    std::vector<ShopTile> rail;  // estantería compacta, TODOS los pets del Shop

    // Segundo plano cálido más profundo bajo el divisor (solo kSelected;
    // w/h == 0 en kBrowse — el modo browse respira sobre el fondo base).
    UiRect shelfBackground;

    // --- Shop vacío ------------------------------------------------
    bool empty = false;         // el Shop no tiene NINGÚN pet público
    std::string emptyText;      // localizado, quieto y no alarmante
    UiRect emptyAnchor;         // ancla centrada

    // Orden de tabulación:
    //   kBrowse    -> pestañas de nav, luego "shopitem:<petId>" por tarjeta.
    //   kSelected  -> pestañas de nav, luego los controles del hero
    //                 (confirmando: cancel/confirm; si no: "get" si está
    //                 habilitado), luego "shopitem:<petId>" por tarjeta del rail.
    std::vector<std::string> focusOrder;

    float contentHeight = 0.0f;

    // Busca por petId en `tiles` (kBrowse) o en `rail` (kSelected).
    const ShopTile* FindTile(const std::string& petId) const;

    // focusId del widget accionable en (x, y), o "" si ninguno. Incluye
    // las pestañas de navegación de la cabecera.
    std::string HitTest(float x, float y) const;
};

struct ShopLayoutInput {
    float viewportW = 800.0f;
    float viewportH = 560.0f;
    float scrollY = 0.0f;
    core::Language language = core::Language::kEn;

    // "" (o un id que no está en el Shop) => modo BROWSE: no hay hero.
    // Un petId válido => modo SELECTED: ese personaje es el hero.
    std::string selectedPetId;

    // petId de la tarjeta bajo el mouse — revela su info liviana. NO
    // cambia la selección (brief §6).
    std::string hoverPetId;

    // true => el hero muestra la confirmación inline en vez del botón
    // "Get <pet>". Solo tiene efecto en kSelected + kAffordable.
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
