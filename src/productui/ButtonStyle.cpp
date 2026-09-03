#include "productui/ButtonStyle.h"

#include "productui/Contrast.h"
#include "productui/VisualTokens.h"

namespace nimvlets::productui {

namespace {

// AA para texto normal. El acento por pet de Bunny/Nidir/Frin ya lo
// cumple con margen (PetAccentTest); esto es el guardrail para un pet
// futuro con un acento claro y para el fallback neutro.
constexpr double kMinLabelContrast = 4.5;

// Relleno de la CTA: un pelín más saturado que el softFill pálido del
// Primary contenido — más "confiado" pero todavía claro, así funciona
// para Bunny (apricot) / Nidir (violeta) / Frin (hielo) por igual sin
// volverse un negro arbitrario (DEC-121).
UiColor CtaFill(const PetAccent& acc) { return Mix(acc.softFill, acc.line, 0.35); }

}  // namespace

ButtonVisual ResolveButtonVisual(ButtonRole role, const PetAccent* accent, ButtonStateFlags st) {
    // Fallback neutro cuando no hay contexto de pet (Cancel, avisos de
    // Settings, …) — PetAccentFor("") devuelve el neutro cálido con
    // todos los campos poblados.
    const PetAccent acc = accent != nullptr ? *accent : PetAccentFor("");

    ButtonVisual v;
    switch (role) {
        case ButtonRole::kPrimaryCta: {
            const UiColor fill = CtaFill(acc);
            v.fill = fill;
            v.border = acc.line;
            v.borderWidth = 1.5f;
            // Etiqueta: la mejor de {deepInk oscuro, cream claro} sobre el
            // relleno — para casi todos los pets gana deepInk (relleno
            // claro), pero si un pet futuro trae un relleno oscuro elige
            // el cream y lo aclara lo justo.
            v.ink = BestForeground(fill, acc.deepInk, Lighten(tokens::kCanvas, 0.55), kMinLabelContrast);
            v.topHighlight = Lighten(fill, 0.35);
            v.bottomShade = Darken(fill, 0.10);
            v.edgeAccent = tokens::kOrnamentNeutral.WithAlpha(150);
            v.pill = true;
            v.sparkle = true;
            break;
        }
        case ButtonRole::kPrimary:
            v.fill = acc.softFill;
            v.border = acc.line;
            v.ink = EnsureContrastOn(acc.deepInk, acc.softFill, kMinLabelContrast);
            v.borderWidth = 1.5f;
            break;
        case ButtonRole::kSecondary:
            v.fill = UiColor{0, 0, 0, 0};
            v.border = tokens::kBorder;
            v.ink = tokens::kTextSecondary;
            v.borderWidth = 1.5f;
            break;
        case ButtonRole::kQuiet:
            v.fill = UiColor{0, 0, 0, 0};
            v.border = UiColor{0, 0, 0, 0};
            v.ink = tokens::kTextSecondary;
            v.borderWidth = 0.0f;
            break;
    }

    if (st.disabled) {
        // Legible pero claramente no accionable — nunca invisible.
        const bool filled = role == ButtonRole::kPrimaryCta || role == ButtonRole::kPrimary;
        v.fill = filled ? tokens::kSurfaceSoft : UiColor{0, 0, 0, 0};
        v.border = role == ButtonRole::kQuiet ? UiColor{0, 0, 0, 0} : tokens::kBorder;
        v.ink = tokens::kTextMuted;
        v.topHighlight = UiColor{0, 0, 0, 0};
        v.bottomShade = UiColor{0, 0, 0, 0};
        v.edgeAccent = UiColor{0, 0, 0, 0};
        v.sparkle = false;
        v.drawFocusRing = false;
        return v;
    }

    const bool filled = role == ButtonRole::kPrimaryCta || role == ButtonRole::kPrimary;
    if (st.pressed) {
        v.fill = Darken(v.fill.a != 0 ? v.fill : tokens::kHoverWash, 0.12);
    } else if (st.hovered) {
        v.fill = filled ? Darken(v.fill, 0.05) : tokens::kHoverWash;
    }

    v.drawFocusRing = st.focused;
    return v;
}

}  // namespace nimvlets::productui
