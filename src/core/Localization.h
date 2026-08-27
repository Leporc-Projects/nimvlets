#pragma once

#include <string_view>

namespace nimvlets::core {

// Capa de localización de producto de Block 06.1 — deliberadamente
// chica: dos idiomas, un catálogo de claves semánticas, sin plurales
// complejos ni interpolación tipo ICU (brief §6). La UI y el menú piden
// core::Localized(clave, idioma); nunca contienen copy inglés
// hard-codeado.
//
// Pura (sin SDL): vive en src/core para que tanto
// platform::BuildQuickMenuModel (nimvlets_platform_policy) como el
// layout de la Collection (nimvlets_productui_core) la consuman, y para
// que los tests corran en cualquier host.

enum class Language {
    kEn,
    kEs,
};

// Id persistido y estable: "en" / "es". NUNCA se traduce (AGENTS.md
// §17) — es lo que va a persistence::AppState::language.
const char* LanguageId(Language lang);

// Parsea el id persistido. Desconocido / vacío -> kEn (el fallback del
// brief §5: "Otherwise default to English").
Language ParseLanguage(std::string_view id);

// El nombre de cada idioma, SIEMPRE mostrado en su propio idioma
// (convención estándar de selectores de idioma): "English" / "Español".
// No depende del idioma activo.
const char* LanguageEndonym(Language lang);

// Claves semánticas de todo el texto de interfaz traducible. Los
// NOMBRES PROPIOS de Nimvlets (Bunny, Nidir, Frin, ...) y los términos
// de marca ("Nimvlets"/"Nimvlet") NO están acá: no se traducen
// (brief §4).
enum class StringKey {
    // Collection
    kCollection,        // "Collection" / "Colección"
    kYourCompanions,    // "Your companions" / "Tus compañeros"
    kOnDesktop,         // "On desktop" / "En el escritorio"
    kUse,              // "Use" (sub-línea del gallery) / "Usar"
    kUsePetPrefix,      // "Use " / "Usar " — se concatena con un nombre propio SIN traducir
    kNotInCollection,   // "Not in your collection" / "No está en tu colección"
    kMale,             // "Male" / "Macho"
    kFemale,           // "Female" / "Hembra"
    kClickSingular,    // "click" / "clic"
    kClickPlural,      // "clicks" / "clics"

    // Menú rápido
    kShowNimvlet,      // "Show Nimvlet" / "Mostrar Nimvlet"
    kHideNimvlet,      // "Hide Nimvlet" / "Ocultar Nimvlet"
    kCollectionMenuItem,  // "Collection…" / "Colección…"
    kSize,             // "Size" / "Tamaño"
    kSizeSmall,        // "Small" / "Pequeño"
    kSizeMedium,       // "Medium" / "Mediano"
    kSizeLarge,        // "Large" / "Grande"
    kOpacity,          // "Opacity" / "Opacidad"
    kLockPosition,     // "Lock Position" / "Bloquear posición"
    kLanguage,         // "Language" / "Idioma"
    kQuitNimvlets,     // "Quit Nimvlets" / "Salir de Nimvlets"

    kCount,  // centinela — no es una clave real
};

// El string localizado para `key` en `lang`. Nunca null. Determinista.
const char* Localized(StringKey key, Language lang);

}  // namespace nimvlets::core
