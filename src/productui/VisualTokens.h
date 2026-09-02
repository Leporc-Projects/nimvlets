#pragma once

#include "productui/UiColor.h"

namespace nimvlets::productui {

// ============================================================
//  Nimvlets Product UI — tokens visuales SEMÁNTICOS (Block 12A, DEC-142)
// ============================================================
//
// La dirección visual base (block brief 06 §2/§3) sigue siendo la misma
// y NO se reemplaza: blanco hueso cálido, casi-negro (no negro puro),
// bordes discretos, sin gradientes, sin glassmorphism, sin sombras
// grandes. Block 12A la EVOLUCIONA con una capa chica de tokens con
// INTENCIÓN NOMBRADA — "Surface", "Border", "TextSecondary", … — para
// que las vistas pidan un rol y no un color suelto, y para que un
// futuro bloque de mundo (Collection / Shop escénicos) tenga de dónde
// tirar.
//
// Reglas:
//   - Pocos tokens, todos semánticos. No es un design system de cientos
//     de variables.
//   - El oro (`kOrnamentNeutral`) es un acento de ORNAMENTO — nunca una
//     segunda moneda, nunca domina un control.
//   - El acento POR PET vive en productui::PetAccent, no acá: estos
//     tokens no cambian por pet.
//
// PURO, sin SDL — vive en nimvlets_productui_core (igual que PetAccent)
// para que la capa de layout y los tests lo consuman. UiTheme.h lo
// reexporta junto con la escala tipográfica y los alias `theme::` de
// Block 06.
namespace tokens {

// --- Planos / superficies ------------------------------------------
constexpr UiColor kCanvas{0xF6, 0xF3, 0xEE, 0xFF};        // pergamino cálido — el fondo de la app
constexpr UiColor kSurface{0xF6, 0xF3, 0xEE, 0xFF};       // superficie por defecto (== canvas)
constexpr UiColor kSurfaceRaised{0xFB, 0xF9, 0xF4, 0xFF}; // un susurro más claro — panel/card que "levanta"
constexpr UiColor kSurfaceSoft{0xF0, 0xEB, 0xE1, 0xFF};   // neutro cálido un poco más profundo — plano de la gallery / rail

// --- Líneas ------------------------------------------------------
constexpr UiColor kBorder{0xE4, 0xDE, 0xD3, 0xFF};        // hairline cálido suave (borde exterior / divisor)
constexpr UiColor kBorderInner{0xFF, 0xFD, 0xF8, 0xF2};   // highlight interior casi-blanco de un panel enmarcado

// --- Texto -----------------------------------------------------
constexpr UiColor kTextPrimary{0x26, 0x22, 0x1E, 0xFF};   // casi-negro cálido (no #000)
constexpr UiColor kTextSecondary{0x6E, 0x68, 0x5C, 0xFF}; // taupe apagado — ~5.0:1 sobre canvas (WCAG AA)
constexpr UiColor kTextMuted{0x8A, 0x81, 0x72, 0xFF};     // etiqueta tenue — ~3.5:1 sobre canvas

// --- Ornamento / interacción --------------------------------------
constexpr UiColor kOrnamentNeutral{0xAE, 0x94, 0x69, 0xFF}; // oro viejo MUY restringido — spark del wordmark, marca de nav, divisores
constexpr UiColor kFocus{0x26, 0x22, 0x1E, 0xFF};           // anillo de foco — oscuro, independiente del pet (forma + contraste, nunca solo tono)
constexpr UiColor kHoverWash{0xEF, 0xEA, 0xE1, 0xFF};       // wash sutil al pasar el mouse

// --- Cápsula del wallet (pill discreto — NO estilo de moneda premium) --
constexpr UiColor kWalletSurface{0xF1, 0xEC, 0xE1, 0xFF};   // relleno cálido: la pill se lee como un objeto del mundo
constexpr UiColor kWalletBorder{0xE1, 0xD9, 0xC9, 0xFF};    // borde cálido fino

}  // namespace tokens

}  // namespace nimvlets::productui
