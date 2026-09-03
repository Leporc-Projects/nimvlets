#include "productui/SectionHeaderView.h"

#include <algorithm>

#include "productui/Ornaments.h"
#include "productui/UiTheme.h"

namespace nimvlets::productui {

using platform::TextWeight;

namespace {

// Wordmark: un EMBLEMA procedural compuesto a la izquierda del nombre
// literal del producto (referencia A / convergencia DEC-147). No es un
// logo de imagen; el nombre de la app en la barra de título del SO NO
// se toca.
constexpr float kBrandEmblem = 15.0f;   // ancho aprox. del clúster
constexpr float kBrandGap = 9.0f;       // emblema -> "Nimvlets"

}  // namespace

void MeasureNavLabels(TextCache& text, core::Language lang, float scale, float outW[3]) {
    const ProductSection order[3] = {ProductSection::kCollection, ProductSection::kShop,
                                     ProductSection::kSettings};
    for (int i = 0; i < 3; ++i) {
        outW[i] = MeasureText(text, SectionLabel(order[i], lang), type::role::kSectionLabel, scale);
    }
}

void DrawSectionHeader(
    UiPainter& painter,
    TextCache& text,
    const SectionHeaderLayout& header,
    const std::string& hoverFocusId,
    const std::string& keyboardFocusId) {
    const float titleBaseline = header.titleAnchor.y + 18.0f;

    // --- Wordmark: [emblema] Nimvlets (serif del sistema) ---------
    const float brandX = header.titleAnchor.x;
    DrawBrandEmblem(painter, brandX + kBrandEmblem * 0.5f, titleBaseline - 6.0f, kBrandEmblem,
                    tokens::kOrnamentNeutral);
    DrawText(painter, text, "Nimvlets", type::role::kBrand, tokens::kTextPrimary,
             brandX + kBrandEmblem + kBrandGap, titleBaseline, HAlign::kLeft);

    // --- Wallet pill (referencia B — aprobada, se mantiene) -------
    // `header.clicksText` sigue siendo el ÚNICO string de balance que se
    // dibuja — invariante del wallet canónico (DEC-138).
    const float walletTextW = MeasureText(text, header.clicksText, type::role::kWallet, painter.Scale());
    const WalletPillMetrics wp = ComputeWalletPill(walletTextW);
    const UiRect pill{
        header.clicksAnchorRight.x - wp.width,
        header.titleAnchor.y + (header.titleAnchor.h - wp.height) * 0.5f,
        wp.width,
        wp.height,
    };
    painter.FillRoundRect(pill, wp.height * 0.5f, tokens::kWalletSurface);
    painter.StrokeRoundRect(pill, wp.height * 0.5f, 1.0f, tokens::kWalletBorder);
    DrawSparkle(painter, pill.x + wp.sparkCenterX, pill.CenterY(), 4.5f, tokens::kOrnamentNeutral);
    DrawText(painter, text, header.clicksText, type::role::kWallet, tokens::kTextSecondary,
             pill.x + wp.textLeftX, pill.CenterY() + 4.0f, HAlign::kLeft);

    // --- Pestañas "Collection · Shop · Settings" (serif) -----------
    // Activa: tono de texto principal + semibold. Inactiva: atenuada
    // pero clickeable. Separadores = un rombo minúsculo en tono
    // ornamento. Indicador activo = "── ◇ ──" en el MISMO tono de
    // ornamento: dos segmentos de regla fina que se extienden un poco
    // MÁS ALLÁ del texto, con un rombo centrado en el hueco (referencia
    // C / convergencia DEC-147). Nada de línea negra.
    for (std::size_t i = 0; i < header.tabs.size(); ++i) {
        const SectionTab& tab = header.tabs[i];
        const bool hovered = !hoverFocusId.empty() && hoverFocusId == tab.focusId;
        const bool focused = !keyboardFocusId.empty() && keyboardFocusId == tab.focusId;
        if (hovered && !tab.active) {
            painter.FillRoundRect(tab.hitRect.Inset(1.0f), 8.0f, tokens::kHoverWash);
        }
        if (focused) {
            painter.StrokeRoundRect(tab.hitRect.Inset(1.0f), 8.0f, 2.0f, tokens::kFocus);
        }
        if (i > 0) {
            const float sepX =
                0.5f * (header.tabs[i - 1].labelAnchor.Right() + tab.labelAnchor.x);
            DrawDiamond(painter, UiRect{sepX - 2.5f, tab.labelAnchor.y + 8.0f, 5.0f, 5.0f},
                        tokens::kOrnamentNeutral.WithAlpha(150));
        }
        DrawText(painter, text, tab.label,
                 type::role::kSectionLabel.WithWeight(tab.active ? TextWeight::kSemibold
                                                                 : TextWeight::kRegular),
                 tab.active ? tokens::kTextPrimary : tokens::kTextSecondary, tab.labelAnchor.x,
                 tab.labelAnchor.y + 16.0f, HAlign::kLeft);
        if (tab.underline.w > 0.0f) {
            constexpr float kExt = 6.0f;    // la regla sobresale del texto a cada lado
            constexpr float kDiamond = 6.0f;
            constexpr float kGap = 5.0f;    // hueco de la regla a cada lado del rombo
            const float ruleY = tab.underline.y + 1.0f;
            const float cx = tab.underline.CenterX();
            const float leftEdge = tab.underline.x - kExt;
            const float rightEdge = tab.underline.Right() + kExt;
            const float segLen = std::max(0.0f, (cx - kGap - kDiamond * 0.5f) - leftEdge);
            if (segLen > 0.0f) {
                painter.FillRect(UiRect{leftEdge, ruleY, segLen, 1.5f}, tokens::kOrnamentNeutral);
                painter.FillRect(UiRect{rightEdge - segLen, ruleY, segLen, 1.5f},
                                 tokens::kOrnamentNeutral);
            }
            painter.FillDiamond(
                UiRect{cx - kDiamond * 0.5f, ruleY + 0.75f - kDiamond * 0.5f, kDiamond, kDiamond},
                tokens::kOrnamentNeutral);
        }
    }
}

}  // namespace nimvlets::productui
