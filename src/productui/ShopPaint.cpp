#include "productui/ShopPaint.h"

#include <SDL3/SDL.h>

#include <algorithm>

#include "productui/ButtonStyle.h"
#include "productui/Contrast.h"
#include "productui/Ornaments.h"
#include "productui/UiTheme.h"

namespace nimvlets::productui {

using catalog::ShopItemStatus;
using platform::TextWeight;

namespace {

constexpr unsigned char kStagePrimaryAlpha = 88;
constexpr unsigned char kStageSecondaryAlpha = 50;
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
    const type::FontRole& nameRole, bool hovered, bool focused, bool revealVisible, bool selectedMark) {
    // UNA card coherente (convergencia DEC-147): superficie + borde,
    // SIN el pedestal-caja detrás del arte que creaba el "doble marco".
    // La identidad del pet vive en: borde/tinte al seleccionar, regla de
    // acento, y el color del texto de precio/estado.
    const UiRect card = t.cell.Inset(2.0f);
    UiColor surface = tokens::kSurfaceRaised;
    UiColor border = tokens::kBorder;
    float borderW = 1.0f;
    if (selectedMark) {
        // La card seleccionada toma la identidad del pet. Un pet "hondo"
        // (Nidir) recibe una superficie de acento MÁS RICA — la card
        // violeta del concept — vía la MISMA estrategia centralizada que
        // el CTA (DEC-148); el resto, el tinte suave de siempre. El
        // nombre (kTextPrimary) queda ≥ 7:1 sobre cualquiera de las dos.
        const bool deep = t.emphasis == AccentEmphasis::kDeep;
        surface = deep ? Mix(tokens::kSurfaceRaised, t.accentSecondary, 0.36)
                       : Mix(tokens::kSurfaceRaised, t.pedestalTint, 0.55);
        border = deep ? Lighten(t.accentLine, 0.16) : t.accentLine;
        borderW = 2.0f;
    } else if (hovered) {
        surface = tokens::kHoverWash;
    } else {
        // Card NO seleccionada: una PIZCA de personalidad — un hairline
        // inferior con el tono del pet, muy tenue (brief §24). Sigue
        // perteneciendo al universo cálido común; no son cinco temas.
        border = Mix(tokens::kBorder, t.accentLine, 0.16);
    }
    painter.FillRoundRect(card, 12.0f, surface);
    painter.StrokeRoundRect(card, 12.0f, borderW, border);
    if (focused) {
        painter.StrokeRoundRect(card.Inset(-3.5f), 15.5f, 2.0f, tokens::kFocus);
    }

    SDL_Texture* art = previews.Get(t.petId, t.variantId);
    painter.DrawTextureContained(art, t.art,
                                 t.status == ShopItemStatus::kOwned ? kOwnedArtAlpha : 255);

    DrawText(painter, text, t.displayName, nameRole, tokens::kTextPrimary, t.name.CenterX(),
             t.name.y + static_cast<float>(nameRole.size), HAlign::kCenter,
             static_cast<int>(card.w - 8.0f));

    if (selectedMark) {
        const float ruleW = std::min(26.0f, card.w * 0.32f);
        painter.FillRect(
            UiRect{t.name.CenterX() - ruleW * 0.5f, t.name.Bottom() + 3.0f, ruleW, 2.0f}, t.accentLine);
    }

    if (revealVisible && !t.revealText.empty()) {
        DrawText(painter, text, t.revealText, type::role::kCaption, RevealColor(t.status, t.accentLine),
                 t.revealAnchor.CenterX(), t.revealAnchor.y + 11.0f, HAlign::kCenter,
                 static_cast<int>(card.w - 8.0f));
    }
}

void DrawShopHero(
    UiPainter& painter, TextCache& text, PetPreviewCache& previews, const ShopHero& h,
    const std::string& focusedId) {
    if (h.petId.empty()) {
        return;
    }
    // Panel enmarcado suave alrededor de TODO el hero — la "editorial
    // panel" de la referencia (convergencia DEC-147). Sin sombra, sin
    // glass: superficie que levanta + hairline + highlight interior.
    DrawSoftPanel(painter, h.heroPanel, 16.0f, tokens::kSurfaceRaised, tokens::kBorder,
                  /*innerHighlight=*/true);

    // Stage recortado al panel — el halo teñido no "asoma" por fuera del
    // marco del hero (brief §8 / DEC-148).
    const UiRect stageBounds = h.heroPanel.Inset(2.0f);
    FillShopStagePrimitive(painter, h.stageSecondary.ClampedTo(stageBounds), h.accent.angularShape,
                           h.accent.shapeTint.WithAlpha(kStageSecondaryAlpha));
    FillShopStagePrimitive(painter, h.stagePrimary.ClampedTo(stageBounds), h.accent.angularShape,
                           h.accent.shapeTint.WithAlpha(kStagePrimaryAlpha));

    SDL_Texture* art = previews.Get(h.petId, h.variantId);
    painter.DrawTextureContained(art, h.art,
                                 h.status == ShopItemStatus::kOwned ? kOwnedArtAlpha : 255);

    // Nombre (serif editorial) + DIVISOR 1: rombo central = acento del
    // pet. La regla llega casi al ancho de la columna de detalle (como el
    // concept), pero no cruza al lado del arte — `nameRule.w` YA es solo
    // la columna de texto (DEC-148).
    constexpr float kHeroDividerMaxW = 430.0f;
    DrawText(painter, text, h.displayName, type::role::kHeroTitle, tokens::kTextPrimary,
             h.nameAnchor.x, h.nameAnchor.y + 24.0f, HAlign::kLeft);
    DrawOrnamentalDivider(
        painter,
        UiRect{h.nameRule.x, h.nameRule.y - 3.5f, std::min(h.nameRule.w, kHeroDividerMaxW), 8.0f},
        tokens::kBorder, h.accent.line);

    // Especie en el TONO del pet (metadata, pero especial).
    if (!h.speciesText.empty()) {
        DrawText(painter, text, h.speciesText, type::role::kMetadata.WithWeight(TextWeight::kMedium),
                 h.accent.deepInk, h.speciesAnchor.x, h.speciesAnchor.y + 12.0f, HAlign::kLeft);
    }
    if (!h.descriptionText.empty()) {
        DrawTextWrapped(painter, text, h.descriptionText, type::kHeroBody, TextWeight::kRegular,
                        tokens::kTextPrimary, h.descriptionAnchor.x, h.descriptionAnchor.y + 13.0f,
                        h.descriptionAnchor.w, 17.0f, 3);
    }

    // DIVISOR 2: identidad/descripción -> economía/acción (rombo neutro).
    if (h.detailDividerRect.w > 0.0f) {
        DrawOrnamentalDivider(
            painter,
            UiRect{h.detailDividerRect.x, h.detailDividerRect.CenterY() - 4.0f,
                   std::min(h.detailDividerRect.w, kHeroDividerMaxW), 8.0f},
            tokens::kBorder, tokens::kOrnamentNeutral);
    }

    // Precio con el spark de moneda (referencia: "✦ 300 clicks", más
    // confiado que el cuerpo). Sans numérico, como el wallet — spark un
    // pelín más grande y con más aire hasta el número (DEC-148).
    if (h.status != ShopItemStatus::kOwned) {
        DrawSparkle(painter, h.priceAnchor.x + 7.0f, h.priceAnchor.y + h.priceAnchor.h * 0.5f, 6.0f,
                    tokens::kOrnamentNeutral);
        DrawText(painter, text, h.priceText, type::role::kPrice, tokens::kTextPrimary,
                 h.priceAnchor.x + 22.0f, h.priceAnchor.y + 15.0f, HAlign::kLeft);
    }

    if (h.confirm.visible) {
        DrawTextWrapped(painter, text, h.confirm.prompt, type::kHeroBody, TextWeight::kRegular,
                        tokens::kTextPrimary, h.confirm.promptAnchor.x, h.confirm.promptAnchor.y + 13.0f,
                        h.confirm.promptAnchor.w, 16.0f, 2);

        // Cancelar = Secondary (hairline). Confirmar = Primary CONTENIDO
        // (relleno de acento + borde) — fuerte pero SIN los adornos de la
        // CTA principal, para no confundir la jerarquía (brief §11).
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
        // La CTA principal: pill generosa, relleno del acento, filo de
        // oro tenue, spark a la derecha.
        DrawButton(painter, text, h.actionButton, h.actionLabel,
                   ResolveButtonVisual(ButtonRole::kPrimaryCta, &h.accent,
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
