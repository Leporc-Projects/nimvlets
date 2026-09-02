#include "productui/SectionHeaderView.h"

#include "productui/Ornaments.h"
#include "productui/UiTheme.h"

namespace nimvlets::productui {

using platform::TextWeight;

namespace {

// Wordmark: un spark procedural chico a la IZQUIERDA del nombre literal
// del producto (referencia A). No es un logo de imagen; el nombre de la
// app en la barra de título del SO NO se toca.
constexpr float kBrandSpark = 11.0f;   // diámetro del spark del wordmark
constexpr float kBrandGap = 8.0f;      // spark -> "Nimvlets"

}  // namespace

void DrawSectionHeader(
    UiPainter& painter,
    TextCache& text,
    const SectionHeaderLayout& header,
    const std::string& hoverFocusId,
    const std::string& keyboardFocusId) {
    const float titleBaseline = header.titleAnchor.y + 18.0f;

    // --- Wordmark: ✦ Nimvlets --------------------------------------
    const float brandX = header.titleAnchor.x;
    DrawSparkle(painter, brandX + kBrandSpark * 0.5f, titleBaseline - 5.0f, kBrandSpark * 0.5f,
                tokens::kOrnamentNeutral);
    DrawText(painter, text, "Nimvlets", type::role::kBrand, TextWeight::kSemibold, tokens::kTextPrimary,
             brandX + kBrandSpark + kBrandGap, titleBaseline, HAlign::kLeft);

    // --- Wallet pill (referencia B) -------------------------------
    // `header.clicksText` sigue siendo el ÚNICO string de balance que se
    // dibuja — invariante del wallet canónico (DEC-138). La pill es una
    // cápsula cálida discreta con un spark chico: parte de la economía
    // del mundo, sin parecer un contador de gemas premium.
    const float walletTextW =
        MeasureText(text, header.clicksText, type::role::kWallet, TextWeight::kMedium, painter.Scale());
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
    DrawText(painter, text, header.clicksText, type::role::kWallet, TextWeight::kMedium,
             tokens::kTextSecondary, pill.x + wp.textLeftX, pill.CenterY() + 4.0f, HAlign::kLeft);

    // --- Pestañas "Collection · Shop · Settings" -----------------
    // La activa en el tono de texto principal + una regla fina y un
    // rombo chico centrado (referencia C); la inactiva atenuada pero
    // clickeable. Separadores = un rombo minúsculo en tono ornamento en
    // vez del "·". Sin cajas tipo tab-bar (brief §15: el carácter de
    // navegación de texto está aprobado).
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
        DrawText(painter, text, tab.label, type::role::kSectionTitle,
                 tab.active ? TextWeight::kSemibold : TextWeight::kRegular,
                 tab.active ? tokens::kTextPrimary : tokens::kTextSecondary, tab.labelAnchor.x,
                 tab.labelAnchor.y + 16.0f, HAlign::kLeft);
        if (tab.underline.w > 0.0f) {
            painter.FillRect(UiRect{tab.underline.x, tab.underline.y, tab.underline.w, 1.5f},
                             tokens::kTextPrimary);
            DrawDiamond(painter,
                        UiRect{tab.underline.CenterX() - 3.0f, tab.underline.y + 3.0f, 6.0f, 6.0f},
                        tokens::kOrnamentNeutral);
        }
    }
}

}  // namespace nimvlets::productui
