#include "productui/ButtonStyle.h"

#include "productui/Contrast.h"
#include "productui/VisualTokens.h"

namespace nimvlets::productui {

namespace {

// AA para texto normal. El acento por pet de Bunny/Nidir/Frin ya lo
// cumple con margen (PetAccentTest); esto es el guardrail para un pet
// futuro con un acento claro y para el fallback neutro.
constexpr double kMinLabelContrast = 4.5;

// Relleno de la CTA según el ÉNFASIS centralizado del pet (DEC-148):
//   kSoft — un pelín más saturado que el softFill pálido del Primary
//           contenido: confiado pero claro, tinta oscura (Bunny/Frin/neutro).
//   kDeep — relleno de acento HONDO (el CTA violeta saturado del concept
//           para Nidir): line mezclada a mitad con deepInk, tinta cream.
// En los dos casos el color de la etiqueta pasa por BestForeground, así
// que un pet futuro sin perfil no puede dar un par ilegible (DEC-121).
UiColor CtaFill(const PetAccent& acc) {
    return acc.emphasis == AccentEmphasis::kDeep ? Mix(acc.line, acc.deepInk, 0.5)
                                                 : Mix(acc.softFill, acc.line, 0.35);
}

}  // namespace

ButtonVisual ResolveButtonVisual(ButtonRole role, const PetAccent* accent, ButtonStateFlags st) {
    // Fallback neutro cuando no hay contexto de pet (Cancel, avisos de
    // Settings, …) — PetAccentFor("") devuelve el neutro cálido con
    // todos los campos poblados.
    const PetAccent acc = accent != nullptr ? *accent : PetAccentFor("");

    ButtonVisual v;
    switch (role) {
        case ButtonRole::kPrimaryCta: {
            const bool deep = acc.emphasis == AccentEmphasis::kDeep;
            const UiColor fill = CtaFill(acc);
            v.fill = fill;
            // Contorno interior LIMPIO e intencional (brief §19 — nada de
            // anillo pálido accidental): sobre un relleno hondo el borde
            // es un tono MÁS OSCURO del mismo relleno (se lee como filo);
            // sobre un relleno claro es la `line` del pet (borde de acento).
            v.border = deep ? Darken(fill, 0.28) : acc.line;
            v.borderWidth = 1.5f;
            // Etiqueta: la mejor de {deepInk oscuro, cream claro} sobre el
            // relleno — kSoft gana deepInk (relleno claro), kDeep gana el
            // cream y lo aclara lo justo para llegar a AA.
            v.ink = BestForeground(fill, acc.deepInk, Lighten(tokens::kCanvas, 0.58), kMinLabelContrast);
            v.topHighlight = Lighten(fill, deep ? 0.20 : 0.35);
            v.bottomShade = Darken(fill, deep ? 0.14 : 0.10);
            v.edgeAccent = tokens::kOrnamentNeutral.WithAlpha(deep ? 175 : 150);
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
