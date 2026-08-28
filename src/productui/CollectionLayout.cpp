#include "productui/CollectionLayout.h"

#include <algorithm>
#include <cctype>

#include "productui/PetEditorial.h"

namespace nimvlets::productui {

using catalog::CollectionItem;
using catalog::CollectionModel;
using catalog::CollectionVariant;
using catalog::OwnershipStatus;
using core::Language;
using core::Localized;
using core::StringKey;

namespace {

// Métricas en PUNTOS lógicos. Composición hero + gallery: jerarquía por
// tamaño/espacio, no por más contenedores (brief §7/§17). Block 06.2:
// hero stage más presente y proporciones que usan el ancho (§11/§21).
// Block 07: la cabecera (título + balance + pestañas Collection/Shop)
// la aporta SectionNav; el hero empieza en header.bodyTop.
constexpr float kMargin = 40.0f;

constexpr float kHeroArt = 216.0f;
constexpr float kHeroTextGap = 40.0f;   // arte -> columna de texto

constexpr float kHeroNameH = 32.0f;
constexpr float kHeroRuleGap = 12.0f;   // base del nombre -> regla de acento
constexpr float kHeroRuleW = 46.0f;
constexpr float kHeroRuleH = 2.0f;
constexpr float kSpeciesGap = 12.0f;
constexpr float kSpeciesH = 16.0f;
constexpr float kDescGap = 8.0f;
// Block 07: la copy editorial pasó de una frase a un par de frases
// (brief §19). Se reserva alto para hasta kDescLines líneas envueltas;
// la vista hace el word-wrap real (DrawTextWrapped) dentro de
// descriptionAnchor.w.
constexpr float kDescLineH = 17.0f;
constexpr int kDescLines = 3;
constexpr float kBlockGap = 14.0f;      // texto -> chips/acción
constexpr float kHeroChipH = 26.0f;
constexpr float kHeroChipPadX = 10.0f;
constexpr float kHeroChipGap = 22.0f;   // incluye el "·" separador
constexpr float kChipToAction = 16.0f;
constexpr float kHeroButtonH = 36.0f;
constexpr float kHeroButtonPadX = 22.0f;
constexpr float kHeroStatusH = 18.0f;

constexpr float kDividerGap = 24.0f;
constexpr float kGalleryGap = 22.0f;
constexpr float kGalleryColMax = 208.0f;  // el cluster de gallery no se estira a lo ancho
constexpr float kGalleryArt = 92.0f;
constexpr float kGalleryNameH = 17.0f;
constexpr float kGalleryStatusH = 14.0f;
constexpr float kGalleryArtToName = 12.0f;
constexpr float kGalleryNameToStatus = 4.0f;
constexpr float kHoverLift = 2.0f;  // micro-lift instantáneo (brief §11/§20)

// Ancho aproximado por carácter para dimensionar botón/chip sin medir
// texto real acá — la vista lo ajusta; alcanza para el hit-test.
constexpr float kApproxCharW = 8.0f;

std::string Capitalize(const std::string& s) {
    if (s.empty()) {
        return s;
    }
    std::string out = s;
    out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
    return out;
}

std::string VariantLabel(const std::string& variantId, Language lang) {
    if (variantId == "male") {
        return Localized(StringKey::kMale, lang);
    }
    if (variantId == "female") {
        return Localized(StringKey::kFemale, lang);
    }
    return Capitalize(variantId);
}

std::string ResolveVariant(const CollectionItem& item, const std::string& requested) {
    const bool valid = std::any_of(item.variants.begin(), item.variants.end(),
                                   [&](const CollectionVariant& v) { return v.variantId == requested; });
    return valid ? requested : item.selectedVariantId;
}

}  // namespace

const char* StatusText(OwnershipStatus status, Language lang) {
    switch (status) {
        case OwnershipStatus::kActive:
            return Localized(StringKey::kOnDesktop, lang);
        case OwnershipStatus::kOwnedInactive:
            return Localized(StringKey::kUse, lang);
        case OwnershipStatus::kLocked:
            return Localized(StringKey::kNotInCollection, lang);
    }
    return "";
}

const GalleryItem* CollectionLayout::FindGalleryItem(const std::string& petId) const {
    for (const GalleryItem& g : gallery) {
        if (g.petId == petId) {
            return &g;
        }
    }
    return nullptr;
}

std::string CollectionLayout::HitTest(float x, float y) const {
    for (const SectionTab& tab : header.tabs) {
        if (tab.hitRect.Contains(x, y)) {
            return tab.focusId;
        }
    }
    for (const HeroVariantChip& chip : hero.variants) {
        if (chip.rect.Contains(x, y)) {
            return chip.focusId;
        }
    }
    if (hero.actionEnabled && hero.actionButton.Contains(x, y)) {
        return hero.actionFocusId;
    }
    for (const GalleryItem& g : gallery) {
        if (g.cell.Contains(x, y)) {
            return g.focusId;
        }
    }
    return "";
}

float ClampScroll(float scrollY, float contentHeight, float viewportH) {
    return std::clamp(scrollY, 0.0f, std::max(0.0f, contentHeight - viewportH));
}

CollectionLayout BuildCollectionLayout(const CollectionModel& model, const CollectionLayoutInput& in) {
    const Language lang = in.language;
    const float sy = in.scrollY;

    CollectionLayout out;
    out.viewport = UiRect{0.0f, 0.0f, in.viewportW, in.viewportH};
    const float contentW = std::max(160.0f, in.viewportW - 2.0f * kMargin);

    out.header = BuildSectionHeaderLayout(in.viewportW, kMargin, sy, ProductSection::kCollection, lang);
    for (const SectionTab& tab : out.header.tabs) {
        out.focusOrder.push_back(tab.focusId);
    }

    if (model.items.empty()) {
        out.contentHeight = out.header.bodyTop + sy;
        return out;
    }

    // --- Resolver el hero ---
    std::string heroPetId = in.selectedPetId;
    if (model.Find(heroPetId) == nullptr) {
        heroPetId = model.activePetId;
        if (model.Find(heroPetId) == nullptr) {
            heroPetId = model.items.front().petId;
        }
    }
    const CollectionItem& heroItem = *model.Find(heroPetId);

    CollectionHero& h = out.hero;
    h.petId = heroItem.petId;
    h.displayName = heroItem.displayName;
    h.speciesText = Species(heroItem.petId, lang);
    h.descriptionText = ShortDescription(heroItem.petId, lang);
    h.status = heroItem.status;
    h.accent = PetAccentFor(heroItem.petId);

    const float heroTop = out.header.bodyTop;
    h.art = UiRect{kMargin, heroTop, kHeroArt, kHeroArt};

    const float textX = h.art.Right() + kHeroTextGap;
    const float textW = std::max(160.0f, kMargin + contentW - textX);

    const std::string selectedVariant = ResolveVariant(heroItem, in.selectedVariantId);
    h.selectedVariantId = selectedVariant;
    const bool selectedVariantOwned =
        heroItem.HasVariants() ? heroItem.VariantOwned(selectedVariant)
                               : (heroItem.variants.front().owned ||
                                  heroItem.status == OwnershipStatus::kActive);

    // Estado / acción: el botón se dibuja SOLO cuando activar haría algo
    // (owned-inactive con la variante poseída, o el pet activo con otra
    // variante elegida). Si no, solo la línea de estado — nunca las dos
    // cosas (brief §18). Una variante NO poseída de un Frin por lo demás
    // poseído: estado contenido "Not in your collection", sin botón y
    // SIN ninguna ruta de compra visible (brief §6 — el shop oculto de
    // starters es trabajo futuro).
    const bool activePet = heroItem.status == OwnershipStatus::kActive;
    const bool variantWouldChange =
        heroItem.HasVariants() && activePet && selectedVariant != model.activeVariantId;
    if (heroItem.status == OwnershipStatus::kLocked || !selectedVariantOwned) {
        h.actionLabel = Localized(StringKey::kNotInCollection, lang);
        h.actionEnabled = false;
    } else if (activePet && !variantWouldChange) {
        h.actionLabel = Localized(StringKey::kOnDesktop, lang);
        h.actionEnabled = false;
    } else {
        h.actionLabel = std::string(Localized(StringKey::kUsePetPrefix, lang)) + heroItem.displayName;
        h.actionEnabled = true;
    }
    // La línea de estado del hero: para una variante no poseída de un
    // pet por lo demás poseído, mostrar el texto "no está en tu
    // colección" aunque el status a nivel de pet sea kOwnedInactive.
    h.statusText = (!selectedVariantOwned && heroItem.status != OwnershipStatus::kLocked)
                       ? Localized(StringKey::kNotInCollection, lang)
                       : StatusText(heroItem.status, lang);
    h.showStatusLine = !h.actionEnabled;
    h.actionFocusId = "use";

    const bool hasSpecies = !h.speciesText.empty();
    const bool hasDesc = !h.descriptionText.empty();
    const bool hasChips = heroItem.HasVariants();

    // Alto del bloque de texto, para centrarlo verticalmente contra el
    // arte (así el hero no queda pesado arriba — brief §8/§26).
    float blockH = kHeroNameH + kHeroRuleGap + kHeroRuleH;
    if (hasSpecies) {
        blockH += kSpeciesGap + kSpeciesH;
    }
    if (hasDesc) {
        blockH += kDescGap + kDescLineH * static_cast<float>(kDescLines);
    }
    blockH += kBlockGap;
    if (hasChips) {
        blockH += kHeroChipH + kChipToAction;
    }
    blockH += h.actionEnabled ? kHeroButtonH : kHeroStatusH;

    const float blockTop = heroTop + std::max(0.0f, (kHeroArt - blockH) * 0.5f);

    // Hero stage: un halo asimétrico ALREDEDOR del arte — se extiende
    // bastante más que un círculo, pero NO invade la columna de texto (el
    // nombre roza su borde derecho; especie/descripción/acción quedan
    // sobre el fondo cálido limpio). Más una primitiva secundaria más
    // chica descentrada hacia abajo-derecha, pegada al arte, para la
    // asimetría de "dos formas" (brief §11).
    const float stageRight = textX - 6.0f;
    h.stagePrimary = UiRect{h.art.x - 40.0f, h.art.y - 10.0f, stageRight - (h.art.x - 40.0f),
                            h.art.h + 30.0f};
    h.stageSecondary = UiRect{h.art.x + h.art.w * 0.34f, h.art.y + h.art.h * 0.42f,
                              h.art.w * 0.84f, h.art.h * 0.70f};

    // Colocación del bloque de texto (cursor descendente).
    float y = blockTop;
    h.nameAnchor = UiRect{textX, y, textW, kHeroNameH};
    y += kHeroNameH + kHeroRuleGap;
    h.nameRule = UiRect{textX, y, kHeroRuleW, kHeroRuleH};
    y += kHeroRuleH;
    if (hasSpecies) {
        y += kSpeciesGap;
        h.speciesAnchor = UiRect{textX, y, textW, kSpeciesH};
        y += kSpeciesH;
    }
    if (hasDesc) {
        y += kDescGap;
        h.descriptionAnchor = UiRect{textX, y, textW, kDescLineH * static_cast<float>(kDescLines)};
        y += kDescLineH * static_cast<float>(kDescLines);
    }
    y += kBlockGap;

    if (hasChips) {
        float chipX = textX;
        const float chipY = y;
        for (const CollectionVariant& v : heroItem.variants) {
            HeroVariantChip chip;
            chip.variantId = v.variantId;
            chip.label = VariantLabel(v.variantId, lang);
            const float w = kHeroChipPadX * 2.0f + static_cast<float>(chip.label.size()) * kApproxCharW;
            chip.rect = UiRect{chipX, chipY, w, kHeroChipH};
            chip.underline = UiRect{chipX + kHeroChipPadX, chip.rect.Bottom() - 3.0f,
                                    w - 2.0f * kHeroChipPadX, 2.0f};
            chip.focusId = "variant:" + v.variantId;
            chip.selected = (v.variantId == selectedVariant);
            chip.owned = v.owned;
            h.variants.push_back(chip);
            out.focusOrder.push_back(chip.focusId);
            chipX += w + kHeroChipGap;
        }
        y += kHeroChipH + kChipToAction;
    }

    if (h.actionEnabled) {
        const float buttonW =
            kHeroButtonPadX * 2.0f + static_cast<float>(h.actionLabel.size()) * kApproxCharW;
        h.actionButton = UiRect{textX, y, buttonW, kHeroButtonH};
        out.focusOrder.push_back(h.actionFocusId);
        y += kHeroButtonH;
    } else {
        h.statusAnchor = UiRect{textX, y, textW, kHeroStatusH};
        y += kHeroStatusH;
    }

    const float heroBottom = std::max(h.art.Bottom(), y);
    out.dividerRect = UiRect{kMargin, heroBottom + kDividerGap, contentW, 1.0f};

    // --- Gallery: todos los pets MENOS el hero ---
    std::vector<const CollectionItem*> galleryPets;
    for (const CollectionItem& item : model.items) {
        if (item.petId != heroPetId) {
            galleryPets.push_back(&item);
        }
    }

    const int count = static_cast<int>(galleryPets.size());
    float galleryBottom = out.dividerRect.Bottom() + kGalleryGap;
    if (count > 0) {
        const int cols = std::min(count, 3);
        const float colW = std::min(kGalleryColMax, contentW / static_cast<float>(cols));
        const float galleryLeft = kMargin + (contentW - colW * static_cast<float>(cols)) * 0.5f;
        const float galleryTop = out.dividerRect.Bottom() + kGalleryGap;

        for (int i = 0; i < count; ++i) {
            const CollectionItem& item = *galleryPets[static_cast<std::size_t>(i)];
            const int col = i % cols;
            const int row = i / cols;
            const float rowH = kGalleryArt + kGalleryArtToName + kGalleryNameH + kGalleryNameToStatus +
                               kGalleryStatusH + 22.0f;

            const float colX = galleryLeft + static_cast<float>(col) * colW;
            const float lift = (item.petId == in.hoverPetId) ? kHoverLift : 0.0f;
            const float baseY = galleryTop + static_cast<float>(row) * rowH - lift;

            GalleryItem g;
            g.petId = item.petId;
            g.displayName = item.displayName;
            g.status = item.status;
            g.statusText = StatusText(item.status, lang);
            g.previewVariantId = item.selectedVariantId;  // "" para un pet sin variantes
            const PetAccent itemAccent = PetAccentFor(item.petId);
            g.accentLine = itemAccent.line;
            g.pedestalTint = itemAccent.shapeTint;
            g.hasVariants = item.HasVariants();
            g.focusId = "item:" + item.petId;

            const float artX = colX + (colW - kGalleryArt) * 0.5f;
            g.art = UiRect{artX, baseY, kGalleryArt, kGalleryArt};
            g.name = UiRect{colX, g.art.Bottom() + kGalleryArtToName, colW, kGalleryNameH};
            g.status_ = UiRect{colX, g.name.Bottom() + kGalleryNameToStatus, colW, kGalleryStatusH};

            const float cellW = std::min(colW - 10.0f, kGalleryArt + 46.0f);
            g.cell = UiRect{colX + (colW - cellW) * 0.5f, baseY - 12.0f, cellW,
                            (g.status_.Bottom() - baseY) + 20.0f};

            galleryBottom = std::max(galleryBottom, g.status_.Bottom());
            out.gallery.push_back(g);
            out.focusOrder.push_back(g.focusId);
        }
    }

    out.contentHeight = galleryBottom + kMargin + sy;

    // Segundo plano: el fondo de la gallery, desde el divisor hacia
    // abajo, generoso para que el scroll nunca descubra un borde. La
    // vista lo recorta con su clip (brief §12).
    out.galleryShelf = UiRect{0.0f, out.dividerRect.y, in.viewportW,
                              (out.contentHeight - out.dividerRect.y) + in.viewportH};

    return out;
}

}  // namespace nimvlets::productui
