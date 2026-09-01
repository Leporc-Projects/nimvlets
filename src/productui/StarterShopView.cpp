#include "productui/StarterShopView.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <utility>

#include "productui/SectionHeaderView.h"
#include "productui/ShopPaint.h"
#include "productui/UiTheme.h"

namespace nimvlets::productui {

using platform::TextWeight;

namespace {

constexpr float kHeaderClipTop = 106.0f;
constexpr float kWheelStep = 48.0f;
constexpr unsigned char kPedestalAlpha = 52;
constexpr unsigned char kRailPedestalAlpha = 44;

bool StartsWith(const std::string& s, const char* prefix) { return s.rfind(prefix, 0) == 0; }

}  // namespace

void StarterShopView::SetModel(catalog::StarterShopModel model, std::uint64_t clickBalance) {
    model_ = std::move(model);
    clickBalance_ = clickBalance;

    // Si la oferta seleccionada ya no existe (típicamente porque se
    // compró), vuelve a BROWSE.
    if (!selectedFocusId_.empty()) {
        const catalog::PetIdentity id = StarterOfferIdentityFromFocusId(selectedFocusId_);
        if (model_.Find(id) == nullptr) {
            selectedFocusId_.clear();
            confirming_ = false;
        }
    }
    if (confirming_) {
        const StarterShopLayout layout = BuildLayout(viewportW_, viewportH_);
        if (layout.presentation != StarterShopPresentation::kSelected ||
            layout.hero.status != catalog::ShopItemStatus::kAffordable) {
            confirming_ = false;
        }
    }
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    dirty_ = true;
}

void StarterShopView::SetLanguage(core::Language language) {
    if (language == language_) {
        return;
    }
    language_ = language;
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    dirty_ = true;
}

void StarterShopView::OnEnterSubmode() {
    selectedFocusId_.clear();
    confirming_ = false;
    hoverId_.clear();
    keyboardFocus_ = false;
    scrollY_ = 0.0f;
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    focus_.Focus("starter:back");
    dirty_ = true;
}

StarterShopLayout StarterShopView::BuildLayout(float w, float h) const {
    StarterShopLayoutInput in;
    in.viewportW = w;
    in.viewportH = h;
    in.scrollY = ClampShopScroll(scrollY_, lastContentHeight_, h);
    in.language = language_;
    in.selectedFocusId = selectedFocusId_;
    in.hoverFocusId = hoverId_;
    in.confirming = confirming_;
    return BuildStarterShopLayout(model_, in);
}

void StarterShopView::SyncFocusList(const StarterShopLayout& layout) {
    focus_.SetItems(layout.focusOrder);
}

void StarterShopView::SelectHero(const std::string& focusId) {
    if (focusId == selectedFocusId_) {
        return;
    }
    const catalog::PetIdentity id = StarterOfferIdentityFromFocusId(focusId);
    if (model_.Find(id) == nullptr) {
        return;
    }
    selectedFocusId_ = focusId;
    confirming_ = false;
    scrollY_ = 0.0f;
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    dirty_ = true;
}

void StarterShopView::SelectOfferForQA(const std::string& petId, const std::string& variantId) {
    if (petId.empty()) {
        return;
    }
    SelectHero("starteritem:" + petId + "/" + variantId);
}

StarterShopViewResult StarterShopView::ActivateWidget(const std::string& focusId) {
    StarterShopViewResult r;
    if (focusId.empty()) {
        return r;
    }
    if (ProductSection target; NavTargetSection(focusId, target)) {
        if (target == ProductSection::kShop) {
            // Ya estamos "en" Shop — la pestaña Shop desde el submodo
            // vuelve al Shop público.
            r.exitSubmode = true;
        } else {
            r.switchSection = true;
            r.targetSection = target;
        }
        r.dirty = true;
        return r;
    }
    if (focusId == "starter:back") {
        r.exitSubmode = true;
        r.dirty = true;
        return r;
    }
    if (StartsWith(focusId, "starteritem:")) {
        SelectHero(focusId);
        r.dirty = true;
        return r;
    }
    if (focusId == "get") {
        confirming_ = true;
        SyncFocusList(BuildLayout(viewportW_, viewportH_));
        focus_.Focus("purchase:cancel");  // el foco arranca en Cancelar
        r.dirty = true;
        return r;
    }
    if (focusId == "purchase:cancel") {
        confirming_ = false;
        SyncFocusList(BuildLayout(viewportW_, viewportH_));
        focus_.Focus("get");
        r.dirty = true;
        return r;
    }
    if (focusId == "purchase:confirm") {
        const StarterShopLayout layout = BuildLayout(viewportW_, viewportH_);
        if (layout.hero.confirm.visible && !layout.hero.petId.empty()) {
            r.hasPurchase = true;
            r.purchase.petId = layout.hero.entitlementTarget.petId;
            r.purchase.variantId = layout.hero.entitlementTarget.variantId;
        }
        confirming_ = false;  // src/app re-empuja el modelo con el estado final
        r.dirty = true;
        return r;
    }
    return r;
}

StarterShopViewResult StarterShopView::OnMouseMove(float x, float y) {
    StarterShopViewResult r;
    const StarterShopLayout layout = BuildLayout(viewportW_, viewportH_);
    const std::string hit = layout.HitTest(x, y);
    // Solo las tarjetas de oferta revelan info liviana; hover sobre nav /
    // back no es un target de reveal.
    const std::string revealHit = StartsWith(hit, "starteritem:") ? hit : std::string();
    if (revealHit != hoverId_) {
        hoverId_ = revealHit;
        dirty_ = true;
        r.dirty = true;
    }
    return r;
}

StarterShopViewResult StarterShopView::OnMouseDown(float x, float y) {
    keyboardFocus_ = false;
    const StarterShopLayout layout = BuildLayout(viewportW_, viewportH_);
    const std::string hit = layout.HitTest(x, y);
    if (hit.empty()) {
        dirty_ = true;
        return StarterShopViewResult{};
    }
    if (StartsWith(hit, "starteritem:") || StartsWith(hit, "nav:") || hit == "starter:back") {
        focus_.Focus(hit);
    }
    StarterShopViewResult r = ActivateWidget(hit);
    r.dirty = true;
    dirty_ = true;
    return r;
}

StarterShopViewResult StarterShopView::OnWheel(float dyLines) {
    StarterShopViewResult r;
    const float before = scrollY_;
    scrollY_ = ClampShopScroll(scrollY_ - dyLines * kWheelStep, lastContentHeight_, viewportH_);
    if (scrollY_ != before) {
        dirty_ = true;
        r.dirty = true;
    }
    return r;
}

StarterShopViewResult StarterShopView::OnKey(int sdlKeycode, bool shiftHeld) {
    StarterShopViewResult r;
    switch (sdlKeycode) {
        case SDLK_TAB:
        case SDLK_RIGHT:
        case SDLK_DOWN:
            if (sdlKeycode == SDLK_TAB && shiftHeld) {
                focus_.Prev();
            } else {
                focus_.Next();
            }
            keyboardFocus_ = true;
            dirty_ = true;
            r.dirty = true;
            return r;
        case SDLK_LEFT:
        case SDLK_UP:
            focus_.Prev();
            keyboardFocus_ = true;
            dirty_ = true;
            r.dirty = true;
            return r;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_SPACE:
            keyboardFocus_ = true;
            return ActivateWidget(focus_.FocusedId());
        case SDLK_ESCAPE:
            // Esc SIEMPRE da un paso atrás — NUNCA cierra la ventana desde
            // el submodo (brief §20).
            if (confirming_) {
                confirming_ = false;
                SyncFocusList(BuildLayout(viewportW_, viewportH_));
                focus_.Focus("get");
                keyboardFocus_ = true;
                dirty_ = true;
                r.dirty = true;
                return r;
            }
            if (!selectedFocusId_.empty()) {
                selectedFocusId_.clear();
                SyncFocusList(BuildLayout(viewportW_, viewportH_));
                focus_.Focus("starter:back");
                keyboardFocus_ = true;
                dirty_ = true;
                r.dirty = true;
                return r;
            }
            r.exitSubmode = true;  // browse -> Shop público
            r.dirty = true;
            return r;
        default:
            return r;
    }
}

StarterShopViewResult StarterShopView::OnViewportChanged() {
    StarterShopViewResult r;
    dirty_ = true;
    r.dirty = true;
    return r;
}

namespace {

void DrawBackAffordance(
    UiPainter& painter, TextCache& text, const StarterShopLayout& layout, bool focused) {
    const UiRect& a = layout.backAnchor;
    DrawText(painter, text, layout.backText, type::kSectionSub, TextWeight::kMedium, theme::kTextMuted,
             a.x, a.y + 12.0f, HAlign::kLeft);
    if (focused) {
        const float w = 74.0f;
        painter.FillRect(UiRect{a.x, a.y + 16.0f, w, 1.0f}, theme::kTextMuted);
    }
}

}  // namespace

void StarterShopView::Render(
    UiPainter& painter, TextCache& text, PetPreviewCache& previews, float viewportW, float viewportH) {
    viewportW_ = viewportW;
    viewportH_ = viewportH;

    const StarterShopLayout layout = BuildLayout(viewportW, viewportH);
    lastContentHeight_ = layout.contentHeight;
    SyncFocusList(layout);
    const std::string focusedId = keyboardFocus_ ? focus_.FocusedId() : std::string();

    painter.Clear(theme::kBackground);
    DrawSectionHeader(painter, text, layout.header, clickBalance_, language_, hoverId_, focusedId);

    painter.PushClip(
        UiRect{0.0f, kHeaderClipTop, viewportW, std::max(0.0f, viewportH - kHeaderClipTop)});

    DrawBackAffordance(painter, text, layout, focusedId == "starter:back");

    if (layout.presentation == StarterShopPresentation::kEmpty) {
        DrawText(painter, text, layout.emptyText, type::kSectionTitle, TextWeight::kRegular,
                 theme::kTextFaint, layout.emptyAnchor.CenterX(), layout.emptyAnchor.y + 16.0f,
                 HAlign::kCenter, static_cast<int>(layout.emptyAnchor.w));
        painter.PopClip();
        return;
    }

    // Encabezado quieto "Starter choices".
    DrawText(painter, text, layout.heading, type::kSectionSub, TextWeight::kRegular, theme::kTextMuted,
             layout.headingAnchor.CenterX(), layout.headingAnchor.y + 13.0f, HAlign::kCenter,
             static_cast<int>(layout.headingAnchor.w));

    if (layout.presentation == StarterShopPresentation::kBrowse) {
        for (const ShopTile& t : layout.tiles) {
            const bool hovered = hoverId_ == t.focusId;
            const bool focused = focusedId == t.focusId;
            DrawShopTile(painter, text, previews, t, type::kGalleryName, kPedestalAlpha, hovered,
                         focused, /*revealVisible=*/hovered || focused, /*selectedMark=*/false);
        }
        painter.PopClip();
        return;
    }

    // SELECTED.
    painter.FillRect(layout.shelfBackground, theme::kGalleryShelf);
    DrawShopHero(painter, text, previews, layout.hero, focusedId);
    painter.FillRect(layout.dividerRect, theme::kHairline);
    for (const ShopTile& t : layout.rail) {
        const bool hovered = hoverId_ == t.focusId;
        const bool focused = focusedId == t.focusId;
        DrawShopTile(painter, text, previews, t, type::kGalleryStatus, kRailPedestalAlpha, hovered,
                     focused, /*revealVisible=*/false, /*selectedMark=*/t.selected);
    }
    painter.PopClip();
}

}  // namespace nimvlets::productui
