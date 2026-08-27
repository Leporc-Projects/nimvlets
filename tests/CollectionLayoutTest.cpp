#include "CollectionLayoutTest.h"

#include "catalog/CollectionModel.h"
#include "productui/CollectionLayout.h"

#include <string>
#include <vector>

using nimvlets::catalog::BuildCollectionModel;
using nimvlets::catalog::CatalogEntry;
using nimvlets::catalog::CollectionModel;
using nimvlets::catalog::OwnershipStatus;
using nimvlets::catalog::PetCatalog;
using nimvlets::catalog::PetIdentity;
using nimvlets::productui::BuildCollectionLayout;
using nimvlets::productui::CollectionLayout;
using nimvlets::productui::CollectionLayoutInput;
using nimvlets::productui::StatusShortLabel;

namespace nimvlets::tests {

namespace {

PetCatalog MakeDevCatalog() {
    std::vector<CatalogEntry> entries;
    CatalogEntry bunny;
    bunny.identity = PetIdentity{"bunny", ""};
    bunny.displayName = "Bunny";
    bunny.packPath = "b.nvpack";
    bunny.isDefault = true;
    bunny.initiallyOwned = true;
    entries.push_back(bunny);
    CatalogEntry nidir;
    nidir.identity = PetIdentity{"nidir", ""};
    nidir.displayName = "Nidir";
    nidir.packPath = "n.nvpack";
    entries.push_back(nidir);
    CatalogEntry fm;
    fm.identity = PetIdentity{"frin", "male"};
    fm.displayName = "Frin";
    fm.packPath = "fm.nvpack";
    fm.initiallyOwned = true;
    entries.push_back(fm);
    CatalogEntry ff;
    ff.identity = PetIdentity{"frin", "female"};
    ff.displayName = "Frin";
    ff.packPath = "ff.nvpack";
    ff.initiallyOwned = true;
    entries.push_back(ff);
    return PetCatalog(std::move(entries));
}

CollectionModel DevModel(const std::string& activePetId, const std::string& activeVariant = "") {
    return BuildCollectionModel(MakeDevCatalog(), {"bunny", "frin"}, PetIdentity{activePetId, activeVariant});
}

bool TestStatusLabelsAreHuman() {
    NIMVLETS_CHECK(std::string(StatusShortLabel(OwnershipStatus::kActive)) == "On desktop");
    NIMVLETS_CHECK(std::string(StatusShortLabel(OwnershipStatus::kOwnedInactive)) == "Use");
    NIMVLETS_CHECK(std::string(StatusShortLabel(OwnershipStatus::kLocked)) == "Not in your collection");
    return true;
}

bool TestGridHasOneBoxPerLogicalPet() {
    const CollectionModel model = DevModel("bunny");
    CollectionLayoutInput in;
    const CollectionLayout layout = BuildCollectionLayout(model, in);

    NIMVLETS_CHECK(layout.items.size() == 3);
    NIMVLETS_CHECK(layout.items[0].petId == "bunny");
    NIMVLETS_CHECK(layout.items[1].petId == "nidir");
    NIMVLETS_CHECK(layout.items[2].petId == "frin");
    NIMVLETS_CHECK(layout.items[2].hasVariants);
    NIMVLETS_CHECK(!layout.items[0].hasVariants);

    // Focus order = un item por pet, en orden de grid, sin detalle.
    NIMVLETS_CHECK((layout.focusOrder == std::vector<std::string>{"item:bunny", "item:nidir", "item:frin"}));
    return true;
}

// Cada caja de arte cae dentro del viewport y las columnas no se
// solapan.
bool TestGridBoxesAreLaidOutLeftToRight() {
    const CollectionLayout layout = BuildCollectionLayout(DevModel("bunny"), CollectionLayoutInput{});
    NIMVLETS_CHECK(layout.items[0].art.x < layout.items[1].art.x);
    NIMVLETS_CHECK(layout.items[1].art.x < layout.items[2].art.x);
    NIMVLETS_CHECK(layout.items[0].art.Right() <= layout.items[1].cell.x + 1.0f);
    for (const auto& box : layout.items) {
        NIMVLETS_CHECK(box.art.x >= 0.0f);
        NIMVLETS_CHECK(box.art.Right() <= layout.viewport.w + 1.0f);
        NIMVLETS_CHECK(box.name.Bottom() <= box.status_.y + 0.5f);
    }
    return true;
}

bool TestHitTestFindsGridItem() {
    const CollectionLayout layout = BuildCollectionLayout(DevModel("bunny"), CollectionLayoutInput{});
    const auto& frin = layout.items[2];
    NIMVLETS_CHECK(layout.HitTest(frin.art.CenterX(), frin.art.CenterY()) == "item:frin");
    // Zona vacía muy abajo -> nada.
    NIMVLETS_CHECK(layout.HitTest(5.0f, layout.viewport.h - 2.0f).empty() ||
                   layout.HitTest(5.0f, 5.0f).empty());
    return true;
}

// Detalle de un pet poseído-inactivo: botón "Use <name>" habilitado y
// en el focus order.
bool TestDetailForOwnedInactivePetEnablesUse() {
    CollectionLayoutInput in;
    in.detailOpen = true;
    in.detailPetId = "nidir";  // locked en DevModel({"bunny","frin"})...
    // nidir NO está en owned -> locked. Usamos frin, que sí es owned-inactive.
    in.detailPetId = "frin";
    const CollectionLayout layout = BuildCollectionLayout(DevModel("bunny"), in);

    NIMVLETS_CHECK(layout.detail.open);
    NIMVLETS_CHECK(layout.detail.petId == "frin");
    NIMVLETS_CHECK(layout.detail.actionEnabled);
    NIMVLETS_CHECK(layout.detail.actionLabel == "Use Frin");
    // Frin tiene chips de variante; el default seleccionado es "male".
    NIMVLETS_CHECK(layout.detail.variants.size() == 2);
    NIMVLETS_CHECK(layout.detail.variants[0].variantId == "male");
    NIMVLETS_CHECK(layout.detail.variants[0].selected);
    NIMVLETS_CHECK(!layout.detail.variants[1].selected);

    // focus order incluye los chips y "use" al final.
    const auto& fo = layout.focusOrder;
    NIMVLETS_CHECK(fo.size() == 6);
    NIMVLETS_CHECK(fo[3] == "variant:male");
    NIMVLETS_CHECK(fo[4] == "variant:female");
    NIMVLETS_CHECK(fo[5] == "use");
    return true;
}

// Detalle del pet activo sin variantes -> botón deshabilitado ("On
// desktop"), NO aparece "use" en el focus order.
bool TestDetailForActivePetDisablesUse() {
    CollectionLayoutInput in;
    in.detailOpen = true;
    in.detailPetId = "bunny";
    const CollectionLayout layout = BuildCollectionLayout(DevModel("bunny"), in);

    NIMVLETS_CHECK(layout.detail.open);
    NIMVLETS_CHECK(!layout.detail.actionEnabled);
    NIMVLETS_CHECK(layout.detail.actionLabel == "On desktop");
    NIMVLETS_CHECK(layout.detail.variants.empty());
    for (const auto& id : layout.focusOrder) {
        NIMVLETS_CHECK(id != "use");
    }
    return true;
}

// Frin ACTIVO como hembra, pero el detalle tiene "male" seleccionado ->
// "Use Frin" se re-habilita (activaría con la otra variante).
bool TestActiveFrinReenablesUseWhenVariantWouldChange() {
    CollectionModel model = DevModel("frin", "female");
    CollectionLayoutInput in;
    in.detailOpen = true;
    in.detailPetId = "frin";
    in.detailSelectedVariantId = "male";
    const CollectionLayout layout = BuildCollectionLayout(model, in);
    NIMVLETS_CHECK(layout.detail.actionEnabled);
    NIMVLETS_CHECK(layout.detail.actionLabel == "Use Frin");

    // Mismo pet, misma variante que la activa -> deshabilitado.
    in.detailSelectedVariantId = "female";
    const CollectionLayout same = BuildCollectionLayout(model, in);
    NIMVLETS_CHECK(!same.detail.actionEnabled);
    NIMVLETS_CHECK(same.detail.actionLabel == "On desktop");
    return true;
}

// Detalle de un pet locked: sin acción, etiqueta humana, sin "use" en
// focus order (block brief §9: no purchase behaviour).
bool TestDetailForLockedPetHasNoAction() {
    CollectionLayoutInput in;
    in.detailOpen = true;
    in.detailPetId = "nidir";
    const CollectionLayout layout = BuildCollectionLayout(DevModel("bunny"), in);
    NIMVLETS_CHECK(layout.detail.open);
    NIMVLETS_CHECK(!layout.detail.actionEnabled);
    NIMVLETS_CHECK(layout.detail.actionLabel == "Not in your collection");
    for (const auto& id : layout.focusOrder) {
        NIMVLETS_CHECK(id != "use");
    }
    return true;
}

bool TestScrollShiftsContentUp() {
    const CollectionModel model = DevModel("bunny");
    const CollectionLayout a = BuildCollectionLayout(model, CollectionLayoutInput{});
    CollectionLayoutInput scrolled;
    scrolled.scrollY = 50.0f;
    const CollectionLayout b = BuildCollectionLayout(model, scrolled);
    NIMVLETS_CHECK(b.items[0].art.y == a.items[0].art.y - 50.0f);
    // contentHeight es independiente del scroll aplicado.
    NIMVLETS_CHECK(b.contentHeight == a.contentHeight);
    return true;
}

}  // namespace

void RegisterCollectionLayoutTests(testing::TestRunner& runner) {
    runner.Add("CollectionLayout/StatusLabelsAreHuman", TestStatusLabelsAreHuman);
    runner.Add("CollectionLayout/GridHasOneBoxPerLogicalPet", TestGridHasOneBoxPerLogicalPet);
    runner.Add("CollectionLayout/GridBoxesAreLaidOutLeftToRight", TestGridBoxesAreLaidOutLeftToRight);
    runner.Add("CollectionLayout/HitTestFindsGridItem", TestHitTestFindsGridItem);
    runner.Add("CollectionLayout/DetailForOwnedInactivePetEnablesUse", TestDetailForOwnedInactivePetEnablesUse);
    runner.Add("CollectionLayout/DetailForActivePetDisablesUse", TestDetailForActivePetDisablesUse);
    runner.Add("CollectionLayout/ActiveFrinReenablesUseWhenVariantWouldChange",
               TestActiveFrinReenablesUseWhenVariantWouldChange);
    runner.Add("CollectionLayout/DetailForLockedPetHasNoAction", TestDetailForLockedPetHasNoAction);
    runner.Add("CollectionLayout/ScrollShiftsContentUp", TestScrollShiftsContentUp);
}

}  // namespace nimvlets::tests
