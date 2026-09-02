#pragma once

#include <cstdint>
#include <string_view>

#include "core/ClickCounting.h"
#include "core/DisplayControls.h"
#include "core/Localization.h"

namespace nimvlets::core {

// Las preferencias de usuario persistidas, reunidas en un solo valor
// tipado y YA normalizado: las CUATRO de Block 06/07 (tamaño, opacidad,
// bloqueo de posición, idioma) más, desde Block 11A, el modo de conteo
// de clics. Es la moneda común entre el menú rápido nativo y la sección
// Settings del Product UI (Block 08): los dos mutan / muestran estos
// campos por la misma ruta canónica (ver docs/PRODUCT_UI.md §20) — con
// una excepción deliberada, `clickCounting`, que solo Settings expone.
// NO es un framework de settings: no hay campos especulativos, y cada
// uno tiene su conjunto cerrado.
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
    // El modo PEDIDO (Block 11A). A diferencia de las otras cuatro, esta
    // preferencia NO aparece en el menú rápido: es la primera que vive
    // solo en Settings (brief §10 — el menú rápido se queda chico a
    // propósito). El camino canónico de mutación es el mismo
    // (SpikeApp::Apply*, DEC-130).
    ClickCountingMode clickCounting = ClickCountingMode::kNimvletOnly;

    friend bool operator==(const Preferences& a, const Preferences& b) {
        return a.size == b.size && a.opacityPercent == b.opacityPercent &&
               a.lockPosition == b.lockPosition && a.language == b.language &&
               a.clickCounting == b.clickCounting;
    }
};

// Cuál preferencia cambia una petición (venga del menú rápido o de
// Settings). Deliberadamente el mismo conjunto que los campos de
// Preferences. `kClickCounting` solo lo emite Settings.
enum class PreferenceField {
    kSize,
    kOpacity,
    kLockPosition,
    kLanguage,
    kClickCounting,
};

// Reconstruye Preferences desde los campos CRUDOS de persistence::AppState
// (sizeChoice string, opacityPercent uint con 0 = "sin preferencia",
// lockPosition bool, language string). Aplica las mismas normalizaciones
// que el resto del runtime: ParsePetSizeChoice (desconocido -> medium),
// opacidad 0 -> 100 y cualquier otro valor ajustado a la opción válida
// más cercana (NormalizeOpacityPercent), ParseLanguage (desconocido ->
// en), ParseClickCountingMode (vacío/desconocido -> kNimvletOnly, el
// default privado). Un archivo de estado editado a mano con valores
// imposibles nunca produce un Preferences imposible — y en particular
// nunca habilita el conteo global por accidente de parseo.
Preferences PreferencesFromStored(
    std::string_view sizeChoiceId, std::uint32_t opacityPercent, bool lockPosition,
    std::string_view languageId, std::string_view clickCountingModeId = std::string_view());

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
