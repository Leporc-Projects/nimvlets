#include "productui/SectionHeaderView.h"

#include "productui/UiTheme.h"

namespace nimvlets::productui {

using platform::TextWeight;

void DrawSectionHeader(
    UiPainter& painter,
    TextCache& text,
    const SectionHeaderLayout& header,
    const std::string& hoverFocusId,
    const std::string& keyboardFocusId) {
    // Título de marca (izquierda) + balance discreto (derecha) — misma
    // línea, mismo tamaño/color que Block 06. `header.clicksText` ya
    // viene formateado del layout puro a partir del balance CANÓNICO de
    // ProductWindow (corrección de QA del owner — ver el .h): acá NO se
    // vuelve a formatear ni a elegir un valor.
    const float titleBaseline = header.titleAnchor.y + 18.0f;
    DrawText(painter, text, "Nimvlets", type::kTitle, TextWeight::kSemibold, theme::kText,
             header.titleAnchor.x, titleBaseline, HAlign::kLeft);
    DrawText(painter, text, header.clicksText, type::kClicks, TextWeight::kRegular,
             theme::kTextMuted, header.clicksAnchorRight.x, titleBaseline, HAlign::kRight);

    // Pestañas "Collection · Shop": texto simple, la activa en el tono
    // de texto principal + una regla de 2pt; la inactiva atenuada pero
    // clickeable. Sin cajas tipo tab-bar (brief §7: composición compacta
    // consistente con la Collection).
    for (std::size_t i = 0; i < header.tabs.size(); ++i) {
        const SectionTab& tab = header.tabs[i];
        const bool hovered = !hoverFocusId.empty() && hoverFocusId == tab.focusId;
        const bool focused = !keyboardFocusId.empty() && keyboardFocusId == tab.focusId;
        if (hovered && !tab.active) {
            painter.FillRoundRect(tab.hitRect.Inset(1.0f), 8.0f, theme::kHoverWash);
        }
        if (focused) {
            painter.StrokeRoundRect(tab.hitRect.Inset(1.0f), 8.0f, 2.0f, theme::kText);
        }
        if (i > 0) {
            const float dotX = 0.5f * (header.tabs[i - 1].labelAnchor.Right() + tab.labelAnchor.x);
            DrawText(painter, text, "·", type::kSectionTitle, TextWeight::kRegular, theme::kTextFaint,
                     dotX, tab.labelAnchor.y + 16.0f, HAlign::kCenter);
        }
        DrawText(painter, text, tab.label, type::kSectionTitle,
                 tab.active ? TextWeight::kSemibold : TextWeight::kRegular,
                 tab.active ? theme::kText : theme::kTextMuted, tab.labelAnchor.x,
                 tab.labelAnchor.y + 16.0f, HAlign::kLeft);
        if (tab.underline.w > 0.0f) {
            painter.FillRect(tab.underline, theme::kText);
        }
    }
}

}  // namespace nimvlets::productui
