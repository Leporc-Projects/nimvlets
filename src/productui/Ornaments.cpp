#include "productui/Ornaments.h"

#include <algorithm>

#include "productui/OrnamentGeometry.h"
#include "productui/UiTheme.h"

namespace nimvlets::productui {

void DrawSparkle(UiPainter& p, float cx, float cy, float radius, UiColor color) {
    if (radius <= 0.0f || color.a == 0) {
        return;
    }
    // Un rombo alto y fino cruzado con uno ancho y fino: juntos leen
    // como una estrella de 4 puntas con lados cóncavos ("✦"). Las
    // puntas llegan a ±radius en los dos ejes (ver SparkleBounds).
    const float thin = radius * 0.62f;
    p.FillDiamond(UiRect{cx - thin * 0.5f, cy - radius, thin, radius * 2.0f}, color);
    p.FillDiamond(UiRect{cx - radius, cy - thin * 0.5f, radius * 2.0f, thin}, color);
}

void DrawDiamond(UiPainter& p, const UiRect& r, UiColor color) {
    p.FillDiamond(r, color);
}

void DrawOrnamentalDivider(UiPainter& p, const UiRect& band, UiColor line, UiColor ornament) {
    const float cy = band.CenterY();
    constexpr float kDiamond = 6.0f;
    constexpr float kGap = 9.0f;  // aire a cada lado del rombo
    const float ruleLen = OrnamentalDividerRuleLen(band.w, kGap);
    if (ruleLen > 0.0f) {
        p.FillRect(UiRect{band.x, cy - 0.5f, ruleLen, 1.0f}, line);
        p.FillRect(UiRect{band.x + band.w - ruleLen, cy - 0.5f, ruleLen, 1.0f}, line);
    }
    p.FillDiamond(UiRect{band.CenterX() - kDiamond * 0.5f, cy - kDiamond * 0.5f, kDiamond, kDiamond},
                  ornament);
}

void DrawAccentRule(UiPainter& p, const UiRect& r, UiColor color) {
    p.FillRect(r, color);
}

float DrawHeadingMotif(UiPainter& p, float x, float centerY, UiColor color) {
    constexpr float kDiamond = 6.0f;
    p.FillRect(UiRect{x, centerY - 0.5f, 5.0f, 1.0f}, color);
    p.FillDiamond(UiRect{x + 7.0f, centerY - kDiamond * 0.5f, kDiamond, kDiamond}, color);
    return HeadingMotifAdvance(kDiamond);
}

void DrawSoftPanel(
    UiPainter& p, const UiRect& r, float radius, UiColor surface, UiColor border, bool innerHighlight) {
    if (surface.a != 0) {
        p.FillRoundRect(r, radius, surface);
    }
    if (innerHighlight) {
        p.StrokeRoundRect(r.Inset(1.0f), std::max(0.0f, radius - 1.0f), 1.0f, tokens::kBorderInner);
    }
    if (border.a != 0) {
        p.StrokeRoundRect(r, radius, 1.0f, border);
    }
}

void DrawButton(
    UiPainter& p, TextCache& text, const UiRect& r, const std::string& label, const ButtonVisual& v,
    double labelSize, platform::TextWeight weight) {
    if (v.fill.a != 0) {
        p.FillRoundRect(r, 9.0f, v.fill);
    }
    if (v.border.a != 0 && v.borderWidth > 0.0f) {
        p.StrokeRoundRect(r, 9.0f, v.borderWidth, v.border);
    }
    if (v.drawFocusRing) {
        p.StrokeRoundRect(r.Inset(-3.0f), 12.0f, 2.0f, tokens::kFocus);
    }
    DrawText(p, text, label, labelSize, weight, v.ink, r.CenterX(), r.CenterY() + 4.5f, HAlign::kCenter,
             static_cast<int>(r.w - 6.0f));
}

}  // namespace nimvlets::productui
