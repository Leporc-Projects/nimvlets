#pragma once

#include "productui/UiColor.h"

namespace nimvlets::productui {

// Paleta base de Nimvlets Product UI — dirección visual ya decidida
// (block brief 06 §2/§3): blanco cálido, casi-negro (no negro puro),
// bordes discretos, sin gradientes, sin glassmorphism. El acento por
// defecto es una terracota cálida; Block 06.1 agrega un acento SUTIL
// por-pet encima (ver productui::PetAccent) para foco/selección/forma
// del hero, nunca recoloreando toda la UI.
namespace theme {

constexpr UiColor kBackground{0xF6, 0xF3, 0xEE, 0xFF};   // blanco hueso cálido
constexpr UiColor kText{0x26, 0x22, 0x1E, 0xFF};         // casi-negro, no #000
// Secundario / estado. Subido de #8C8578 (contraste 3.3:1 sobre el
// fondo — el owner lo reportó demasiado pálido, Block 06.2 §10) a
// #6E685C = ~5.0:1, ahora pasa WCAG AA para texto normal, sin volverse
// casi-negro.
constexpr UiColor kTextMuted{0x6E, 0x68, 0x5C, 0xFF};
// Etiqueta de sección / separadores. Subido de #B2AA9C (2.1:1) a
// #8A8172 = ~3.5:1: sigue siendo un escalón claro por debajo de
// kTextMuted (jerarquía intacta), pero legible.
constexpr UiColor kTextFaint{0x8A, 0x81, 0x72, 0xFF};

constexpr UiColor kHairline{0xE4, 0xDE, 0xD3, 0xFF};     // bordes discretos
constexpr UiColor kHoverWash{0xEF, 0xEA, 0xE1, 0xFF};    // fondo sutil al pasar el mouse
constexpr UiColor kSelectedWash{0xEC, 0xE5, 0xD8, 0xFF}; // fondo del item seleccionado

constexpr UiColor kAccent{0xB4, 0x6E, 0x3C, 0xFF};       // terracota — foco / variante activa
constexpr UiColor kAccentSoft{0xEB, 0xDD, 0xCF, 0xFF};   // relleno del chip seleccionado

constexpr UiColor kButtonFill{0x2A, 0x25, 0x20, 0xFF};   // botón de acción primario
constexpr UiColor kButtonText{0xF6, 0xF3, 0xEE, 0xFF};
constexpr UiColor kButtonDisabledText{0xA9, 0xA1, 0x93, 0xFF};

// Caja tenue detrás del arte de un pet (NO una card fuerte — apenas se
// insinúa, block brief §8).
constexpr UiColor kArtBed{0xF0, 0xEC, 0xE3, 0xFF};

}  // namespace theme

// Tamaños tipográficos en PUNTOS lógicos. Block 06.1: jerarquía por
// tamaño/peso/espacio, no por más contenedores (brief §7/§17).
namespace type {

constexpr double kTitle = 16.0;        // "Nimvlets" (cabecera discreta)
constexpr double kClicks = 13.0;       // "1 248 clicks"
constexpr double kSectionTitle = 15.0; // "Collection"
constexpr double kSectionSub = 12.0;   // "Your companions"

constexpr double kHeroName = 25.0;     // el nombre del Nimvlet protagonista
constexpr double kHeroMeta = 13.0;     // especie / estado del hero
constexpr double kHeroVariant = 14.0;  // "Male · Female"

constexpr double kGalleryName = 13.5;
constexpr double kGalleryStatus = 11.5;

constexpr double kButton = 13.0;

}  // namespace type

}  // namespace nimvlets::productui
