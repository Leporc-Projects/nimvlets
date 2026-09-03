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

void DrawBrandEmblem(UiPainter& p, float cx, float cy, float size, UiColor color) {
    if (size <= 0.0f || color.a == 0) {
        return;
    }
    // Jerarquía: spark principal centrado, spark chico arriba-derecha,
    // rombo minúsculo abajo-izquierda. Composición fija y determinista.
    const float main = size * 0.42f;
    DrawSparkle(p, cx, cy, main, color);
    DrawSparkle(p, cx + size * 0.34f, cy - size * 0.30f, size * 0.20f, color);
    const float d = size * 0.14f;
    p.FillDiamond(UiRect{cx - size * 0.36f - d * 0.5f, cy + size * 0.26f - d * 0.5f, d, d},
                  color.WithAlpha(color.a > 190 ? 190 : color.a));
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

void DrawFlankedLabel(
    UiPainter& p, TextCache& text, const std::string& label, const type::FontRole& role,
    UiColor labelColor, UiColor ornamentColor, float centerX, float baselineY) {
    constexpr float kDiamond = 5.0f;  // lado del rombo
    constexpr float kGap = 10.0f;     // rombo <-> texto
    const float w = MeasureText(text, label, role, p.Scale());
    const float halfBlock = FlankedLabelHalfBlock(w, kDiamond, kGap);
    // Centro visual del texto ~ baseline - 4 pt (altura de x aproximada).
    const float cy = baselineY - 4.0f;
    p.FillDiamond(UiRect{centerX - halfBlock, cy - kDiamond * 0.5f, kDiamond, kDiamond}, ornamentColor);
    p.FillDiamond(UiRect{centerX + halfBlock - kDiamond, cy - kDiamond * 0.5f, kDiamond, kDiamond},
                  ornamentColor);
    DrawText(p, text, label, role, labelColor, centerX, baselineY, HAlign::kCenter);
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
    const type::FontRole& role) {
    const float radius = v.pill ? r.h * 0.5f : 9.0f;

    // Filo de oro cálido, POR FUERA del botón (referencia concepto).
    if (v.edgeAccent.a != 0) {
        p.StrokeRoundRect(r.Inset(-1.5f), radius + 1.5f, 1.0f, v.edgeAccent);
    }
    if (v.fill.a != 0) {
        p.FillRoundRect(r, radius, v.fill);
    }
    // 2-tono sutil: un hairline claro arriba + uno oscuro abajo, sin
    // gradiente real ni filtro caro (sensación "levantada").
    if (v.topHighlight.a != 0) {
        p.FillRect(UiRect{r.x + radius, r.y + 1.0f, std::max(0.0f, r.w - 2.0f * radius), 1.0f},
                   v.topHighlight);
    }
    if (v.bottomShade.a != 0) {
        p.FillRect(UiRect{r.x + radius, r.Bottom() - 2.0f, std::max(0.0f, r.w - 2.0f * radius), 1.0f},
                   v.bottomShade);
    }
    if (v.border.a != 0 && v.borderWidth > 0.0f) {
        p.StrokeRoundRect(r, radius, v.borderWidth, v.border);
    }
    if (v.drawFocusRing) {
        p.StrokeRoundRect(r.Inset(-3.5f), radius + 3.5f, 2.0f, tokens::kFocus);
    }

    const float labelCap = v.sparkle ? r.w - 34.0f : r.w - 6.0f;
    DrawText(p, text, label, role, v.ink, r.CenterX(), r.CenterY() + 4.5f, HAlign::kCenter,
             static_cast<int>(std::max(8.0f, labelCap)));
    if (v.sparkle) {
        DrawSparkle(p, r.Right() - 15.0f, r.CenterY(), 4.0f, v.ink);
    }
}

}  // namespace nimvlets::productui
