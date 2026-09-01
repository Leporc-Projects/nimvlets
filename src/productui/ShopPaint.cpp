#include "productui/ShopPaint.h"

#include <SDL3/SDL.h>

#include <algorithm>

#include "productui/UiTheme.h"

namespace nimvlets::productui {

using catalog::ShopItemStatus;
using platform::TextWeight;

namespace {

constexpr unsigned char kStagePrimaryAlpha = 92;
constexpr unsigned char kStageSecondaryAlpha = 52;
constexpr unsigned char kOwnedArtAlpha = 235;

// Color de la línea de info liviana revelada bajo una tarjeta: distingue
// asequible / insuficiente / poseído "quietly" (brief §6) — sin rojo,
// sin "no te alcanza".
UiColor RevealColor(ShopItemStatus status, UiColor accentLine) {
    switch (status) {
        case ShopItemStatus::kOwned:
            return accentLine;
        case ShopItemStatus::kAffordable:
            return theme::kTextMuted;
        case ShopItemStatus::kInsufficientBalance:
            return theme::kTextFaint;
    }
    return theme::kTextMuted;
}

}  // namespace

void FillShopStagePrimitive(UiPainter& painter, const UiRect& r, bool angular, UiColor color) {
    if (angular) {
        painter.FillRoundRect(r, std::min(r.w, r.h) * 0.32f, color);
    } else {
        painter.FillEllipse(r, color);
    }
}

void DrawShopTile(
    UiPainter& painter, TextCache& text, PetPreviewCache& previews, const ShopTile& t,
    double nameSize, unsigned char pedestalAlpha, bool hovered, bool focused, bool revealVisible,
    bool selectedMark) {
    if (hovered) {
        painter.FillRoundRect(t.cell.Inset(2.0f), 12.0f, theme::kHoverWash);
    }
    if (focused) {
        painter.StrokeRoundRect(t.cell.Inset(2.0f), 12.0f, 2.0f, t.accentLine);
    }
    painter.FillRoundRect(t.art.Inset(-5.0f), 12.0f, t.pedestalTint.WithAlpha(pedestalAlpha));

    SDL_Texture* art = previews.Get(t.petId, t.variantId);
    painter.DrawTextureContained(art,
                                 t.art,
                                 t.status == ShopItemStatus::kOwned ? kOwnedArtAlpha : 255);

    DrawText(painter, text, t.displayName, nameSize, TextWeight::kMedium, theme::kText,
             t.name.CenterX(), t.name.y + static_cast<float>(nameSize), HAlign::kCenter,
             static_cast<int>(t.cell.w - 6.0f));

    if (selectedMark) {
        const float ruleW = std::min(28.0f, t.cell.w * 0.34f);
        painter.FillRect(
            UiRect{t.name.CenterX() - ruleW * 0.5f, t.name.Bottom() + 3.0f, ruleW, 2.0f}, t.accentLine);
    }

    if (revealVisible && !t.revealText.empty()) {
        DrawText(painter, text, t.revealText, type::kGalleryStatus, TextWeight::kRegular,
                 RevealColor(t.status, t.accentLine), t.revealAnchor.CenterX(),
                 t.revealAnchor.y + 11.0f, HAlign::kCenter, static_cast<int>(t.cell.w - 6.0f));
    }
}

void DrawShopHero(
    UiPainter& painter, TextCache& text, PetPreviewCache& previews, const ShopHero& h,
    const std::string& focusedId) {
    if (h.petId.empty()) {
        return;
    }
    FillShopStagePrimitive(painter, h.stageSecondary, h.accent.angularShape,
                           h.accent.shapeTint.WithAlpha(kStageSecondaryAlpha));
    FillShopStagePrimitive(painter, h.stagePrimary, h.accent.angularShape,
                           h.accent.shapeTint.WithAlpha(kStagePrimaryAlpha));

    SDL_Texture* art = previews.Get(h.petId, h.variantId);
    const unsigned char artAlpha = h.status == ShopItemStatus::kOwned ? kOwnedArtAlpha : 255;
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

    if (h.status != ShopItemStatus::kOwned) {
        DrawText(painter, text, h.priceText, type::kHeroStatus, TextWeight::kMedium, theme::kText,
                 h.priceAnchor.x, h.priceAnchor.y + 12.0f, HAlign::kLeft);
    }

    if (h.confirm.visible) {
        DrawTextWrapped(painter, text, h.confirm.prompt, type::kHeroBody, TextWeight::kRegular,
                        theme::kText, h.confirm.promptAnchor.x, h.confirm.promptAnchor.y + 13.0f,
                        h.confirm.promptAnchor.w, 16.0f, 2);

        painter.StrokeRoundRect(h.confirm.cancelButton, 8.0f, 1.5f, theme::kHairline);
        if (focusedId == h.confirm.cancelFocusId) {
            painter.StrokeRoundRect(h.confirm.cancelButton.Inset(-3.0f), 11.0f, 2.0f, theme::kText);
        }
        DrawText(painter, text, h.confirm.cancelLabel, type::kButton, TextWeight::kMedium,
                 theme::kTextMuted, h.confirm.cancelButton.CenterX(),
                 h.confirm.cancelButton.CenterY() + 4.5f, HAlign::kCenter);

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

}  // namespace nimvlets::productui
