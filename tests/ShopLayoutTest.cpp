#include "ShopLayoutTest.h"

#include "catalog/ShopModel.h"
#include "core/Localization.h"
#include "productui/ShopLayout.h"

#include <string>
#include <vector>

using nimvlets::catalog::BuildShopModel;
using nimvlets::catalog::CatalogEntry;
using nimvlets::catalog::PetCatalog;
using nimvlets::catalog::PetEntitlement;
using nimvlets::catalog::PetIdentity;
using nimvlets::catalog::ShopModel;
using nimvlets::core::Language;
using nimvlets::productui::BuildShopLayout;
using nimvlets::productui::ShopLayout;
using nimvlets::productui::ShopLayoutInput;
using nimvlets::productui::ShopPresentation;
using nimvlets::productui::ShopTile;
using nimvlets::productui::UiRect;

using Ents = std::vector<PetEntitlement>;

namespace nimvlets::tests {

namespace {

// Catálogo estilo dev: Bunny (público, 120), Nidir (público, 300), Frin
// male/female (NO público) — el mismo que Block 07.
PetCatalog MakeShopCatalog() {
    std::vector<CatalogEntry> e;
    CatalogEntry bunny;
    bunny.identity = PetIdentity{"bunny", ""};
    bunny.displayName = "Bunny";
    bunny.packPath = "b.nvpack";
    bunny.isDefault = true;
    bunny.priceClicks = 120;
    bunny.publiclyPurchasable = true;
    e.push_back(bunny);
    CatalogEntry nidir;
    nidir.identity = PetIdentity{"nidir", ""};
    nidir.displayName = "Nidir";
    nidir.packPath = "n.nvpack";
    nidir.priceClicks = 300;
    nidir.publiclyPurchasable = true;
    e.push_back(nidir);
    CatalogEntry fm;
    fm.identity = PetIdentity{"frin", "male"};
    fm.displayName = "Frin";
    fm.packPath = "fm.nvpack";
    e.push_back(fm);
    CatalogEntry ff;
    ff.identity = PetIdentity{"frin", "female"};
    ff.displayName = "Frin";
    ff.packPath = "ff.nvpack";
    e.push_back(ff);
    return PetCatalog(std::move(e));
}

// SINTÉTICO (solo tests de layout, jamás en el catálogo shipeado): `n`
// pets públicamente comprables "shop0".."shop{n-1}", precios
// ascendentes, para ejercer 1/2/4/8 personajes (brief §17/§22-A).
PetCatalog MakeSyntheticShopCatalog(int n) {
    std::vector<CatalogEntry> e;
    for (int i = 0; i < n; ++i) {
        CatalogEntry c;
        c.identity = PetIdentity{"shop" + std::to_string(i), ""};
        c.displayName = "Shop" + std::to_string(i);
        c.packPath = "s" + std::to_string(i) + ".nvpack";
        c.isDefault = (i == 0);
        c.priceClicks = static_cast<std::uint64_t>(100 * (i + 1));
        c.publiclyPurchasable = true;
        e.push_back(c);
    }
    return PetCatalog(std::move(e));
}

PetEntitlement NoVar(const std::string& p) { return PetEntitlement{p, ""}; }

bool Has(const std::vector<std::string>& v, const std::string& s) {
    for (const auto& x : v) {
        if (x == s) return true;
    }
    return false;
}

bool RectsOverlap(const UiRect& a, const UiRect& b) {
    // Bordes tocándose no cuenta como solape.
    return a.x < b.Right() - 0.01f && b.x < a.Right() - 0.01f && a.y < b.Bottom() - 0.01f &&
           b.y < a.Bottom() - 0.01f;
}

// Ninguna tarjeta se solapa con otra.
bool NoTileOverlap(const std::vector<ShopTile>& tiles) {
    for (std::size_t i = 0; i < tiles.size(); ++i) {
        for (std::size_t j = i + 1; j < tiles.size(); ++j) {
            if (RectsOverlap(tiles[i].cell, tiles[j].cell)) {
                return false;
            }
        }
    }
    return true;
}

// Toda tarjeta dentro de [0, viewportW] en x y [bodyTop, contentHeight]
// en y — sin hit-boxes fuera del contenido (brief §16).
bool TilesWithinContent(const ShopLayout& l, float viewportW) {
    const float top = l.header.bodyTop - 1.0f;
    for (const ShopTile& t : l.tiles) {
        if (t.cell.x < -0.01f || t.cell.Right() > viewportW + 0.01f) return false;
        if (t.cell.y < top || t.cell.Bottom() > l.contentHeight + 1.0f) return false;
        // El arte y el nombre viven dentro de la celda.
        if (t.art.x < t.cell.x - 0.01f || t.art.Right() > t.cell.Right() + 0.01f) return false;
        // Espacio para el nombre: la celda es más ancha que el arte.
        if (t.cell.w < t.art.w) return false;
    }
    return true;
}

ShopLayout Layout(std::uint64_t balance, const Ents& owned, const std::string& selected = "",
                  bool confirming = false, Language lang = Language::kEn,
                  const std::string& hover = "", float w = 800.0f, float h = 560.0f) {
    const auto model = BuildShopModel(MakeShopCatalog(), balance, owned);
    ShopLayoutInput in;
    in.viewportW = w;
    in.viewportH = h;
    in.language = lang;
    in.selectedPetId = selected;
    in.hoverPetId = hover;
    in.confirming = confirming;
    return BuildShopLayout(model, in);
}

ShopLayout SyntheticLayout(int count, const std::string& selected = "", float w = 800.0f,
                           float h = 560.0f, float scrollY = 0.0f) {
    const auto model = BuildShopModel(MakeSyntheticShopCatalog(count), 1000, {});
    ShopLayoutInput in;
    in.viewportW = w;
    in.viewportH = h;
    in.scrollY = scrollY;
    in.selectedPetId = selected;
    return BuildShopLayout(model, in);
}

// ============================ HEADER =================================

bool TestHeaderShopTabActive() {
    const ShopLayout es = Layout(0, {}, "", false, Language::kEs);
    NIMVLETS_CHECK(es.header.tabs.size() == 3);
    NIMVLETS_CHECK(es.header.tabs[0].label == "Colección" && !es.header.tabs[0].active);
    NIMVLETS_CHECK(es.header.tabs[1].label == "Tienda" && es.header.tabs[1].active);
    NIMVLETS_CHECK(es.header.tabs[2].label == "Ajustes" && !es.header.tabs[2].active);
    NIMVLETS_CHECK(Has(es.focusOrder, "nav:collection"));
    NIMVLETS_CHECK(Has(es.focusOrder, "nav:shop"));
    NIMVLETS_CHECK(Has(es.focusOrder, "nav:settings"));
    return true;
}

// ===================== A. BROWSE-FIRST / LAYOUT =====================

// Sin selección => modo BROWSE: NO hay hero gigante; la estantería es
// todo el contenido. Ni "get" ni confirmación en el focus order.
bool TestDefaultIsBrowseNoHero() {
    const ShopLayout l = Layout(1000, {});
    NIMVLETS_CHECK(l.presentation == ShopPresentation::kBrowse);
    NIMVLETS_CHECK(l.hero.petId.empty());
    NIMVLETS_CHECK(l.rail.empty());
    NIMVLETS_CHECK(l.tiles.size() == 2);  // Bunny + Nidir; Frin ausente
    NIMVLETS_CHECK(l.tiles[0].petId == "bunny");
    NIMVLETS_CHECK(l.tiles[1].petId == "nidir");
    NIMVLETS_CHECK(l.FindTile("frin") == nullptr);
    NIMVLETS_CHECK(!Has(l.focusOrder, "get"));
    NIMVLETS_CHECK(!Has(l.focusOrder, "purchase:confirm"));
    NIMVLETS_CHECK(!l.browseHeading.empty());
    // El focus order es nav tabs y luego una tarjeta por pet, en orden.
    NIMVLETS_CHECK(l.focusOrder.size() == 5);
    NIMVLETS_CHECK(l.focusOrder[3] == "shopitem:bunny");
    NIMVLETS_CHECK(l.focusOrder[4] == "shopitem:nidir");
    return true;
}

// Rejilla de browse para 1 / 2 / 4 / 8 candidatos: sin solapes, todo
// dentro del contenido, orden de foco determinista = orden de catálogo.
bool TestBrowseGridCounts() {
    for (int n : {1, 2, 3, 4, 6, 8}) {
        const ShopLayout l = SyntheticLayout(n);
        NIMVLETS_CHECK(l.presentation == ShopPresentation::kBrowse);
        NIMVLETS_CHECK(static_cast<int>(l.tiles.size()) == n);
        NIMVLETS_CHECK(NoTileOverlap(l.tiles));
        NIMVLETS_CHECK(TilesWithinContent(l, 800.0f));
        // Orden de foco: 3 tabs + n tarjetas, en orden de catálogo.
        NIMVLETS_CHECK(static_cast<int>(l.focusOrder.size()) == 3 + n);
        for (int i = 0; i < n; ++i) {
            NIMVLETS_CHECK(l.focusOrder[static_cast<std::size_t>(3 + i)] ==
                           "shopitem:shop" + std::to_string(i));
            NIMVLETS_CHECK(l.tiles[static_cast<std::size_t>(i)].petId == "shop" + std::to_string(i));
        }
    }
    return true;
}

// La ventana por defecto (800x560) compone SIN scroll para el catálogo
// real (2 pets) y para 1..4 candidatos sintéticos.
bool TestBrowseComposesWithoutScrollAtDefault() {
    NIMVLETS_CHECK(Layout(1000, {}).contentHeight <= 560.0f + 0.01f);
    for (int n : {1, 2, 3, 4}) {
        NIMVLETS_CHECK(SyntheticLayout(n).contentHeight <= 560.0f + 0.01f);
    }
    return true;
}

// Resize: mínimo soportado, y una ventana más ancha. Sin solapes, sin
// hit-boxes fuera del contenido; nunca scroll horizontal (brief §16).
bool TestBrowseResize() {
    for (auto wh : {std::pair<float, float>{600.0f, 460.0f}, std::pair<float, float>{1100.0f, 700.0f},
                    std::pair<float, float>{760.0f, 520.0f}}) {
        for (int n : {1, 2, 4, 8}) {
            const ShopLayout l = SyntheticLayout(n, "", wh.first, wh.second);
            NIMVLETS_CHECK(NoTileOverlap(l.tiles));
            NIMVLETS_CHECK(TilesWithinContent(l, wh.first));
        }
    }
    return true;
}

// Determinista: mismas entradas -> layout idéntico (posiciones incluidas).
bool TestDeterministic() {
    const ShopLayout a = SyntheticLayout(8);
    const ShopLayout b = SyntheticLayout(8);
    NIMVLETS_CHECK(a.tiles.size() == b.tiles.size());
    for (std::size_t i = 0; i < a.tiles.size(); ++i) {
        NIMVLETS_CHECK(a.tiles[i].petId == b.tiles[i].petId);
        NIMVLETS_CHECK(a.tiles[i].cell.x == b.tiles[i].cell.x);
        NIMVLETS_CHECK(a.tiles[i].cell.y == b.tiles[i].cell.y);
    }
    NIMVLETS_CHECK(a.focusOrder == b.focusOrder);
    return true;
}

// ==================== B. HOVER / FOCUS REVEAL =======================

// La info liviana revelada por hover/foco es correcta por estado:
// poseído -> "In your collection"; asequible / insuficiente -> el precio.
bool TestTileRevealTextPerStatus() {
    // balance 200: bunny poseído, nidir insuficiente (300).
    const ShopLayout en = Layout(200, {NoVar("bunny")});
    const ShopTile* bunny = en.FindTile("bunny");
    const ShopTile* nidir = en.FindTile("nidir");
    NIMVLETS_CHECK(bunny != nullptr && nidir != nullptr);
    NIMVLETS_CHECK(bunny->status == catalog::ShopItemStatus::kOwned);
    NIMVLETS_CHECK(bunny->revealText == "In your collection");
    NIMVLETS_CHECK(nidir->status == catalog::ShopItemStatus::kInsufficientBalance);
    NIMVLETS_CHECK(nidir->revealText == "300 clicks");

    // balance 500: nidir asequible -> sigue mostrando el precio (no un
    // estado distinto): la distinción asequible/insuficiente es "quiet"
    // (la hace la vista con el color), el texto es el mismo precio.
    const ShopLayout aff = Layout(500, {NoVar("bunny")});
    NIMVLETS_CHECK(aff.FindTile("nidir")->status == catalog::ShopItemStatus::kAffordable);
    NIMVLETS_CHECK(aff.FindTile("nidir")->revealText == "300 clicks");

    // ES: precio localizado, estado poseído localizado.
    const ShopLayout es = Layout(200, {NoVar("bunny")}, "", false, Language::kEs);
    NIMVLETS_CHECK(es.FindTile("bunny")->revealText == "En tu colección");
    NIMVLETS_CHECK(es.FindTile("nidir")->revealText == "300 clics");
    return true;
}

// Hover NO cambia la selección ni promueve un hero (brief §6).
bool TestHoverDoesNotSelect() {
    const ShopLayout browse = Layout(1000, {}, /*selected=*/"", false, Language::kEn, /*hover=*/"nidir");
    NIMVLETS_CHECK(browse.presentation == ShopPresentation::kBrowse);
    NIMVLETS_CHECK(browse.hero.petId.empty());

    // Con un hero ya seleccionado, hover sobre otra tarjeta NO mueve el hero.
    const ShopLayout sel = Layout(1000, {}, /*selected=*/"bunny", false, Language::kEn, /*hover=*/"nidir");
    NIMVLETS_CHECK(sel.presentation == ShopPresentation::kSelected);
    NIMVLETS_CHECK(sel.hero.petId == "bunny");
    return true;
}

// Hit-test: pestañas de nav, y tarjetas de browse por petId.
bool TestHitTestBrowse() {
    const ShopLayout l = Layout(1000, {});
    const ShopTile* nidir = l.FindTile("nidir");
    NIMVLETS_CHECK(nidir != nullptr);
    NIMVLETS_CHECK(l.HitTest(nidir->cell.CenterX(), nidir->cell.CenterY()) == "shopitem:nidir");
    NIMVLETS_CHECK(l.HitTest(l.header.tabs[0].hitRect.CenterX(), l.header.tabs[0].hitRect.CenterY()) ==
                   "nav:collection");
    // Zona vacía muy abajo -> nada accionable.
    NIMVLETS_CHECK(l.HitTest(5.0f, 555.0f).empty());
    return true;
}

// ==================== C. SELECCIÓN -> HERO ==========================

// Seleccionar promueve ese personaje a hero grande + rail compacto con
// TODOS los pets del Shop; el hero corresponde a la identidad elegida.
bool TestSelectionPromotesHero() {
    const ShopLayout l = Layout(500, {NoVar("bunny")}, /*selected=*/"nidir");
    NIMVLETS_CHECK(l.presentation == ShopPresentation::kSelected);
    NIMVLETS_CHECK(l.hero.petId == "nidir");
    NIMVLETS_CHECK(l.tiles.empty());
    NIMVLETS_CHECK(l.rail.size() == 2);  // Bunny + Nidir
    NIMVLETS_CHECK(l.FindTile("frin") == nullptr);
    // La tarjeta del hero está marcada en el rail; la otra no.
    const ShopTile* railNidir = l.FindTile("nidir");
    const ShopTile* railBunny = l.FindTile("bunny");
    NIMVLETS_CHECK(railNidir != nullptr && railNidir->selected);
    NIMVLETS_CHECK(railBunny != nullptr && !railBunny->selected);
    // El hero es sustancialmente más grande que una tarjeta del rail.
    NIMVLETS_CHECK(l.hero.art.w > railNidir->art.w * 2.0f);
    return true;
}

// Elegir OTRO personaje reemplaza la selección de forma coherente.
bool TestSelectingAnotherReplaces() {
    const ShopLayout a = Layout(1000, {NoVar("bunny")}, /*selected=*/"nidir");
    NIMVLETS_CHECK(a.hero.petId == "nidir");
    const ShopLayout b = Layout(1000, {NoVar("bunny")}, /*selected=*/"bunny");
    NIMVLETS_CHECK(b.hero.petId == "bunny");
    NIMVLETS_CHECK(b.rail.size() == 2);
    NIMVLETS_CHECK(b.FindTile("bunny")->selected);
    NIMVLETS_CHECK(!b.FindTile("nidir")->selected);
    return true;
}

// Un selectedPetId que no está en el Shop (Frin, o desconocido) cae a
// modo BROWSE — nunca un hero fantasma.
bool TestUnknownOrPrivateSelectionFallsToBrowse() {
    NIMVLETS_CHECK(Layout(1000, {}, "frin").presentation == ShopPresentation::kBrowse);
    NIMVLETS_CHECK(Layout(1000, {}, "ghost").presentation == ShopPresentation::kBrowse);
    NIMVLETS_CHECK(Layout(1000, {}, "frin").hero.petId.empty());
    return true;
}

// ==================== D. COMPRA (Block 07 preservado) ================

// Asequible: precio + botón "Get <name>" (localizado, nombre propio sin
// traducir), entitlementTarget correcto, "get" en el focus order.
bool TestSelectedAffordableShowsGetButton() {
    const ShopLayout en = Layout(500, {NoVar("bunny")}, "nidir");
    NIMVLETS_CHECK(en.hero.actionEnabled);
    NIMVLETS_CHECK(en.hero.actionLabel == "Get Nidir");
    NIMVLETS_CHECK(en.hero.priceText == "300 clicks");
    NIMVLETS_CHECK((en.hero.entitlementTarget == NoVar("nidir")));
    NIMVLETS_CHECK(!en.hero.confirm.visible);
    NIMVLETS_CHECK(Has(en.focusOrder, "get"));
    NIMVLETS_CHECK(!Has(en.focusOrder, "purchase:confirm"));
    // El "get" va ANTES de las tarjetas del rail en el orden de tabulación.
    std::size_t iGet = 0, iRail = 0;
    for (std::size_t i = 0; i < en.focusOrder.size(); ++i) {
        if (en.focusOrder[i] == "get") iGet = i;
        if (en.focusOrder[i] == "shopitem:bunny") iRail = i;
    }
    NIMVLETS_CHECK(iGet < iRail);

    const ShopLayout es = Layout(500, {NoVar("bunny")}, "nidir", false, Language::kEs);
    NIMVLETS_CHECK(es.hero.actionLabel == "Obtener Nidir");
    NIMVLETS_CHECK(es.hero.priceText == "300 clics");
    return true;
}

// Saldo insuficiente: sin botón, línea "Need N more clicks", "get" fuera
// del focus order.
bool TestSelectedInsufficient() {
    const ShopLayout en = Layout(258, {NoVar("bunny")}, "nidir");
    NIMVLETS_CHECK(!en.hero.actionEnabled);
    NIMVLETS_CHECK(en.hero.showStatusLine);
    NIMVLETS_CHECK(en.hero.statusText == "Need 42 more clicks");
    NIMVLETS_CHECK(en.hero.priceText == "300 clicks");
    NIMVLETS_CHECK(!Has(en.focusOrder, "get"));
    const ShopLayout es = Layout(299, {NoVar("bunny")}, "nidir", false, Language::kEs);
    NIMVLETS_CHECK(es.hero.statusText == "Te falta 1 clic");  // singular
    return true;
}

// Poseído: "In your collection", sin precio accionable, sin botón, sin
// CTA de compra.
bool TestSelectedOwnedHasNoPurchaseCta() {
    const ShopLayout en = Layout(1000, {NoVar("bunny")}, "bunny");
    NIMVLETS_CHECK(en.hero.petId == "bunny");
    NIMVLETS_CHECK(!en.hero.actionEnabled);
    NIMVLETS_CHECK(!en.hero.confirm.visible);
    NIMVLETS_CHECK(en.hero.showStatusLine);
    NIMVLETS_CHECK(en.hero.statusText == "In your collection");
    NIMVLETS_CHECK(!Has(en.focusOrder, "get"));
    NIMVLETS_CHECK(!Has(en.focusOrder, "purchase:confirm"));
    const ShopLayout es = Layout(1000, {NoVar("bunny")}, "bunny", false, Language::kEs);
    NIMVLETS_CHECK(es.hero.statusText == "En tu colección");
    return true;
}

// Confirmación inline: pregunta localizada + Cancelar/Confirmar en el
// focus order, "get" fuera, el botón "Get" cede a la confirmación.
bool TestConfirmationLayout() {
    const ShopLayout en = Layout(500, {NoVar("bunny")}, "nidir", /*confirming=*/true);
    NIMVLETS_CHECK(en.presentation == ShopPresentation::kSelected);
    NIMVLETS_CHECK(en.hero.confirm.visible);
    NIMVLETS_CHECK(en.hero.confirm.prompt == "Spend 300 clicks to add Nidir to your collection?");
    NIMVLETS_CHECK(en.hero.confirm.cancelLabel == "Cancel");
    NIMVLETS_CHECK(en.hero.confirm.confirmLabel == "Confirm");
    NIMVLETS_CHECK(en.hero.confirm.cancelFocusId == "purchase:cancel");
    NIMVLETS_CHECK(en.hero.confirm.confirmFocusId == "purchase:confirm");
    NIMVLETS_CHECK(!en.hero.actionEnabled);
    NIMVLETS_CHECK(Has(en.focusOrder, "purchase:cancel"));
    NIMVLETS_CHECK(Has(en.focusOrder, "purchase:confirm"));
    NIMVLETS_CHECK(!Has(en.focusOrder, "get"));
    // Cancel va antes que Confirm (el foco arranca en Cancel — brief §12).
    std::size_t iCancel = 99, iConfirm = 0;
    for (std::size_t i = 0; i < en.focusOrder.size(); ++i) {
        if (en.focusOrder[i] == "purchase:cancel") iCancel = i;
        if (en.focusOrder[i] == "purchase:confirm") iConfirm = i;
    }
    NIMVLETS_CHECK(iCancel < iConfirm);

    const ShopLayout es = Layout(500, {NoVar("bunny")}, "nidir", true, Language::kEs);
    NIMVLETS_CHECK(es.hero.confirm.prompt == "¿Gastar 300 clics para añadir Nidir a tu colección?");
    NIMVLETS_CHECK(es.hero.confirm.cancelLabel == "Cancelar");
    NIMVLETS_CHECK(es.hero.confirm.confirmLabel == "Confirmar");

    // Hit-test de los botones de confirmación.
    NIMVLETS_CHECK(en.HitTest(en.hero.confirm.cancelButton.CenterX(),
                              en.hero.confirm.cancelButton.CenterY()) == "purchase:cancel");
    NIMVLETS_CHECK(en.HitTest(en.hero.confirm.confirmButton.CenterX(),
                              en.hero.confirm.confirmButton.CenterY()) == "purchase:confirm");
    return true;
}

// `confirming` solo tiene efecto en SELECTED + asequible: en browse, o
// para un pet poseído / insuficiente, se ignora.
bool TestConfirmingIgnoredUnlessSelectedAffordable() {
    NIMVLETS_CHECK(!Layout(1000, {}, "", /*confirming=*/true).hero.confirm.visible);   // browse
    NIMVLETS_CHECK(!Layout(1000, {NoVar("bunny")}, "bunny", true).hero.confirm.visible);  // poseído
    NIMVLETS_CHECK(!Layout(10, {NoVar("bunny")}, "nidir", true).hero.confirm.visible);    // insuficiente
    return true;
}

// El "get" hace hit-test correcto en modo SELECTED.
bool TestHitTestGetButton() {
    const ShopLayout l = Layout(500, {NoVar("bunny")}, "nidir");
    NIMVLETS_CHECK(l.HitTest(l.hero.actionButton.CenterX(), l.hero.actionButton.CenterY()) == "get");
    const ShopTile* railBunny = l.FindTile("bunny");
    NIMVLETS_CHECK(railBunny != nullptr);
    NIMVLETS_CHECK(l.HitTest(railBunny->cell.CenterX(), railBunny->cell.CenterY()) == "shopitem:bunny");
    return true;
}

// ==================== E. LOCALIZACIÓN =============================

bool TestBrowseHeadingLocalized() {
    NIMVLETS_CHECK(Layout(0, {}, "", false, Language::kEn).browseHeading == "Nimvlets you can meet");
    NIMVLETS_CHECK(Layout(0, {}, "", false, Language::kEs).browseHeading ==
                   "Nimvlets que puedes conocer");
    return true;
}

// El precio se sigue formateando igual (Block 07): "N clicks" / "N clics".
bool TestPriceFormattingPreserved() {
    NIMVLETS_CHECK(Layout(1000, {}).FindTile("nidir")->revealText == "300 clicks");
    NIMVLETS_CHECK(Layout(1000, {}, "", false, Language::kEs).FindTile("nidir")->revealText ==
                   "300 clics");
    NIMVLETS_CHECK(Layout(1000, {}, "nidir").hero.priceText == "300 clicks");
    return true;
}

// Cambiar de idioma con un personaje seleccionado no pierde la selección.
bool TestLanguageSwitchKeepsSelection() {
    const ShopLayout es = Layout(1000, {}, "nidir", false, Language::kEs);
    NIMVLETS_CHECK(es.presentation == ShopPresentation::kSelected);
    NIMVLETS_CHECK(es.hero.petId == "nidir");
    NIMVLETS_CHECK(es.hero.speciesText == "Dragón negro");  // editorial ES
    return true;
}

// ==================== EMPTY SHOP ================================

bool TestEmptyShop() {
    ShopModel empty;  // sin items
    ShopLayoutInput in;
    const ShopLayout en = BuildShopLayout(empty, in);
    NIMVLETS_CHECK(en.empty);
    NIMVLETS_CHECK(en.presentation == ShopPresentation::kBrowse);
    NIMVLETS_CHECK(en.tiles.empty());
    NIMVLETS_CHECK(en.hero.petId.empty());
    NIMVLETS_CHECK(en.emptyText == "No Nimvlets to show yet.");
    NIMVLETS_CHECK(en.focusOrder.size() == 3);  // solo las pestañas de nav
    NIMVLETS_CHECK(en.HitTest(en.emptyAnchor.CenterX(), en.emptyAnchor.CenterY()).empty());

    ShopLayoutInput es;
    es.language = Language::kEs;
    NIMVLETS_CHECK(BuildShopLayout(empty, es).emptyText == "Todavía no hay Nimvlets para mostrar.");
    return true;
}

// ==================== F. RESIZE (SELECTED) =====================

// Modo SELECTED a distintos tamaños: el rail no se solapa y queda dentro
// del contenido; el "get" es alcanzable.
bool TestSelectedResize() {
    for (auto wh : {std::pair<float, float>{800.0f, 560.0f}, std::pair<float, float>{600.0f, 460.0f},
                    std::pair<float, float>{1200.0f, 760.0f}}) {
        const ShopLayout l = SyntheticLayout(8, "shop3", wh.first, wh.second);
        NIMVLETS_CHECK(l.presentation == ShopPresentation::kSelected);
        NIMVLETS_CHECK(l.hero.petId == "shop3");
        NIMVLETS_CHECK(l.rail.size() == 8);
        NIMVLETS_CHECK(NoTileOverlap(l.rail));
        for (const ShopTile& t : l.rail) {
            NIMVLETS_CHECK(t.cell.x >= -0.01f && t.cell.Right() <= wh.first + 0.01f);
            NIMVLETS_CHECK(t.cell.Bottom() <= l.contentHeight + 1.0f);
        }
        // El botón "get" queda dentro del ancho de contenido.
        NIMVLETS_CHECK(l.hero.actionButton.Right() <= wh.first - 20.0f);
    }
    return true;
}

// ==================== G. AFORDANCIA "Starter choices…" (Block 10) ====

// Sin `starterAffordanceVisible` (el default) NO hay afordancia: ni
// texto, ni "starter:enter" en el focus order, ni hit-test — el Shop
// público se comporta EXACTAMENTE como antes (brief §9/§10).
bool TestNoStarterAffordanceByDefault() {
    const ShopLayout browse = Layout(1000, {});
    NIMVLETS_CHECK(!browse.starterAffordanceVisible);
    NIMVLETS_CHECK(!Has(browse.focusOrder, "starter:enter"));
    NIMVLETS_CHECK(browse.focusOrder.size() == 5);  // 3 nav + 2 tiles

    const ShopLayout selected = Layout(1000, {NoVar("bunny")}, "nidir");
    NIMVLETS_CHECK(!selected.starterAffordanceVisible);
    NIMVLETS_CHECK(!Has(selected.focusOrder, "starter:enter"));

    ShopModel empty;
    ShopLayoutInput ein;
    NIMVLETS_CHECK(!BuildShopLayout(empty, ein).starterAffordanceVisible);
    return true;
}

// Con `starterAffordanceVisible = true` aparece UNA línea al final del
// focus order, hittest-able, en browse / selected / vacío. No reordena
// las tarjetas ni causa scroll a 800x560 (brief §10).
bool TestStarterAffordanceWhenRequested() {
    auto withAffordance = [](std::uint64_t balance, const Ents& owned, const std::string& sel) {
        const auto model = BuildShopModel(MakeShopCatalog(), balance, owned);
        ShopLayoutInput in;
        in.selectedPetId = sel;
        in.starterAffordanceVisible = true;
        return BuildShopLayout(model, in);
    };

    const ShopLayout browse = withAffordance(1000, {}, "");
    NIMVLETS_CHECK(browse.starterAffordanceVisible);
    NIMVLETS_CHECK(browse.starterAffordanceText == "Starter choices\xE2\x80\xA6");  // "Starter choices…"
    NIMVLETS_CHECK(browse.focusOrder.back() == "starter:enter");
    NIMVLETS_CHECK(browse.focusOrder.size() == 6);  // 3 nav + 2 tiles + starter:enter
    NIMVLETS_CHECK(browse.HitTest(browse.starterAffordanceAnchor.CenterX(),
                                  browse.starterAffordanceAnchor.CenterY()) == "starter:enter");
    NIMVLETS_CHECK(browse.contentHeight <= 560.0f + 0.01f);  // no scroll
    // Las 2 tarjetas siguen ahí, sin moverse por la afordancia.
    NIMVLETS_CHECK(browse.tiles.size() == 2);

    const ShopLayout selected = withAffordance(500, {NoVar("bunny")}, "nidir");
    NIMVLETS_CHECK(selected.starterAffordanceVisible);
    NIMVLETS_CHECK(Has(selected.focusOrder, "starter:enter"));
    NIMVLETS_CHECK(selected.HitTest(selected.starterAffordanceAnchor.CenterX(),
                                    selected.starterAffordanceAnchor.CenterY()) == "starter:enter");

    // Shop vacío (catálogo DEV) + ofertas de starter -> la afordancia
    // igual aparece (brief §10 "even in empty state").
    ShopModel empty;
    ShopLayoutInput ein;
    ein.starterAffordanceVisible = true;
    const ShopLayout e = BuildShopLayout(empty, ein);
    NIMVLETS_CHECK(e.empty && e.starterAffordanceVisible);
    NIMVLETS_CHECK(e.focusOrder.back() == "starter:enter");

    // ES.
    const auto model = BuildShopModel(MakeShopCatalog(), 1000, {});
    ShopLayoutInput es;
    es.language = Language::kEs;
    es.starterAffordanceVisible = true;
    NIMVLETS_CHECK(BuildShopLayout(model, es).starterAffordanceText == "Opciones iniciales\xE2\x80\xA6");
    return true;
}

}  // namespace

void RegisterShopLayoutTests(testing::TestRunner& runner) {
    runner.Add("ShopLayout/HeaderShopTabActive", TestHeaderShopTabActive);
    runner.Add("ShopLayout/DefaultIsBrowseNoHero", TestDefaultIsBrowseNoHero);
    runner.Add("ShopLayout/BrowseGridCounts", TestBrowseGridCounts);
    runner.Add("ShopLayout/BrowseComposesWithoutScrollAtDefault", TestBrowseComposesWithoutScrollAtDefault);
    runner.Add("ShopLayout/BrowseResize", TestBrowseResize);
    runner.Add("ShopLayout/Deterministic", TestDeterministic);
    runner.Add("ShopLayout/TileRevealTextPerStatus", TestTileRevealTextPerStatus);
    runner.Add("ShopLayout/HoverDoesNotSelect", TestHoverDoesNotSelect);
    runner.Add("ShopLayout/HitTestBrowse", TestHitTestBrowse);
    runner.Add("ShopLayout/SelectionPromotesHero", TestSelectionPromotesHero);
    runner.Add("ShopLayout/SelectingAnotherReplaces", TestSelectingAnotherReplaces);
    runner.Add("ShopLayout/UnknownOrPrivateSelectionFallsToBrowse", TestUnknownOrPrivateSelectionFallsToBrowse);
    runner.Add("ShopLayout/SelectedAffordableShowsGetButton", TestSelectedAffordableShowsGetButton);
    runner.Add("ShopLayout/SelectedInsufficient", TestSelectedInsufficient);
    runner.Add("ShopLayout/SelectedOwnedHasNoPurchaseCta", TestSelectedOwnedHasNoPurchaseCta);
    runner.Add("ShopLayout/ConfirmationLayout", TestConfirmationLayout);
    runner.Add("ShopLayout/ConfirmingIgnoredUnlessSelectedAffordable", TestConfirmingIgnoredUnlessSelectedAffordable);
    runner.Add("ShopLayout/HitTestGetButton", TestHitTestGetButton);
    runner.Add("ShopLayout/BrowseHeadingLocalized", TestBrowseHeadingLocalized);
    runner.Add("ShopLayout/PriceFormattingPreserved", TestPriceFormattingPreserved);
    runner.Add("ShopLayout/LanguageSwitchKeepsSelection", TestLanguageSwitchKeepsSelection);
    runner.Add("ShopLayout/EmptyShop", TestEmptyShop);
    runner.Add("ShopLayout/SelectedResize", TestSelectedResize);
    runner.Add("ShopLayout/NoStarterAffordanceByDefault", TestNoStarterAffordanceByDefault);
    runner.Add("ShopLayout/StarterAffordanceWhenRequested", TestStarterAffordanceWhenRequested);
}

}  // namespace nimvlets::tests
