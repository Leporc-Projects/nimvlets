#include "productui/ShopView.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <utility>

#include "productui/Format.h"
#include "productui/SectionHeaderView.h"
#include "productui/UiTheme.h"

namespace nimvlets::productui {

using catalog::ShopItemStatus;
using platform::TextWeight;

namespace {

constexpr float kHeaderClipTop = 106.0f;
constexpr float kWheelStep = 48.0f;

constexpr unsigned char kStagePrimaryAlpha = 92;
constexpr unsigned char kStageSecondaryAlpha = 52;
constexpr unsigned char kOwnedArtAlpha = 235;
constexpr unsigned char kPedestalAlpha = 52;

bool StartsWith(const std::string& s, const char* prefix) { return s.rfind(prefix, 0) == 0; }

std::string PetIdFromShopFocusId(const std::string& focusId) {
    return StartsWith(focusId, "shopitem:") ? focusId.substr(9) : std::string();
}

void FillStagePrimitive(UiPainter& painter, const UiRect& r, bool angular, UiColor color) {
    if (angular) {
        painter.FillRoundRect(r, std::min(r.w, r.h) * 0.32f, color);
    } else {
        painter.FillEllipse(r, color);
    }
}

}  // namespace

void ShopView::SetModel(catalog::ShopModel model, std::uint64_t clickBalance) {
    model_ = std::move(model);
    clickBalance_ = clickBalance;

    if (!selectedPetId_.empty() && model_.Find(selectedPetId_) == nullptr) {
        selectedPetId_.clear();
    }
    // Una confirmación abierta se cierra si su pet ya NO es asequible
    // (típicamente porque la compra tuvo éxito y ahora está poseído —
    // brief §27: "successful confirm clears confirmation").
    if (confirming_) {
        const ShopLayout layout = BuildLayout(viewportW_, viewportH_);
        if (layout.hero.status != ShopItemStatus::kAffordable) {
            confirming_ = false;
        }
    }
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    dirty_ = true;
}

void ShopView::SetLanguage(core::Language language) {
    if (language == language_) {
        return;
    }
    language_ = language;
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    dirty_ = true;
}

void ShopView::OnEnterSection() {
    // Entrar a la sección arranca con el foco en la pestaña "Shop" y sin
    // confirmación pendiente ni hover, para un punto de partida
    // predecible (brief §17).
    confirming_ = false;
    hoverId_.clear();
    keyboardFocus_ = false;
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    focus_.Focus("nav:shop");
    dirty_ = true;
}

ShopLayout ShopView::BuildLayout(float w, float h) const {
    ShopLayoutInput in;
    in.viewportW = w;
    in.viewportH = h;
    in.scrollY = ClampShopScroll(scrollY_, lastContentHeight_, h);
    in.language = language_;
    in.selectedPetId = selectedPetId_;
    in.hoverPetId = HoverPetId();
    in.confirming = confirming_;
    return BuildShopLayout(model_, in);
}

void ShopView::SyncFocusList(const ShopLayout& layout) {
    focus_.SetItems(layout.focusOrder);
}

std::string ShopView::HoverPetId() const {
    return StartsWith(hoverId_, "shopitem:") ? hoverId_.substr(9) : std::string();
}

void ShopView::SelectHero(const std::string& petId) {
    if (model_.Find(petId) == nullptr || petId == selectedPetId_) {
        return;
    }
    selectedPetId_ = petId;
    confirming_ = false;  // cambiar de hero descarta cualquier confirmación
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    dirty_ = true;
}

void ShopView::SelectHeroForQA(const std::string& petId) {
    SelectHero(petId);
}

ShopViewResult ShopView::ActivateWidget(const std::string& focusId) {
    ShopViewResult r;
    if (focusId.empty()) {
        return r;
    }
    if (ProductSection target; NavTargetSection(focusId, target)) {
        // Las TRES pestañas (Collection · Shop · Settings) se rutean por
        // la misma tabla — ver productui::NavTargetSection.
        r.switchSection = true;
        r.targetSection = target;
        r.dirty = true;
        return r;
    }
    if (StartsWith(focusId, "shopitem:")) {
        SelectHero(PetIdFromShopFocusId(focusId));
        r.dirty = true;
        return r;
    }
    if (focusId == "get") {
        // Primer paso deliberado: abre la confirmación inline. NO compra.
        confirming_ = true;
        SyncFocusList(BuildLayout(viewportW_, viewportH_));
        focus_.Focus("purchase:cancel");  // el foco arranca en Cancelar (no gastar por accidente)
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
        // Segundo paso deliberado: compra de verdad. El objetivo es la
        // identidad de catálogo del ítem (entitlementTarget), NO una
        // suposición sobre petId.
        const ShopLayout layout = BuildLayout(viewportW_, viewportH_);
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

ShopViewResult ShopView::OnMouseMove(float x, float y) {
    ShopViewResult r;
    const ShopLayout layout = BuildLayout(viewportW_, viewportH_);
    const std::string hit = layout.HitTest(x, y);
    if (hit != hoverId_) {
        hoverId_ = hit;
        dirty_ = true;
        r.dirty = true;
    }
    return r;
}

ShopViewResult ShopView::OnMouseDown(float x, float y) {
    keyboardFocus_ = false;
    const ShopLayout layout = BuildLayout(viewportW_, viewportH_);
    const std::string hit = layout.HitTest(x, y);
    if (hit.empty()) {
        dirty_ = true;
        return ShopViewResult{};
    }
    if (StartsWith(hit, "shopitem:") || StartsWith(hit, "nav:")) {
        focus_.Focus(hit);
    }
    ShopViewResult r = ActivateWidget(hit);
    r.dirty = true;
    dirty_ = true;
    return r;
}

ShopViewResult ShopView::OnWheel(float dyLines) {
    ShopViewResult r;
    const float before = scrollY_;
    scrollY_ = ClampShopScroll(scrollY_ - dyLines * kWheelStep, lastContentHeight_, viewportH_);
    if (scrollY_ != before) {
        dirty_ = true;
        r.dirty = true;
    }
    return r;
}

ShopViewResult ShopView::OnKey(int sdlKeycode, bool shiftHeld) {
    ShopViewResult r;
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
            if (confirming_) {
                // Esc cancela la confirmación, no cierra la ventana
                // (brief §23).
                confirming_ = false;
                SyncFocusList(BuildLayout(viewportW_, viewportH_));
                focus_.Focus("get");
                keyboardFocus_ = true;
                dirty_ = true;
                r.dirty = true;
                return r;
            }
            r.requestClose = true;
            return r;
        default:
            return r;
    }
}

ShopViewResult ShopView::OnViewportChanged() {
    ShopViewResult r;
    dirty_ = true;
    r.dirty = true;
    return r;
}

void ShopView::Render(
    UiPainter& painter, TextCache& text, PetPreviewCache& previews, float viewportW, float viewportH) {
    viewportW_ = viewportW;
    viewportH_ = viewportH;

    const ShopLayout layout = BuildLayout(viewportW, viewportH);
    lastContentHeight_ = layout.contentHeight;
    SyncFocusList(layout);
    const std::string focusedId = keyboardFocus_ ? focus_.FocusedId() : std::string();

    painter.Clear(theme::kBackground);
    DrawSectionHeader(painter, text, layout.header, clickBalance_, language_, hoverId_, focusedId);

    painter.PushClip(UiRect{0.0f, kHeaderClipTop, viewportW, std::max(0.0f, viewportH - kHeaderClipTop)});

    if (layout.empty) {
        painter.PopClip();
        return;
    }

    painter.FillRect(layout.galleryShelf, theme::kGalleryShelf);

    // --- Hero ---
    const ShopHero& h = layout.hero;
    if (!h.petId.empty()) {
        FillStagePrimitive(painter, h.stageSecondary, h.accent.angularShape,
                           h.accent.shapeTint.WithAlpha(kStageSecondaryAlpha));
        FillStagePrimitive(painter, h.stagePrimary, h.accent.angularShape,
                           h.accent.shapeTint.WithAlpha(kStagePrimaryAlpha));

        SDL_Texture* art = previews.Get(h.petId, std::string());
        const unsigned char artAlpha =
            h.status == ShopItemStatus::kOwned ? kOwnedArtAlpha : 255;
        painter.DrawTextureContained(art, h.art, artAlpha);

        DrawText(painter, text, h.displayName, type::kHeroName, TextWeight::kSemibold, theme::kText,
                 h.nameAnchor.x, h.nameAnchor.y + 24.0f, HAlign::kLeft);
        painter.FillRect(h.nameRule, h.accent.line);

        if (!h.speciesText.empty()) {
            DrawText(painter, text, h.speciesText, type::kHeroSpecies, TextWeight::kRegular,
                     theme::kTextMuted, h.speciesAnchor.x, h.speciesAnchor.y + 12.0f, HAlign::kLeft);
        }
        if (!h.descriptionText.empty()) {
            DrawTextWrapped(painter, text, h.descriptionText, type::kHeroBody, TextWeight::kRegular,
                            theme::kText, h.descriptionAnchor.x, h.descriptionAnchor.y + 13.0f,
                            h.descriptionAnchor.w, 17.0f, 3);
        }

        // Precio (salvo que ya esté en la colección).
        if (h.status != ShopItemStatus::kOwned) {
            DrawText(painter, text, h.priceText, type::kHeroStatus, TextWeight::kMedium, theme::kText,
                     h.priceAnchor.x, h.priceAnchor.y + 12.0f, HAlign::kLeft);
        }

        if (h.confirm.visible) {
            // Confirmación inline: pregunta + Cancelar / Confirmar. No es
            // un modal gigante — cabe en la columna del hero (brief §12).
            DrawTextWrapped(painter, text, h.confirm.prompt, type::kHeroBody, TextWeight::kRegular,
                            theme::kText, h.confirm.promptAnchor.x, h.confirm.promptAnchor.y + 13.0f,
                            h.confirm.promptAnchor.w, 16.0f, 2);

            // Cancelar: contorno discreto.
            painter.StrokeRoundRect(h.confirm.cancelButton, 8.0f, 1.5f, theme::kHairline);
            if (focusedId == h.confirm.cancelFocusId) {
                painter.StrokeRoundRect(h.confirm.cancelButton.Inset(-3.0f), 11.0f, 2.0f, theme::kText);
            }
            DrawText(painter, text, h.confirm.cancelLabel, type::kButton, TextWeight::kMedium,
                     theme::kTextMuted, h.confirm.cancelButton.CenterX(),
                     h.confirm.cancelButton.CenterY() + 4.5f, HAlign::kCenter);

            // Confirmar: relleno + tinta del acento del pet, igual que el
            // botón primario de la Collection — nunca casi-negro.
            painter.FillRoundRect(h.confirm.confirmButton, 8.0f, h.accent.softFill);
            painter.StrokeRoundRect(h.confirm.confirmButton, 8.0f, 1.5f, h.accent.line);
            if (focusedId == h.confirm.confirmFocusId) {
                painter.StrokeRoundRect(h.confirm.confirmButton.Inset(-3.0f), 11.0f, 2.0f, h.accent.line);
            }
            DrawText(painter, text, h.confirm.confirmLabel, type::kButton, TextWeight::kSemibold,
                     h.accent.deepInk, h.confirm.confirmButton.CenterX(),
                     h.confirm.confirmButton.CenterY() + 4.5f, HAlign::kCenter);
        } else if (h.actionEnabled) {
            painter.FillRoundRect(h.actionButton, 9.0f, h.accent.softFill);
            painter.StrokeRoundRect(h.actionButton, 9.0f, 1.5f, h.accent.line);
            if (focusedId == h.actionFocusId) {
                painter.StrokeRoundRect(h.actionButton.Inset(-3.0f), 12.0f, 2.0f, h.accent.line);
            }
            DrawText(painter, text, h.actionLabel, type::kButton, TextWeight::kSemibold, h.accent.deepInk,
                     h.actionButton.CenterX(), h.actionButton.CenterY() + 4.5f, HAlign::kCenter);
        } else if (h.showStatusLine) {
            // kOwned: "In your collection" (punto de acento). kInsufficient:
            // "Need N more clicks", en tono atenuado — sin acción, sin
            // ninguna insinuación de "gana clics así" (brief §15).
            float statusX = h.statusAnchor.x;
            const bool owned = h.status == ShopItemStatus::kOwned;
            if (owned) {
                painter.FillEllipse(UiRect{statusX, h.statusAnchor.y + 3.0f, 7.0f, 7.0f}, h.accent.line);
                statusX += 14.0f;
            }
            DrawText(painter, text, h.statusText, type::kHeroStatus, TextWeight::kMedium,
                     owned ? theme::kTextMuted : theme::kTextFaint, statusX, h.statusAnchor.y + 12.0f,
                     HAlign::kLeft);
        }
    }

    // --- Divisor + gallery ---
    painter.FillRect(layout.dividerRect, theme::kHairline);

    for (const ShopGalleryItem& g : layout.gallery) {
        const bool hovered = hoverId_ == g.focusId;
        const bool focused = focusedId == g.focusId;
        if (hovered) {
            painter.FillRoundRect(g.cell.Inset(2.0f), 12.0f, theme::kHoverWash);
        }
        if (focused) {
            painter.StrokeRoundRect(g.cell.Inset(2.0f), 12.0f, 2.0f, g.accentLine);
        }
        painter.FillRoundRect(g.art.Inset(-5.0f), 12.0f, g.pedestalTint.WithAlpha(kPedestalAlpha));

        SDL_Texture* art = previews.Get(g.petId, std::string());
        painter.DrawTextureContained(art, g.art,
                                     g.status == ShopItemStatus::kOwned ? kOwnedArtAlpha : 255);

        DrawText(painter, text, g.displayName, type::kGalleryName, TextWeight::kMedium, theme::kText,
                 g.name.CenterX(), g.name.y + 13.0f, HAlign::kCenter, static_cast<int>(g.cell.w - 6.0f));
        const UiColor secColor =
            g.status == ShopItemStatus::kOwned ? g.accentLine : theme::kTextMuted;
        DrawText(painter, text, g.secondaryText, type::kGalleryStatus, TextWeight::kRegular, secColor,
                 g.secondary_.CenterX(), g.secondary_.y + 11.0f, HAlign::kCenter,
                 static_cast<int>(g.cell.w - 6.0f));
    }

    painter.PopClip();
}

}  // namespace nimvlets::productui
