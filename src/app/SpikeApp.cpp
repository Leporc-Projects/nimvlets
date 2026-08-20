#include "app/SpikeApp.h"

#include "catalog/PetCatalogLoader.h"
#include "graphics/FrameTexture.h"
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

// TEMP diagnostic-only, Block 05 Bunny root-cause investigation: si
// está seteada a un directorio, cada RenderFrame() real vuelca los
// pixeles YA COMPUESTOS por el renderer (SDL_RenderReadPixels, lo que
// de verdad se presentó) a un .rgba crudo -- evidencia directa de la
// etapa E (textura/render) contra el pipeline real corriendo, sin
// captura de pantalla (lee el backbuffer de NUESTRO propio renderer,
// nunca el escritorio). Ver el informe final para lo que esto encontró
// y sus límites honestos.
constexpr const char* kDevDumpFramesDirEnvVar = "NIMVLETS_DEV_DUMP_FRAMES_DIR";

// Separación de wall-clock del redraw de confirmación (ver el
// comentario de confirmRedrawDeadlineMs_ en SpikeApp.h). 120ms es
// perceptualmente instantáneo pero suficiente separación real de
// tiempo como para que el compositor/GPU tenga una oportunidad genuina
// de asentarse entre presents.
constexpr double kConfirmRedrawDelayMs = 120.0;

// Cooldown INDEPENDIENTE del disparo por hover (Block 05 -- ver el
// comentario de MaybeTriggerHoverAction()). Deliberadamente chico y
// desacoplado del intervalo ambient (15s, dato por-pet) -- el owner
// pidió explícitamente que hover no quede bloqueado por el timer
// ambient. Valor genérico, no por-pet: evita que un hover
// entrando/saliendo repetidamente en el borde exacto del hit-mask
// dispare una acción nueva en cada flanco.
constexpr double kHoverCooldownSeconds = 2.0;

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

void SpikeApp::AttachAllTextures() {
    auto attach = [&](content::AnimationDefinition& anim) {
        for (content::FrameDefinition& frame : anim.frames) {
            graphics::AttachFrameTexture(renderer_, frame);
        }
    };
    auto attachOverrides = [&](std::vector<content::DirectionalAnimationOverride>& overrides) {
        for (content::DirectionalAnimationOverride& override_ : overrides) {
            attach(override_.animation);
        }
    };
    auto attachActions = [&](std::vector<content::WeightedAction>& actions) {
        for (content::WeightedAction& action : actions) {
            attach(action.animation);
            attachOverrides(action.directionOverrides);
        }
    };

    for (content::BehaviorState& state : pet_.states) {
        attach(state.baseAnimation);
        attachOverrides(state.baseAnimationDirectionOverrides);
        attachActions(state.ambientActions);
        attachActions(state.hoverActions);
        attachActions(state.clickActions);
    }
}

void SpikeApp::ReleaseAllTextures() {
    auto release = [&](content::AnimationDefinition& anim) {
        for (content::FrameDefinition& frame : anim.frames) {
            graphics::ReleaseFrameTexture(frame);
        }
    };
    auto releaseOverrides = [&](std::vector<content::DirectionalAnimationOverride>& overrides) {
        for (content::DirectionalAnimationOverride& override_ : overrides) {
            release(override_.animation);
        }
    };
    auto releaseActions = [&](std::vector<content::WeightedAction>& actions) {
        for (content::WeightedAction& action : actions) {
            release(action.animation);
            releaseOverrides(action.directionOverrides);
        }
    };

    for (content::BehaviorState& state : pet_.states) {
        release(state.baseAnimation);
        releaseOverrides(state.baseAnimationDirectionOverrides);
        releaseActions(state.ambientActions);
        releaseActions(state.hoverActions);
        releaseActions(state.clickActions);
    }
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

    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (renderer_ == nullptr) {
        SDL_Log("nimvlets: SDL_CreateRenderer failed: %s", SDL_GetError());
        return false;
    }

    SDL_SetRenderLogicalPresentation(
        renderer_,
        EffectiveCanvasWidth(),
        EffectiveCanvasHeight(),
        SDL_LOGICAL_PRESENTATION_LETTERBOX);

    platform::ConfigureCompanionWindow(window_);

    AttachAllTextures();

    animController_.emplace(pet_);
    lastKnownStateId_ = animController_->CurrentStateId();

    UpdateDirectionFromWindowPosition();

    usingNativeShapeHitTest_ = platform::NativeShapeHitTestIsRenderSafe();
    usingPollDrivenClickThrough_ = !usingNativeShapeHitTest_ && platform::ClickThroughPollingIsMeaningful();
    SDL_Log(
        "nimvlets: click-through mechanism = %s",
        usingNativeShapeHitTest_
            ? "native SDL_SetWindowShape (event-driven, no polling)"
            : (usingPollDrivenClickThrough_
                   ? "poll-driven fallback"
                   : "none available (see docs/LINUX_PLATFORM.md) -- relying on IsPointInteractive() app-side gating only"));

    RenderFrame();
    ApplyCurrentHitMask();
    needsRedraw_ = false;

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

    ReleaseAllTextures();
    animController_.reset();
    pet_ = std::move(newPet);
    AttachAllTextures();
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
        return;
    }
    if (!hoverPassiveGate_.Update(cursorOverOpaque)) {
        return;  // no es un flanco de subida -- nada que hacer
    }
    if (nowMs < hoverCooldownUntilMs_) {
        // Cooldown INDEPENDIENTE del timer ambient (Block 05) -- ver el
        // comentario de este método en SpikeApp.h. Nunca consulta
        // ambientDeadlineMs_.
        return;
    }

    if (animController_->TriggerHoverAction(NextUniformRandom01(), nowMs)) {
        hoverCooldownUntilMs_ = nowMs + kHoverCooldownSeconds * 1000.0;
        MarkNeedsRedraw(nowMs);
        SDL_Log("nimvlets: hover-triggered action (state='%s')", animController_->CurrentStateId().c_str());
    }
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

    // Tiempo SIMULADO, no wall-clock real. Igual que antes: el PRIMER
    // ciclo con el cooldown ya vencido dispara de verdad; los
    // siguientes deben quedar en no-op -- ahora porque el controller
    // queda en modo AmbientOrHoverAction (nada lo avanza de vuelta a
    // kBase dentro de esta corrida sincrónica), no porque el cooldown
    // independiente siga vigente. Ese patrón ("dispara una vez, después
    // se queda callado") sigue siendo la observación correcta: prueba
    // que el hover nunca hace spam de acciones seguidas.
    double simulatedNowMs = static_cast<double>(SDL_GetTicks());
    hoverCooldownUntilMs_ = simulatedNowMs;  // vencido de entrada
    const double shortStepMs = kHoverCooldownSeconds * 1000.0 / 4.0;
    const double longStepMs = kHoverCooldownSeconds * 1000.0 + 1.0;
    for (std::size_t i = 0; i < count; ++i) {
        simulatedNowMs += (i % 4 == 0) ? longStepMs : shortStepMs;
        MaybeTriggerHoverAction(true, simulatedNowMs);
        simulatedNowMs += 1.0;
        MaybeTriggerHoverAction(false, simulatedNowMs);
    }

    // Restaura el cooldown a un valor real de wall-clock antes de
    // devolver el control.
    hoverCooldownUntilMs_ = static_cast<double>(SDL_GetTicks());

    SDL_Log("nimvlets: DEV hover smoke test complete — %zu hover cycle(s) requested", count);
}

void SpikeApp::Shutdown() {
    FlushPersistedState();

    ReleaseAllTextures();
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

void SpikeApp::RenderFrame() {
    const content::FrameDefinition& frame = animController_->CurrentFrame();
    SDL_Texture* texture = static_cast<SDL_Texture*>(frame.rendererHandle);

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
                "rendering fully transparent; check AttachAllTextures() coverage",
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

    UpdateClickThrough(cursorOverOpaque);
    MaybeTriggerHoverAction(cursorOverOpaque, nowMs);

    hoverScheduler_.OnFramePresented(nowMs);
}

void SpikeApp::UpdateClickThrough(bool cursorOverOpaque) {
    const bool shouldBeClickThrough = dragClassifier_.IsActive() ? false : !cursorOverOpaque;

#ifndef NDEBUG
    if (shouldBeClickThrough != diagRequestedClickThrough_ || !diagHasValue_) {
        SDL_Log("nimvlets: [diag] requested click-through=%s", shouldBeClickThrough ? "true" : "false");
        diagRequestedClickThrough_ = shouldBeClickThrough;
    }
#endif  // NDEBUG

    if (shouldBeClickThrough != currentlyClickThrough_) {
        const bool actual = platform::SetWindowClickThrough(window_, shouldBeClickThrough);
        currentlyClickThrough_ = shouldBeClickThrough;

#ifndef NDEBUG
        if (actual != diagActualIgnoresMouseEvents_ || !diagHasValue_) {
            SDL_Log(
                "nimvlets: [diag] NSWindow.ignoresMouseEvents actual=%s%s",
                actual ? "true" : "false",
                actual == shouldBeClickThrough ? "" : "  <-- MISMATCH vs. requested value!");
            diagActualIgnoresMouseEvents_ = actual;
        }
#else
        (void)actual;
#endif  // NDEBUG
    }
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
            UpdateDirectionFromWindowPosition();
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
                dragClassifier_.Update(localCurrent);

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

            MaybeTriggerHoverAction(IsPointInteractive(localCurrent), static_cast<double>(SDL_GetTicks()));
            break;
        }

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
            if (gesture == core::PointerGesture::kClick) {
                ++clickCount_;
                ++appState_.clickBalance;
                persistenceScheduler_.MarkDirty(nowMs);
                animController_->TriggerClick(NextUniformRandom01(), nowMs);
                MarkNeedsRedraw(nowMs);
                SDL_Log(
                    "nimvlets: click #%d this session (balance: %llu)",
                    clickCount_, static_cast<unsigned long long>(appState_.clickBalance));
            } else {
                int endX = 0;
                int endY = 0;
                SDL_GetWindowPosition(window_, &endX, &endY);
                appState_.lastWindowPosition = persistence::WindowPosition{endX, endY};
                persistenceScheduler_.MarkDirty(nowMs);
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
        if (usingPollDrivenClickThrough_) {
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

        if (animController_->Advance(afterMs)) {
            needsRedraw_ = true;
        }

        // Detecta una transición de BehaviorState real (por cualquier
        // camino: ambient/hover/click terminando un one-shot, o un
        // switch de pet) y rearma ambientDeadlineMs_ para el estado
        // NUEVO -- ver el comentario de RearmAmbientDeadline(). Un
        // simple string compare, barato, sobre ids de 1-3 estados como
        // mucho por pet.
        if (animController_->CurrentStateId() != lastKnownStateId_) {
            lastKnownStateId_ = animController_->CurrentStateId();
            RearmAmbientDeadline(afterMs);
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

        if (usingPollDrivenClickThrough_ && afterMs >= hoverScheduler_.NextFrameDeadline(afterMs)) {
            PollHover();
        }
    }

    Shutdown();
    return 0;
}

}  // namespace nimvlets::app
