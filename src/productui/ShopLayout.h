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
    // Variante EXACTA para resolver la preview `.nvprev` (Block 10 —
    // Starter Shop de Frin). "" para el Shop público (pets sin variantes).
    std::string variantId;
    std::string displayName;        // nombre propio — nunca traducido (Starter Shop: "Frin · Male")
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
    // Variante EXACTA para la preview `.nvprev` (Block 10 — Starter Shop
    // de Frin). "" para el Shop público.
    std::string variantId;
    std::string displayName;      // nombre propio — nunca traducido (Starter Shop: "Frin · Male")
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

    // Panel enmarcado suave alrededor de TODO el hero (arte + detalle) —
    // la "editorial panel" de la referencia (convergencia DEC-147).
    UiRect heroPanel;

    // Divisor 1: bajo el nombre, ancho de la columna de texto, rombo
    // central en el ACENTO DEL PET (identidad).
    UiRect nameRule;
    // Divisor 2: entre la descripción y el precio/acción, ancho de la
    // columna, rombo central NEUTRO (separa identidad de economía —
    // brief §8). w == 0 si no hay descripción (no se dibuja).
    UiRect detailDividerRect;

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

    // --- Hotspot INVISIBLE del Shop oculto de starters (Block 10, ----
    //     corrección de QA del owner: DEC-137 pasada 2)
    //
    // El owner rechazó la afordancia visible "Starter choices…". El
    // Shop oculto ahora es DE VERDAD oculto: un click primario en una
    // pequeña región INVISIBLE anclada a la esquina INFERIOR DERECHA del
    // Shop público abre el submodo. Sin texto, sin foco, sin Tab stop,
    // sin hover, sin cursor especial, sin dibujarse. Solo tiene efecto
    // cuando `starterHotspotArmed` (== el StarterShopModel tiene >= 1
    // oferta legítima — la elegibilidad sigue siendo autoridad del
    // modelo aguas arriba). NO aparece en `focusOrder` ni en `HitTest`.
    bool starterHotspotArmed = false;
    UiRect starterHotspotRect;  // esquina inf-der, ~48x48 pt, en coords de VIEWPORT (sin scroll)

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
    // las pestañas de navegación de la cabecera. **NUNCA devuelve nada
    // para el hotspot invisible** — ese es una consulta aparte.
    std::string HitTest(float x, float y) const;

    // ¿(x, y) cae en el hotspot INVISIBLE de la esquina inf-der Y el
    // hotspot está armado? Consulta separada de HitTest: el hotspot no
    // es un control con focusId. El caller (ShopView) solo la mira si
    // HitTest ya devolvió "" (zona muerta) — así nunca le roba el click
    // a un control visible.
    bool HitStarterHotspot(float x, float y) const {
        return starterHotspotArmed && starterHotspotRect.Contains(x, y);
    }
};

struct ShopLayoutInput {
    float viewportW = 800.0f;
    float viewportH = 560.0f;
    float scrollY = 0.0f;
    core::Language language = core::Language::kEn;
    // Balance de clics CANÓNICO (de ProductWindow) para la cabecera
    // compartida — ver SectionHeaderLayout::clicksText.
    std::uint64_t clickBalance = 0;

    // "" (o un id que no está en el Shop) => modo BROWSE: no hay hero.
    // Un petId válido => modo SELECTED: ese personaje es el hero.
    std::string selectedPetId;

    // petId de la tarjeta bajo el mouse — revela su info liviana. NO
    // cambia la selección (brief §6).
    std::string hoverPetId;

    // true => el hero muestra la confirmación inline en vez del botón
    // "Get <pet>". Solo tiene efecto en kSelected + kAffordable.
    bool confirming = false;

    // true => ARMAR el hotspot INVISIBLE de la esquina inf-der (Block 10,
    // corrección de QA del owner). El caller (ShopView) lo pone en true
    // SOLO cuando el StarterShopModel oculto tiene >= 1 oferta legítima.
    // No dibuja nada; sin él, un click en la esquina es no-op total.
    bool starterHotspotArmed = false;
};

// Construye el layout del Shop. Puro y determinista. Todo el texto
// traducible ya viene localizado.
ShopLayout BuildShopLayout(const catalog::ShopModel& model, const ShopLayoutInput& in);

// --- Helpers de layout compartidos con el Starter Shop (Block 10) ----
//
// El Starter Shop oculto reusa la MISMA geometría (rejilla de browse +
// hero + rail): solo cambia el MODELO (ofertas de identidad EXACTA en
// vez de filas por pet lógico). Estos helpers son la única copia de esa
// geometría — ver productui/StarterShopLayout.{h,cpp} y DEC-137. Puros y
// deterministas.

struct BrowseGridMetrics {
    int cols = 1;
    int rows = 1;
    float tileW = 0.0f;
    float artSize = 0.0f;
    float tileH = 0.0f;
    float blockH = 0.0f;  // alto total de la rejilla (sin encabezado)
};

BrowseGridMetrics ComputeBrowseGrid(int n, float contentW);

// Coloca `tiles` en la rejilla `m`, cada fila centrada en `viewportW`.
// Devuelve el bottom de la última fila.
float LayoutBrowseGrid(
    std::vector<ShopTile>& tiles, float viewportW, const BrowseGridMetrics& m, float gridTop);

// Coloca el rail compacto bajo un hero. `hoverFocusId` (el focusId de la
// tarjeta, no un petId — el Starter Shop tiene dos "frin") lleva el
// micro-lift de hover. Devuelve el bottom de la última fila.
float LayoutShopRail(
    std::vector<ShopTile>& rail, float viewportW, float contentW, float railTop,
    const std::string& hoverFocusId);

// Contenido de un hero ya resuelto: la especie/descripción/nombre
// compuesto los arma el caller (para el Starter Shop de Frin,
// `displayName` == "Frin · Male"; `petId`/`variantId` siguen exactos).
struct ShopHeroContent {
    std::string petId;         // acento + preview base + editorial
    std::string variantId;     // preview EXACTA ("" salvo Starter Shop de Frin)
    std::string displayName;   // ya compuesto
    std::string speciesText;   // "" si no hay
    std::string descriptionText;
    catalog::ShopItemStatus status = catalog::ShopItemStatus::kAffordable;
    catalog::PetEntitlement entitlementTarget;
    std::uint64_t priceClicks = 0;
    std::uint64_t clicksShort = 0;
};

// Rellena `h` (hero stage / arte / nombre / regla / especie / descr /
// precio / acción / confirmación) y agrega a `focusOrder` los ids
// accionables ("get" si asequible sin confirmar; "purchase:cancel" +
// "purchase:confirm" si confirmando). Devuelve el bottom del hero.
float LayoutShopHero(
    ShopHero& h, std::vector<std::string>& focusOrder, const ShopHeroContent& c,
    float headerBodyTop, float contentW, core::Language language, bool confirming);

// Acota `scrollY` a [0, max(0, contentHeight - viewportH)] (idéntico a
// productui::ClampScroll de la Collection — se reexpone acá para no
// obligar a la vista del Shop a incluir CollectionLayout.h).
float ClampShopScroll(float scrollY, float contentHeight, float viewportH);

}  // namespace nimvlets::productui
