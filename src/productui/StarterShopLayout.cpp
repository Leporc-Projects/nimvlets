#include "productui/StarterShopLayout.h"

#include <algorithm>

#include "productui/Format.h"
#include "productui/PetEditorial.h"

namespace nimvlets::productui {

using catalog::PetIdentity;
using catalog::ShopItemStatus;
using catalog::StarterShopModel;
using catalog::StarterShopOffer;
using catalog::StarterShopOfferStatus;
using core::Language;
using core::Localized;
using core::StringKey;

namespace {

constexpr float kMargin = 40.0f;  // == ShopLayout::kMargin — LayoutShopHero lo asume

constexpr float kBackH = 16.0f;
constexpr float kBackGap = 22.0f;         // back affordance -> encabezado
constexpr float kHeadingH = 18.0f;
constexpr float kHeadingGap = 22.0f;      // encabezado -> rejilla
constexpr float kRailDividerGap = 24.0f;
constexpr float kRailTopGap = 20.0f;

std::string ComposeName(const StarterShopOffer& offer, Language lang) {
    const std::string label = StarterVariantLabel(offer.VariantId(), lang);
    if (label.empty()) {
        return offer.displayName;
    }
    return offer.displayName + " \xC2\xB7 " + label;  // "Frin · Male" (· = U+00B7)
}

ShopItemStatus MapStatus(StarterShopOfferStatus s) {
    return s == StarterShopOfferStatus::kAffordable ? ShopItemStatus::kAffordable
                                                    : ShopItemStatus::kInsufficientBalance;
}

ShopTile MakeOfferTile(const StarterShopOffer& offer, Language lang, bool selected) {
    ShopTile t;
    t.petId = offer.PetId();
    t.variantId = offer.VariantId();
    t.displayName = ComposeName(offer, lang);
    t.status = MapStatus(offer.status);
    t.revealText = FormatClickCount(offer.priceClicks, lang);  // nunca "owned" — se filtró
    const PetAccent accent = PetAccentFor(offer.PetId());
    t.accentLine = accent.line;
    t.pedestalTint = accent.shapeTint;
    t.selected = selected;
    t.focusId = StarterOfferFocusId(offer.target);
    return t;
}

}  // namespace

std::string StarterOfferFocusId(const PetIdentity& target) {
    return "starteritem:" + target.petId + "/" + target.variantId;
}

PetIdentity StarterOfferIdentityFromFocusId(const std::string& focusId) {
    constexpr const char* kPrefix = "starteritem:";
    if (focusId.rfind(kPrefix, 0) != 0) {
        return PetIdentity{};
    }
    const std::string body = focusId.substr(std::string(kPrefix).size());
    const std::size_t slash = body.find('/');
    if (slash == std::string::npos) {
        return PetIdentity{body, ""};
    }
    return PetIdentity{body.substr(0, slash), body.substr(slash + 1)};
}

std::string StarterVariantLabel(const std::string& variantId, Language lang) {
    if (variantId == "male") {
        return Localized(StringKey::kMale, lang);
    }
    if (variantId == "female") {
        return Localized(StringKey::kFemale, lang);
    }
    return "";
}

const ShopTile* StarterShopLayout::FindTileByFocusId(const std::string& focusId) const {
    for (const ShopTile& t : tiles) {
        if (t.focusId == focusId) {
            return &t;
        }
    }
    for (const ShopTile& t : rail) {
        if (t.focusId == focusId) {
            return &t;
        }
    }
    return nullptr;
}

std::string StarterShopLayout::HitTest(float x, float y) const {
    for (const SectionTab& tab : header.tabs) {
        if (tab.hitRect.Contains(x, y)) {
            return tab.focusId;
        }
    }
    if (backAnchor.Contains(x, y)) {
        return "starter:back";
    }
    if (presentation == StarterShopPresentation::kSelected) {
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
        for (const ShopTile& t : rail) {
            if (t.cell.Contains(x, y)) {
                return t.focusId;
            }
        }
        return "";
    }
    for (const ShopTile& t : tiles) {
        if (t.cell.Contains(x, y)) {
            return t.focusId;
        }
    }
    return "";
}

StarterShopLayout BuildStarterShopLayout(
    const StarterShopModel& model, const StarterShopLayoutInput& in) {
    const Language lang = in.language;
    const float sy = in.scrollY;

    StarterShopLayout out;
    out.viewport = UiRect{0.0f, 0.0f, in.viewportW, in.viewportH};
    out.header = BuildSectionHeaderLayout(in.viewportW, kMargin, sy, ProductSection::kShop, lang);
    for (const SectionTab& tab : out.header.tabs) {
        out.focusOrder.push_back(tab.focusId);
    }

    const float contentW = std::max(160.0f, in.viewportW - 2.0f * kMargin);
    const float bodyTop = out.header.bodyTop;

    out.backText = Localized(StringKey::kStarterShopBack, lang);
    out.backAnchor = UiRect{kMargin, bodyTop, contentW, kBackH};
    out.focusOrder.push_back("starter:back");
    const float afterBack = bodyTop + kBackH + kBackGap;

    // --- Estado vacío: se compró la última oferta (brief §19) --------
    if (model.Empty()) {
        out.presentation = StarterShopPresentation::kEmpty;
        out.emptyText = Localized(StringKey::kStarterShopEmpty, lang);
        out.emptyAnchor =
            UiRect{kMargin, afterBack + (in.viewportH - afterBack) * 0.30f, contentW, 24.0f};
        out.contentHeight = std::max(in.viewportH, afterBack + sy + kMargin);
        return out;
    }

    // ¿Oferta seleccionada y sigue en el modelo?
    const StarterShopOffer* heroOffer = nullptr;
    if (!in.selectedFocusId.empty()) {
        const PetIdentity id = StarterOfferIdentityFromFocusId(in.selectedFocusId);
        heroOffer = model.Find(id);
    }

    // ============================ BROWSE ============================
    if (heroOffer == nullptr) {
        out.presentation = StarterShopPresentation::kBrowse;
        out.heading = Localized(StringKey::kStarterChoicesHeading, lang);

        std::vector<ShopTile> tiles;
        tiles.reserve(model.offers.size());
        for (const StarterShopOffer& o : model.offers) {
            tiles.push_back(MakeOfferTile(o, lang, /*selected=*/false));
        }

        const BrowseGridMetrics m = ComputeBrowseGrid(static_cast<int>(tiles.size()), contentW);
        const float wholeH = kHeadingH + kHeadingGap + m.blockH;
        const float freeSpace = (in.viewportH - kMargin - (afterBack + sy)) - wholeH;
        const float topPad = freeSpace > 0.0f ? std::min(freeSpace * 0.38f, 72.0f) : 0.0f;

        const float headingTop = afterBack + topPad;
        out.headingAnchor = UiRect{kMargin, headingTop, contentW, kHeadingH};
        const float gridTop = headingTop + kHeadingH + kHeadingGap;

        const float gridBottom = LayoutBrowseGrid(tiles, in.viewportW, m, gridTop);
        out.tiles = std::move(tiles);
        for (const ShopTile& t : out.tiles) {
            out.focusOrder.push_back(t.focusId);
        }
        out.contentHeight = std::max(in.viewportH, gridBottom + kMargin + sy);
        return out;
    }

    // =========================== SELECTED ==========================
    out.presentation = StarterShopPresentation::kSelected;
    out.heading = Localized(StringKey::kStarterChoicesHeading, lang);
    out.headingAnchor = UiRect{kMargin, afterBack, contentW, kHeadingH};

    ShopHeroContent hc;
    hc.petId = heroOffer->PetId();
    hc.variantId = heroOffer->VariantId();
    hc.displayName = ComposeName(*heroOffer, lang);
    hc.speciesText = Species(heroOffer->PetId(), lang);
    hc.descriptionText = ShortDescription(heroOffer->PetId(), lang);
    hc.status = MapStatus(heroOffer->status);
    hc.entitlementTarget = heroOffer->entitlementTarget;
    hc.priceClicks = heroOffer->priceClicks;
    hc.clicksShort = heroOffer->clicksShort;

    const float heroTop = afterBack + kHeadingH + kHeadingGap;
    const float heroBottom =
        LayoutShopHero(out.hero, out.focusOrder, hc, heroTop, contentW, lang, in.confirming);
    out.dividerRect = UiRect{kMargin, heroBottom + kRailDividerGap, contentW, 1.0f};

    std::vector<ShopTile> rail;
    rail.reserve(model.offers.size());
    for (const StarterShopOffer& o : model.offers) {
        rail.push_back(MakeOfferTile(o, lang, /*selected=*/o.target == heroOffer->target));
    }
    const float railBottom = LayoutShopRail(
        rail, in.viewportW, contentW, out.dividerRect.Bottom() + kRailTopGap, in.hoverFocusId);
    out.rail = std::move(rail);
    for (const ShopTile& t : out.rail) {
        out.focusOrder.push_back(t.focusId);
    }

    out.contentHeight = std::max(in.viewportH, railBottom + kMargin + sy);
    out.shelfBackground = UiRect{0.0f, out.dividerRect.y, in.viewportW,
                                 (out.contentHeight - out.dividerRect.y) + in.viewportH};
    return out;
}

}  // namespace nimvlets::productui
