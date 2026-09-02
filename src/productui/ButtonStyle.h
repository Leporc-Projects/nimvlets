#pragma once

#include "productui/PetAccent.h"
#include "productui/UiColor.h"

namespace nimvlets::productui {

// Estilos de botón SEMÁNTICOS (Block 12A — DEC-144). Unifican el
// dibujo de botones ad-hoc de cada vista en tres roles nombrados. El
// resolvedor es PURO, así los tests fijan las decisiones de color /
// contraste. Solo tres roles — todos con uso real hoy (brief §16: no
// crear estilos sin uso).
enum class ButtonRole {
    kPrimary,    // la acción más fuerte; consume el acento del pet cuando hay contexto de pet
    kSecondary,  // Cancelar / neutral — contorno hairline discreto
    kQuiet,      // baja énfasis (descartar / volver) — sin borde, tinta apagada, solo wash al hover
};

struct ButtonStateFlags {
    bool hovered = false;
    bool pressed = false;
    bool focused = false;
    bool disabled = false;
};

struct ButtonVisual {
    UiColor fill;                // a == 0 -> sin relleno
    UiColor border;              // a == 0 -> sin borde
    UiColor ink;                 // color de la etiqueta
    float borderWidth = 1.5f;
    bool drawFocusRing = false;  // el caller dibuja un anillo desplazado en tokens::kFocus
};

// `accent` puede ser null (sin contexto de pet) — Primary usa entonces
// el fallback neutro. Si el acento del pet no le da a la etiqueta de
// Primary un contraste legible sobre su relleno, el resolvedor CLAMPA
// la tinta más oscura (nunca devuelve un par ilegible). Determinista.
ButtonVisual ResolveButtonVisual(ButtonRole role, const PetAccent* accent, ButtonStateFlags st);

}  // namespace nimvlets::productui
