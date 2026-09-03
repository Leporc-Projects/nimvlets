#pragma once

#include "productui/PetAccent.h"
#include "productui/UiColor.h"

namespace nimvlets::productui {

// Estilos de botón SEMÁNTICOS (Block 12A — DEC-144, ampliado en la
// convergencia DEC-147). El resolvedor es PURO, así los tests fijan las
// decisiones de color / contraste. Cuatro roles, todos con uso real:
enum class ButtonRole {
    kPrimaryCta,  // la acción de personaje/economía más fuerte (Get <pet>, Use <pet>): pill
                  // generosa, relleno de acento del pet, filo de oro tenue, spark a la
                  // derecha (opcional), 2-tono sutil. La "gran" CTA de la referencia.
    kPrimary,     // acción fuerte pero contenida (Confirm): relleno de acento + borde, sin
                  // pill/spark/filo — no debe competir con la CTA principal (brief §11)
    kSecondary,   // Cancelar / neutral — contorno hairline discreto
    kQuiet,       // baja énfasis (descartar / volver) — sin borde, tinta apagada, wash al hover
};

struct ButtonStateFlags {
    bool hovered = false;
    bool pressed = false;
    bool focused = false;
    bool disabled = false;
};

struct ButtonVisual {
    UiColor fill{0, 0, 0, 0};    // a == 0 -> sin relleno
    UiColor border{0, 0, 0, 0};  // a == 0 -> sin borde
    UiColor ink;                 // color de la etiqueta
    float borderWidth = 1.5f;
    bool drawFocusRing = false;  // el caller dibuja un anillo desplazado en tokens::kFocus

    // --- Detalles de la CTA (convergencia DEC-147) — a==0 / false los apaga ---
    // Por defecto TRANSPARENTES: un Primary / Secondary / Quiet normal
    // no dibuja ninguno de estos.
    UiColor topHighlight{0, 0, 0, 0};  // hairline interior superior 1pt (sensación "levantada")
    UiColor bottomShade{0, 0, 0, 0};   // hairline interior inferior 1pt
    UiColor edgeAccent{0, 0, 0, 0};    // hairline exterior tenue (filo de oro cálido)
    bool pill = false;                 // radio = alto/2 en vez de 9pt
    bool sparkle = false;              // spark procedural chico a la derecha, en `ink`
};

// `accent` puede ser null (sin contexto de pet) — usa entonces el
// fallback neutro. Si el acento del pet no le da a la etiqueta un
// contraste legible sobre su relleno, el resolvedor CLAMPA la tinta
// (nunca devuelve un par ilegible). Determinista.
ButtonVisual ResolveButtonVisual(ButtonRole role, const PetAccent* accent, ButtonStateFlags st);

}  // namespace nimvlets::productui
