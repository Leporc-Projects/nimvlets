#include "productui/ButtonStyle.h"

#include "productui/Contrast.h"
#include "productui/VisualTokens.h"

namespace nimvlets::productui {

namespace {

// AA para texto normal. El acento por pet de Bunny/Nidir/Frin ya lo
// cumple con margen (PetAccentTest); esto es el guardrail para un pet
// futuro con un acento claro y para el fallback neutro.
constexpr double kMinLabelContrast = 4.5;

}  // namespace

ButtonVisual ResolveButtonVisual(ButtonRole role, const PetAccent* accent, ButtonStateFlags st) {
    // Fallback neutro cuando no hay contexto de pet (Cancel, avisos de
    // Settings, …) — PetAccentFor("") devuelve el neutro cálido con
    // todos los campos poblados.
    const PetAccent acc = accent != nullptr ? *accent : PetAccentFor("");

    ButtonVisual v;
    switch (role) {
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
        v.fill = role == ButtonRole::kPrimary ? tokens::kSurfaceSoft : UiColor{0, 0, 0, 0};
        v.border = role == ButtonRole::kQuiet ? UiColor{0, 0, 0, 0} : tokens::kBorder;
        v.ink = tokens::kTextMuted;
        v.drawFocusRing = false;
        return v;
    }

    if (st.pressed) {
        v.fill = Darken(role == ButtonRole::kPrimary ? acc.softFill : tokens::kHoverWash, 0.12);
    } else if (st.hovered) {
        v.fill = role == ButtonRole::kPrimary ? Darken(acc.softFill, 0.05) : tokens::kHoverWash;
    }

    v.drawFocusRing = st.focused;
    return v;
}

}  // namespace nimvlets::productui
