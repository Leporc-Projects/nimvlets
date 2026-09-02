#pragma once

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
//  Escala tipográfica + ROLES semánticos (Block 12A)
// ============================================================
//
// Tamaños en PUNTOS lógicos. Jerarquía por tamaño / peso / espacio /
// composición, NO por más contenedores ni por una tipografía empacada:
// el look serif editorial del mockup es INSPIRACIÓN para la jerarquía
// y el aire, no un mandato de traer una fuente externa (brief §6). Se
// sigue usando la rasterización nativa segura por plataforma
// (platform::RasterizeText).
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

// --- Roles semánticos (Block 12A) --------------------------------
// El Product UI pide un ROL, no un número mágico. Mapean sobre la
// escala de arriba — cambiar un rol acá lo cambia en todo su uso.
namespace role {

constexpr double kBrand = kTitle;          // 16 — el wordmark "✦ Nimvlets"
constexpr double kHeroTitle = kHeroName;   // 25 — nombre del personaje seleccionado
constexpr double kSectionTitle = 15.0;     // encabezados de sección / grupo
constexpr double kBody = kHeroBody;        // 13.5 — descripción editorial
constexpr double kMetadata = 13.0;         // especie, precio, estado
constexpr double kCaption = 11.5;          // sub-línea de gallery, hints, encabezados contextuales
constexpr double kButton = 13.0;           // etiqueta de CTA
constexpr double kWallet = kClicks;        // 13 — balance en la pill del wallet

}  // namespace role

}  // namespace type

}  // namespace nimvlets::productui
