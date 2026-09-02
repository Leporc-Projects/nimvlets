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
    // Línea quieta bajo el hero cuando el owner solo tiene un Nimvlet
    // (la gallery quedaría vacía) — Block 09C / DEC-136. Apunta al Shop
    // sin ser un CTA. "Nimvlets" / "Shop" siguen sus reglas de traducción.
    kCollectionOnlyActive,  // "Meet more Nimvlets in the Shop." / "Conoce más Nimvlets en la Tienda."
    kOnDesktop,         // "On desktop" / "En el escritorio"
    kUse,              // "Use" (sub-línea del gallery) / "Usar"
    kUsePetPrefix,      // "Use " / "Usar " — se concatena con un nombre propio SIN traducir
    kNotInCollection,   // "Not in your collection" / "No está en tu colección"
    kMale,             // "Male" / "Macho"
    kFemale,           // "Female" / "Hembra"
    kClickSingular,    // "click" / "clic"
    kClickPlural,      // "clicks" / "clics"

    // Shop + wallet (Block 07). Los nombres propios de pet NO se
    // traducen; "Shop" SÍ. Ver docs/PRODUCT_UI.md §7-§12.
    kShop,                 // "Shop" / "Tienda"
    kGetPetPrefix,         // "Get " / "Obtener " — se concatena con un nombre propio
    kInYourCollection,     // "In your collection" / "En tu colección"
    kNeedMoreClicksOne,    // "Need 1 more click" / "Te falta 1 clic"
    kNeedMoreClicksMany,   // "Need {n} more clicks" / "Te faltan {n} clics" ({n} lo sustituye Format)
    kCancel,               // "Cancel" / "Cancelar"
    kConfirm,              // "Confirm" / "Confirmar"
    kSpendPromptOne,       // "Spend 1 click to add {pet} to your collection?" / "¿Gastar 1 clic para añadir {pet} a tu colección?"
    kSpendPromptMany,      // "Spend {n} clicks to add {pet} to your collection?" / "¿Gastar {n} clics para añadir {pet} a tu colección?"
    // Shop browse-first (Block 09C). El Shop abre en modo BROWSE: una
    // estantería de personajes que se puede conocer; solo tras
    // seleccionar uno aparece el hero grande. Ver docs/PRODUCT_UI.md §16
    // y DEC-135. "Nimvlets" nunca se traduce.
    kShopBrowseHeading,    // "Nimvlets you can meet" / "Nimvlets que puedes conocer"
    kShopEmpty,            // "No Nimvlets to show yet." / "Todavía no hay Nimvlets para mostrar."

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

    // Settings — sección del Product UI (Block 08). Mismas preferencias
    // que el menú rápido; kSize..kLanguage de arriba se reusan tal cual.
    // Los nombres de idioma del selector son endónimos (core::
    // LanguageEndonym), no una clave nueva. Ver docs/PRODUCT_UI.md §20.
    kSettings,         // "Settings" / "Ajustes"
    kSettingsCompanion,  // "Companion" / "Compañero" — encabezado de grupo
    kLockPositionHint,   // frase corta bajo el control de lock
    kOn,               // "On" / "Activado"
    kOff,              // "Off" / "Desactivado"

    // Onboarding de primer arranque (Block 09A). Los nombres propios
    // (Nimvlet, Artu, Rato, Rin Rin, Frin) NUNCA se traducen; "Male"/
    // "Female" reusan kMale/kFemale; "Cancel" reusa kCancel. Ver
    // docs/ONBOARDING.md.
    kOnboardingChooseFirst,      // "Choose your first Nimvlet" / "Elige tu primer Nimvlet"
    kOnboardingConfirmStarter,   // "Make {pet} your first Nimvlet?" / "¿Quieres que {pet} sea tu primer Nimvlet?"
    kOnboardingConfirmChoosePrefix,  // "Choose " / "Elegir a " — se concatena con un nombre propio
    kOnboardingWhichVariant,     // "Which Frin?" / "¿Qué Frin?"

    // Shop oculto de starters (Block 10). Submodo contextual de la
    // sección Shop — NO una cuarta pestaña de navegación. El acceso desde
    // el Shop público es un HOTSPOT INVISIBLE (corrección de QA del
    // owner: no hay copy visible que revele la feature). Los nombres
    // propios (Frin) nunca se traducen; "Male"/"Female" reusan kMale/
    // kFemale. Ver docs/ONBOARDING.md §15 y DEC-137.
    kStarterChoicesHeading,  // "Starter choices" / "Opciones iniciales" — encabezado DENTRO del submodo
    kStarterShopBack,        // "← Shop" / "← Tienda" — volver al Shop público
    kStarterShopEmpty,       // "No more starter choices." / "No quedan opciones iniciales."

    // Interaction — modo OPT-IN de conteo de clics (Block 11A). Vive
    // SOLO en Settings; el menú rápido no lo gana (brief §10). "Nimvlet"
    // no se traduce. El nombre del permiso del OS ("Input Monitoring")
    // NO es una clave: lo aporta el adapter de plataforma vía
    // GlobalClickStatus::permissionName y se sustituye en {permission},
    // así el Product UI no tiene ninguna rama por plataforma (brief §18).
    // Ver docs/GLOBAL_CLICK_MODE.md.
    kSettingsInteraction,       // "Interaction" / "Interacción" — encabezado de grupo
    kClickCounting,             // "Click counting" / "Conteo de clics"
    kClickCountingNimvletOnly,  // "Nimvlet only" / "Solo el Nimvlet"
    kClickCountingAnywhere,     // "Anywhere" / "En cualquier lugar"
    kClickCountingHint,         // frase corta bajo la fila
    // La explicación de PRIMERA PARTE que se muestra ANTES de pedir el
    // permiso (brief §8). Nombra qué se cuenta y, sobre todo, qué NO se
    // observa nunca. {permission} = el nombre del permiso del OS.
    kGlobalClickExplain,
    kGlobalClickContinue,       // "Continue" / "Continuar"
    kGlobalClickNotNow,         // "Not now" / "Ahora no"
    kGlobalClickCheckAgain,     // "Check again" / "Comprobar de nuevo"
    kGlobalClickActive,         // "Active" / "Activo"
    kGlobalClickPermissionNeeded,  // "{permission} permission needed" / "Falta el permiso {permission}"
    kGlobalClickUnavailable,    // "Not available on this system" / "No disponible en este sistema"
    kGlobalClickFailed,         // "Could not start" / "No se pudo iniciar"
    // Qué hacer tras un pedido denegado/pendiente. Se nombra el lugar
    // del OS sin usar ningún deep link no documentado (brief §9).
    kGlobalClickGrantHint,
    // Semántica de drag, dicha en voz alta en Settings (brief §21): en
    // modo global una presión primaria cuenta una vez, aunque se
    // convierta en arrastre.
    kGlobalClickDragNote,

    kCount,  // centinela — no es una clave real
};

// El string localizado para `key` en `lang`. Nunca null. Determinista.
const char* Localized(StringKey key, Language lang);

}  // namespace nimvlets::core
