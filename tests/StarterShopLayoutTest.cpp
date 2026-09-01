#include "StarterShopLayoutTest.h"

#include <string>
#include <vector>

#include "catalog/StarterShopModel.h"
#include "core/Localization.h"
#include "productui/StarterShopLayout.h"

using nimvlets::catalog::CatalogEntry;
using nimvlets::catalog::PetCatalog;
using nimvlets::catalog::PetEntitlement;
using nimvlets::catalog::PetIdentity;
using nimvlets::catalog::StarterRole;
using nimvlets::catalog::BuildStarterShopModel;
using nimvlets::catalog::StarterShopModel;
using nimvlets::core::Language;
using nimvlets::productui::BuildStarterShopLayout;
using nimvlets::productui::ShopTile;
using nimvlets::productui::StarterOfferFocusId;
using nimvlets::productui::StarterOfferIdentityFromFocusId;
using nimvlets::productui::StarterShopLayout;
using nimvlets::productui::StarterShopLayoutInput;
using nimvlets::productui::StarterShopPresentation;
using nimvlets::productui::UiRect;

namespace nimvlets::tests {

namespace {

CatalogEntry Entry(
    const std::string& petId, const std::string& variantId, StarterRole role, std::uint64_t price,
    bool isDefault = false) {
    CatalogEntry e;
    e.identity = PetIdentity{petId, variantId};
    e.displayName = petId == "frin" ? "Frin" : petId;
    e.packPath = petId + variantId + ".nvpack";
    e.isDefault = isDefault;
    e.priceClicks = price;
    e.starterRole = role;
    return e;
}

PetCatalog MakeCat() {
    std::vector<CatalogEntry> e;
    e.push_back(Entry("frin", "male", StarterRole::kSecret, 150, true));
    e.push_back(Entry("frin", "female", StarterRole::kSecret, 150));
    e.push_back(Entry("artu_dev", "", StarterRole::kNormal, 80));
    e.push_back(Entry("rato_dev", "", StarterRole::kNormal, 100));
    e.push_back(Entry("rinrin_dev", "", StarterRole::kNormal, 120));
    return PetCatalog(std::move(e));
}

// Modelo de un usuario que eligió frin/female (offers: frin/male + 3 normales).
StarterShopModel FrinFemaleModel(std::uint64_t balance = 10000) {
    return BuildStarterShopModel(MakeCat(), true, {PetEntitlement{"frin", "female"}}, balance);
}

StarterShopLayout Layout(
    const StarterShopModel& model, const std::string& selected = "", bool confirming = false,
    Language lang = Language::kEn, const std::string& hover = "", float w = 800.0f, float h = 560.0f) {
    StarterShopLayoutInput in;
    in.viewportW = w;
    in.viewportH = h;
    in.language = lang;
    in.selectedFocusId = selected;
    in.hoverFocusId = hover;
    in.confirming = confirming;
    return BuildStarterShopLayout(model, in);
}

bool Has(const std::vector<std::string>& v, const std::string& s) {
    for (const auto& x : v) {
        if (x == s) return true;
    }
    return false;
}

bool RectsOverlap(const UiRect& a, const UiRect& b) {
    return a.x < b.Right() - 0.01f && b.x < a.Right() - 0.01f && a.y < b.Bottom() - 0.01f &&
           b.y < a.Bottom() - 0.01f;
}

bool NoOverlap(const std::vector<ShopTile>& tiles) {
    for (std::size_t i = 0; i < tiles.size(); ++i) {
        for (std::size_t j = i + 1; j < tiles.size(); ++j) {
            if (RectsOverlap(tiles[i].cell, tiles[j].cell)) return false;
        }
    }
    return true;
}

// --- focusId round-trip ------------------------------------------------

bool TestFocusIdRoundTrips() {
    const PetIdentity fm{"frin", "male"};
    const std::string fid = StarterOfferFocusId(fm);
    NIMVLETS_CHECK(fid == "starteritem:frin/male");
    NIMVLETS_CHECK(StarterOfferIdentityFromFocusId(fid) == fm);
    const PetIdentity artu{"artu_dev", ""};
    NIMVLETS_CHECK(StarterOfferFocusId(artu) == "starteritem:artu_dev/");
    NIMVLETS_CHECK(StarterOfferIdentityFromFocusId("starteritem:artu_dev/") == artu);
    NIMVLETS_CHECK(StarterOfferIdentityFromFocusId("nope").petId.empty());
    return true;
}

// --- BROWSE -------------------------------------------------------

bool TestBrowseHasBackAndOffersInFocusOrder() {
    const StarterShopLayout l = Layout(FrinFemaleModel());
    NIMVLETS_CHECK(l.presentation == StarterShopPresentation::kBrowse);
    NIMVLETS_CHECK(l.tiles.size() == 4);
    NIMVLETS_CHECK(!l.heading.empty());
    NIMVLETS_CHECK(!l.backText.empty());
    // nav tabs, luego "starter:back", luego una tarjeta por oferta.
    NIMVLETS_CHECK(l.header.tabs.size() == 3);
    NIMVLETS_CHECK(l.header.tabs[1].active);  // "Shop" activo (submodo)
    NIMVLETS_CHECK(l.focusOrder.size() == 3 + 1 + 4);
    NIMVLETS_CHECK(l.focusOrder[3] == "starter:back");
    NIMVLETS_CHECK(l.focusOrder[4] == "starteritem:frin/male");
    NIMVLETS_CHECK(l.focusOrder[5] == "starteritem:artu_dev/");
    NIMVLETS_CHECK(!Has(l.focusOrder, "get"));
    NIMVLETS_CHECK(NoOverlap(l.tiles));
    NIMVLETS_CHECK(l.contentHeight <= 560.0f + 0.01f);  // compone sin scroll
    return true;
}

bool TestExactFrinVariantLabelOnTile() {
    const StarterShopLayout en = Layout(FrinFemaleModel());
    const ShopTile* fm = en.FindTileByFocusId("starteritem:frin/male");
    NIMVLETS_CHECK(fm != nullptr);
    NIMVLETS_CHECK(fm->petId == "frin");
    NIMVLETS_CHECK(fm->variantId == "male");         // preview EXACTA
    NIMVLETS_CHECK(fm->displayName == "Frin \xC2\xB7 Male");
    NIMVLETS_CHECK(fm->revealText == "150 clicks");
    // Un starter normal: sin " · " (nombre a secas).
    const ShopTile* artu = en.FindTileByFocusId("starteritem:artu_dev/");
    NIMVLETS_CHECK(artu != nullptr && artu->displayName == "artu_dev" && artu->variantId.empty());

    const StarterShopLayout es = Layout(FrinFemaleModel(), "", false, Language::kEs);
    NIMVLETS_CHECK(es.FindTileByFocusId("starteritem:frin/male")->displayName == "Frin \xC2\xB7 Macho");
    NIMVLETS_CHECK(es.FindTileByFocusId("starteritem:frin/male")->revealText == "150 clics");
    return true;
}

bool TestBrowseHitTest() {
    const StarterShopLayout l = Layout(FrinFemaleModel());
    const ShopTile* fm = l.FindTileByFocusId("starteritem:frin/male");
    NIMVLETS_CHECK(l.HitTest(fm->cell.CenterX(), fm->cell.CenterY()) == "starteritem:frin/male");
    NIMVLETS_CHECK(l.HitTest(l.backAnchor.CenterX(), l.backAnchor.CenterY()) == "starter:back");
    NIMVLETS_CHECK(l.HitTest(l.header.tabs[0].hitRect.CenterX(),
                             l.header.tabs[0].hitRect.CenterY()) == "nav:collection");
    return true;
}

// --- SELECTED -> hero ---------------------------------------------

bool TestSelectionPromotesExactVariantHero() {
    const StarterShopLayout l = Layout(FrinFemaleModel(), "starteritem:frin/male");
    NIMVLETS_CHECK(l.presentation == StarterShopPresentation::kSelected);
    NIMVLETS_CHECK(l.hero.petId == "frin");
    NIMVLETS_CHECK(l.hero.variantId == "male");  // .nvprev EXACTA
    NIMVLETS_CHECK((l.hero.entitlementTarget == PetEntitlement{"frin", "male"}));
    NIMVLETS_CHECK(l.hero.displayName == "Frin \xC2\xB7 Male");
    NIMVLETS_CHECK(l.hero.speciesText == "White wolf");  // editorial de "frin"
    NIMVLETS_CHECK(l.hero.actionEnabled);                // asequible (balance 10000)
    NIMVLETS_CHECK(l.hero.actionLabel == "Get Frin \xC2\xB7 Male");
    NIMVLETS_CHECK(l.hero.priceText == "150 clicks");
    NIMVLETS_CHECK(l.tiles.empty());
    NIMVLETS_CHECK(l.rail.size() == 4);
    NIMVLETS_CHECK(l.FindTileByFocusId("starteritem:frin/male")->selected);
    NIMVLETS_CHECK(!l.FindTileByFocusId("starteritem:artu_dev/")->selected);
    // "get" antes de las tarjetas del rail.
    std::size_t iGet = 0, iRail = 0;
    for (std::size_t i = 0; i < l.focusOrder.size(); ++i) {
        if (l.focusOrder[i] == "get") iGet = i;
        if (l.focusOrder[i] == "starteritem:artu_dev/") iRail = i;
    }
    NIMVLETS_CHECK(iGet < iRail);
    return true;
}

bool TestSelectedInsufficient() {
    const StarterShopLayout l = Layout(FrinFemaleModel(/*balance=*/100), "starteritem:frin/male");
    NIMVLETS_CHECK(!l.hero.actionEnabled);
    NIMVLETS_CHECK(l.hero.showStatusLine);
    NIMVLETS_CHECK(l.hero.statusText == "Need 50 more clicks");
    NIMVLETS_CHECK(!Has(l.focusOrder, "get"));
    return true;
}

bool TestUnknownSelectionFallsToBrowse() {
    NIMVLETS_CHECK(Layout(FrinFemaleModel(), "starteritem:ghost/").presentation ==
                   StarterShopPresentation::kBrowse);
    // frin/female está poseída -> no es oferta -> selección cae a browse.
    NIMVLETS_CHECK(Layout(FrinFemaleModel(), "starteritem:frin/female").presentation ==
                   StarterShopPresentation::kBrowse);
    return true;
}

// --- CONFIRMATION ------------------------------------------------

bool TestConfirmationLayout() {
    const StarterShopLayout en =
        Layout(FrinFemaleModel(), "starteritem:frin/male", /*confirming=*/true);
    NIMVLETS_CHECK(en.hero.confirm.visible);
    NIMVLETS_CHECK(en.hero.confirm.prompt ==
                   "Spend 150 clicks to add Frin \xC2\xB7 Male to your collection?");
    NIMVLETS_CHECK(en.hero.confirm.cancelFocusId == "purchase:cancel");
    NIMVLETS_CHECK(en.hero.confirm.confirmFocusId == "purchase:confirm");
    NIMVLETS_CHECK(!en.hero.actionEnabled);
    NIMVLETS_CHECK(Has(en.focusOrder, "purchase:cancel"));
    NIMVLETS_CHECK(Has(en.focusOrder, "purchase:confirm"));
    NIMVLETS_CHECK(!Has(en.focusOrder, "get"));
    std::size_t iC = 99, iOk = 0;
    for (std::size_t i = 0; i < en.focusOrder.size(); ++i) {
        if (en.focusOrder[i] == "purchase:cancel") iC = i;
        if (en.focusOrder[i] == "purchase:confirm") iOk = i;
    }
    NIMVLETS_CHECK(iC < iOk);  // Cancel primero (foco arranca ahí)
    NIMVLETS_CHECK(en.HitTest(en.hero.confirm.cancelButton.CenterX(),
                              en.hero.confirm.cancelButton.CenterY()) == "purchase:cancel");
    NIMVLETS_CHECK(en.HitTest(en.hero.confirm.confirmButton.CenterX(),
                              en.hero.confirm.confirmButton.CenterY()) == "purchase:confirm");

    const StarterShopLayout es =
        Layout(FrinFemaleModel(), "starteritem:frin/male", true, Language::kEs);
    // El nombre compuesto usa la clave kFemale/kMale ("Macho"); "Frin" y
    // el "·" no se traducen. La plantilla ES ya está cubierta por
    // ShopLayoutTest — acá solo importa que el nombre compuesto entre.
    NIMVLETS_CHECK(es.hero.confirm.prompt.find("Frin \xC2\xB7 Macho") != std::string::npos);
    NIMVLETS_CHECK(es.hero.confirm.prompt.rfind("\xC2\xBFGastar 150 clics", 0) == 0);
    NIMVLETS_CHECK(es.hero.confirm.cancelLabel == "Cancelar");
    return true;
}

bool TestConfirmingIgnoredUnlessSelectedAffordable() {
    NIMVLETS_CHECK(!Layout(FrinFemaleModel(), "", /*confirming=*/true).hero.confirm.visible);
    NIMVLETS_CHECK(!Layout(FrinFemaleModel(100), "starteritem:frin/male", true).hero.confirm.visible);
    return true;
}

// --- EMPTY (todas compradas) -----------------------------------

bool TestEmptyStateHasBackAndQuietLine() {
    // Un usuario con ambas variantes de Frin y todos los normales -> 0 ofertas.
    const StarterShopModel empty = BuildStarterShopModel(
        MakeCat(), true,
        {PetEntitlement{"frin", "male"}, PetEntitlement{"frin", "female"},
         PetEntitlement{"artu_dev", ""}, PetEntitlement{"rato_dev", ""},
         PetEntitlement{"rinrin_dev", ""}},
        10000);
    NIMVLETS_CHECK(empty.Empty());
    const StarterShopLayout en = Layout(empty);
    NIMVLETS_CHECK(en.presentation == StarterShopPresentation::kEmpty);
    NIMVLETS_CHECK(en.tiles.empty());
    NIMVLETS_CHECK(en.emptyText == "No more starter choices.");
    NIMVLETS_CHECK(Has(en.focusOrder, "starter:back"));
    NIMVLETS_CHECK(!Has(en.focusOrder, "get"));
    NIMVLETS_CHECK(en.HitTest(en.backAnchor.CenterX(), en.backAnchor.CenterY()) == "starter:back");
    NIMVLETS_CHECK(en.contentHeight <= 560.0f + 0.01f);
    const StarterShopLayout es = Layout(empty, "", false, Language::kEs);
    NIMVLETS_CHECK(es.emptyText == "No quedan opciones iniciales.");
    return true;
}

// --- resize -----------------------------------------------------

bool TestResizeNoOverlap() {
    for (auto wh : {std::pair<float, float>{800.0f, 560.0f}, std::pair<float, float>{600.0f, 460.0f},
                    std::pair<float, float>{1100.0f, 720.0f}}) {
        const StarterShopLayout b = Layout(FrinFemaleModel(), "", false, Language::kEn, "", wh.first,
                                           wh.second);
        NIMVLETS_CHECK(NoOverlap(b.tiles));
        for (const ShopTile& t : b.tiles) {
            NIMVLETS_CHECK(t.cell.x >= -0.01f && t.cell.Right() <= wh.first + 0.01f);
        }
        const StarterShopLayout s = Layout(FrinFemaleModel(), "starteritem:frin/male", false,
                                           Language::kEn, "", wh.first, wh.second);
        NIMVLETS_CHECK(s.presentation == StarterShopPresentation::kSelected);
        NIMVLETS_CHECK(NoOverlap(s.rail));
        NIMVLETS_CHECK(s.hero.actionButton.Right() <= wh.first - 20.0f);
    }
    return true;
}

bool TestDeterministic() {
    const StarterShopLayout a = Layout(FrinFemaleModel());
    const StarterShopLayout b = Layout(FrinFemaleModel());
    NIMVLETS_CHECK(a.focusOrder == b.focusOrder);
    NIMVLETS_CHECK(a.tiles.size() == b.tiles.size());
    for (std::size_t i = 0; i < a.tiles.size(); ++i) {
        NIMVLETS_CHECK(a.tiles[i].focusId == b.tiles[i].focusId);
        NIMVLETS_CHECK(a.tiles[i].cell.x == b.tiles[i].cell.x);
        NIMVLETS_CHECK(a.tiles[i].cell.y == b.tiles[i].cell.y);
    }
    return true;
}

}  // namespace

void RegisterStarterShopLayoutTests(testing::TestRunner& runner) {
    runner.Add("StarterShopLayout/FocusIdRoundTrips", TestFocusIdRoundTrips);
    runner.Add("StarterShopLayout/BrowseHasBackAndOffersInFocusOrder",
               TestBrowseHasBackAndOffersInFocusOrder);
    runner.Add("StarterShopLayout/ExactFrinVariantLabelOnTile", TestExactFrinVariantLabelOnTile);
    runner.Add("StarterShopLayout/BrowseHitTest", TestBrowseHitTest);
    runner.Add("StarterShopLayout/SelectionPromotesExactVariantHero",
               TestSelectionPromotesExactVariantHero);
    runner.Add("StarterShopLayout/SelectedInsufficient", TestSelectedInsufficient);
    runner.Add("StarterShopLayout/UnknownSelectionFallsToBrowse", TestUnknownSelectionFallsToBrowse);
    runner.Add("StarterShopLayout/ConfirmationLayout", TestConfirmationLayout);
    runner.Add("StarterShopLayout/ConfirmingIgnoredUnlessSelectedAffordable",
               TestConfirmingIgnoredUnlessSelectedAffordable);
    runner.Add("StarterShopLayout/EmptyStateHasBackAndQuietLine", TestEmptyStateHasBackAndQuietLine);
    runner.Add("StarterShopLayout/ResizeNoOverlap", TestResizeNoOverlap);
    runner.Add("StarterShopLayout/Deterministic", TestDeterministic);
}

}  // namespace nimvlets::tests
