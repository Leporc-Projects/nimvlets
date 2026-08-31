#pragma once

#include <cstdint>
#include <string_view>

#include "core/DisplayControls.h"
#include "core/Localization.h"

namespace nimvlets::core {

// Las CUATRO preferencias de usuario que Block 06/07 ya persisten
// (tamaño, opacidad, bloqueo de posición, idioma), reunidas en un solo
// valor tipado y YA normalizado. Es la moneda común entre el menú
// rápido nativo y la sección Settings del Product UI (Block 08): ambos
// mutan / muestran EXACTAMENTE estos campos, por la misma ruta canónica
// (ver docs/PRODUCT_UI.md §20). NO es un framework de settings — no hay
// campos especulativos, y cada uno tiene su conjunto cerrado.
//
// Pura (sin SDL): vive en src/core para que la construyan por igual
// src/app, el layout de Settings (nimvlets_productui_core) y los tests,
// en cualquier host.
struct Preferences {
    PetSizeChoice size = PetSizeChoice::kMedium;
    // SIEMPRE una de core::kOpacityChoicesPercent (100/85/70/55). El
    // "0 = sin preferencia" de AppState se resuelve a 100 al construir.
    int opacityPercent = 100;
    bool lockPosition = false;
    Language language = Language::kEn;

    friend bool operator==(const Preferences& a, const Preferences& b) {
        return a.size == b.size && a.opacityPercent == b.opacityPercent &&
               a.lockPosition == b.lockPosition && a.language == b.language;
    }
};

// Cuál de las cuatro preferencias cambia una petición (venga del menú
// rápido o de Settings). Deliberadamente el mismo conjunto que los
// campos de Preferences.
enum class PreferenceField {
    kSize,
    kOpacity,
    kLockPosition,
    kLanguage,
};

// Reconstruye Preferences desde los campos CRUDOS de persistence::AppState
// (sizeChoice string, opacityPercent uint con 0 = "sin preferencia",
// lockPosition bool, language string). Aplica las mismas normalizaciones
// que el resto del runtime: ParsePetSizeChoice (desconocido -> medium),
// opacidad 0 -> 100 y cualquier otro valor ajustado a la opción válida
// más cercana (NormalizeOpacityPercent), ParseLanguage (desconocido ->
// en). Un archivo de estado editado a mano con valores imposibles nunca
// produce un Preferences imposible.
Preferences PreferencesFromStored(
    std::string_view sizeChoiceId, std::uint32_t opacityPercent, bool lockPosition,
    std::string_view languageId);

// --- Ciclado de opciones para un control segmentado -----------------
//
// `dir` es -1 (opción anterior / izquierda) o +1 (siguiente / derecha).
// Segmentos en el orden en que la UI los dibuja:
//   size:     Small · Medium · Large
//   opacity:  100% · 85% · 70% · 55%   (= core::kOpacityChoicesPercent)
//   language: English · Español
// `clamp == true`  -> se detiene en los extremos (flechas ← →).
// `clamp == false` -> envuelve (Enter / Espacio = "siguiente, cíclico").
// El lock es booleano: se alterna, sin dirección.

PetSizeChoice StepSize(PetSizeChoice current, int dir, bool clamp);
int StepOpacityPercent(int currentPercent, int dir, bool clamp);
Language OtherLanguage(Language current);

}  // namespace nimvlets::core
