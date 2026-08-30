#include "ShopModelTest.h"

#include "catalog/ShopModel.h"

#include <string>
#include <vector>

using nimvlets::catalog::BuildShopModel;
using nimvlets::catalog::CatalogEntry;
using nimvlets::catalog::PetCatalog;
using nimvlets::catalog::PetEntitlement;
using nimvlets::catalog::PetIdentity;
using nimvlets::catalog::ShopItemStatus;
using nimvlets::catalog::ShopModel;

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

PetEntitlement NoVar(const std::string& p) { return PetEntitlement{p, ""}; }  // Nimvlet sin variantes

// Solo los pets públicamente comprables aparecen; Frin NUNCA (brief §11).
bool TestOnlyPublicPetsAppearFrinAbsent() {
    const PetCatalog cat = MakeShopCatalog();
    const ShopModel model = BuildShopModel(cat, 1000, {});
    NIMVLETS_CHECK(model.items.size() == 2);
    NIMVLETS_CHECK(model.items[0].petId == "bunny");
    NIMVLETS_CHECK(model.items[1].petId == "nidir");
    NIMVLETS_CHECK(model.Find("frin") == nullptr);
    return true;
}

bool TestAffordableWhenBalanceCoversPrice() {
    const PetCatalog cat = MakeShopCatalog();
    const ShopModel model = BuildShopModel(cat, 300, {});
    const auto* nidir = model.Find("nidir");
    NIMVLETS_CHECK(nidir != nullptr);
    NIMVLETS_CHECK(nidir->status == ShopItemStatus::kAffordable);
    NIMVLETS_CHECK(nidir->priceClicks == 300);
    NIMVLETS_CHECK(nidir->clicksShort == 0);
    NIMVLETS_CHECK((nidir->entitlementTarget == NoVar("nidir")));
    return true;
}

bool TestInsufficientReportsShortfall() {
    const PetCatalog cat = MakeShopCatalog();
    const ShopModel model = BuildShopModel(cat, 258, {});
    const auto* nidir = model.Find("nidir");
    NIMVLETS_CHECK(nidir->status == ShopItemStatus::kInsufficientBalance);
    NIMVLETS_CHECK(nidir->clicksShort == 42);  // 300 - 258 (el ejemplo del brief §9)
    return true;
}

bool TestOwnedPetShowsOwnedNoAction() {
    const PetCatalog cat = MakeShopCatalog();
    const ShopModel model = BuildShopModel(cat, 1000, {NoVar("bunny")});
    const auto* bunny = model.Find("bunny");
    NIMVLETS_CHECK(bunny->status == ShopItemStatus::kOwned);
    NIMVLETS_CHECK(bunny->clicksShort == 0);
    return true;
}

// Determinista: mismas entradas -> mismo modelo.
bool TestDeterministic() {
    const PetCatalog cat = MakeShopCatalog();
    const ShopModel a = BuildShopModel(cat, 200, {NoVar("bunny")});
    const ShopModel b = BuildShopModel(cat, 200, {NoVar("bunny")});
    NIMVLETS_CHECK(a.items.size() == b.items.size());
    for (std::size_t i = 0; i < a.items.size(); ++i) {
        NIMVLETS_CHECK(a.items[i].petId == b.items[i].petId);
        NIMVLETS_CHECK(a.items[i].status == b.items[i].status);
        NIMVLETS_CHECK(a.items[i].clicksShort == b.items[i].clicksShort);
    }
    return true;
}

}  // namespace

void RegisterShopModelTests(testing::TestRunner& runner) {
    runner.Add("ShopModel/OnlyPublicPetsAppearFrinAbsent", TestOnlyPublicPetsAppearFrinAbsent);
    runner.Add("ShopModel/AffordableWhenBalanceCoversPrice", TestAffordableWhenBalanceCoversPrice);
    runner.Add("ShopModel/InsufficientReportsShortfall", TestInsufficientReportsShortfall);
    runner.Add("ShopModel/OwnedPetShowsOwnedNoAction", TestOwnedPetShowsOwnedNoAction);
    runner.Add("ShopModel/Deterministic", TestDeterministic);
}

}  // namespace nimvlets::tests
