#include "SettingsCompanionTest.h"

#include <optional>

#include "core/DisplayControls.h"
#include "persistence/AppState.h"

// Block 11B — los dos controles TRANSITORIOS de Companion, probados al
// nivel del MODELO de src/app (sin SDL), igual que
// tests/ProductWindowStateTest.cpp espeja SpikeApp::HandleCountedClick.
//
// Lo que se fija acá:
//   1. Visibility es RUNTIME, nunca AppState: ApplyPetVisibility no toca
//      appState_ ni el debounce de persistencia — el pet arranca visible
//      en cada lanzamiento (brief §4).
//   2. Settings y el menú rápido derivan la visibilidad de UN solo bool,
//      así que no pueden divergir (brief §5/§22).
//   3. "Reset position" escribe por la MISMA ruta que el fin de un drag
//      (appState_.lastWindowPosition + MarkDirty), al destino SEGURO de
//      core::SafePetPlacement, dentro de los límites del display objetivo
//      (brief §7/§11/§22).
//   4. Lock Position NO bloquea el reset explícito, y funciona con el pet
//      oculto (brief §7/§22).
//   5. En un backend sin capacidad de colocar toplevels (Wayland) el
//      reset es un no-op HONESTO: no escribe nada (brief §9).
//
// El movimiento real de la ventana nativa y el refresco de Settings los
// prueban el smoke DEV NIMVLETS_DEV_RESET_POSITION y la QA física del
// owner, no este archivo.

using nimvlets::core::DisplayBounds;
using nimvlets::core::SafePetPlacement;
using nimvlets::core::WindowTopLeft;
using nimvlets::persistence::WindowPosition;

namespace nimvlets::tests {

namespace {

// Espejo del estado de src/app que estos dos controles tocan, y de sus
// rutas canónicas (SpikeApp::ApplyPetVisibility /
// ResetPetPositionToSafeDefault).
struct CompanionModel {
    // --- Estado del programa ---
    bool petHidden = false;             // SpikeApp::petHidden_ (TRANSITORIO)
    bool lockPosition = false;          // appState_.lockPosition

    // --- Superficie PERSISTIDA que vigilamos ---
    std::optional<WindowPosition> lastWindowPosition;  // appState_.lastWindowPosition
    bool persistenceDirty = false;                     // persistenceScheduler_.MarkDirty()
    // Cualquier mutación de un campo de schema de AppState por causa de
    // la visibilidad incrementaría esto. DEBE quedar en 0: no hay campo
    // de visibilidad, y no hay bump de schema (brief §4).
    int appStateSchemaWrites = 0;

    // --- Capacidad de plataforma ---
    bool positionResetAvailable = true;  // platform::AbsoluteWindowPositioningSupported()

    // Lo que CADA superficie muestra — las dos derivan de `petHidden`.
    bool settingsShownSegmentSelected() const { return !petHidden; }
    bool quickMenuOffersHide() const { return !petHidden; }  // "Hide Nimvlet" con el pet visible

    // Ruta canónica: SpikeApp::ApplyPetVisibility. NO toca appState_ ni
    // el debounce.
    void ApplyPetVisibility(bool hidden) { petHidden = hidden; }

    // Ruta canónica: SpikeApp::ResetPetPositionToSafeDefault. Lock NO la
    // bloquea (Lock solo gatea el inicio de un DRAG). Devuelve false —
    // sin escribir nada — en un backend sin capacidad.
    bool ResetPetPosition(DisplayBounds display, int petW, int petH) {
        if (!positionResetAvailable) {
            return false;  // Wayland: no-op honesto
        }
        const WindowTopLeft p = SafePetPlacement(display, petW, petH);
        lastWindowPosition = WindowPosition{p.x, p.y};  // MISMA ruta que el fin de un drag
        persistenceDirty = true;                        // MISMO MarkDirty
        return true;
    }
};

bool Inside(const WindowPosition& p, DisplayBounds d, int w, int h) {
    const bool leftTopInside = p.x >= d.x && p.y >= d.y;
    const bool fitsX = w > d.w || p.x + w <= d.x + d.w;
    const bool fitsY = h > d.h || p.y + h <= d.y + d.h;
    return leftTopInside && fitsX && fitsY;
}

// --- 1. Visibility es transitoria ---------------------------------

bool TestVisibilityIsTransientNeverPersisted() {
    CompanionModel m;
    m.lastWindowPosition = WindowPosition{123, 456};  // algo persistido de antes

    m.ApplyPetVisibility(true);
    m.ApplyPetVisibility(false);
    m.ApplyPetVisibility(true);

    NIMVLETS_CHECK(m.petHidden);                       // el runtime cambió
    NIMVLETS_CHECK(!m.persistenceDirty);               // ...pero NADA se marcó para guardar
    NIMVLETS_CHECK(m.appStateSchemaWrites == 0);       // ...ni se tocó ningún campo de schema
    NIMVLETS_CHECK((m.lastWindowPosition == WindowPosition{123, 456}));  // intacto
    return true;
}

// Un "AppState" recién construido (== sin archivo == relanzar la app) no
// tiene ningún campo de visibilidad, y su default no es "oculto": el pet
// siempre arranca visible.
bool TestFreshAppStateHasNoVisibilityAndStartsVisible() {
    const persistence::AppState a;
    const persistence::AppState b;
    NIMVLETS_CHECK(a == b);  // dos estados por defecto son idénticos...
    // ...y el modelo de runtime arranca con el pet visible, sin leer nada
    // de AppState para eso.
    CompanionModel m;
    NIMVLETS_CHECK(!m.petHidden);
    return true;
}

// --- 2. Una sola fuente de verdad para las dos superficies --------

bool TestSettingsAndQuickMenuShareOneVisibilityBool() {
    CompanionModel m;

    // Pet visible: Settings marca "Shown", el menú ofrece "Hide".
    NIMVLETS_CHECK(m.settingsShownSegmentSelected());
    NIMVLETS_CHECK(m.quickMenuOffersHide());

    // Cambiar desde CUALQUIER superficie mueve el MISMO bool -> las dos
    // reflejan lo nuevo, sin un segundo estado que pueda quedar viejo.
    m.ApplyPetVisibility(true);  // p. ej. desde Settings
    NIMVLETS_CHECK(!m.settingsShownSegmentSelected());
    NIMVLETS_CHECK(!m.quickMenuOffersHide());  // ahora ofrece "Show"

    m.ApplyPetVisibility(false);  // p. ej. desde el menú rápido
    NIMVLETS_CHECK(m.settingsShownSegmentSelected());
    NIMVLETS_CHECK(m.quickMenuOffersHide());
    return true;
}

// --- 3. Reset position escribe por la ruta de siempre ------------

bool TestResetPositionWritesThroughTheDragEndPath() {
    CompanionModel m;
    NIMVLETS_CHECK(!m.lastWindowPosition.has_value());
    NIMVLETS_CHECK(!m.persistenceDirty);

    const DisplayBounds display{0, 0, 1920, 1080};
    NIMVLETS_CHECK(m.ResetPetPosition(display, 160, 160));

    // El MISMO campo que escribe el fin de un drag, con el destino
    // centrado-y-acotado, y el MISMO MarkDirty.
    NIMVLETS_CHECK(m.lastWindowPosition.has_value());
    NIMVLETS_CHECK(m.lastWindowPosition->x == (1920 - 160) / 2);
    NIMVLETS_CHECK(m.lastWindowPosition->y == (1080 - 160) / 2);
    NIMVLETS_CHECK(m.persistenceDirty);
    return true;
}

bool TestResetPositionTargetIsInsideTheTargetDisplay() {
    const DisplayBounds displays[] = {
        {0, 0, 1440, 900},
        {1440, 0, 2560, 1440},   // segundo monitor a la derecha
        {-1920, -120, 1920, 1080},
    };
    for (const DisplayBounds& d : displays) {
        CompanionModel m;
        NIMVLETS_CHECK(m.ResetPetPosition(d, 220, 260));
        NIMVLETS_CHECK(Inside(*m.lastWindowPosition, d, 220, 260));
    }
    return true;
}

// --- 4. Lock Position NO bloquea el reset; y funciona oculto -----

bool TestResetPositionIgnoresLockPosition() {
    CompanionModel m;
    m.lockPosition = true;  // el owner tiene la posición bloqueada

    const DisplayBounds display{100, 100, 1280, 720};
    NIMVLETS_CHECK(m.ResetPetPosition(display, 200, 200));  // igual funciona
    NIMVLETS_CHECK(m.lastWindowPosition.has_value());
    NIMVLETS_CHECK(m.persistenceDirty);
    // Lock sigue siendo lo que era — el reset no lo tocó.
    NIMVLETS_CHECK(m.lockPosition);
    return true;
}

bool TestResetPositionWorksWhileHidden() {
    CompanionModel m;
    m.ApplyPetVisibility(true);  // pet oculto

    const DisplayBounds display{0, 0, 1600, 1000};
    NIMVLETS_CHECK(m.ResetPetPosition(display, 300, 300));
    NIMVLETS_CHECK(m.lastWindowPosition->x == (1600 - 300) / 2);
    NIMVLETS_CHECK(m.lastWindowPosition->y == (1000 - 300) / 2);
    // Sigue oculto — mover una ventana no la muestra.
    NIMVLETS_CHECK(m.petHidden);
    return true;
}

// --- 5. Backend sin capacidad: no-op honesto --------------------

bool TestResetPositionIsHonestNoOpWhenUnavailable() {
    CompanionModel m;
    m.positionResetAvailable = false;  // Wayland

    const DisplayBounds display{0, 0, 1920, 1080};
    NIMVLETS_CHECK(!m.ResetPetPosition(display, 160, 160));

    // NADA se escribió: ni la posición, ni el debounce.
    NIMVLETS_CHECK(!m.lastWindowPosition.has_value());
    NIMVLETS_CHECK(!m.persistenceDirty);
    return true;
}

}  // namespace

void RegisterSettingsCompanionTests(testing::TestRunner& runner) {
    runner.Add("SettingsCompanion/VisibilityIsTransientNeverPersisted",
               TestVisibilityIsTransientNeverPersisted);
    runner.Add("SettingsCompanion/FreshAppStateHasNoVisibilityAndStartsVisible",
               TestFreshAppStateHasNoVisibilityAndStartsVisible);
    runner.Add("SettingsCompanion/SettingsAndQuickMenuShareOneVisibilityBool",
               TestSettingsAndQuickMenuShareOneVisibilityBool);
    runner.Add("SettingsCompanion/ResetPositionWritesThroughTheDragEndPath",
               TestResetPositionWritesThroughTheDragEndPath);
    runner.Add("SettingsCompanion/ResetPositionTargetIsInsideTheTargetDisplay",
               TestResetPositionTargetIsInsideTheTargetDisplay);
    runner.Add("SettingsCompanion/ResetPositionIgnoresLockPosition", TestResetPositionIgnoresLockPosition);
    runner.Add("SettingsCompanion/ResetPositionWorksWhileHidden", TestResetPositionWorksWhileHidden);
    runner.Add("SettingsCompanion/ResetPositionIsHonestNoOpWhenUnavailable",
               TestResetPositionIsHonestNoOpWhenUnavailable);
}

}  // namespace nimvlets::tests
