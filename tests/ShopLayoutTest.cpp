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
using nimvlets::core::Language;
using nimvlets::productui::BuildShopLayout;
using nimvlets::productui::ShopLayout;
using nimvlets::productui::ShopLayoutInput;

using Ents = std::vector<PetEntitlement>;

namespace nimvlets::tests {

namespace {

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

PetEntitlement Whole(const std::string& p) { return PetEntitlement{p, ""}; }

bool Has(const std::vector<std::string>& v, const std::string& s) {
    for (const auto& x : v) {
        if (x == s) return true;
    }
    return false;
}

ShopLayout Layout(std::uint64_t balance, const Ents& owned, const std::string& selected = "",
                  bool confirming = false, Language lang = Language::kEn) {
    const auto model = BuildShopModel(MakeShopCatalog(), balance, owned);
    ShopLayoutInput in;
    in.language = lang;
    in.selectedPetId = selected;
    in.confirming = confirming;
    return BuildShopLayout(model, in);
}

// La cabecera trae las pestañas con "Shop" (Tienda) activa.
bool TestHeaderShopTabActive() {
    const ShopLayout es = Layout(0, {}, "", false, Language::kEs);
    NIMVLETS_CHECK(es.header.tabs.size() == 2);
    NIMVLETS_CHECK(es.header.tabs[0].label == "Colección" && !es.header.tabs[0].active);
    NIMVLETS_CHECK(es.header.tabs[1].label == "Tienda" && es.header.tabs[1].active);
    NIMVLETS_CHECK(Has(es.focusOrder, "nav:collection"));
    NIMVLETS_CHECK(Has(es.focusOrder, "nav:shop"));
    return true;
}

// El hero por defecto es el primer pet del Shop; el otro va a la
// gallery. Frin nunca aparece.
bool TestHeroDefaultsToFirstShopPet() {
    const ShopLayout l = Layout(1000, {});
    NIMVLETS_CHECK(l.hero.petId == "bunny");
    NIMVLETS_CHECK(l.gallery.size() == 1);
    NIMVLETS_CHECK(l.gallery[0].petId == "nidir");
    NIMVLETS_CHECK(l.FindGalleryItem("frin") == nullptr);
    return true;
}

// Estado ASEQUIBLE: botón "Get <pet>" (localizado, nombre propio sin
// traducir), precio visible, "get" en el focus order.
bool TestAffordableShowsGetButton() {
    const ShopLayout en = Layout(500, {Whole("bunny")}, "nidir");
    NIMVLETS_CHECK(en.hero.petId == "nidir");
    NIMVLETS_CHECK(en.hero.actionEnabled);
    NIMVLETS_CHECK(en.hero.actionLabel == "Get Nidir");
    NIMVLETS_CHECK(en.hero.priceText == "300 clicks");
    NIMVLETS_CHECK(!en.hero.confirm.visible);
    NIMVLETS_CHECK(Has(en.focusOrder, "get"));
    NIMVLETS_CHECK(!Has(en.focusOrder, "purchase:confirm"));

    const ShopLayout es = Layout(500, {Whole("bunny")}, "nidir", false, Language::kEs);
    NIMVLETS_CHECK(es.hero.actionLabel == "Obtener Nidir");
    NIMVLETS_CHECK(es.hero.priceText == "300 clics");
    return true;
}

// Estado SALDO INSUFICIENTE: sin botón, línea "Need N more clicks", "get"
// fuera del focus order.
bool TestInsufficientShowsNeedMore() {
    const ShopLayout en = Layout(258, {Whole("bunny")}, "nidir");
    NIMVLETS_CHECK(!en.hero.actionEnabled);
    NIMVLETS_CHECK(en.hero.showStatusLine);
    NIMVLETS_CHECK(en.hero.statusText == "Need 42 more clicks");
    NIMVLETS_CHECK(en.hero.priceText == "300 clicks");
    NIMVLETS_CHECK(!Has(en.focusOrder, "get"));

    const ShopLayout es = Layout(299, {Whole("bunny")}, "nidir", false, Language::kEs);
    NIMVLETS_CHECK(es.hero.statusText == "Te falta 1 clic");  // singular
    return true;
}

// Estado POSEÍDO: "In your collection", sin precio accionable, sin botón.
bool TestOwnedShowsInYourCollection() {
    const ShopLayout en = Layout(1000, {Whole("bunny")}, "bunny");
    NIMVLETS_CHECK(en.hero.petId == "bunny");
    NIMVLETS_CHECK(!en.hero.actionEnabled);
    NIMVLETS_CHECK(!en.hero.confirm.visible);
    NIMVLETS_CHECK(en.hero.showStatusLine);
    NIMVLETS_CHECK(en.hero.statusText == "In your collection");

    const ShopLayout es = Layout(1000, {Whole("bunny")}, "bunny", false, Language::kEs);
    NIMVLETS_CHECK(es.hero.statusText == "En tu colección");
    return true;
}

// Confirmación inline: la pregunta localizada + Cancelar/Confirmar, con
// sus focusId en el orden de tabulación, y "get" fuera.
bool TestConfirmationLayout() {
    const ShopLayout en = Layout(500, {Whole("bunny")}, "nidir", /*confirming=*/true);
    NIMVLETS_CHECK(en.hero.confirm.visible);
    NIMVLETS_CHECK(en.hero.confirm.prompt == "Spend 300 clicks to add Nidir to your collection?");
    NIMVLETS_CHECK(en.hero.confirm.cancelLabel == "Cancel");
    NIMVLETS_CHECK(en.hero.confirm.confirmLabel == "Confirm");
    NIMVLETS_CHECK(en.hero.confirm.cancelFocusId == "purchase:cancel");
    NIMVLETS_CHECK(en.hero.confirm.confirmFocusId == "purchase:confirm");
    NIMVLETS_CHECK(!en.hero.actionEnabled);  // el botón "Get" cede a la confirmación
    NIMVLETS_CHECK(Has(en.focusOrder, "purchase:cancel"));
    NIMVLETS_CHECK(Has(en.focusOrder, "purchase:confirm"));
    NIMVLETS_CHECK(!Has(en.focusOrder, "get"));

    const ShopLayout es = Layout(500, {Whole("bunny")}, "nidir", true, Language::kEs);
    NIMVLETS_CHECK(es.hero.confirm.prompt == "¿Gastar 300 clics para añadir Nidir a tu colección?");
    NIMVLETS_CHECK(es.hero.confirm.cancelLabel == "Cancelar");
    NIMVLETS_CHECK(es.hero.confirm.confirmLabel == "Confirmar");
    return true;
}

// La confirmación solo tiene efecto si el pet es asequible: un pet
// poseído nunca muestra la confirmación aunque `confirming` sea true.
bool TestConfirmingIgnoredWhenNotAffordable() {
    const ShopLayout owned = Layout(1000, {Whole("bunny")}, "bunny", /*confirming=*/true);
    NIMVLETS_CHECK(!owned.hero.confirm.visible);
    const ShopLayout poor = Layout(10, {Whole("bunny")}, "nidir", /*confirming=*/true);
    NIMVLETS_CHECK(!poor.hero.confirm.visible);
    return true;
}

// Hit-test: pestañas de nav, botón "Get", y botones de confirmación.
bool TestHitTest() {
    const ShopLayout l = Layout(500, {Whole("bunny")}, "nidir");
    NIMVLETS_CHECK(l.HitTest(l.hero.actionButton.CenterX(), l.hero.actionButton.CenterY()) == "get");
    NIMVLETS_CHECK(l.HitTest(l.header.tabs[0].hitRect.CenterX(),
                             l.header.tabs[0].hitRect.CenterY()) == "nav:collection");
    const auto* nidirGal = l.FindGalleryItem("bunny");
    NIMVLETS_CHECK(nidirGal != nullptr);
    NIMVLETS_CHECK(l.HitTest(nidirGal->art.CenterX(), nidirGal->art.CenterY()) == "shopitem:bunny");

    const ShopLayout c = Layout(500, {Whole("bunny")}, "nidir", /*confirming=*/true);
    NIMVLETS_CHECK(c.HitTest(c.hero.confirm.cancelButton.CenterX(),
                             c.hero.confirm.cancelButton.CenterY()) == "purchase:cancel");
    NIMVLETS_CHECK(c.HitTest(c.hero.confirm.confirmButton.CenterX(),
                             c.hero.confirm.confirmButton.CenterY()) == "purchase:confirm");
    // Muy arriba (cabecera vacía) -> nada accionable.
    NIMVLETS_CHECK(l.HitTest(5.0f, 4.0f).empty());
    return true;
}

}  // namespace

void RegisterShopLayoutTests(testing::TestRunner& runner) {
    runner.Add("ShopLayout/HeaderShopTabActive", TestHeaderShopTabActive);
    runner.Add("ShopLayout/HeroDefaultsToFirstShopPet", TestHeroDefaultsToFirstShopPet);
    runner.Add("ShopLayout/AffordableShowsGetButton", TestAffordableShowsGetButton);
    runner.Add("ShopLayout/InsufficientShowsNeedMore", TestInsufficientShowsNeedMore);
    runner.Add("ShopLayout/OwnedShowsInYourCollection", TestOwnedShowsInYourCollection);
    runner.Add("ShopLayout/ConfirmationLayout", TestConfirmationLayout);
    runner.Add("ShopLayout/ConfirmingIgnoredWhenNotAffordable", TestConfirmingIgnoredWhenNotAffordable);
    runner.Add("ShopLayout/HitTest", TestHitTest);
}

}  // namespace nimvlets::tests
