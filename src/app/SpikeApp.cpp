#include "app/SpikeApp.h"

#include "catalog/PetCatalogLoader.h"
#include "platform/RendererPolicy.h"
#include "platform/TransparentWindowSupport.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>

namespace nimvlets::app {

namespace {

// De dónde se carga el catálogo de pets. Ruta relativa, resuelta desde
// el directorio de trabajo del proceso — ver docs/CATALOG.md.
constexpr const char* kCatalogPath = "assets/dev/pet_catalog.nvcat";

// Reads NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS — see
// SpikeApp::ComputeEffectiveAmbientIntervalSeconds()'s doc comment.
constexpr const char* kDevPassiveIntervalEnvVar = "NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS";

// Lee NIMVLETS_DEV_SWITCH_TEST_COUNT — ver el comentario de
// SpikeApp::RunDevSwitchSmokeTestIfRequested().
constexpr const char* kDevSwitchTestCountEnvVar = "NIMVLETS_DEV_SWITCH_TEST_COUNT";

// Lee NIMVLETS_DEV_DIRECTION_TEST_COUNT — ver el comentario de
// SpikeApp::RunDevDirectionSmokeTestIfRequested().
constexpr const char* kDevDirectionTestCountEnvVar = "NIMVLETS_DEV_DIRECTION_TEST_COUNT";

// Lee NIMVLETS_DEV_CLICK_TEST_COUNT — ver el comentario de
// SpikeApp::RunDevClickSmokeTestIfRequested().
constexpr const char* kDevClickTestCountEnvVar = "NIMVLETS_DEV_CLICK_TEST_COUNT";

// Lee NIMVLETS_DEV_HOVER_TEST_COUNT — ver el comentario de
// SpikeApp::RunDevHoverSmokeTestIfRequested().
constexpr const char* kDevHoverTestCountEnvVar = "NIMVLETS_DEV_HOVER_TEST_COUNT";

// Lee NIMVLETS_DEV_RENDERER_DRIVER — mecanismo solo-DEV que fuerza un
// driver de SDL_Renderer específico (p. ej. "opengl", "metal",
// "software"), sin importar la plataforma o su default de producto
// (ver platform::PreferredRendererDriverName() y DEC-083 en
// docs/DECISION_LOG.md) -- así el owner puede comparar drivers en
// cualquier sistema operativo sin recompilar. Ausente/vacía: no-op,
// se usa el default de la plataforma actual.
constexpr const char* kDevRendererDriverEnvVar = "NIMVLETS_DEV_RENDERER_DRIVER";

// Lee NIMVLETS_DEV_SELECT_PET — mecanismo solo-DEV (Block 05, §10 del
// brief: "leave a simple DEV mechanism to manually launch/select"). Si
// está seteada a un "petId" o "petId/variantId" que exista en el
// catálogo, ese pet se activa en vez del default/persistido — sin
// tocar appState_.activePetId ni el archivo de estado real del owner
// (esto solo reemplaza QUÉ se carga, no lo que se persiste después:
// cualquier click/drag que ocurra en esta sesión SÍ se guarda contra
// el pet efectivamente activo, igual que un switch en runtime normal —
// ver TrySwitchActivePet()). Ausente o vacía: no-op total, mismo
// comportamiento que sin esta variable.
constexpr const char* kDevSelectPetEnvVar = "NIMVLETS_DEV_SELECT_PET";

// Mecanismo DEV genérico y reusado varias veces ya (no una sonda
// descartable de una sola investigación): si está seteada a un
// directorio, cada RenderFrame() real vuelca los pixeles YA COMPUESTOS
// por el renderer (SDL_RenderReadPixels, lo que de verdad se
// presentó) a un .rgba crudo -- evidencia directa de lo que el
// pipeline real produce, sin captura de pantalla (lee el backbuffer de
// NUESTRO propio renderer, nunca el escritorio). Usado originalmente
// para el diagnóstico de raíz de Bunny, y de nuevo para el A/B de
// renderer reutilizable-vs-por-frame (ver DEC-081) y para comparar
// software vs. acelerado en esta pasada (ver DEC-083) -- se conserva
// como herramienta permanente porque comparar "lo que realmente se
// presentó" entre dos configuraciones sigue siendo la forma más barata
// de verificar equivalencia visual sin depender de captura de pantalla.
constexpr const char* kDevDumpFramesDirEnvVar = "NIMVLETS_DEV_DUMP_FRAMES_DIR";

// Separación de wall-clock del redraw de confirmación (ver el
// comentario de confirmRedrawDeadlineMs_ en SpikeApp.h). 120ms es
// perceptualmente instantáneo pero suficiente separación real de
// tiempo como para que el compositor/GPU tenga una oportunidad genuina
// de asentarse entre presents.
constexpr double kConfirmRedrawDelayMs = 120.0;

constexpr const char* kPrefPathOrg = "Leporc Projects";
constexpr const char* kPrefPathApp = "Nimvlets";

constexpr const char* kDevAppDataDirEnvVar = "NIMVLETS_DEV_APPDATA_DIR";

std::atomic<bool> g_shutdownRequested{false};

extern "C" void HandleTerminationSignal(int /*signum*/) {
    g_shutdownRequested.store(true, std::memory_order_relaxed);
}

struct CursorSample {
    float globalX = 0.0f;
    float globalY = 0.0f;
    int windowX = 0;
    int windowY = 0;
    core::Point localPoint{};
};

CursorSample SampleCursor(SDL_Window* window) {
    CursorSample sample;
    SDL_GetGlobalMouseState(&sample.globalX, &sample.globalY);
    SDL_GetWindowPosition(window, &sample.windowX, &sample.windowY);
    sample.localPoint = core::Point{
        static_cast<double>(sample.globalX) - static_cast<double>(sample.windowX),
        static_cast<double>(sample.globalY) - static_cast<double>(sample.windowY),
    };
    return sample;
}

SDL_Surface* BuildHitTestShapeSurface(const core::AlphaMask& mask) {
    SDL_Surface* surface = SDL_CreateSurface(mask.Width(), mask.Height(), SDL_PIXELFORMAT_RGBA32);
    if (surface == nullptr) {
        SDL_Log("nimvlets: SDL_CreateSurface (hit-test shape) failed: %s", SDL_GetError());
        return nullptr;
    }

    for (int y = 0; y < mask.Height(); ++y) {
        for (int x = 0; x < mask.Width(); ++x) {
            const bool opaque = mask.Contains(core::Point{static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5});
            SDL_WriteSurfacePixel(surface, x, y, 255, 255, 255, opaque ? 255 : 0);
        }
    }
    return surface;
}

#ifndef NDEBUG
// Solo usado por el log de diagnóstico transition-only del loop
// principal (compilado fuera en Release, igual que el resto de la
// instrumentación #ifndef NDEBUG de este archivo).
const char* ControllerModeName(content::ControllerMode mode) {
    switch (mode) {
        case content::ControllerMode::kBase:
            return "Base";
        case content::ControllerMode::kAmbientOrHoverAction:
            return "AmbientOrHoverAction";
        case content::ControllerMode::kClickAction:
            return "ClickAction";
    }
    return "Unknown";
}
#endif  // NDEBUG

}  // namespace

bool ShutdownRequested() {
    return g_shutdownRequested.load(std::memory_order_relaxed);
}

int SpikeApp::EffectiveCanvasWidth() const {
    return std::max(1, static_cast<int>(std::lround(pet_.canvasWidth * pet_.visualScale)));
}

int SpikeApp::EffectiveCanvasHeight() const {
    return std::max(1, static_cast<int>(std::lround(pet_.canvasHeight * pet_.visualScale)));
}

double SpikeApp::ComputeEffectiveAmbientIntervalSeconds(const content::BehaviorState& state) const {
    const char* overrideEnv = std::getenv(kDevPassiveIntervalEnvVar);
    if (overrideEnv == nullptr) {
        return state.ambientIntervalSeconds;
    }

    char* end = nullptr;
    const double parsed = std::strtod(overrideEnv, &end);
    if (end == overrideEnv || parsed <= 0.0) {
        SDL_Log(
            "nimvlets: %s='%s' is not a valid positive number; ignoring (using state default %.1fs)",
            kDevPassiveIntervalEnvVar, overrideEnv, state.ambientIntervalSeconds);
        return state.ambientIntervalSeconds;
    }
    return parsed;
}

void SpikeApp::RearmAmbientDeadline(double nowMs) {
    const content::BehaviorState& state = animController_->CurrentState();
    if (state.ambientActions.empty()) {
        ambientDeadlineMs_.reset();
        return;
    }
    ambientDeadlineMs_ = nowMs + ComputeEffectiveAmbientIntervalSeconds(state) * 1000.0;
}

void SpikeApp::MarkNeedsRedraw(double nowMs) {
    needsRedraw_ = true;
    confirmRedrawDeadlineMs_ = nowMs + kConfirmRedrawDelayMs;
}

bool SpikeApp::Init() {
    std::signal(SIGINT, HandleTerminationSignal);
    std::signal(SIGTERM, HandleTerminationSignal);

    SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP, "1");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("nimvlets: SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    std::string catalogError;
    if (!catalog::LoadCatalogFromFile(kCatalogPath, catalog_, catalogError)) {
        SDL_Log("nimvlets: FATAL: could not load pet catalog '%s': %s", kCatalogPath, catalogError.c_str());
        SDL_Log("nimvlets: (run from the repository root, or regenerate it: python3 tools/compile_pet_catalog.py assets/dev/pet_catalog_manifest.json assets/dev/pet_catalog.nvcat)");
        return false;
    }

    std::string appDataDir;
    if (const char* devDir = std::getenv(kDevAppDataDirEnvVar); devDir != nullptr && devDir[0] != '\0') {
        std::error_code ec;
        std::filesystem::create_directories(devDir, ec);
        if (ec) {
            SDL_Log(
                "nimvlets: could not create DEV app-data dir '%s' (%s) -- persistence disabled for this run",
                devDir, ec.message().c_str());
        } else {
            appDataDir = devDir;
            SDL_Log(
                "nimvlets: DEV override active — %s='%s' (production uses SDL_GetPrefPath instead)",
                kDevAppDataDirEnvVar, devDir);
        }
    } else {
        char* prefPathRaw = SDL_GetPrefPath(kPrefPathOrg, kPrefPathApp);
        if (prefPathRaw != nullptr) {
            appDataDir = prefPathRaw;
            SDL_free(prefPathRaw);
        } else {
            SDL_Log("nimvlets: SDL_GetPrefPath failed: %s -- persistence disabled for this run", SDL_GetError());
        }
    }

    if (!appDataDir.empty()) {
        appStateStore_.emplace(appDataDir);

        std::string loadWarning;
        appState_ = appStateStore_->Load(&loadWarning);
        if (!loadWarning.empty()) {
            SDL_Log("nimvlets: %s", loadWarning.c_str());
        }
    }

    // Selección solo-DEV (ver kDevSelectPetEnvVar) -- resuelta ANTES
    // que la persistida, así que un valor válido siempre gana sobre lo
    // que appState_ diga para efectos de qué se carga esta sesión.
    catalog::PetIdentity requestedIdentity{appState_.activePetId, appState_.activeVariantId};
    bool haveDevSelection = false;
    if (const char* devSelect = std::getenv(kDevSelectPetEnvVar); devSelect != nullptr && devSelect[0] != '\0') {
        const std::string spec(devSelect);
        const std::size_t slash = spec.find('/');
        catalog::PetIdentity devIdentity{
            slash == std::string::npos ? spec : spec.substr(0, slash),
            slash == std::string::npos ? std::string() : spec.substr(slash + 1),
        };
        if (catalog_.Find(devIdentity) != nullptr) {
            requestedIdentity = devIdentity;
            haveDevSelection = true;
            SDL_Log(
                "nimvlets: DEV override active — %s='%s' (persisted selection ignored for this run only)",
                kDevSelectPetEnvVar, devSelect);
        } else {
            SDL_Log(
                "nimvlets: %s='%s' does not match any catalog entry; ignoring (using persisted/default selection)",
                kDevSelectPetEnvVar, devSelect);
        }
    }

    const catalog::ResolvedSelection resolved = catalog::ResolveActiveSelection(catalog_, requestedIdentity);

    content::PetDefinition loadedPet;
    std::string packError;
    const catalog::CatalogEntry* loadedEntry = resolved.entry;
    bool usedFallback = resolved.usedFallback;
    if (!catalog::LoadPetForIdentity(catalog_, loadedEntry->identity, loadedPet, packError)) {
        SDL_Log("nimvlets: pack for '%s' failed to load (%s)", loadedEntry->identity.petId.c_str(), packError.c_str());
        if (loadedEntry != &catalog_.Default()) {
            SDL_Log("nimvlets: falling back to catalog default");
            loadedEntry = &catalog_.Default();
            usedFallback = true;
            if (!catalog::LoadPetForIdentity(catalog_, loadedEntry->identity, loadedPet, packError)) {
                SDL_Log("nimvlets: FATAL: catalog default pack also failed to load: %s", packError.c_str());
                return false;
            }
        } else {
            SDL_Log("nimvlets: FATAL: catalog default pack failed to load; no further fallback possible");
            return false;
        }
    }
    pet_ = std::move(loadedPet);

    // Repara la selección persistida en memoria si terminamos usando
    // algo distinto de lo guardado -- pero NUNCA si la razón fue la
    // selección solo-DEV de arriba (esa es deliberadamente transitoria
    // para esta sesión, no debe sobrescribir el estado real del owner).
    if (usedFallback && !haveDevSelection) {
        appState_.activePetId = loadedEntry->identity.petId;
        appState_.activeVariantId = loadedEntry->identity.variantId;
        persistenceScheduler_.MarkDirty(static_cast<double>(SDL_GetTicks()));
        SDL_Log(
            "nimvlets: persisted pet selection repaired to '%s'%s%s",
            loadedEntry->identity.petId.c_str(),
            loadedEntry->identity.variantId.empty() ? "" : "/",
            loadedEntry->identity.variantId.c_str());
    }

    const SDL_WindowFlags flags =
        SDL_WINDOW_TRANSPARENT |
        SDL_WINDOW_BORDERLESS |
        SDL_WINDOW_ALWAYS_ON_TOP |
        SDL_WINDOW_UTILITY |
        SDL_WINDOW_NOT_FOCUSABLE |
        SDL_WINDOW_HIGH_PIXEL_DENSITY;

    window_ = SDL_CreateWindow(
        "Nimvlets Foundation Spike",
        EffectiveCanvasWidth(),
        EffectiveCanvasHeight(),
        flags);
    if (window_ == nullptr) {
        SDL_Log("nimvlets: SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    if (appState_.lastWindowPosition.has_value()) {
        if (!SDL_SetWindowPosition(window_, appState_.lastWindowPosition->x, appState_.lastWindowPosition->y)) {
            SDL_Log(
                "nimvlets: could not restore saved window position (%d, %d): %s -- this window system "
                "does not support positioning a normal window here; the saved position is kept in the "
                "state file for a session/platform where it does work",
                appState_.lastWindowPosition->x, appState_.lastWindowPosition->y, SDL_GetError());
        }
    } else {
        SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }

    {
        const char* devOverride = std::getenv(kDevRendererDriverEnvVar);
        const platform::RendererPlatform currentPlatform = platform::CurrentRendererPlatform();
        const char* preferredDriver = platform::PreferredRendererDriverName(currentPlatform, devOverride);

        renderer_ = SDL_CreateRenderer(window_, preferredDriver);
        bool preferredDriverHonored = renderer_ != nullptr;
        if (renderer_ == nullptr && preferredDriver != nullptr) {
            // Fallback documentado, nunca silencioso: si el driver
            // preferido (p. ej. "software" en macOS, ver DEC-083, o
            // cualquier NIMVLETS_DEV_RENDERER_DRIVER inválido) no pudo
            // crear un renderer en ESTA máquina -- SDL no lo tiene
            // compilado, o algún otro fallo real -- se cae al default
            // histórico de SDL en vez de fallar el arranque entero por
            // una preferencia que no es esencial para funcionar.
            SDL_Log(
                "nimvlets: preferred renderer driver '%s' failed (%s); falling back to SDL's own default driver",
                preferredDriver, SDL_GetError());
            renderer_ = SDL_CreateRenderer(window_, nullptr);
        }
        if (renderer_ == nullptr) {
            SDL_Log("nimvlets: SDL_CreateRenderer failed: %s", SDL_GetError());
            return false;
        }
        const bool devOverrideRequested = devOverride != nullptr && devOverride[0] != '\0';
        SDL_Log("nimvlets: renderer driver selected: '%s'%s", SDL_GetRendererName(renderer_),
                (devOverrideRequested && preferredDriverHonored) ? " (NIMVLETS_DEV_RENDERER_DRIVER override)" : "");
    }

    SDL_SetRenderLogicalPresentation(
        renderer_,
        EffectiveCanvasWidth(),
        EffectiveCanvasHeight(),
        SDL_LOGICAL_PRESENTATION_LETTERBOX);

    platform::ConfigureCompanionWindow(window_);


    animController_.emplace(pet_);
    lastKnownStateId_ = animController_->CurrentStateId();

    UpdateDirectionFromWindowPosition();

    // Hecho de runtime, no de plataforma: qué driver terminó eligiendo
    // SDL de verdad (ver el log "renderer driver selected" arriba), no
    // cuál pedimos. Ambas funciones de políticas de click-through abajo
    // lo necesitan porque Block 05 encontró que SDL_SetWindowShape()
    // corrompe el render bajo el driver "software" en macOS -- ver
    // NativeShapeHitTestIsRenderSafe() en platform/TransparentWindowSupport.h
    // para la evidencia completa.
    const bool usingSoftwareRenderer = SDL_strcmp(SDL_GetRendererName(renderer_), SDL_SOFTWARE_RENDERER) == 0;
    usingNativeShapeHitTest_ = platform::NativeShapeHitTestIsRenderSafe(usingSoftwareRenderer);
    usingPollDrivenClickThrough_ =
        !usingNativeShapeHitTest_ && platform::ClickThroughPollingIsMeaningful(usingSoftwareRenderer);

    // Antes de escribir el estado nativo por primera vez: hacer que la
    // política per-pixel de Nimvlets sea la ÚNICA escritora de esa
    // propiedad. En macOS bajo el renderer de software esto es lo que
    // convierte el mecanismo de "un poll que pelea contra el backend
    // Cocoa de SDL y pierde" en una decisión autoritativa -- ver
    // platform::MakeClickThroughAuthoritative() para la causa raíz
    // completa. En Windows/Linux es un no-op declarado (no hay otro
    // escritor), así que esta llamada es incondicional y sin #ifdef.
    if (usingPollDrivenClickThrough_) {
        clickThroughOwnershipInstalled_ = platform::MakeClickThroughAuthoritative(window_);
    }

    SDL_Log(
        "nimvlets: click-through mechanism = %s",
        usingNativeShapeHitTest_
            ? "native SDL_SetWindowShape (event-driven, no polling)"
            : (usingPollDrivenClickThrough_
                   ? (clickThroughOwnershipInstalled_
                          ? "Nimvlets-owned per-pixel state (authoritative; cursor sampled only while the cursor is inside the window)"
                          : "Nimvlets-driven per-pixel state (no ownership guard needed on this platform)")
                   : "none available (see docs/LINUX_PLATFORM.md) -- relying on IsPointInteractive() app-side gating only"));

    RenderFrame();
    ApplyCurrentHitMask();
    needsRedraw_ = false;

    if (usingPollDrivenClickThrough_) {
        // Estado inicial resuelto contra la posición REAL del cursor:
        // sin esto, el primer estado sería el default de
        // currentlyClickThrough_ (false) hasta el primer evento, y con
        // el muestreo apagado (arranca desarmado) ese primer evento
        // podría no llegar nunca si el cursor ya está quieto encima.
        const CursorSample startupSample = SampleCursor(window_);
        UpdateClickThrough(
            IsPointInsideWindow(startupSample.localPoint), IsPointInteractive(startupSample.localPoint));
    }

    RearmAmbientDeadline(static_cast<double>(SDL_GetTicks()));

    passiveActionRng_.seed(std::random_device{}());

    int logicalW = 0;
    int logicalH = 0;
    int pixelW = 0;
    int pixelH = 0;
    SDL_GetWindowSize(window_, &logicalW, &logicalH);
    SDL_GetWindowSizeInPixels(window_, &pixelW, &pixelH);
    SDL_Log(
        "nimvlets: window size logical=%dx%d pixels=%dx%d (pixel density=%.2f, display scale=%.2f)",
        logicalW, logicalH, pixelW, pixelH,
        static_cast<double>(SDL_GetWindowPixelDensity(window_)),
        static_cast<double>(SDL_GetWindowDisplayScale(window_)));

    const content::BehaviorState& startupState = animController_->CurrentState();
    std::string ambientDescription = "no ambient action for this state";
    if (!startupState.ambientActions.empty()) {
        char ambientBuf[96];
        std::snprintf(
            ambientBuf, sizeof(ambientBuf), "ambient action every ~%.0fs", ComputeEffectiveAmbientIntervalSeconds(startupState));
        ambientDescription = ambientBuf;
    }
    SDL_Log(
        "nimvlets: pet '%s' (%s) ready — native canvas %dx%d x visualScale=%.3f = %dx%d on screen, "
        "alpha hit threshold=%d/255, state='%s' (%s). Click the shape; drag to move; close the window to "
        "quit. Click balance: %llu (%s).",
        pet_.id.c_str(), pet_.displayName.c_str(), pet_.canvasWidth, pet_.canvasHeight, pet_.visualScale,
        EffectiveCanvasWidth(), EffectiveCanvasHeight(), static_cast<int>(pet_.alphaHitThreshold),
        startupState.id.c_str(), ambientDescription.c_str(),
        static_cast<unsigned long long>(appState_.clickBalance),
        appStateStore_.has_value() ? "persisted locally" : "persistence unavailable this run");

    return true;
}

void SpikeApp::FlushPersistedState() {
    if (!appStateStore_.has_value() || !persistenceScheduler_.IsDirty()) {
        return;
    }
    std::string error;
    if (appStateStore_->Save(appState_, error)) {
        persistenceScheduler_.OnFlushSucceeded();
    } else {
        SDL_Log("nimvlets: failed to save app state: %s", error.c_str());
        persistenceScheduler_.OnFlushFailed(static_cast<double>(SDL_GetTicks()));
    }
}

bool SpikeApp::TrySwitchActivePet(const catalog::PetIdentity& target) {
    content::PetDefinition newPet;
    std::string error;
    if (!catalog::LoadPetForIdentity(catalog_, target, newPet, error)) {
        SDL_Log(
            "nimvlets: switch to '%s'%s%s failed: %s (current pet unchanged)",
            target.petId.c_str(), target.variantId.empty() ? "" : "/", target.variantId.c_str(), error.c_str());
        return false;
    }

    // Un pet nuevo tiene (potencialmente) otras dimensiones de frame:
    // se descarta la textura activa para que RenderFrame() la vuelva a
    // crear con el tamaño correcto en el próximo dibujo.
    activeFrameTexture_.Reset();
    animController_.reset();
    pet_ = std::move(newPet);
    animController_.emplace(pet_);  // arranca en states[0]/kBase del pet nuevo
    lastKnownStateId_ = animController_->CurrentStateId();

    UpdateDirectionFromWindowPosition();

    SDL_SetWindowSize(window_, EffectiveCanvasWidth(), EffectiveCanvasHeight());
    SDL_SetRenderLogicalPresentation(renderer_, EffectiveCanvasWidth(), EffectiveCanvasHeight(), SDL_LOGICAL_PRESENTATION_LETTERBOX);

    appState_.activePetId = target.petId;
    appState_.activeVariantId = target.variantId;
    persistenceScheduler_.MarkDirty(static_cast<double>(SDL_GetTicks()));
    RearmAmbientDeadline(static_cast<double>(SDL_GetTicks()));
    MarkNeedsRedraw(static_cast<double>(SDL_GetTicks()));

    SDL_Log(
        "nimvlets: switched active pet to '%s'%s%s ('%s')",
        pet_.id.c_str(), pet_.variantGroup.empty() ? "" : "/", pet_.variantGroup.c_str(), pet_.displayName.c_str());
    return true;
}

void SpikeApp::RunDevSwitchSmokeTestIfRequested() {
    const char* countEnv = std::getenv(kDevSwitchTestCountEnvVar);
    if (countEnv == nullptr || countEnv[0] == '\0') {
        return;
    }

    char* end = nullptr;
    const long parsed = std::strtol(countEnv, &end, 10);
    if (end == countEnv || parsed <= 0) {
        SDL_Log("nimvlets: %s='%s' is not a valid positive integer; ignoring", kDevSwitchTestCountEnvVar, countEnv);
        return;
    }
    const auto count = static_cast<std::size_t>(parsed);

    SDL_Log(
        "nimvlets: DEV switch smoke test active — %s=%zu (%zu catalog entr%s available)",
        kDevSwitchTestCountEnvVar, count, catalog_.Entries().size(), catalog_.Entries().size() == 1 ? "y" : "ies");

    std::size_t successCount = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const catalog::CatalogEntry& target = catalog_.Entries()[i % catalog_.Entries().size()];
        if (TrySwitchActivePet(target.identity)) {
            ++successCount;
        }
    }

    SDL_Log("nimvlets: DEV switch smoke test complete — %zu/%zu switches succeeded", successCount, count);
}

void SpikeApp::SetActiveDirection(content::Direction direction) {
    const double nowMs = static_cast<double>(SDL_GetTicks());
    const bool changed = animController_->SetDirection(direction, nowMs);
    SDL_Log(
        "nimvlets: active direction -> %s%s",
        content::ToString(direction), changed ? "" : " (no visual change -- already active, or a gesture is in progress)");
    if (changed) {
        MarkNeedsRedraw(nowMs);
        // Un cambio de dirección real (incluyendo el causado por un
        // drag que cruza la mitad de pantalla -- ver
        // UpdateDirectionFromWindowPosition()) es una interacción
        // visible del owner con el pet (Block 05 segunda pasada).
        RearmAmbientDeadline(nowMs);
    }
}

void SpikeApp::UpdateDirectionFromWindowPosition() {
    int winX = 0;
    int winY = 0;
    int winW = 0;
    int winH = 0;
    SDL_GetWindowPosition(window_, &winX, &winY);
    SDL_GetWindowSize(window_, &winW, &winH);
    const double windowCenterX = static_cast<double>(winX) + static_cast<double>(winW) / 2.0;

    const SDL_DisplayID displayId = SDL_GetDisplayForWindow(window_);
    if (displayId == 0) {
        SDL_Log("nimvlets: UpdateDirectionFromWindowPosition: SDL_GetDisplayForWindow failed: %s", SDL_GetError());
        return;
    }
    SDL_Rect displayBounds{};
    if (!SDL_GetDisplayBounds(displayId, &displayBounds)) {
        SDL_Log("nimvlets: UpdateDirectionFromWindowPosition: SDL_GetDisplayBounds failed: %s", SDL_GetError());
        return;
    }

    const double displayCenterX = static_cast<double>(displayBounds.x) + static_cast<double>(displayBounds.w) / 2.0;
    const content::Direction target =
        (windowCenterX < displayCenterX) ? content::Direction::kLeft : content::Direction::kRight;
    SDL_Log(
        "nimvlets: screen-half direction check: windowCenterX=%.1f displayBounds=(%d,%d,%dx%d) displayCenterX=%.1f -> %s",
        windowCenterX, displayBounds.x, displayBounds.y, displayBounds.w, displayBounds.h, displayCenterX,
        content::ToString(target));
    SetActiveDirection(target);
}

void SpikeApp::RunDevDirectionSmokeTestIfRequested() {
    const char* countEnv = std::getenv(kDevDirectionTestCountEnvVar);
    if (countEnv == nullptr || countEnv[0] == '\0') {
        return;
    }

    char* end = nullptr;
    const long parsed = std::strtol(countEnv, &end, 10);
    if (end == countEnv || parsed <= 0) {
        SDL_Log("nimvlets: %s='%s' is not a valid positive integer; ignoring", kDevDirectionTestCountEnvVar, countEnv);
        return;
    }
    const auto count = static_cast<std::size_t>(parsed);

    SDL_Log("nimvlets: DEV direction smoke test active — %s=%zu", kDevDirectionTestCountEnvVar, count);

    for (std::size_t i = 0; i < count; ++i) {
        const content::Direction target = (i % 2 == 0) ? content::Direction::kLeft : content::Direction::kRight;
        SetActiveDirection(target);
    }

    SDL_Log("nimvlets: DEV direction smoke test complete — %zu direction change(s) requested", count);
}

void SpikeApp::RunDevClickSmokeTestIfRequested() {
    const char* countEnv = std::getenv(kDevClickTestCountEnvVar);
    if (countEnv == nullptr || countEnv[0] == '\0') {
        return;
    }

    char* end = nullptr;
    const long parsed = std::strtol(countEnv, &end, 10);
    if (end == countEnv || parsed <= 0) {
        SDL_Log("nimvlets: %s='%s' is not a valid positive integer; ignoring", kDevClickTestCountEnvVar, countEnv);
        return;
    }
    const auto count = static_cast<std::size_t>(parsed);

    SDL_Log("nimvlets: DEV click smoke test active — %s=%zu", kDevClickTestCountEnvVar, count);

    const double nowMs = static_cast<double>(SDL_GetTicks());
    for (std::size_t i = 0; i < count; ++i) {
        ++clickCount_;
        ++appState_.clickBalance;
        persistenceScheduler_.MarkDirty(nowMs);
        animController_->TriggerClick(NextUniformRandom01(), nowMs);
    }
    MarkNeedsRedraw(nowMs);

    SDL_Log("nimvlets: DEV click smoke test complete — %zu click(s) requested", count);
}

double SpikeApp::NextUniformRandom01() {
    return std::uniform_real_distribution<double>(0.0, 1.0)(passiveActionRng_);
}

void SpikeApp::MaybeTriggerHoverAction(bool cursorOverOpaque, double nowMs) {
    if (dragClassifier_.IsActive()) {
        // Un gesto en curso resetea el dwell -- "arrastrando" no
        // cuenta como reposo tranquilo, y el owner pidió
        // explícitamente que un drag reinicie el contador.
        ResetHoverDwell();
        return;
    }

    const bool wasDwelling = hoverDwellTracker_.IsDwelling();
    const bool crossedThreshold = hoverDwellTracker_.Update(cursorOverOpaque, nowMs);

    if (!wasDwelling && hoverDwellTracker_.IsDwelling()) {
        // El cursor ACABA de entrar a la región interactiva -- un
        // dwell nuevo recién arrancó. "Visible pointer enter" cuenta
        // como interacción real por sí solo (owner: "the pet should
        // not perform an ambient action immediately after the owner
        // just interacted with it" -- pasar el mouse por encima
        // también cuenta, incluso si nunca llega a mantenerse quieto
        // lo suficiente como para disparar una acción de hover).
        RearmAmbientDeadline(nowMs);
    }

    // Mantiene armado (o limpia) hoverDwellDeadlineMs_ para que el
    // loop principal pueda despertar exactamente cuando el dwell EN
    // CURSO cruzaría el umbral, incluso sin más eventos de motion --
    // ver el comentario del campo en SpikeApp.h.
    if (hoverDwellTracker_.IsDwelling()) {
        if (!hoverDwellDeadlineMs_.has_value()) {
            hoverDwellDeadlineMs_ = nowMs + kHoverDwellSeconds * 1000.0;
        }
    } else {
        hoverDwellDeadlineMs_.reset();
    }

    if (!crossedThreshold) {
        return;
    }

    if (animController_->TriggerHoverAction(NextUniformRandom01(), nowMs)) {
        MarkNeedsRedraw(nowMs);
        // Un disparo de hover completo es, con más razón, una
        // interacción real -- el próximo ambient debe volver a quedar
        // a ~15s de distancia desde ESTE momento, no desde el enter
        // previo (ya reiniciado arriba, pero una acción real
        // completada lo confirma/reafirma).
        RearmAmbientDeadline(nowMs);
        SDL_Log("nimvlets: hover-triggered action after %.0fs dwell (state='%s')", kHoverDwellSeconds, animController_->CurrentStateId().c_str());
    }
}

void SpikeApp::ResetHoverDwell() {
    hoverDwellTracker_.Reset();
    hoverDwellDeadlineMs_.reset();
}

void SpikeApp::RunDevHoverSmokeTestIfRequested() {
    const char* countEnv = std::getenv(kDevHoverTestCountEnvVar);
    if (countEnv == nullptr || countEnv[0] == '\0') {
        return;
    }

    char* end = nullptr;
    const long parsed = std::strtol(countEnv, &end, 10);
    if (end == countEnv || parsed <= 0) {
        SDL_Log("nimvlets: %s='%s' is not a valid positive integer; ignoring", kDevHoverTestCountEnvVar, countEnv);
        return;
    }
    const auto count = static_cast<std::size_t>(parsed);

    SDL_Log("nimvlets: DEV hover smoke test active — %s=%zu", kDevHoverTestCountEnvVar, count);

    // Tiempo SIMULADO, no wall-clock real. Cada ciclo: el cursor
    // "entra" (arranca el dwell, no dispara todavía), permanece
    // encima kHoverDwellSeconds completos (cruza el umbral -- dispara,
    // solo en el primer ciclo real: los siguientes quedan en no-op
    // porque el controller pasa a modo AmbientOrHoverAction y nada lo
    // avanza de vuelta a Base dentro de esta corrida sincrónica --
    // mismo patrón honesto que antes: "dispara una vez, después se
    // queda callado", prueba que nunca hace spam), y luego "sale"
    // (resetea el dwell) antes del próximo ciclo.
    double simulatedNowMs = static_cast<double>(SDL_GetTicks());
    for (std::size_t i = 0; i < count; ++i) {
        simulatedNowMs += 1.0;
        MaybeTriggerHoverAction(true, simulatedNowMs);  // entra -- arranca el dwell
        simulatedNowMs += kHoverDwellSeconds * 1000.0 + 1.0;
        MaybeTriggerHoverAction(true, simulatedNowMs);  // cruza el umbral de dwell
        simulatedNowMs += 1.0;
        MaybeTriggerHoverAction(false, simulatedNowMs);  // sale -- resetea
    }

    // Deja un estado limpio de wall-clock antes de devolver el control
    // -- el tiempo simulado de arriba no debe dejar ningún deadline de
    // dwell pendiente que dispare inesperadamente en el loop real.
    ResetHoverDwell();

    SDL_Log("nimvlets: DEV hover smoke test complete — %zu hover cycle(s) requested", count);
}

void SpikeApp::Shutdown() {
    FlushPersistedState();

    // Antes de destruir el renderer: la textura activa debe morir
    // primero (SDL exige que ninguna textura sobreviva a su renderer).
    activeFrameTexture_.Reset();
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Log(
        "nimvlets: clean shutdown, %d click(s) this session, click balance %llu",
        clickCount_, static_cast<unsigned long long>(appState_.clickBalance));
    SDL_Quit();
}

bool SpikeApp::IsPointInteractive(core::Point localPoint) const {
    return activeHitMask_.Contains(localPoint);
}

bool SpikeApp::IsPointInsideWindow(core::Point localPoint) const {
    return localPoint.x >= 0.0 && localPoint.y >= 0.0 &&
           localPoint.x < static_cast<double>(EffectiveCanvasWidth()) &&
           localPoint.y < static_cast<double>(EffectiveCanvasHeight());
}

void SpikeApp::RenderFrame() {
    const content::FrameDefinition& frame = animController_->CurrentFrame();

    // UNA textura reutilizable por pet, actualizada en el lugar (ver
    // graphics::ActiveFrameTexture / DEC-081): el objeto SDL_Texture NO
    // cambia entre frames de una animación -- solo su contenido. Se
    // crea perezosamente en el primer dibujo de cada pet.
    SDL_Texture* texture = nullptr;
    if (activeFrameTexture_.SetFrame(renderer_, frame)) {
        texture = activeFrameTexture_.Get();
    }

    // Se presenta el mismo contenido DOS veces seguidas -- ver el
    // comentario histórico de Block 04.3 que sigue aplicando sin
    // cambios: un SDL_RenderPresent() "frío" tras un período largo sin
    // presentar nada puede mostrar un drawable todavía no asentado en
    // Metal. Block 05 amplía CUÁNDO se arma un redraw de confirmación
    // adicional 120ms más tarde (ver MarkNeedsRedraw()) a cualquier
    // transición de animación, no solo cambios de dirección -- ver el
    // informe final, sección de diagnóstico de Bunny, para la evidencia
    // real (un dump de frames vía SDL_RenderReadPixels) que motivó
    // ampliar esto, y sus límites honestos.
    for (int presentPass = 0; presentPass < 2; ++presentPass) {
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
        SDL_RenderClear(renderer_);
        if (texture != nullptr) {
            const SDL_FRect dst{
                0.0f, 0.0f, static_cast<float>(EffectiveCanvasWidth()), static_cast<float>(EffectiveCanvasHeight())};
            SDL_RenderTexture(renderer_, texture, nullptr, &dst);
        } else if (!frame.pixels.empty() && presentPass == 0) {
            SDL_Log(
                "nimvlets: RenderFrame: current frame (%dx%d) has pixel data but no attached texture -- "
                "rendering fully transparent; check ActiveFrameTexture::SetFrame() failures above",
                frame.width, frame.height);
        }
        SDL_RenderPresent(renderer_);
    }

    if (const char* dumpDir = std::getenv(kDevDumpFramesDirEnvVar); dumpDir != nullptr && dumpDir[0] != '\0') {
        DumpCurrentRenderedFrame(dumpDir);
    }
}

void SpikeApp::DumpCurrentRenderedFrame(const std::string& dir) {
    SDL_Surface* raw = SDL_RenderReadPixels(renderer_, nullptr);
    if (raw == nullptr) {
        SDL_Log("nimvlets: [dev-dump] SDL_RenderReadPixels failed: %s", SDL_GetError());
        return;
    }
    SDL_Surface* rgba = raw;
    bool ownsRgba = false;
    if (raw->format != SDL_PIXELFORMAT_RGBA32) {
        rgba = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_RGBA32);
        ownsRgba = true;
        if (rgba == nullptr) {
            SDL_Log("nimvlets: [dev-dump] SDL_ConvertSurface failed: %s", SDL_GetError());
            SDL_DestroySurface(raw);
            return;
        }
    }

    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const std::string path = dir + "/frame_" + std::to_string(devDumpFrameCount_) + ".rgba";
    std::ofstream out(path, std::ios::binary);
    const std::uint32_t w = static_cast<std::uint32_t>(rgba->w);
    const std::uint32_t h = static_cast<std::uint32_t>(rgba->h);
    out.write(reinterpret_cast<const char*>(&w), sizeof(w));
    out.write(reinterpret_cast<const char*>(&h), sizeof(h));
    for (int y = 0; y < rgba->h; ++y) {
        const auto* row = static_cast<const char*>(rgba->pixels) + static_cast<std::size_t>(y) * static_cast<std::size_t>(rgba->pitch);
        out.write(row, static_cast<std::streamsize>(w) * 4);
    }
    SDL_Log("nimvlets: [dev-dump] wrote %s (%ux%u)", path.c_str(), w, h);
    ++devDumpFrameCount_;

    if (ownsRgba) {
        SDL_DestroySurface(rgba);
    }
    SDL_DestroySurface(raw);
}

void SpikeApp::ApplyCurrentHitMask() {
    const content::FrameDefinition& frame = animController_->CurrentFrame();
    activeHitMask_ = core::AlphaMask::FromAlphaChannel(
        frame.pixels.empty() ? nullptr : frame.pixels.data(),
        frame.width, frame.height,
        EffectiveCanvasWidth(), EffectiveCanvasHeight(),
        pet_.alphaHitThreshold);

    if (usingNativeShapeHitTest_) {
        SDL_Surface* shape = BuildHitTestShapeSurface(activeHitMask_);
        if (shape != nullptr) {
            if (!SDL_SetWindowShape(window_, shape)) {
                SDL_Log("nimvlets: SDL_SetWindowShape (frame update) failed: %s", SDL_GetError());
            }
            SDL_DestroySurface(shape);
        }
    }
}

void SpikeApp::PollHover() {
    const double nowMs = static_cast<double>(SDL_GetTicks());

    const CursorSample sample = SampleCursor(window_);
    const bool cursorOverOpaque = IsPointInteractive(sample.localPoint);

#ifndef NDEBUG
    if (!diagHasValue_ || sample.globalX != diagGlobalX_ || sample.globalY != diagGlobalY_) {
        SDL_Log("nimvlets: [diag] global cursor position: (%.1f, %.1f)", sample.globalX, sample.globalY);
        diagGlobalX_ = sample.globalX;
        diagGlobalY_ = sample.globalY;
    }
    if (!diagHasValue_ || sample.windowX != diagWindowX_ || sample.windowY != diagWindowY_) {
        SDL_Log("nimvlets: [diag] window position: (%d, %d)", sample.windowX, sample.windowY);
        diagWindowX_ = sample.windowX;
        diagWindowY_ = sample.windowY;
    }
    if (!diagHasValue_ || sample.localPoint.x != diagLocalPoint_.x || sample.localPoint.y != diagLocalPoint_.y) {
        SDL_Log("nimvlets: [diag] local coordinate (global - window): (%.2f, %.2f)", sample.localPoint.x, sample.localPoint.y);
        diagLocalPoint_ = sample.localPoint;
    }
    if (!diagHasValue_ || cursorOverOpaque != diagContains_) {
        SDL_Log("nimvlets: [diag] IsPointInteractive(local)=%s", cursorOverOpaque ? "true" : "false");
        diagContains_ = cursorOverOpaque;
    }
    diagHasValue_ = true;
#endif  // NDEBUG

    UpdateClickThrough(IsPointInsideWindow(sample.localPoint), cursorOverOpaque);
    MaybeTriggerHoverAction(cursorOverOpaque, nowMs);

    hoverScheduler_.OnFramePresented(nowMs);
}

void SpikeApp::UpdateClickThrough(bool cursorInsideWindow, bool cursorOverOpaque) {
    const core::ClickThroughDecision decision =
        core::EvaluateClickThrough(cursorInsideWindow, cursorOverOpaque, dragClassifier_.IsActive());

    // Arma/desarma el muestreo periódico. Esto es lo que hace que el
    // costo en reposo sea CERO despertares con el cursor en cualquier
    // otro lugar de la pantalla -- ver core/ClickThroughPolicy.h.
    clickThroughSamplingActive_ = decision.samplingRequired;

#ifndef NDEBUG
    if (decision.clickThrough != diagRequestedClickThrough_ || !diagHasValue_) {
        SDL_Log("nimvlets: [diag] requested click-through=%s (sampling=%s)",
                decision.clickThrough ? "true" : "false", decision.samplingRequired ? "on" : "off");
        diagRequestedClickThrough_ = decision.clickThrough;
    }
#endif  // NDEBUG

    if (decision.clickThrough != currentlyClickThrough_) {
        const bool actual = platform::SetWindowClickThrough(window_, decision.clickThrough);
        currentlyClickThrough_ = decision.clickThrough;

#ifndef NDEBUG
        if (actual != diagActualIgnoresMouseEvents_ || !diagHasValue_) {
            SDL_Log(
                "nimvlets: [diag] native click-through actual=%s%s",
                actual ? "true" : "false",
                actual == decision.clickThrough ? "" : "  <-- MISMATCH vs. requested value!");
            diagActualIgnoresMouseEvents_ = actual;
        }
#else
        (void)actual;
#endif  // NDEBUG
        return;
    }

#ifndef NDEBUG
    // Diagnóstico que el brief de este bloque pide explícitamente:
    // comparar "lo que pedimos" contra "lo que el OS realmente tiene"
    // DESPUÉS de actividad real de mouse de Cocoa/SDL, no solo en el
    // instante de la asignación. Sin esto, el modo de falla histórico
    // (SDL pisando NSWindow.ignoresMouseEvents por la espalda) era
    // literalmente invisible desde el lado de la app: currentlyClickThrough_
    // decía "ya está en el valor correcto" y nunca se volvía a mirar.
    // ReadWindowClickThrough() LEE sin escribir, así que este chequeo no
    // puede enmascarar el bug arreglándolo de casualidad.
    const bool actualNow = platform::ReadWindowClickThrough(window_);
    if (actualNow != currentlyClickThrough_) {
        SDL_Log(
            "nimvlets: [diag] click-through DRIFT: requested=%s but native state=%s "
            "(foreign writes intercepted so far: %llu)",
            currentlyClickThrough_ ? "true" : "false", actualNow ? "true" : "false",
            platform::ForeignClickThroughWriteCount());
    }
#endif  // NDEBUG
}

void SpikeApp::HandleEvent(const SDL_Event& event, bool& running) {
    switch (event.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            running = false;
            break;

        case SDL_EVENT_WINDOW_EXPOSED:
            needsRedraw_ = true;
            break;

        case SDL_EVENT_WINDOW_MOVED:
            // Durante un drag REAL no se resuelve dirección: cada
            // SetActiveDirection() que cambia algo dispara un redraw
            // (y con él un swap de textura) JUSTO mientras la ventana
            // se está moviendo -- exactamente el escenario que el owner
            // reportó como corrupción visual al arrastrar durante una
            // animación. La dirección final se resuelve UNA vez al
            // soltar (ver SDL_EVENT_MOUSE_BUTTON_UP).
            if (!dragClassifier_.IsDragging()) {
                UpdateDirectionFromWindowPosition();
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            if (event.button.button != SDL_BUTTON_LEFT) {
                break;
            }
            const core::Point localOrigin{
                static_cast<double>(event.button.x),
                static_cast<double>(event.button.y),
            };

            if (!IsPointInteractive(localOrigin)) {
                break;
            }

            dragClassifier_.Begin(localOrigin);
            // Un gesto que arranca fija el estado en "no click-through"
            // y apaga el muestreo -- durante el arrastre los eventos
            // reales alcanzan.
            UpdateClickThrough(true, true);
            // Un click/drag que arranca reinicia cualquier dwell de
            // hover en curso -- el owner lo pidió explícitamente,
            // incluso si termina siendo un click corto (no un drag) y
            // el cursor nunca sale de la región interactiva.
            ResetHoverDwell();
            // "Drag start" es también una interacción real que debe
            // reiniciar el conteo ambient (owner, Block 05 segunda
            // pasada) -- así un drag largo no deja que el ambient
            // dispare mientras el owner sigue con el botón apretado.
            RearmAmbientDeadline(static_cast<double>(SDL_GetTicks()));

            int winX = 0;
            int winY = 0;
            SDL_GetWindowPosition(window_, &winX, &winY);
            float globalX = 0.0f;
            float globalY = 0.0f;
            SDL_GetGlobalMouseState(&globalX, &globalY);
            dragGrabOffsetX_ = static_cast<double>(globalX) - static_cast<double>(winX);
            dragGrabOffsetY_ = static_cast<double>(globalY) - static_cast<double>(winY);
            break;
        }

        case SDL_EVENT_MOUSE_MOTION: {
            const core::Point localCurrent{
                static_cast<double>(event.motion.x),
                static_cast<double>(event.motion.y),
            };

            if (dragClassifier_.IsActive()) {
                const bool wasDragging = dragClassifier_.IsDragging();
                dragClassifier_.Update(localCurrent);

                if (!wasDragging && dragClassifier_.IsDragging()) {
                    // El gesto acaba de calificar como DRAG real (cruzó
                    // el umbral de DragClassifier). Prioridad
                    // DRAG > CLICK > HOVER/AMBIENT > BASE: se aborta
                    // cualquier acción one-shot en curso y se vuelve a
                    // la pose base ESTABLE del estado ACTUAL, para que
                    // el arrastre mueva una imagen quieta en vez de una
                    // animación reproduciéndose (ver DEC-080).
                    const double dragStartMs = static_cast<double>(SDL_GetTicks());
                    if (animController_->CancelActionToCurrentState(dragStartMs)) {
                        MarkNeedsRedraw(dragStartMs);
                        SDL_Log(
                            "nimvlets: drag started -- in-progress action cancelled, holding base pose of state='%s'",
                            animController_->CurrentStateId().c_str());
                    }
                }

                if (dragClassifier_.IsDragging()) {
                    float globalX = 0.0f;
                    float globalY = 0.0f;
                    SDL_GetGlobalMouseState(&globalX, &globalY);
                    const int newWinX = static_cast<int>(std::lround(static_cast<double>(globalX) - dragGrabOffsetX_));
                    const int newWinY = static_cast<int>(std::lround(static_cast<double>(globalY) - dragGrabOffsetY_));
                    SDL_SetWindowPosition(window_, newWinX, newWinY);
                }
                break;
            }

            // Camino EVENT-DRIVEN del click-through: mientras la
            // ventana NO está en modo click-through recibe eventos de
            // mouse reales, así que cada motion resuelve el estado con
            // latencia cero y sin ningún despertar periódico. El
            // muestreo de PollHover() solo hace falta para el caso
            // inverso (ya en click-through, la ventana deja de recibir
            // eventos) -- ver core/ClickThroughPolicy.h.
            UpdateClickThrough(IsPointInsideWindow(localCurrent), IsPointInteractive(localCurrent));
            MaybeTriggerHoverAction(IsPointInteractive(localCurrent), static_cast<double>(SDL_GetTicks()));
            break;
        }

        case SDL_EVENT_WINDOW_MOUSE_ENTER: {
            const CursorSample sample = SampleCursor(window_);
            UpdateClickThrough(IsPointInsideWindow(sample.localPoint), IsPointInteractive(sample.localPoint));
            break;
        }

        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            // El cursor salió del rectángulo: el estado de
            // click-through deja de ser observable (ningún click de ahí
            // puede llegar a esta ventana). Se elige explícitamente NO
            // click-through para recuperar la entrega normal de eventos
            // y poder detectar el próximo ingreso sin encuestar.
            UpdateClickThrough(false, false);
            ResetHoverDwell();
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP: {
            if (event.button.button != SDL_BUTTON_LEFT || !dragClassifier_.IsActive()) {
                break;
            }
            const core::Point localEnd{
                static_cast<double>(event.button.x),
                static_cast<double>(event.button.y),
            };
            const core::PointerGesture gesture = dragClassifier_.End(localEnd);
            const double nowMs = static_cast<double>(SDL_GetTicks());
            // El gesto terminó: la política vuelve a depender solo de
            // dónde está el cursor, así que se re-evalúa de inmediato
            // en vez de esperar al próximo evento/muestra ("releasing
            // restores normal per-pixel behavior", brief §5).
            UpdateClickThrough(IsPointInsideWindow(localEnd), IsPointInteractive(localEnd));
            if (gesture == core::PointerGesture::kClick) {
                ++clickCount_;
                ++appState_.clickBalance;
                persistenceScheduler_.MarkDirty(nowMs);
                animController_->TriggerClick(NextUniformRandom01(), nowMs);
                MarkNeedsRedraw(nowMs);
                RearmAmbientDeadline(nowMs);  // un click es una interacción real -- ver el comentario del campo
                SDL_Log(
                    "nimvlets: click #%d this session (balance: %llu)",
                    clickCount_, static_cast<unsigned long long>(appState_.clickBalance));
            } else {
                int endX = 0;
                int endY = 0;
                SDL_GetWindowPosition(window_, &endX, &endY);
                appState_.lastWindowPosition = persistence::WindowPosition{endX, endY};
                persistenceScheduler_.MarkDirty(nowMs);
                // Al soltar: se resuelve la dirección UNA sola vez
                // contra la posición FINAL (durante el arrastre se
                // suprime -- ver SDL_EVENT_WINDOW_MOVED), se reinicia
                // el dwell de hover desde cero, y el conteo ambient
                // arranca de nuevo completo desde acá.
                UpdateDirectionFromWindowPosition();
                ResetHoverDwell();
                RearmAmbientDeadline(nowMs);
                SDL_Log("nimvlets: drag ended at (%d, %d) (correctly not counted as a click)", endX, endY);
            }
            break;
        }

        default:
            break;
    }
}

int SpikeApp::Run() {
    if (!Init()) {
        Shutdown();
        return 1;
    }

    RunDevSwitchSmokeTestIfRequested();
    RunDevDirectionSmokeTestIfRequested();
    RunDevClickSmokeTestIfRequested();
    RunDevHoverSmokeTestIfRequested();

    bool running = true;

    SDL_Event event;
    while (running && !ShutdownRequested()) {
        const double nowMs = static_cast<double>(SDL_GetTicks());

        double waitMs = 1000.0;  // ver kMaxWaitMs más abajo -- valor inicial, se recorta enseguida
        if (ambientDeadlineMs_) {
            waitMs = *ambientDeadlineMs_ - nowMs;
        }
        if (const std::optional<double> frameDeadline = animController_->NextFrameDeadlineMs()) {
            waitMs = std::min(waitMs, *frameDeadline - nowMs);
        }
        if (const std::optional<double> flushDeadline = persistenceScheduler_.NextFlushDeadlineMs()) {
            waitMs = std::min(waitMs, *flushDeadline - nowMs);
        }
        if (confirmRedrawDeadlineMs_) {
            waitMs = std::min(waitMs, *confirmRedrawDeadlineMs_ - nowMs);
        }
        if (hoverDwellDeadlineMs_) {
            // Ver el comentario del campo en SpikeApp.h -- despierta el
            // loop exactamente cuando un dwell en curso cruzaría el
            // umbral de dwell, incluso sin más eventos de motion reales
            // (el caso de un cursor perfectamente quieto).
            waitMs = std::min(waitMs, *hoverDwellDeadlineMs_ - nowMs);
        }
        if (usingPollDrivenClickThrough_ && clickThroughSamplingActive_) {
            // Solo se acorta la espera mientras el muestreo está
            // realmente armado (cursor DENTRO del rectángulo de la
            // ventana). En reposo -- el caso dominante -- esta rama no
            // corre, así que el loop puede dormir hasta el próximo
            // evento real o deadline de animación, sin ningún despertar
            // a 60Hz. Ver core/ClickThroughPolicy.h y el informe de
            // este bloque para la medición de CPU.
            waitMs = std::min(waitMs, hoverScheduler_.MillisUntilNextFrame(nowMs));
        }
        if (waitMs < 0.0) {
            waitMs = 0.0;
        }
        // Acotado para que un SIGINT/SIGTERM pendiente siempre se note
        // dentro de aproximadamente un segundo -- ver el comentario
        // histórico de Block 02 en docs/ANIMATION_RUNTIME.md.
        constexpr double kMaxWaitMs = 1000.0;
        waitMs = std::min(waitMs, kMaxWaitMs);
        const Sint32 timeoutMs = static_cast<Sint32>(waitMs);

        if (SDL_WaitEventTimeout(&event, timeoutMs)) {
            HandleEvent(event, running);
            while (running && SDL_PollEvent(&event)) {
                HandleEvent(event, running);
            }
        }
        if (!running || ShutdownRequested()) {
            break;
        }

        const double afterMs = static_cast<double>(SDL_GetTicks());

        // Ninguna animación avanza mientras hay un drag REAL en curso
        // (Block 05, corrección de ciclo de vida): el drag tiene la
        // prioridad más alta y arrastra una pose base ESTABLE -- ver
        // MaybeCancelActionForDragStart(). En la práctica esto ya sería
        // un no-op (cancelar deja una pose estática, y Advance() sobre
        // kStatic no hace nada), pero se explicita para que el
        // invariante "cero avance de frame durante el arrastre" no
        // dependa de que el contenido resulte ser estático.
        if (!dragClassifier_.IsDragging() && animController_->Advance(afterMs)) {
            needsRedraw_ = true;
        }

        // Una acción one-shot que TERMINA reinicia el contador ambient
        // completo desde su instante exacto de terminación (Block 05,
        // corrección de ciclo de vida -- ver DEC-079). Esto reemplaza
        // al viejo disparador basado en "cambió CurrentStateId()", que
        // se perdía TODA terminación de self-loop (el click de Bunny/
        // Nidir, howl/tail_greet de Frin: empiezan y terminan en el
        // mismo estado, el id nunca cambia), dejando que la duración de
        // la animación se comiera parte del intervalo ambient.
        // RearmAmbientDeadline() lee el estado ACTUAL del controller,
        // que en este punto ya es el estado post-transición -- así que
        // esto cubre por igual el self-loop y el cambio de estado real.
        if (const std::optional<double> completedMs = animController_->ActionCompletedDuringLastAdvance()) {
            RearmAmbientDeadline(*completedMs);
        }

        // Una transición de BehaviorState real (p. ej. Frin seated ->
        // lying) invalida cualquier dwell de hover acumulado contra el
        // estado ANTERIOR. El rearme del timer ambient ya lo cubre el
        // bloque de terminación de arriba (más preciso: usa el instante
        // real de terminación, no `afterMs`), así que acá solo queda el
        // reset de hover.
        if (animController_->CurrentStateId() != lastKnownStateId_) {
            lastKnownStateId_ = animController_->CurrentStateId();
            ResetHoverDwell();
        }

        if (ambientDeadlineMs_ && afterMs >= *ambientDeadlineMs_) {
            // Solo dispara de verdad si el controller está en modo Base
            // (TriggerAmbientAction() ya es un no-op en cualquier otro
            // caso) -- pero el deadline SIEMPRE se reprograma, para que
            // un deadline que cae durante un click se salte
            // coherentemente en vez de encolarse.
            if (animController_->TriggerAmbientAction(NextUniformRandom01(), afterMs)) {
                MarkNeedsRedraw(afterMs);
            }
            RearmAmbientDeadline(afterMs);
        }

        if (hoverDwellDeadlineMs_ && afterMs >= *hoverDwellDeadlineMs_) {
            // El dwell en curso (si sigue vigente) cruza el umbral
            // justo ahora -- pero no asumimos que el cursor SIGUE
            // encima solo porque nadie mandó un evento de motion
            // nuevo (podría haberse quedado perfectamente quieto
            // encima, o podría haberse ido sin que llegara ningún
            // evento adicional en este camino). Se vuelve a muestrear
            // la posición real -- misma función que ya usa el
            // fallback poll-driven de Windows, ver SampleCursor() -- y
            // se alimenta por el mismo camino que cualquier otra
            // muestra de hover.
            const CursorSample sample = SampleCursor(window_);
            MaybeTriggerHoverAction(IsPointInteractive(sample.localPoint), afterMs);
        }

        if (const std::optional<double> flushDeadline = persistenceScheduler_.NextFlushDeadlineMs();
            flushDeadline && afterMs >= *flushDeadline) {
            FlushPersistedState();
        }

        if (confirmRedrawDeadlineMs_ && afterMs >= *confirmRedrawDeadlineMs_) {
            needsRedraw_ = true;
            confirmRedrawDeadlineMs_.reset();
        }

        if (needsRedraw_) {
#ifndef NDEBUG
            SDL_Log(
                "nimvlets: [diag] animation redraw: mode=%s state='%s' frame=%p t=%.0fms",
                ControllerModeName(animController_->Mode()), animController_->CurrentStateId().c_str(),
                static_cast<const void*>(&animController_->CurrentFrame()), afterMs);
#endif  // NDEBUG
            RenderFrame();
            ApplyCurrentHitMask();
            needsRedraw_ = false;
        }

        if (usingPollDrivenClickThrough_ && clickThroughSamplingActive_ &&
            afterMs >= hoverScheduler_.NextFrameDeadline(afterMs)) {
            PollHover();
        }
    }

    Shutdown();
    return 0;
}

}  // namespace nimvlets::app
