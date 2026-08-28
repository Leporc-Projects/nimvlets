#include "CollectionLayoutTest.h"

#include "catalog/CollectionModel.h"
#include "catalog/PetEntitlement.h"
#include "core/Localization.h"
#include "productui/CollectionLayout.h"

#include <string>
#include <vector>

using nimvlets::catalog::BuildCollectionModel;
using nimvlets::catalog::CatalogEntry;
using nimvlets::catalog::CollectionModel;
using nimvlets::catalog::OwnershipStatus;
using nimvlets::catalog::PetCatalog;
using nimvlets::catalog::PetEntitlement;
using nimvlets::catalog::PetIdentity;
using nimvlets::core::Language;
using nimvlets::productui::BuildCollectionLayout;
using nimvlets::productui::CollectionLayout;
using nimvlets::productui::CollectionLayoutInput;
using nimvlets::productui::StatusText;

using Ents = std::vector<PetEntitlement>;

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
    // El owner tras la migración: Bunny + Frin como PET ENTERO (las dos
    // variantes de Frin poseídas).
    const Ents owned = {PetEntitlement{"bunny", ""}, PetEntitlement{"frin", ""}};
    return BuildCollectionModel(MakeDevCatalog(), owned, PetIdentity{activePetId, activeVariant});
}

// Un modelo donde SOLO Frin macho está poseído (hembra no) — para el
// estado "variante no disponible" del hero (brief §6).
CollectionModel FrinMaleOnlyModel(const std::string& activePetId) {
    const Ents owned = {PetEntitlement{"bunny", ""}, PetEntitlement{"frin", "male"}};
    return BuildCollectionModel(MakeDevCatalog(), owned, PetIdentity{activePetId, ""});
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

    // Block 07: las pestañas de navegación van primero en el orden de
    // tabulación, luego los widgets del hero, luego la gallery.
    NIMVLETS_CHECK(layout.focusOrder.size() == 7);
    NIMVLETS_CHECK(layout.focusOrder[0] == "nav:collection");
    NIMVLETS_CHECK(layout.focusOrder[1] == "nav:shop");
    NIMVLETS_CHECK(layout.focusOrder[2] == "variant:male");
    NIMVLETS_CHECK(layout.focusOrder[3] == "variant:female");
    NIMVLETS_CHECK(layout.focusOrder[4] == "use");
    NIMVLETS_CHECK(layout.focusOrder[5] == "item:bunny");
    NIMVLETS_CHECK(layout.focusOrder[6] == "item:nidir");
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
    // Block 07: descripción de un par de frases (brief §19).
    NIMVLETS_CHECK(frinEn.hero.descriptionText.rfind("Watchful, calm, and happiest close by.", 0) == 0);
    NIMVLETS_CHECK(frinEn.hero.descriptionText.find("quiet wolf") != std::string::npos);

    CollectionLayoutInput es = en;
    es.language = Language::kEs;
    const auto frinEs = BuildCollectionLayout(DevModel("bunny"), es);
    NIMVLETS_CHECK(frinEs.hero.speciesText == "Lobo blanco");
    NIMVLETS_CHECK(frinEs.hero.descriptionText.rfind("Atento, tranquilo y más feliz cerca.", 0) == 0);
    NIMVLETS_CHECK(frinEs.hero.descriptionText.find("lobo sereno") != std::string::npos);
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
// scroll para ver el hero + la gallery, incluso con la descripción
// editorial más larga de Block 07 (brief §19).
bool TestDefaultLayoutFitsWithoutScroll() {
    const CollectionLayout layout = BuildCollectionLayout(DevModel("frin", "male"), CollectionLayoutInput{});
    NIMVLETS_CHECK(layout.contentHeight <= 560.0f + 1.0f);
    return true;
}

// Block 07: la cabecera trae las pestañas "Collection · Shop"
// localizadas, con la de la sección actual marcada activa, y ambas en el
// focus order + hit-testeables.
bool TestSectionNavTabs() {
    CollectionLayoutInput es;
    es.language = Language::kEs;
    const CollectionLayout layout = BuildCollectionLayout(DevModel("bunny"), es);
    NIMVLETS_CHECK(layout.header.tabs.size() == 2);
    NIMVLETS_CHECK(layout.header.tabs[0].label == "Colección");
    NIMVLETS_CHECK(layout.header.tabs[0].active);
    NIMVLETS_CHECK(layout.header.tabs[0].focusId == "nav:collection");
    NIMVLETS_CHECK(layout.header.tabs[1].label == "Tienda");
    NIMVLETS_CHECK(!layout.header.tabs[1].active);
    NIMVLETS_CHECK(layout.header.tabs[1].focusId == "nav:shop");

    NIMVLETS_CHECK(Has(layout.focusOrder, "nav:collection"));
    NIMVLETS_CHECK(Has(layout.focusOrder, "nav:shop"));
    const auto& shopTab = layout.header.tabs[1];
    NIMVLETS_CHECK(layout.HitTest(shopTab.hitRect.CenterX(), shopTab.hitRect.CenterY()) == "nav:shop");
    return true;
}

// Frin con SOLO macho poseído, hero mostrando la hembra (no poseída):
// sin botón "Use", estado contenido "Not in your collection", y "use"
// fuera del focus order — sin ninguna ruta de compra (brief §6).
bool TestUnownedVariantHasRestrainedStateNoAction() {
    CollectionLayoutInput in;
    in.selectedPetId = "frin";
    in.selectedVariantId = "female";
    const CollectionLayout layout = BuildCollectionLayout(FrinMaleOnlyModel("bunny"), in);
    NIMVLETS_CHECK(layout.hero.petId == "frin");
    NIMVLETS_CHECK(!layout.hero.actionEnabled);
    NIMVLETS_CHECK(layout.hero.showStatusLine);
    NIMVLETS_CHECK(layout.hero.statusText == "Not in your collection");
    NIMVLETS_CHECK(!Has(layout.focusOrder, "use"));
    // El chip de la hembra existe pero marcado no poseído; el macho sí.
    NIMVLETS_CHECK(layout.hero.variants.size() == 2);
    NIMVLETS_CHECK(layout.hero.variants[0].variantId == "male" && layout.hero.variants[0].owned);
    NIMVLETS_CHECK(layout.hero.variants[1].variantId == "female" && !layout.hero.variants[1].owned);
    return true;
}

// La MISMA Frin, hero mostrando el macho (poseído): "Use Frin"
// habilitado normalmente.
bool TestOwnedVariantOfPartiallyOwnedFrinEnablesUse() {
    CollectionLayoutInput in;
    in.selectedPetId = "frin";
    in.selectedVariantId = "male";
    const CollectionLayout layout = BuildCollectionLayout(FrinMaleOnlyModel("bunny"), in);
    NIMVLETS_CHECK(layout.hero.actionEnabled);
    NIMVLETS_CHECK(layout.hero.actionLabel == "Use Frin");
    NIMVLETS_CHECK(Has(layout.focusOrder, "use"));
    return true;
}

// La descripción del hero reserva alto para varias líneas (word-wrap lo
// hace la vista) — el bloque de texto crece con la copy más larga.
bool TestDescriptionReservesMultipleLines() {
    const CollectionLayout layout = BuildCollectionLayout(DevModel("nidir"), CollectionLayoutInput{});
    NIMVLETS_CHECK(!layout.hero.descriptionText.empty());
    NIMVLETS_CHECK(layout.hero.descriptionAnchor.h >= 34.0f);  // >= 2 líneas
    NIMVLETS_CHECK(layout.hero.descriptionAnchor.w > 100.0f);
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
    runner.Add("CollectionLayout/SectionNavTabs", TestSectionNavTabs);
    runner.Add("CollectionLayout/UnownedVariantHasRestrainedStateNoAction",
               TestUnownedVariantHasRestrainedStateNoAction);
    runner.Add("CollectionLayout/OwnedVariantOfPartiallyOwnedFrinEnablesUse",
               TestOwnedVariantOfPartiallyOwnedFrinEnablesUse);
    runner.Add("CollectionLayout/DescriptionReservesMultipleLines", TestDescriptionReservesMultipleLines);
}

}  // namespace nimvlets::tests
