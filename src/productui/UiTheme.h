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
constexpr UiColor kTextMuted{0x8C, 0x85, 0x78, 0xFF};    // texto secundario / estado
constexpr UiColor kTextFaint{0xB2, 0xAA, 0x9C, 0xFF};    // etiqueta de sección, locked

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

// Tamaños tipográficos en PUNTOS lógicos.
namespace type {

constexpr double kTitle = 17.0;       // "Nimvlets"
constexpr double kClicks = 13.0;      // "1 248 clicks"
constexpr double kSectionLabel = 11.5;  // "COLLECTION"
constexpr double kPetName = 14.0;
constexpr double kStatus = 12.0;
constexpr double kDetailName = 20.0;
constexpr double kChip = 12.5;
constexpr double kButton = 13.0;

}  // namespace type

}  // namespace nimvlets::productui
