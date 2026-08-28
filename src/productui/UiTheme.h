#pragma once

#include "productui/UiColor.h"

namespace nimvlets::productui {

// Paleta base de Nimvlets Product UI — dirección visual ya decidida
// (block brief 06 §2/§3): blanco cálido, casi-negro (no negro puro),
// bordes discretos, sin gradientes, sin glassmorphism. Block 06.1 agrega
// un acento SUTIL por-pet (productui::PetAccent). Block 06.2:
//   - contraste del texto secundario subido (§10),
//   - la Collection se lee como DOS planos: hero stage sobre el fondo
//     cálido base, gallery sobre un neutro un pelín más profundo (§12),
//   - el botón de acción primario ya NO es casi-negro: usa el softFill/
//     deepInk del PetAccent del pet seleccionado (§17).
namespace theme {

constexpr UiColor kBackground{0xF6, 0xF3, 0xEE, 0xFF};   // blanco hueso cálido — plano del hero
constexpr UiColor kText{0x26, 0x22, 0x1E, 0xFF};         // casi-negro, no #000

// Secundario / estado. Subido de #8C8578 (contraste 3.3:1 sobre el
// fondo — el owner lo reportó demasiado pálido, Block 06.2 §10) a
// #6E685C = ~5.0:1, ahora pasa WCAG AA para texto normal, sin volverse
// casi-negro.
constexpr UiColor kTextMuted{0x6E, 0x68, 0x5C, 0xFF};
// Etiqueta de sección / separadores / "·". Subido de #B2AA9C (2.1:1) a
// #8A8172 = ~3.5:1: sigue siendo un escalón claro por debajo de
// kTextMuted (jerarquía intacta), pero legible.
constexpr UiColor kTextFaint{0x8A, 0x81, 0x72, 0xFF};

constexpr UiColor kHairline{0xE4, 0xDE, 0xD3, 0xFF};     // bordes discretos / divisor
constexpr UiColor kHoverWash{0xEF, 0xEA, 0xE1, 0xFF};    // fondo sutil al pasar el mouse

// Segundo plano: la zona de la gallery, un neutro cálido un poco más
// profundo que kBackground — da profundidad sin convertir cada pet en
// una card (Block 06.2 §12).
constexpr UiColor kGalleryShelf{0xF0, 0xEB, 0xE1, 0xFF};

}  // namespace theme

// Tamaños tipográficos en PUNTOS lógicos. Jerarquía por tamaño/peso/
// espacio, no por más contenedores (brief §7/§17).
namespace type {

constexpr double kTitle = 16.0;        // "Nimvlets" (cabecera discreta)
constexpr double kClicks = 13.0;       // "1 248 clicks"
constexpr double kSectionTitle = 15.0; // "Collection"
constexpr double kSectionSub = 12.0;   // "Your companions"

constexpr double kHeroName = 25.0;     // el nombre del Nimvlet protagonista
constexpr double kHeroSpecies = 13.0;  // etiqueta de especie
constexpr double kHeroBody = 13.5;     // la línea de descripción (Block 06.2 §13)
constexpr double kHeroStatus = 13.0;   // "On desktop" / "Not in your collection"
constexpr double kHeroVariant = 14.0;  // "Male · Female"

constexpr double kGalleryName = 13.5;
constexpr double kGalleryStatus = 11.5;

constexpr double kButton = 13.0;

}  // namespace type

}  // namespace nimvlets::productui
