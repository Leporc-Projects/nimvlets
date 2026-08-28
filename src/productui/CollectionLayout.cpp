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

// Métricas en PUNTOS lógicos. Composición hero + gallery de Block 06.1
// (§7): jerarquía por tamaño/espacio, no por más contenedores.
constexpr float kMargin = 40.0f;
constexpr float kTitleTop = 30.0f;
constexpr float kTitleH = 24.0f;
constexpr float kSectionTitleTop = 70.0f;
constexpr float kSectionSubtitleTop = 90.0f;
constexpr float kLabelH = 16.0f;

constexpr float kHeroTop = 118.0f;
constexpr float kHeroArt = 202.0f;
constexpr float kHeroTextGap = 36.0f;   // arte -> bloque de texto
constexpr float kHeroNameH = 30.0f;
// Offsets relativos al TOPE del bloque de texto (que se centra
// verticalmente contra el arte — ver `blockTop` más abajo).
constexpr float kHeroSpeciesRel = 38.0f;
constexpr float kHeroStatusRel = 59.0f;
constexpr float kHeroChipsRel = 92.0f;
constexpr float kHeroChipH = 26.0f;
constexpr float kHeroChipPadX = 10.0f;
constexpr float kHeroChipGap = 22.0f;    // incluye el "·" separador
constexpr float kHeroButtonH = 36.0f;
constexpr float kHeroButtonPadX = 22.0f;
constexpr float kHeroButtonRelWithChips = 138.0f;
constexpr float kHeroButtonRelNoChips = 92.0f;
constexpr float kHeroBlockHWithChips = 174.0f;  // alto aprox. del bloque de texto para centrarlo
constexpr float kHeroBlockHNoChips = 128.0f;

constexpr float kDividerGap = 22.0f;
constexpr float kGalleryGap = 20.0f;
constexpr float kGalleryColMax = 208.0f;  // el cluster de gallery no se estira a lo ancho
constexpr float kGalleryArt = 90.0f;
constexpr float kGalleryNameH = 17.0f;
constexpr float kGalleryStatusH = 14.0f;
constexpr float kGalleryArtToName = 12.0f;
constexpr float kGalleryNameToStatus = 4.0f;
constexpr float kHoverLift = 2.0f;  // micro-lift instantáneo (brief §11)

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

// Etiqueta localizada de una variante. "male"/"female" -> Male/Female o
// Macho/Hembra; cualquier otra -> Capitalize del id (defensivo).
std::string VariantLabel(const std::string& variantId, Language lang) {
    if (variantId == "male") {
        return Localized(StringKey::kMale, lang);
    }
    if (variantId == "female") {
        return Localized(StringKey::kFemale, lang);
    }
    return Capitalize(variantId);
}

// Resuelve qué variante mostrar: la pedida si es válida, si no la del
// item.
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

    out.titleAnchor = UiRect{kMargin, kTitleTop - sy, contentW, kTitleH};
    out.clicksAnchorRight = UiRect{in.viewportW - kMargin, kTitleTop - sy, 0.0f, kTitleH};
    out.sectionTitleAnchor = UiRect{kMargin, kSectionTitleTop - sy, contentW, kLabelH};
    out.sectionSubtitleAnchor = UiRect{kMargin, kSectionSubtitleTop - sy, contentW, kLabelH};

    if (model.items.empty()) {
        out.contentHeight = kHeroTop;
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
    h.speciesText = ProvisionalSpecies(heroItem.petId, lang);
    h.status = heroItem.status;
    h.statusText = StatusText(heroItem.status, lang);
    h.accent = PetAccentFor(heroItem.petId);

    const float heroTop = kHeroTop - sy;
    h.art = UiRect{kMargin, heroTop, kHeroArt, kHeroArt};
    // Forma de fondo: un poco más grande que el arte y descentrada
    // (orgánico, no concéntrico — brief §10). La vista elige óvalo vs
    // round-rect según h.accent.angularShape y la dibuja a alpha bajo.
    h.shape = UiRect{h.art.x - 14.0f, h.art.y + 4.0f, h.art.w + 34.0f, h.art.h + 12.0f};

    const float textX = h.art.Right() + kHeroTextGap;
    const float textW = std::max(140.0f, kMargin + contentW - textX);

    // El bloque de texto se centra verticalmente contra el arte, así el
    // hero no queda "pesado arriba" cuando el pet no tiene chips de
    // variante (brief §8/§26).
    const float blockH = heroItem.HasVariants() ? kHeroBlockHWithChips : kHeroBlockHNoChips;
    const float blockTop = heroTop + std::max(0.0f, (kHeroArt - blockH) * 0.5f);

    h.nameAnchor = UiRect{textX, blockTop, textW, kHeroNameH};
    h.speciesAnchor = UiRect{textX, blockTop + kHeroSpeciesRel, textW, kLabelH};
    h.statusAnchor = UiRect{textX, blockTop + kHeroStatusRel, textW, kLabelH};

    const std::string selectedVariant = ResolveVariant(heroItem, in.selectedVariantId);
    h.selectedVariantId = selectedVariant;

    if (heroItem.HasVariants()) {
        float chipX = textX;
        const float chipY = blockTop + kHeroChipsRel;
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
            h.variants.push_back(chip);
            out.focusOrder.push_back(chip.focusId);
            chipX += w + kHeroChipGap;
        }
    }

    // Botón de acción.
    const bool activePet = heroItem.status == OwnershipStatus::kActive;
    const bool variantWouldChange =
        heroItem.HasVariants() && activePet && selectedVariant != model.activeVariantId;
    if (heroItem.status == OwnershipStatus::kLocked) {
        h.actionLabel = Localized(StringKey::kNotInCollection, lang);
        h.actionEnabled = false;
    } else if (activePet && !variantWouldChange) {
        h.actionLabel = Localized(StringKey::kOnDesktop, lang);
        h.actionEnabled = false;
    } else {
        h.actionLabel = std::string(Localized(StringKey::kUsePetPrefix, lang)) + heroItem.displayName;
        h.actionEnabled = true;
    }
    h.actionFocusId = "use";
    const float buttonTop =
        blockTop + (heroItem.HasVariants() ? kHeroButtonRelWithChips : kHeroButtonRelNoChips);
    const float buttonW =
        kHeroButtonPadX * 2.0f + static_cast<float>(h.actionLabel.size()) * kApproxCharW;
    h.actionButton = UiRect{textX, buttonTop, buttonW, kHeroButtonH};
    if (h.actionEnabled) {
        out.focusOrder.push_back(h.actionFocusId);
    }

    const float heroBottom = std::max(h.art.Bottom(), h.actionButton.Bottom());
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
        // Las columnas no se estiran a todo el ancho: un cluster
        // centrado se lee más tranquilo que dos items flotando lejos
        // (brief §11/§26).
        const float colW = std::min(kGalleryColMax, contentW / static_cast<float>(cols));
        const float galleryLeft = kMargin + (contentW - colW * static_cast<float>(cols)) * 0.5f;
        const float galleryTop = out.dividerRect.Bottom() + kGalleryGap;

        for (int i = 0; i < count; ++i) {
            const CollectionItem& item = *galleryPets[static_cast<std::size_t>(i)];
            const int col = i % cols;
            const int row = i / cols;
            const float rowH = kGalleryArt + kGalleryArtToName + kGalleryNameH + kGalleryNameToStatus +
                               kGalleryStatusH + 20.0f;

            const float colX = galleryLeft + static_cast<float>(col) * colW;
            const float lift = (item.petId == in.hoverPetId) ? kHoverLift : 0.0f;
            const float baseY = galleryTop + static_cast<float>(row) * rowH - lift;

            GalleryItem g;
            g.petId = item.petId;
            g.displayName = item.displayName;
            g.status = item.status;
            g.statusText = StatusText(item.status, lang);
            g.previewVariantId = item.selectedVariantId;  // "" para un pet sin variantes
            g.accentLine = PetAccentFor(item.petId).line;
            g.hasVariants = item.HasVariants();
            g.focusId = "item:" + item.petId;

            const float artX = colX + (colW - kGalleryArt) * 0.5f;
            g.art = UiRect{artX, baseY, kGalleryArt, kGalleryArt};
            g.name = UiRect{colX, g.art.Bottom() + kGalleryArtToName, colW, kGalleryNameH};
            g.status_ = UiRect{colX, g.name.Bottom() + kGalleryNameToStatus, colW, kGalleryStatusH};

            const float cellW = std::min(colW - 10.0f, kGalleryArt + 44.0f);
            g.cell = UiRect{colX + (colW - cellW) * 0.5f, baseY - 10.0f, cellW,
                            (g.status_.Bottom() - baseY) + 18.0f};

            galleryBottom = std::max(galleryBottom, g.status_.Bottom());
            out.gallery.push_back(g);
            out.focusOrder.push_back(g.focusId);
        }
    }

    out.contentHeight = galleryBottom + kMargin + sy;
    return out;
}

}  // namespace nimvlets::productui
