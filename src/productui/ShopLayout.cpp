#include "productui/ShopLayout.h"

#include <algorithm>

#include "productui/Format.h"
#include "productui/PetEditorial.h"

namespace nimvlets::productui {

using catalog::ShopItem;
using catalog::ShopItemStatus;
using catalog::ShopModel;
using core::Language;
using core::Localized;
using core::StringKey;

namespace {

// Mismas métricas base que CollectionLayout — el Shop reusa la
// composición hero + gallery para que las dos secciones se sientan una
// sola pantalla (brief §8).
constexpr float kMargin = 40.0f;
constexpr float kHeroArt = 216.0f;
constexpr float kHeroTextGap = 40.0f;

constexpr float kHeroNameH = 32.0f;
constexpr float kHeroRuleGap = 12.0f;
constexpr float kHeroRuleW = 46.0f;
constexpr float kHeroRuleH = 2.0f;
constexpr float kSpeciesGap = 12.0f;
constexpr float kSpeciesH = 16.0f;
constexpr float kDescGap = 8.0f;
constexpr float kDescLineH = 17.0f;
constexpr int kDescMaxLines = 3;
constexpr float kBlockGap = 14.0f;
constexpr float kPriceH = 17.0f;
constexpr float kPriceToAction = 12.0f;
constexpr float kHeroButtonH = 36.0f;
constexpr float kHeroButtonPadX = 22.0f;
constexpr float kHeroStatusH = 18.0f;

constexpr float kConfirmPromptH = 34.0f;   // hasta 2 líneas
constexpr float kConfirmGap = 12.0f;
constexpr float kConfirmButtonH = 32.0f;
constexpr float kConfirmButtonPadX = 18.0f;
constexpr float kConfirmButtonGap = 12.0f;

constexpr float kDividerGap = 24.0f;
constexpr float kGalleryGap = 22.0f;
constexpr float kGalleryColMax = 208.0f;
constexpr float kGalleryArt = 92.0f;
constexpr float kGalleryNameH = 17.0f;
constexpr float kGalleryStatusH = 14.0f;
constexpr float kGalleryArtToName = 12.0f;
constexpr float kGalleryNameToStatus = 4.0f;
constexpr float kHoverLift = 2.0f;

constexpr float kApproxCharW = 8.0f;

std::string SecondaryTextFor(const ShopItem& item, Language lang) {
    switch (item.status) {
        case ShopItemStatus::kOwned:
            return Localized(StringKey::kInYourCollection, lang);
        case ShopItemStatus::kInsufficientBalance:
        case ShopItemStatus::kAffordable:
            return FormatClickCount(item.priceClicks, lang);
    }
    return "";
}

}  // namespace

const ShopGalleryItem* ShopLayout::FindGalleryItem(const std::string& petId) const {
    for (const ShopGalleryItem& g : gallery) {
        if (g.petId == petId) {
            return &g;
        }
    }
    return nullptr;
}

std::string ShopLayout::HitTest(float x, float y) const {
    for (const SectionTab& tab : header.tabs) {
        if (tab.hitRect.Contains(x, y)) {
            return tab.focusId;
        }
    }
    if (hero.confirm.visible) {
        if (hero.confirm.cancelButton.Contains(x, y)) {
            return hero.confirm.cancelFocusId;
        }
        if (hero.confirm.confirmButton.Contains(x, y)) {
            return hero.confirm.confirmFocusId;
        }
    } else if (hero.actionEnabled && hero.actionButton.Contains(x, y)) {
        return hero.actionFocusId;
    }
    for (const ShopGalleryItem& g : gallery) {
        if (g.cell.Contains(x, y)) {
            return g.focusId;
        }
    }
    return "";
}

float ClampShopScroll(float scrollY, float contentHeight, float viewportH) {
    return std::clamp(scrollY, 0.0f, std::max(0.0f, contentHeight - viewportH));
}

ShopLayout BuildShopLayout(const ShopModel& model, const ShopLayoutInput& in) {
    const Language lang = in.language;
    const float sy = in.scrollY;

    ShopLayout out;
    out.viewport = UiRect{0.0f, 0.0f, in.viewportW, in.viewportH};
    out.header = BuildSectionHeaderLayout(in.viewportW, kMargin, sy, ProductSection::kShop, lang);
    for (const SectionTab& tab : out.header.tabs) {
        out.focusOrder.push_back(tab.focusId);
    }

    const float contentW = std::max(160.0f, in.viewportW - 2.0f * kMargin);

    if (model.items.empty()) {
        out.empty = true;
        out.contentHeight = out.header.bodyTop + sy + kMargin;
        return out;
    }
    out.empty = false;

    // --- Resolver el hero ---
    std::string heroPetId = in.selectedPetId;
    if (model.Find(heroPetId) == nullptr) {
        heroPetId = model.items.front().petId;
    }
    const ShopItem& heroItem = *model.Find(heroPetId);

    ShopHero& h = out.hero;
    h.petId = heroItem.petId;
    h.displayName = heroItem.displayName;
    h.speciesText = Species(heroItem.petId, lang);
    h.descriptionText = ShortDescription(heroItem.petId, lang);
    h.status = heroItem.status;
    h.accent = PetAccentFor(heroItem.petId);
    h.priceClicks = heroItem.priceClicks;
    h.priceText = FormatClickCount(heroItem.priceClicks, lang);

    const float heroTop = out.header.bodyTop;
    h.art = UiRect{kMargin, heroTop, kHeroArt, kHeroArt};

    const float textX = h.art.Right() + kHeroTextGap;
    const float textW = std::max(160.0f, kMargin + contentW - textX);

    const bool owned = heroItem.status == ShopItemStatus::kOwned;
    const bool affordable = heroItem.status == ShopItemStatus::kAffordable;
    const bool confirming = in.confirming && affordable;

    // Estado / acción / confirmación — mutuamente excluyentes.
    if (owned) {
        h.showStatusLine = true;
        h.statusText = Localized(StringKey::kInYourCollection, lang);
    } else if (confirming) {
        h.confirm.visible = true;
        h.confirm.prompt = FormatSpendPrompt(heroItem.priceClicks, heroItem.displayName, lang);
        h.confirm.cancelLabel = Localized(StringKey::kCancel, lang);
        h.confirm.cancelFocusId = "purchase:cancel";
        h.confirm.confirmLabel = Localized(StringKey::kConfirm, lang);
        h.confirm.confirmFocusId = "purchase:confirm";
    } else if (affordable) {
        h.actionEnabled = true;
        h.actionLabel = std::string(Localized(StringKey::kGetPetPrefix, lang)) + heroItem.displayName;
        h.actionFocusId = "get";
    } else {  // kInsufficientBalance
        h.showStatusLine = true;
        h.statusText = FormatNeedMoreClicks(heroItem.clicksShort, lang);
    }

    const bool hasSpecies = !h.speciesText.empty();
    const bool hasDesc = !h.descriptionText.empty();

    // Alto del bloque de texto, para centrarlo verticalmente contra el
    // arte.
    float blockH = kHeroNameH + kHeroRuleGap + kHeroRuleH;
    if (hasSpecies) {
        blockH += kSpeciesGap + kSpeciesH;
    }
    if (hasDesc) {
        blockH += kDescGap + kDescLineH * static_cast<float>(kDescMaxLines);
    }
    blockH += kBlockGap;
    if (!owned) {
        blockH += kPriceH + kPriceToAction;
    }
    if (h.confirm.visible) {
        blockH += kConfirmPromptH + kConfirmGap + kConfirmButtonH;
    } else if (h.actionEnabled) {
        blockH += kHeroButtonH;
    } else {
        blockH += kHeroStatusH;
    }

    const float blockTop = heroTop + std::max(0.0f, (kHeroArt - blockH) * 0.5f);

    const float stageRight = textX - 6.0f;
    h.stagePrimary = UiRect{h.art.x - 40.0f, h.art.y - 10.0f, stageRight - (h.art.x - 40.0f), h.art.h + 30.0f};
    h.stageSecondary = UiRect{h.art.x + h.art.w * 0.34f, h.art.y + h.art.h * 0.42f, h.art.w * 0.84f, h.art.h * 0.70f};

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
        h.descriptionAnchor = UiRect{textX, y, textW, kDescLineH * static_cast<float>(kDescMaxLines)};
        y += kDescLineH * static_cast<float>(kDescMaxLines);
    }
    y += kBlockGap;

    if (!owned) {
        h.priceAnchor = UiRect{textX, y, textW, kPriceH};
        y += kPriceH + kPriceToAction;
    }

    if (h.confirm.visible) {
        h.confirm.promptAnchor = UiRect{textX, y, textW, kConfirmPromptH};
        y += kConfirmPromptH + kConfirmGap;
        const float cancelW =
            kConfirmButtonPadX * 2.0f + static_cast<float>(h.confirm.cancelLabel.size()) * kApproxCharW;
        const float confirmW =
            kConfirmButtonPadX * 2.0f + static_cast<float>(h.confirm.confirmLabel.size()) * kApproxCharW;
        h.confirm.cancelButton = UiRect{textX, y, cancelW, kConfirmButtonH};
        h.confirm.confirmButton = UiRect{textX + cancelW + kConfirmButtonGap, y, confirmW, kConfirmButtonH};
        out.focusOrder.push_back(h.confirm.cancelFocusId);
        out.focusOrder.push_back(h.confirm.confirmFocusId);
        y += kConfirmButtonH;
    } else if (h.actionEnabled) {
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

    // --- Gallery: los demás pets del Shop ---
    std::vector<const ShopItem*> galleryPets;
    for (const ShopItem& item : model.items) {
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
            const ShopItem& item = *galleryPets[static_cast<std::size_t>(i)];
            const int col = i % cols;
            const int row = i / cols;
            const float rowH = kGalleryArt + kGalleryArtToName + kGalleryNameH + kGalleryNameToStatus +
                               kGalleryStatusH + 22.0f;

            const float colX = galleryLeft + static_cast<float>(col) * colW;
            const float lift = (item.petId == in.hoverPetId) ? kHoverLift : 0.0f;
            const float baseY = galleryTop + static_cast<float>(row) * rowH - lift;

            ShopGalleryItem g;
            g.petId = item.petId;
            g.displayName = item.displayName;
            g.status = item.status;
            g.secondaryText = SecondaryTextFor(item, lang);
            const PetAccent itemAccent = PetAccentFor(item.petId);
            g.accentLine = itemAccent.line;
            g.pedestalTint = itemAccent.shapeTint;
            g.focusId = "shopitem:" + item.petId;

            const float artX = colX + (colW - kGalleryArt) * 0.5f;
            g.art = UiRect{artX, baseY, kGalleryArt, kGalleryArt};
            g.name = UiRect{colX, g.art.Bottom() + kGalleryArtToName, colW, kGalleryNameH};
            g.secondary_ = UiRect{colX, g.name.Bottom() + kGalleryNameToStatus, colW, kGalleryStatusH};

            const float cellW = std::min(colW - 10.0f, kGalleryArt + 46.0f);
            g.cell = UiRect{colX + (colW - cellW) * 0.5f, baseY - 12.0f, cellW,
                            (g.secondary_.Bottom() - baseY) + 20.0f};

            galleryBottom = std::max(galleryBottom, g.secondary_.Bottom());
            out.gallery.push_back(g);
            out.focusOrder.push_back(g.focusId);
        }
    }

    out.contentHeight = galleryBottom + kMargin + sy;
    out.galleryShelf = UiRect{0.0f, out.dividerRect.y, in.viewportW,
                              (out.contentHeight - out.dividerRect.y) + in.viewportH};
    return out;
}

}  // namespace nimvlets::productui
