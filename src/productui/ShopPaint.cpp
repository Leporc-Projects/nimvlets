#include "productui/ShopPaint.h"

#include <SDL3/SDL.h>

#include <algorithm>

#include "productui/ButtonStyle.h"
#include "productui/Ornaments.h"
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
    // Tarjeta = panel enmarcado suave (Block 12A, brief §21): superficie
    // que "levanta" un susurro sobre el fondo + hairline; wash al hover;
    // anillo de foco de teclado neutro. Geometría de la rejilla,
    // contenido y semántica de hover/selección SIN cambios.
    DrawSoftPanel(painter, t.cell.Inset(2.0f), 12.0f,
                  hovered ? tokens::kHoverWash : tokens::kSurfaceRaised, tokens::kBorder,
                  /*innerHighlight=*/false);
    if (focused) {
        painter.StrokeRoundRect(t.cell.Inset(2.0f), 12.0f, 2.0f, tokens::kFocus);
    }
    // Pedestal con el tinte de identidad del pet detrás del arte —
    // conserva el acento por pet en la card (brief §21).
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

    DrawText(painter, text, h.displayName, type::role::kHeroTitle, tokens::kTextPrimary,
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
                        tokens::kTextPrimary, h.confirm.promptAnchor.x, h.confirm.promptAnchor.y + 13.0f,
                        h.confirm.promptAnchor.w, 16.0f, 2);

        // Cancelar = Secondary (contorno hairline); Confirmar = Primary
        // con el acento del pet (Block 12A — sistema de botones).
        DrawButton(painter, text, h.confirm.cancelButton, h.confirm.cancelLabel,
                   ResolveButtonVisual(ButtonRole::kSecondary, nullptr,
                                       ButtonStateFlags{false, false,
                                                        focusedId == h.confirm.cancelFocusId, false}),
                   type::role::kButton.WithWeight(TextWeight::kMedium));
        DrawButton(painter, text, h.confirm.confirmButton, h.confirm.confirmLabel,
                   ResolveButtonVisual(ButtonRole::kPrimary, &h.accent,
                                       ButtonStateFlags{false, false,
                                                        focusedId == h.confirm.confirmFocusId, false}),
                   type::role::kButton);
    } else if (h.actionEnabled) {
        DrawButton(painter, text, h.actionButton, h.actionLabel,
                   ResolveButtonVisual(ButtonRole::kPrimary, &h.accent,
                                       ButtonStateFlags{false, false,
                                                        focusedId == h.actionFocusId, false}),
                   type::role::kButton);
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
