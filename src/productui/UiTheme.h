#pragma once

#include "platform/TextRasterizer.h"
#include "productui/UiColor.h"
#include "productui/VisualTokens.h"

namespace nimvlets::productui {

// Los tokens semánticos (productui::tokens) viven en VisualTokens.h
// (puro, nimvlets_productui_core). Este header los reexporta junto con
// la escala tipográfica y los ALIAS de Block 06.

// ------------------------------------------------------------
//  Alias de compatibilidad: los nombres de Block 06, ahora definidos
//  en términos de los tokens semánticos. Las vistas existentes
//  compilan sin tocar nada; el código nuevo o modificado prefiere
//  `tokens::` por claridad de intención.
// ------------------------------------------------------------
namespace theme {

constexpr UiColor kBackground = tokens::kCanvas;
constexpr UiColor kText = tokens::kTextPrimary;
constexpr UiColor kTextMuted = tokens::kTextSecondary;
constexpr UiColor kTextFaint = tokens::kTextMuted;
constexpr UiColor kHairline = tokens::kBorder;
constexpr UiColor kHoverWash = tokens::kHoverWash;
constexpr UiColor kGalleryShelf = tokens::kSurfaceSoft;

}  // namespace theme

// ============================================================
//  Escala tipográfica + ROLES DE FUENTE tokenizados
//  (Block 12A + refinement DEC-146)
// ============================================================
//
// Tamaños en PUNTOS lógicos. Jerarquía por familia / tamaño / peso /
// interletraje / composición — NO por más contenedores ni por una
// tipografía DECORATIVA empacada. El refinamiento owner-QA le da a los
// roles de DISPLAY (marca, título de hero, rótulos de sección y de
// nav) el *diseño serif del sistema* (New York en macOS, resuelto por
// NSFontDescriptor — SIN asset), mientras que cuerpo / metadata /
// caption / botón / wallet siguen en la SANS del sistema (SF Pro):
// legibilidad primero. Ver DEC-146.
namespace type {

// --- Escala cruda (sans, natural) — se conserva para las llamadas que
//     pasan (tamaño, peso) por separado. ---
constexpr double kTitle = 16.0;
constexpr double kClicks = 13.0;
constexpr double kSectionTitle = 15.0;
constexpr double kSectionSub = 12.0;

constexpr double kHeroName = 25.0;
constexpr double kHeroSpecies = 13.0;
constexpr double kHeroBody = 13.5;     // la línea de descripción (Block 06.2 §13)
constexpr double kHeroStatus = 13.0;
constexpr double kHeroVariant = 14.0;

constexpr double kGalleryName = 13.5;
constexpr double kGalleryStatus = 11.5;

constexpr double kButton = 13.0;

// --- Rol de fuente tokenizado -----------------------------------
// El Product UI pide un ROL, no un número mágico ni una familia suelta.
// Cambiar un rol acá lo cambia en todo su uso.
struct FontRole {
    double size = 13.0;
    platform::TextWeight weight = platform::TextWeight::kRegular;
    platform::TextFamily family = platform::TextFamily::kSans;
    // Interletraje extra en PUNTOS lógicos (0 = natural). Un toque chico
    // sobre los rótulos serif chicos.
    double tracking = 0.0;

    constexpr FontRole WithWeight(platform::TextWeight w) const {
        return FontRole{size, w, family, tracking};
    }
    constexpr FontRole WithSize(double s) const { return FontRole{s, weight, family, tracking}; }
};

namespace role {

// DISPLAY / editorial -> SERIF del sistema.
constexpr FontRole kBrand{17.0, platform::TextWeight::kSemibold, platform::TextFamily::kSerif, 0.2};
constexpr FontRole kHeroTitle{26.0, platform::TextWeight::kSemibold, platform::TextFamily::kSerif, 0.0};
// Rótulos de sección + pestañas de navegación (el peso lo fija el
// caller con WithWeight: semibold activa, regular inactiva).
constexpr FontRole kSectionLabel{15.0, platform::TextWeight::kMedium, platform::TextFamily::kSerif, 0.3};
// Títulos de grupo de Settings ("Companion" / "Language" / …) — chico,
// apagado, un pelín más de interletraje para leerse como rótulo.
constexpr FontRole kGroupTitle{12.0, platform::TextWeight::kSemibold, platform::TextFamily::kSerif, 0.6};
// Nombres de personaje en las cards de browse / gallery (convergencia
// DEC-147): serif chico, para que las cards se lean editoriales sin
// perder legibilidad. El precio / estado de la card sigue en sans.
constexpr FontRole kCardName{13.5, platform::TextWeight::kMedium, platform::TextFamily::kSerif, 0.0};

// TEXTO -> SANS del sistema, sin cambios: legibilidad primero.
constexpr FontRole kBody{13.5, platform::TextWeight::kRegular, platform::TextFamily::kSans, 0.0};
constexpr FontRole kMetadata{13.0, platform::TextWeight::kRegular, platform::TextFamily::kSans, 0.0};
constexpr FontRole kCaption{11.5, platform::TextWeight::kRegular, platform::TextFamily::kSans, 0.0};
constexpr FontRole kButton{13.0, platform::TextWeight::kSemibold, platform::TextFamily::kSans, 0.2};
constexpr FontRole kWallet{13.0, platform::TextWeight::kMedium, platform::TextFamily::kSans, 0.0};
// Precio del hero del Shop (referencia concepto: "✦ 300 clicks" más
// confiado que el cuerpo). Sans numérico, como el wallet.
constexpr FontRole kPrice{15.0, platform::TextWeight::kSemibold, platform::TextFamily::kSans, 0.0};

}  // namespace role

}  // namespace type

}  // namespace nimvlets::productui
