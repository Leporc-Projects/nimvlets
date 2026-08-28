#include "CollectionLayoutTest.h"

#include "catalog/CollectionModel.h"
#include "core/Localization.h"
#include "productui/CollectionLayout.h"

#include <string>
#include <vector>

using nimvlets::catalog::BuildCollectionModel;
using nimvlets::catalog::CatalogEntry;
using nimvlets::catalog::CollectionModel;
using nimvlets::catalog::OwnershipStatus;
using nimvlets::catalog::PetCatalog;
using nimvlets::catalog::PetIdentity;
using nimvlets::core::Language;
using nimvlets::productui::BuildCollectionLayout;
using nimvlets::productui::CollectionLayout;
using nimvlets::productui::CollectionLayoutInput;
using nimvlets::productui::StatusText;

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

bool Has(const std::vector<std::string>& v, const std::string& s) {
    for (const auto& x : v) {
        if (x == s) {
            return true;
        }
    }
    return false;
}

bool TestStatusTextLocalized() {
    NIMVLETS_CHECK(std::string(StatusText(OwnershipStatus::kActive, Language::kEn)) == "On desktop");
    NIMVLETS_CHECK(std::string(StatusText(OwnershipStatus::kActive, Language::kEs)) == "En el escritorio");
    NIMVLETS_CHECK(std::string(StatusText(OwnershipStatus::kOwnedInactive, Language::kEn)) == "Use");
    NIMVLETS_CHECK(std::string(StatusText(OwnershipStatus::kOwnedInactive, Language::kEs)) == "Usar");
    NIMVLETS_CHECK(std::string(StatusText(OwnershipStatus::kLocked, Language::kEs)) == "No está en tu colección");
    return true;
}

// Sin selección explícita, el hero es el pet ACTIVO; la gallery son los
// otros dos, en orden de catálogo.
bool TestHeroDefaultsToActivePetGalleryHasTheRest() {
    const CollectionLayout layout = BuildCollectionLayout(DevModel("bunny"), CollectionLayoutInput{});
    NIMVLETS_CHECK(layout.hero.petId == "bunny");
    NIMVLETS_CHECK(layout.hero.status == OwnershipStatus::kActive);
    NIMVLETS_CHECK(layout.gallery.size() == 2);
    NIMVLETS_CHECK(layout.gallery[0].petId == "nidir");
    NIMVLETS_CHECK(layout.gallery[1].petId == "frin");
    // El hero NUNCA aparece en la gallery.
    NIMVLETS_CHECK(layout.FindGalleryItem("bunny") == nullptr);
    return true;
}

// selectedPetId elige el hero; el que era hero por defecto pasa a la
// gallery.
bool TestSelectedPetBecomesHero() {
    CollectionLayoutInput in;
    in.selectedPetId = "frin";
    const CollectionLayout layout = BuildCollectionLayout(DevModel("bunny"), in);
    NIMVLETS_CHECK(layout.hero.petId == "frin");
    NIMVLETS_CHECK(layout.gallery.size() == 2);
    NIMVLETS_CHECK(layout.FindGalleryItem("bunny") != nullptr);
    NIMVLETS_CHECK(layout.FindGalleryItem("nidir") != nullptr);
    NIMVLETS_CHECK(layout.FindGalleryItem("frin") == nullptr);
    return true;
}

// selectedPetId desconocido -> cae al pet activo, sin crashear.
bool TestUnknownSelectionFallsBackToActive() {
    CollectionLayoutInput in;
    in.selectedPetId = "ghost";
    const CollectionLayout layout = BuildCollectionLayout(DevModel("frin", "male"), in);
    NIMVLETS_CHECK(layout.hero.petId == "frin");
    return true;
}

// Hero = pet activo sin variantes -> acción deshabilitada ("On
// desktop"), "use" NO está en el focus order.
bool TestHeroActivePetDisablesUse() {
    const CollectionLayout layout = BuildCollectionLayout(DevModel("bunny"), CollectionLayoutInput{});
    NIMVLETS_CHECK(!layout.hero.actionEnabled);
    NIMVLETS_CHECK(layout.hero.actionLabel == "On desktop");
    NIMVLETS_CHECK(layout.hero.variants.empty());
    NIMVLETS_CHECK(!Has(layout.focusOrder, "use"));
    return true;
}

// Hero = pet poseído-inactivo -> "Use Frin" habilitado, chips de
// variante, focus order = variantes + use + items de gallery.
bool TestHeroOwnedInactiveEnablesUse() {
    CollectionLayoutInput in;
    in.selectedPetId = "frin";
    const CollectionLayout layout = BuildCollectionLayout(DevModel("bunny"), in);

    NIMVLETS_CHECK(layout.hero.actionEnabled);
    NIMVLETS_CHECK(layout.hero.actionLabel == "Use Frin");  // nombre propio NO traducido
    NIMVLETS_CHECK(layout.hero.variants.size() == 2);
    NIMVLETS_CHECK(layout.hero.variants[0].variantId == "male");
    NIMVLETS_CHECK(layout.hero.variants[0].label == "Male");
    NIMVLETS_CHECK(layout.hero.variants[0].selected);
    NIMVLETS_CHECK(!layout.hero.variants[1].selected);

    NIMVLETS_CHECK(layout.focusOrder.size() == 5);  // variant:male, variant:female, use, item:bunny, item:nidir
    NIMVLETS_CHECK(layout.focusOrder[0] == "variant:male");
    NIMVLETS_CHECK(layout.focusOrder[1] == "variant:female");
    NIMVLETS_CHECK(layout.focusOrder[2] == "use");
    NIMVLETS_CHECK(layout.focusOrder[3] == "item:bunny");
    NIMVLETS_CHECK(layout.focusOrder[4] == "item:nidir");
    return true;
}

// Frin ACTIVO como hembra, hero muestra "male" -> "Use Frin" se
// re-habilita (activaría con la otra variante). Misma variante que la
// activa -> deshabilitado ("On desktop").
bool TestActiveFrinReenablesUseWhenVariantWouldChange() {
    const CollectionModel model = DevModel("frin", "female");
    CollectionLayoutInput in;
    in.selectedPetId = "frin";
    in.selectedVariantId = "male";
    NIMVLETS_CHECK(BuildCollectionLayout(model, in).hero.actionEnabled);
    NIMVLETS_CHECK(BuildCollectionLayout(model, in).hero.actionLabel == "Use Frin");

    in.selectedVariantId = "female";
    NIMVLETS_CHECK(!BuildCollectionLayout(model, in).hero.actionEnabled);
    NIMVLETS_CHECK(BuildCollectionLayout(model, in).hero.actionLabel == "On desktop");
    return true;
}

// Hero = pet locked -> sin acción, etiqueta humana localizada, "use"
// NO en el focus order (brief §12: no purchase behaviour).
bool TestHeroLockedHasNoAction() {
    CollectionLayoutInput in;
    in.selectedPetId = "nidir";
    in.language = Language::kEs;
    const CollectionLayout layout = BuildCollectionLayout(DevModel("bunny"), in);
    NIMVLETS_CHECK(layout.hero.petId == "nidir");
    NIMVLETS_CHECK(!layout.hero.actionEnabled);
    NIMVLETS_CHECK(layout.hero.actionLabel == "No está en tu colección");
    NIMVLETS_CHECK(!Has(layout.focusOrder, "use"));
    // Un locked SÍ tiene arte ahora (brief §12) -> la vista lo carga;
    // el layout solo marca el estado.
    NIMVLETS_CHECK(layout.hero.status == OwnershipStatus::kLocked);
    return true;
}

// El hero lleva el acento de identidad del pet; la gallery también (por
// entrada). Bunny cálido, Frin frío.
bool TestHeroAndGalleryCarryPetAccent() {
    const CollectionLayout bunnyHero = BuildCollectionLayout(DevModel("bunny"), CollectionLayoutInput{});
    NIMVLETS_CHECK(bunnyHero.hero.accent.line.r > bunnyHero.hero.accent.line.b);  // Bunny cálido

    const auto* frinInGallery = bunnyHero.FindGalleryItem("frin");
    NIMVLETS_CHECK(frinInGallery != nullptr);
    NIMVLETS_CHECK(frinInGallery->accentLine.b > frinInGallery->accentLine.r);  // Frin frío
    return true;
}

// El texto del hero se localiza (species + status), los nombres propios
// no.
bool TestHeroTextLocalized() {
    CollectionLayoutInput es;
    es.selectedPetId = "nidir";
    es.language = Language::kEs;
    const CollectionLayout layout = BuildCollectionLayout(DevModel("bunny"), es);
    NIMVLETS_CHECK(layout.hero.displayName == "Nidir");            // nombre propio
    NIMVLETS_CHECK(layout.hero.speciesText == "Dragón negro");     // especie provisional, localizada
    NIMVLETS_CHECK(layout.hero.statusText == "No está en tu colección");
    return true;
}

// Block 06.2 §18: NUNCA se muestran a la vez la línea "Use" y el botón
// "Use <pet>". showStatusLine == !actionEnabled, sin excepción.
bool TestNoDuplicateUseStatusAndButton() {
    // Owned-inactive: botón, sin línea de estado.
    CollectionLayoutInput ownedInactive;
    ownedInactive.selectedPetId = "frin";
    const auto oi = BuildCollectionLayout(DevModel("bunny"), ownedInactive);
    NIMVLETS_CHECK(oi.hero.actionEnabled);
    NIMVLETS_CHECK(!oi.hero.showStatusLine);

    // Activo, misma variante: línea de estado ("On desktop"), sin botón.
    const auto active = BuildCollectionLayout(DevModel("bunny"), CollectionLayoutInput{});
    NIMVLETS_CHECK(!active.hero.actionEnabled);
    NIMVLETS_CHECK(active.hero.showStatusLine);

    // Locked: línea de estado, sin botón.
    CollectionLayoutInput locked;
    locked.selectedPetId = "nidir";
    const auto lk = BuildCollectionLayout(DevModel("bunny"), locked);
    NIMVLETS_CHECK(!lk.hero.actionEnabled);
    NIMVLETS_CHECK(lk.hero.showStatusLine);

    // Frin activo con otra variante elegida: botón (cambiaría variante),
    // sin línea de estado.
    CollectionLayoutInput frinOther;
    frinOther.selectedPetId = "frin";
    frinOther.selectedVariantId = "male";
    const auto fo = BuildCollectionLayout(DevModel("frin", "female"), frinOther);
    NIMVLETS_CHECK(fo.hero.actionEnabled);
    NIMVLETS_CHECK(!fo.hero.showStatusLine);
    return true;
}

// El hero de los tres pets con arte real trae especie + descripción
// aprobadas; se localizan (brief §13-§16).
bool TestHeroCarriesApprovedEditorial() {
    CollectionLayoutInput en;
    en.selectedPetId = "frin";
    const auto frinEn = BuildCollectionLayout(DevModel("bunny"), en);
    NIMVLETS_CHECK(frinEn.hero.speciesText == "White wolf");
    NIMVLETS_CHECK(frinEn.hero.descriptionText == "Watchful, calm, and happiest close by.");

    CollectionLayoutInput es = en;
    es.language = Language::kEs;
    const auto frinEs = BuildCollectionLayout(DevModel("bunny"), es);
    NIMVLETS_CHECK(frinEs.hero.speciesText == "Lobo blanco");
    NIMVLETS_CHECK(frinEs.hero.descriptionText == "Atento, tranquilo y más feliz cerca.");
    return true;
}

// El hero stage: una primitiva de fondo que se extiende bastante MÁS
// que el arte (brief §11: "extending farther than the current circle"),
// más una secundaria descentrada, más la regla de acento fina.
bool TestHeroStageExtendsBeyondArt() {
    const auto layout = BuildCollectionLayout(DevModel("bunny"), CollectionLayoutInput{});
    const auto& h = layout.hero;
    // La primaria es un halo asimétrico que se extiende bastante más que
    // el arte, pero sin invadir la columna de texto.
    NIMVLETS_CHECK(h.stagePrimary.w > h.art.w + 30.0f);
    NIMVLETS_CHECK(h.stagePrimary.x < h.art.x);
    NIMVLETS_CHECK(h.stagePrimary.Right() > h.art.Right());
    NIMVLETS_CHECK(h.stagePrimary.Right() <= h.nameAnchor.x);  // no invade el texto
    // La secundaria es más chica y está descentrada respecto del arte
    // (su centro cae abajo-derecha del centro del arte) — asimetría.
    NIMVLETS_CHECK(h.stageSecondary.w < h.stagePrimary.w);
    NIMVLETS_CHECK(h.stageSecondary.CenterX() > h.art.CenterX());
    NIMVLETS_CHECK(h.stageSecondary.CenterY() > h.art.CenterY());
    // Regla de acento fina bajo el nombre.
    NIMVLETS_CHECK(h.nameRule.h <= 3.0f && h.nameRule.w > 0.0f);
    return true;
}

// Segundo plano: el fondo de la gallery cubre desde el divisor hacia
// abajo, ancho completo (brief §12).
bool TestGalleryShelfIsSecondPlane() {
    const auto layout = BuildCollectionLayout(DevModel("bunny"), CollectionLayoutInput{});
    NIMVLETS_CHECK(layout.galleryShelf.x == 0.0f);
    NIMVLETS_CHECK(layout.galleryShelf.w >= 800.0f);
    NIMVLETS_CHECK(layout.galleryShelf.y == layout.dividerRect.y);
    NIMVLETS_CHECK(layout.galleryShelf.Bottom() >= layout.contentHeight);
    return true;
}

// Cada pedestal de la gallery lleva un tinte de identidad del pet, no el
// mismo cuadro neutro para todos (brief §20).
bool TestGalleryPedestalCarriesPetTint() {
    const auto layout = BuildCollectionLayout(DevModel("bunny"), CollectionLayoutInput{});
    const auto* frin = layout.FindGalleryItem("frin");
    const auto* nidir = layout.FindGalleryItem("nidir");
    NIMVLETS_CHECK(frin != nullptr && nidir != nullptr);
    NIMVLETS_CHECK(!(frin->pedestalTint == nidir->pedestalTint));
    NIMVLETS_CHECK(frin->pedestalTint.a == 255);  // el alpha bajo lo aplica la vista
    return true;
}

bool TestHitTestFindsGalleryItemAndHeroWidgets() {
    CollectionLayoutInput in;
    in.selectedPetId = "frin";
    const CollectionLayout layout = BuildCollectionLayout(DevModel("bunny"), in);

    const auto* bunnyGal = layout.FindGalleryItem("bunny");
    NIMVLETS_CHECK(layout.HitTest(bunnyGal->art.CenterX(), bunnyGal->art.CenterY()) == "item:bunny");

    const auto& maleChip = layout.hero.variants[0];
    NIMVLETS_CHECK(layout.HitTest(maleChip.rect.CenterX(), maleChip.rect.CenterY()) == "variant:male");

    NIMVLETS_CHECK(layout.HitTest(layout.hero.actionButton.CenterX(), layout.hero.actionButton.CenterY()) == "use");

    // Muy arriba (cabecera) -> nada accionable.
    NIMVLETS_CHECK(layout.HitTest(5.0f, 5.0f).empty());
    return true;
}

// El micro-lift de hover: la entrada de gallery bajo el mouse se
// dibuja 2pt más arriba. Instantáneo, sin timer (brief §11).
bool TestHoverLiftShiftsGalleryItemUp() {
    CollectionLayoutInput plain;
    const CollectionLayout a = BuildCollectionLayout(DevModel("bunny"), plain);

    CollectionLayoutInput hovered = plain;
    hovered.hoverPetId = "frin";
    const CollectionLayout b = BuildCollectionLayout(DevModel("bunny"), hovered);

    const auto* frinA = a.FindGalleryItem("frin");
    const auto* frinB = b.FindGalleryItem("frin");
    NIMVLETS_CHECK(frinB->art.y == frinA->art.y - 2.0f);
    // El item NO hovereado no se mueve.
    NIMVLETS_CHECK(b.FindGalleryItem("nidir")->art.y == a.FindGalleryItem("nidir")->art.y);
    return true;
}

bool TestScrollShiftsContentUp() {
    const CollectionModel model = DevModel("bunny");
    const CollectionLayout base = BuildCollectionLayout(model, CollectionLayoutInput{});
    CollectionLayoutInput scrolled;
    scrolled.scrollY = 50.0f;
    const CollectionLayout s = BuildCollectionLayout(model, scrolled);
    NIMVLETS_CHECK(s.hero.art.y == base.hero.art.y - 50.0f);
    NIMVLETS_CHECK(s.contentHeight == base.contentHeight);  // independiente del scroll aplicado
    return true;
}

// El contenido cabe en la ventana por defecto (800x560): no hace falta
// scroll para ver el hero + la gallery.
bool TestDefaultLayoutFitsWithoutScroll() {
    const CollectionLayout layout = BuildCollectionLayout(DevModel("frin", "male"), CollectionLayoutInput{});
    NIMVLETS_CHECK(layout.contentHeight <= 560.0f + 1.0f);
    return true;
}

}  // namespace

void RegisterCollectionLayoutTests(testing::TestRunner& runner) {
    runner.Add("CollectionLayout/StatusTextLocalized", TestStatusTextLocalized);
    runner.Add("CollectionLayout/HeroDefaultsToActivePetGalleryHasTheRest", TestHeroDefaultsToActivePetGalleryHasTheRest);
    runner.Add("CollectionLayout/SelectedPetBecomesHero", TestSelectedPetBecomesHero);
    runner.Add("CollectionLayout/UnknownSelectionFallsBackToActive", TestUnknownSelectionFallsBackToActive);
    runner.Add("CollectionLayout/HeroActivePetDisablesUse", TestHeroActivePetDisablesUse);
    runner.Add("CollectionLayout/HeroOwnedInactiveEnablesUse", TestHeroOwnedInactiveEnablesUse);
    runner.Add("CollectionLayout/ActiveFrinReenablesUseWhenVariantWouldChange",
               TestActiveFrinReenablesUseWhenVariantWouldChange);
    runner.Add("CollectionLayout/HeroLockedHasNoAction", TestHeroLockedHasNoAction);
    runner.Add("CollectionLayout/HeroAndGalleryCarryPetAccent", TestHeroAndGalleryCarryPetAccent);
    runner.Add("CollectionLayout/HeroTextLocalized", TestHeroTextLocalized);
    runner.Add("CollectionLayout/NoDuplicateUseStatusAndButton", TestNoDuplicateUseStatusAndButton);
    runner.Add("CollectionLayout/HeroCarriesApprovedEditorial", TestHeroCarriesApprovedEditorial);
    runner.Add("CollectionLayout/HeroStageExtendsBeyondArt", TestHeroStageExtendsBeyondArt);
    runner.Add("CollectionLayout/GalleryShelfIsSecondPlane", TestGalleryShelfIsSecondPlane);
    runner.Add("CollectionLayout/GalleryPedestalCarriesPetTint", TestGalleryPedestalCarriesPetTint);
    runner.Add("CollectionLayout/HitTestFindsGalleryItemAndHeroWidgets", TestHitTestFindsGalleryItemAndHeroWidgets);
    runner.Add("CollectionLayout/HoverLiftShiftsGalleryItemUp", TestHoverLiftShiftsGalleryItemUp);
    runner.Add("CollectionLayout/ScrollShiftsContentUp", TestScrollShiftsContentUp);
    runner.Add("CollectionLayout/DefaultLayoutFitsWithoutScroll", TestDefaultLayoutFitsWithoutScroll);
}

}  // namespace nimvlets::tests
