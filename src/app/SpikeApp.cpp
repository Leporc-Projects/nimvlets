#include "app/SpikeApp.h"

#include "catalog/PetCatalogLoader.h"
#include "graphics/FrameTexture.h"
#include "platform/TransparentWindowSupport.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <filesystem>

namespace nimvlets::app {

namespace {

// De dónde se carga el catálogo de pets. Ruta relativa, resuelta desde
// el directorio de trabajo del proceso (mismo precedente que
// kPetPackPath tenía en Block 02/03) — ver docs/CATALOG.md. El único
// string específico de un pet en todo este archivo ya no es
// "bunny_dev" ni ninguna ruta de pack: es esta única ruta de catálogo
// -- qué pack termina cargándose lo decide por completo el contenido
// del catálogo, nunca este código.
constexpr const char* kCatalogPath = "assets/dev/pet_catalog.nvcat";

// Reads NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS — see
// SpikeApp::ComputeEffectivePassiveIntervalSeconds()'s doc comment.
constexpr const char* kDevPassiveIntervalEnvVar = "NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS";

// Lee NIMVLETS_DEV_SWITCH_TEST_COUNT — ver el comentario de
// SpikeApp::RunDevSwitchSmokeTestIfRequested().
constexpr const char* kDevSwitchTestCountEnvVar = "NIMVLETS_DEV_SWITCH_TEST_COUNT";

// Identifica el directorio de app-data por usuario que resuelve
// SDL_GetPrefPath() (ver docs/PERSISTENCE.md, "política de ubicación
// de almacenamiento"). "org" coincide con el "built by Leporc
// Projects" de AGENTS.md; ambos strings contienen solo letras/espacios
// según las reglas de nombrado documentadas por el propio
// SDL_GetPrefPath(), y — según esa misma documentación — nunca deben
// cambiar una vez elegidos, ya que pasan a formar parte de la ruta en
// disco.
constexpr const char* kPrefPathOrg = "Leporc Projects";
constexpr const char* kPrefPathApp = "Nimvlets";

// Override solo-DEV para el directorio de persistencia, reflejando el
// patrón de NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS (Block 02): si se
// setea a una ruta no vacía, esa ruta se usa *en vez de*
// SDL_GetPrefPath() solo para esta ejecución — el comportamiento de
// producción (el caso sin setear) queda completamente igual. Esto es
// lo que permite que la QA manual y los smoke tests automatizados
// ejerciten el camino real de save/load contra un directorio temporal
// aislado en vez de la ubicación real de app-data del usuario — ver
// docs/PERSISTENCE.md.
constexpr const char* kDevAppDataDirEnvVar = "NIMVLETS_DEV_APPDATA_DIR";

// signal-safe: only ever written by the signal handler and read by the
// main loop, both via std::atomic. No allocation or cleanup happens
// inside the handler itself, so it is safe to run at signal-delivery
// time (see ShutdownRequested()'s doc comment for why this exists).
std::atomic<bool> g_shutdownRequested{false};

extern "C" void HandleTerminationSignal(int /*signum*/) {
    g_shutdownRequested.store(true, std::memory_order_relaxed);
}

// One sample of the click-through pipeline's input side: where SDL says
// the cursor is globally, where SDL says our window is, and the local
// point derived from subtracting the two. All three are logged as
// separate instrumented stages by PollHover() — see its doc comment.
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

// Rasterizes an already-built core::AlphaMask (the current animation
// frame's real alpha channel, thresholded per pet_.alphaHitThreshold)
// into an SDL_Surface for SDL_SetWindowShape() — the primary click-
// through mechanism on platforms where
// platform::NativeShapeHitTestIsRenderSafe() is true (currently macOS;
// see that function's doc comment for the SDL-source-level evidence
// behind this).
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

}  // namespace

bool ShutdownRequested() {
    return g_shutdownRequested.load(std::memory_order_relaxed);
}

double SpikeApp::ComputeEffectivePassiveIntervalSeconds() const {
    const char* overrideEnv = std::getenv(kDevPassiveIntervalEnvVar);
    if (overrideEnv == nullptr) {
        return pet_.passiveIntervalSeconds;
    }

    char* end = nullptr;
    const double parsed = std::strtod(overrideEnv, &end);
    if (end == overrideEnv || parsed <= 0.0) {
        SDL_Log(
            "nimvlets: %s='%s' is not a valid positive number; ignoring (using pack default %.1fs)",
            kDevPassiveIntervalEnvVar, overrideEnv, pet_.passiveIntervalSeconds);
        return pet_.passiveIntervalSeconds;
    }

    SDL_Log(
        "nimvlets: DEV override active — %s=%.3fs (pack/production default stays %.1fs; "
        "this only affects this run)",
        kDevPassiveIntervalEnvVar, parsed, pet_.passiveIntervalSeconds);
    return parsed;
}

void SpikeApp::AttachAllTextures() {
    auto attach = [&](content::AnimationDefinition& anim) {
        for (content::FrameDefinition& frame : anim.frames) {
            graphics::AttachFrameTexture(renderer_, frame);
        }
    };
    attach(pet_.idle);
    attach(pet_.clickReaction);
    for (content::AnimationDefinition& passive : pet_.passiveActions) {
        attach(passive);
    }
}

void SpikeApp::ReleaseAllTextures() {
    auto release = [&](content::AnimationDefinition& anim) {
        for (content::FrameDefinition& frame : anim.frames) {
            graphics::ReleaseFrameTexture(frame);
        }
    };
    release(pet_.idle);
    release(pet_.clickReaction);
    for (content::AnimationDefinition& passive : pet_.passiveActions) {
        release(passive);
    }
}

bool SpikeApp::Init() {
    std::signal(SIGINT, HandleTerminationSignal);
    std::signal(SIGTERM, HandleTerminationSignal);

    // Without this, SDL's Cocoa backend calls
    // [NSApp activateIgnoringOtherApps:YES] during startup, which makes
    // the spike steal the foreground/active-app status (and, with it,
    // the menu bar) from whatever the user was using — even though
    // SDL_WINDOW_NOT_FOCUSABLE (below) already stops the *window* from
    // ever becoming key. Found by interactive macOS QA — see
    // docs/PLATFORM_SPIKE.md, "focus behavior". Must be set before
    // SDL_Init().
    SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP, "1");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("nimvlets: SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    // Carga el catálogo *antes* que nada más — fail loud y sale con
    // código no-cero si no carga, igual que el pack de un pet
    // individual en Block 02/03: sin catálogo no hay forma de saber
    // qué pet mostrar en absoluto. Ver docs/CATALOG.md y
    // docs/DECISION_LOG.md.
    std::string catalogError;
    if (!catalog::LoadCatalogFromFile(kCatalogPath, catalog_, catalogError)) {
        SDL_Log("nimvlets: FATAL: could not load pet catalog '%s': %s", kCatalogPath, catalogError.c_str());
        SDL_Log("nimvlets: (run from the repository root, or regenerate it: python3 tools/compile_pet_catalog.py assets/dev/pet_catalog_manifest.json assets/dev/pet_catalog.nvcat)");
        return false;
    }

    // Resuelve el directorio de app-data por usuario y carga cualquier
    // save existente (ver docs/PERSISTENCE.md) *antes* de resolver cuál
    // pet mostrar -- hace falta appState_.activePetId/activeVariantId
    // para eso. A diferencia del catálogo de arriba, un fallo aquí NO
    // es fatal: la persistencia no es necesaria para que la app sea
    // visual/interactivamente funcional, así que esto solo deshabilita
    // load/save para la sesión en vez de abortar el arranque.
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
        // SDL_GetPrefPath() crea el directorio ella misma si hace
        // falta y retorna un string absoluto que siempre se puede
        // liberar — ver su comentario de documentación en
        // <SDL3/SDL_filesystem.h>.
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

    // Resuelve la selección persistida contra el catálogo (block brief
    // §3): calza exactamente, o cae al default. Luego intenta cargar
    // ESE pack; si falla (p. ej. el pet guardado ya no existe en disco,
    // aunque siga listado en el catálogo) y no era ya el default,
    // reintenta una vez con el default -- nunca debe crashear solo
    // porque un pet guardado dejó de estar disponible. Si ambos
    // intentos fallan, recién ahí es un fallo de arranque genuino, sin
    // más fallback posible.
    const catalog::PetIdentity persistedIdentity{appState_.activePetId, appState_.activeVariantId};
    const catalog::ResolvedSelection resolved = catalog::ResolveActiveSelection(catalog_, persistedIdentity);

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
    // algo distinto de lo guardado (primera ejecución, id desconocido,
    // o el pack guardado ya no cargaba) y la marca dirty para que se
    // guarde -- ver docs/CATALOG.md y docs/PERSISTENCE.md.
    if (usedFallback) {
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
        pet_.canvasWidth,
        pet_.canvasHeight,
        flags);
    if (window_ == nullptr) {
        SDL_Log("nimvlets: SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    // Reabre donde el usuario la dejó por última vez (ver
    // docs/PERSISTENCE.md, "last window position") si alguna vez se
    // persistió un drag; si no, usa el default original de centrado al
    // iniciar. Una posición guardada se usa exactamente como se
    // guardó — este bloque no intenta ninguna validación de
    // límites de pantalla/monitor (ver las "limitaciones" del informe
    // de Block 03).
    if (appState_.lastWindowPosition.has_value()) {
        SDL_SetWindowPosition(window_, appState_.lastWindowPosition->x, appState_.lastWindowPosition->y);
    } else {
        SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }

    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (renderer_ == nullptr) {
        SDL_Log("nimvlets: SDL_CreateRenderer failed: %s", SDL_GetError());
        return false;
    }

    // Without this, SDL_LOGICAL_PRESENTATION_DISABLED is the default and
    // render coordinates map 1:1 to physical backbuffer pixels — so on a
    // 2x Retina display our pet's logical canvas coordinates would only
    // cover a quarter of the real backbuffer at half the intended size,
    // instead of filling the window. Found by pixel-inspecting an actual
    // captured frame during Block 01's interactive macOS QA — see
    // docs/PLATFORM_SPIKE.md. LETTERBOX (not STRETCH) because it's
    // correct even if window and canvas logical sizes ever diverge; the
    // letterbox fill color is our own transparent clear color, so no
    // visible bars appear in the square case this block actually uses.
    SDL_SetRenderLogicalPresentation(
        renderer_,
        pet_.canvasWidth,
        pet_.canvasHeight,
        SDL_LOGICAL_PRESENTATION_LETTERBOX);

    platform::ConfigureCompanionWindow(window_);

    AttachAllTextures();

    animController_.emplace(pet_);

    // Hand click-through hit-testing to the platform's own native
    // mechanism where it's safe to do so (see
    // platform::NativeShapeHitTestIsRenderSafe()'s doc comment — this
    // was added after interactive macOS QA proved the poll-driven
    // fallback below unreliable: SDL's own Cocoa backend silently resets
    // NSWindow.ignoresMouseEvents on every real mouse-moved event unless
    // a shape is set). Determined once, up front: ApplyCurrentHitMask()
    // (called just below, and on every later frame change) branches on
    // this flag.
    usingNativeShapeHitTest_ = platform::NativeShapeHitTestIsRenderSafe();
    SDL_Log(
        "nimvlets: click-through mechanism = %s",
        usingNativeShapeHitTest_ ? "native SDL_SetWindowShape (event-driven, no polling)" : "poll-driven fallback");

    // Establish the initial frame + hit-test region before entering the
    // wait loop, mirroring Block 01's precedent of an immediate first
    // frame. needsRedraw_ starts true (see its doc comment) but this
    // call handles it explicitly rather than relying on the loop's first
    // iteration, so the window is never briefly visible-but-unshaped.
    RenderFrame();
    ApplyCurrentHitMask();
    needsRedraw_ = false;

    passiveIntervalSecondsEffective_ = ComputeEffectivePassiveIntervalSeconds();
    nextPassiveDeadlineMs_ = static_cast<double>(SDL_GetTicks()) + passiveIntervalSecondsEffective_ * 1000.0;

    // Objective, non-visual confirmation of high-DPI backing: logical
    // ("point") size vs. actual backbuffer size in pixels. On a Retina
    // display with SDL_WINDOW_HIGH_PIXEL_DENSITY honored, the pixel size
    // should be a whole-number multiple (typically 2x) of the logical
    // size. Logged unconditionally (not just in debug builds) since this
    // is cheap and exactly the kind of fact PLATFORM_SPIKE.md needs
    // without requiring a screenshot.
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

    SDL_Log(
        "nimvlets: pet '%s' (%s) ready — %dx%d canvas, alpha hit threshold=%d/255, "
        "passive action every ~%.0fs. Click the shape; drag to move; close the window to quit. "
        "Click balance: %llu (%s).",
        pet_.id.c_str(), pet_.displayName.c_str(), pet_.canvasWidth, pet_.canvasHeight,
        static_cast<int>(pet_.alphaHitThreshold), passiveIntervalSecondsEffective_,
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
        return false;  // pet_ / animController_ / appState_ intactos -- el pet activo anterior sigue usable
    }

    // Suelta las texturas del pet ANTERIOR antes de reemplazar pet_ --
    // ver docs/CATALOG.md, "recursos". animController_.reset() antes de
    // reasignar pet_ evita que su puntero interno a la animación activa
    // quede colgando si el tamaño de pet_.passiveActions cambia entre
    // el pet viejo y el nuevo (un vector puede reubicar su buffer al
    // reasignarse) -- nunca existe un AnimationController vivo mientras
    // el contenido de pet_ está siendo reemplazado.
    ReleaseAllTextures();
    animController_.reset();
    pet_ = std::move(newPet);
    AttachAllTextures();
    animController_.emplace(pet_);  // arranca en Idle del pet nuevo -- justo lo requerido

    // El tamaño lógico del canvas puede diferir entre pets -- se
    // reaplican ambas llamadas incondicionalmente tras cada switch
    // (baratas, sin costo real cuando el tamaño no cambió) en vez de
    // rastrear si de verdad cambió.
    SDL_SetWindowSize(window_, pet_.canvasWidth, pet_.canvasHeight);
    SDL_SetRenderLogicalPresentation(renderer_, pet_.canvasWidth, pet_.canvasHeight, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    appState_.activePetId = target.petId;
    appState_.activeVariantId = target.variantId;
    persistenceScheduler_.MarkDirty(static_cast<double>(SDL_GetTicks()));
    needsRedraw_ = true;  // el loop principal se encarga de RenderFrame()+ApplyCurrentHitMask() en su próxima vuelta

    SDL_Log(
        "nimvlets: switched active pet to '%s'%s%s ('%s')",
        pet_.id.c_str(), pet_.variantGroup.empty() ? "" : "/", pet_.variantGroup.c_str(), pet_.displayName.c_str());
    return true;
}

void SpikeApp::RunDevSwitchSmokeTestIfRequested() {
    const char* countEnv = std::getenv(kDevSwitchTestCountEnvVar);
    if (countEnv == nullptr || countEnv[0] == '\0') {
        return;  // sin la variable de entorno, esto es un no-op total
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

void SpikeApp::Shutdown() {
    // Flushea primero, antes de desmontar cualquier otra cosa — ver el
    // comentario de FlushPersistedState(): el shutdown limpio siempre
    // escribe lo que quede dirty, sin importar el deadline de
    // debounce.
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

    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
    SDL_RenderClear(renderer_);
    if (texture != nullptr) {
        const SDL_FRect dst{0.0f, 0.0f, static_cast<float>(pet_.canvasWidth), static_cast<float>(pet_.canvasHeight)};
        SDL_RenderTexture(renderer_, texture, nullptr, &dst);
    }
    SDL_RenderPresent(renderer_);
}

void SpikeApp::ApplyCurrentHitMask() {
    const content::FrameDefinition& frame = animController_->CurrentFrame();
    activeHitMask_ = core::AlphaMask::FromAlphaChannel(
        frame.pixels.empty() ? nullptr : frame.pixels.data(),
        frame.width, frame.height,
        pet_.canvasWidth, pet_.canvasHeight,
        pet_.alphaHitThreshold);

    // Only pushed to the OS on platforms where it's render-safe (see
    // usingNativeShapeHitTest_'s doc comment). On the Windows fallback
    // path, activeHitMask_ is simply left updated here for PollHover()'s
    // next scheduled tick to read — no extra native call needed.
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
    // Fallback path only (Windows, or if SDL_SetWindowShape ever fails
    // on macOS) — see usingNativeShapeHitTest_'s doc comment. Runs on
    // its own ~60Hz schedule, independent of animation frame timing —
    // click-through responsiveness and animation cadence are two
    // different concerns with two different (and, for the pet's static
    // idle, very different) natural rates. Still a plain, bounded
    // SDL_WaitEventTimeout wakeup, not a busy-wait, and still a
    // cursor-*position* poll (SDL_GetGlobalMouseState), not a global
    // input hook — see AGENTS.md's privacy rules.
    //
    // The diagnostic logging below (six pipeline stages, transition-only
    // — a value is only printed when it differs from the last-logged
    // value for that stage) was added to debug exactly this mechanism
    // during Block 01's macOS QA — see docs/PLATFORM_SPIKE.md,
    // "click-through instrumentation" — and is compiled out entirely in
    // Release builds (#ifndef NDEBUG) so it can't flood a normal run's
    // log. It stays available in Debug builds for whoever eventually
    // brings up the Windows fallback on real hardware.
    const double nowMs = static_cast<double>(SDL_GetTicks());

    const CursorSample sample = SampleCursor(window_);
    const bool cursorOverOpaque = IsPointInteractive(sample.localPoint);

#ifndef NDEBUG
    // --- instrumented stage 1: global cursor position ---
    if (!diagHasValue_ || sample.globalX != diagGlobalX_ || sample.globalY != diagGlobalY_) {
        SDL_Log("nimvlets: [diag] global cursor position: (%.1f, %.1f)", sample.globalX, sample.globalY);
        diagGlobalX_ = sample.globalX;
        diagGlobalY_ = sample.globalY;
    }
    // --- instrumented stage 2: window position ---
    if (!diagHasValue_ || sample.windowX != diagWindowX_ || sample.windowY != diagWindowY_) {
        SDL_Log("nimvlets: [diag] window position: (%d, %d)", sample.windowX, sample.windowY);
        diagWindowX_ = sample.windowX;
        diagWindowY_ = sample.windowY;
    }
    // --- instrumented stage 3: computed local coordinate ---
    if (!diagHasValue_ || sample.localPoint.x != diagLocalPoint_.x || sample.localPoint.y != diagLocalPoint_.y) {
        SDL_Log("nimvlets: [diag] local coordinate (global - window): (%.2f, %.2f)", sample.localPoint.x, sample.localPoint.y);
        diagLocalPoint_ = sample.localPoint;
    }
    // --- instrumented stage 4: hit-test ---
    if (!diagHasValue_ || cursorOverOpaque != diagContains_) {
        SDL_Log("nimvlets: [diag] IsPointInteractive(local)=%s", cursorOverOpaque ? "true" : "false");
        diagContains_ = cursorOverOpaque;
    }
    diagHasValue_ = true;
#endif  // NDEBUG

    UpdateClickThrough(cursorOverOpaque);

    hoverScheduler_.OnFramePresented(nowMs);
}

void SpikeApp::UpdateClickThrough(bool cursorOverOpaque) {
    // Never toggle click-through away mid-gesture: once the user has
    // pressed on the shape, keep receiving events until they release,
    // regardless of exactly which pixel the cursor is over on any given
    // sample (the window itself is also moving under the cursor during a
    // drag, which would otherwise make this noisy).
    const bool shouldBeClickThrough = dragClassifier_.IsActive() ? false : !cursorOverOpaque;

#ifndef NDEBUG
    // --- instrumented stage 5: requested click-through state ---
    if (shouldBeClickThrough != diagRequestedClickThrough_ || !diagHasValue_) {
        SDL_Log("nimvlets: [diag] requested click-through=%s", shouldBeClickThrough ? "true" : "false");
        diagRequestedClickThrough_ = shouldBeClickThrough;
    }
#endif  // NDEBUG

    if (shouldBeClickThrough != currentlyClickThrough_) {
        const bool actual = platform::SetWindowClickThrough(window_, shouldBeClickThrough);
        currentlyClickThrough_ = shouldBeClickThrough;

#ifndef NDEBUG
        // --- instrumented stage 6: NSWindow.ignoresMouseEvents, read
        // back immediately after setting it — this is the ground truth,
        // not an assumption that the assignment stuck.
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

        // The OS is asking us to repaint (e.g. another window that was
        // covering ours moved away, or we were minimized/restored) —
        // "platform/render semantics require it", independent of
        // whether the pet's animation state changed at all. See
        // needsRedraw_'s doc comment.
        case SDL_EVENT_WINDOW_EXPOSED:
            needsRedraw_ = true;
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            if (event.button.button != SDL_BUTTON_LEFT) {
                break;
            }
            const core::Point localOrigin{
                static_cast<double>(event.button.x),
                static_cast<double>(event.button.y),
            };

            // Defense in depth alongside the click-through mechanism
            // (native shape hit-test on macOS, poll-driven fallback
            // elsewhere): never start a click/drag gesture for a press
            // that isn't actually on the visible region. Only the
            // opaque/interactive region is interactive — see
            // docs/ANIMATION_RUNTIME.md.
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
            if (!dragClassifier_.IsActive()) {
                break;
            }
            const core::Point localCurrent{
                static_cast<double>(event.motion.x),
                static_cast<double>(event.motion.y),
            };
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
                // Click counting is unconditional and entirely separate
                // from the visual reaction: it always increments, even
                // if a click reaction is already playing and
                // TriggerClick() below is therefore a no-op for
                // *animation* state (see AnimationController's doc
                // comment). Repeated clicks during an active reaction
                // count but never restart the visual.
                //
                // clickCount_ (solo esta sesión) y appState_.clickBalance
                // (persistido, acumulado entre sesiones — ver
                // docs/PERSISTENCE.md) son deliberadamente dos
                // contadores separados, no un campo reutilizado: uno
                // es un diagnóstico, el otro es la moneda real del
                // producto (ver AGENTS.md §2).
                ++clickCount_;
                ++appState_.clickBalance;
                persistenceScheduler_.MarkDirty(nowMs);
                animController_->TriggerClick(nowMs);
                needsRedraw_ = true;
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

    // No-op salvo que NIMVLETS_DEV_SWITCH_TEST_COUNT esté seteada — ver
    // el comentario del método. Corre una sola vez, sincrónicamente,
    // antes de entrar al loop principal (no agrega ningún polling).
    RunDevSwitchSmokeTestIfRequested();

    bool running = true;

    SDL_Event event;
    while (running && !ShutdownRequested()) {
        const double nowMs = static_cast<double>(SDL_GetTicks());

        // La espera está acotada por el deadline de acción pasiva (a
        // lo sumo ~300s por defecto), luego se ajusta según cuál de
        // estos esté en juego: el propio deadline de siguiente frame
        // de la animación (nullopt mientras el idle es estático — ver
        // AnimationController::NextFrameDeadlineMs()), el deadline de
        // flush de persistencia pendiente (nullopt salvo que algo esté
        // realmente dirty — ver persistence::PersistenceScheduler y
        // docs/PERSISTENCE.md), y el schedule de poll de hover
        // (fallback de Windows) — y después se acota a kMaxWaitMs más
        // abajo, únicamente por capacidad de respuesta ante señales de
        // shutdown (ver su propio comentario). Un idle verdaderamente
        // estático sin nada pendiente que guardar sigue sin hacer
        // ningún trabajo de redraw/hit-mask/disco entre despertares:
        // este es el mecanismo detrás de la mejora de CPU en idle
        // estático de Block 02 sobre el tick fijo de ~12fps de
        // Block 01 — ver docs/ANIMATION_RUNTIME.md y
        // docs/PERFORMANCE_BUDGETS.md — y el deadline de persistencia
        // de Block 03 reutiliza exactamente el mismo mecanismo en vez
        // de agregar ningún polling propio. Los eventos de input
        // reales (incluyendo los eventos de mouse-moved que el propio
        // backend de Cocoa de SDL necesita para mantener correcto el
        // click-through) despiertan SDL_WaitEventTimeout de inmediato
        // sin importar cuán largo sea este timeout; el timeout solo
        // acota cuánto tiempo *nosotros* bloqueamos cuando no pasa
        // nada.
        double waitMs = nextPassiveDeadlineMs_ - nowMs;
        if (const std::optional<double> frameDeadline = animController_->NextFrameDeadlineMs()) {
            waitMs = std::min(waitMs, *frameDeadline - nowMs);
        }
        if (const std::optional<double> flushDeadline = persistenceScheduler_.NextFlushDeadlineMs()) {
            waitMs = std::min(waitMs, *flushDeadline - nowMs);
        }
        if (!usingNativeShapeHitTest_) {
            waitMs = std::min(waitMs, hoverScheduler_.MillisUntilNextFrame(nowMs));
        }
        if (waitMs < 0.0) {
            waitMs = 0.0;
        }
        // Acotado para que un SIGINT/SIGTERM pendiente siempre se note
        // dentro de aproximadamente un segundo, incluso durante un
        // tramo de idle verdaderamente estático sin nada más programado
        // por minutos (ver el comentario de ShutdownRequested() y la
        // nota de "capacidad de respuesta ante shutdown" en
        // docs/PERSISTENCE.md) — encontrado por los propios smoke tests
        // no interactivos de este bloque: una señal no interrumpe por
        // sí misma un SDL_WaitEventTimeout bloqueante en esta
        // plataforma, así que sin este cap el loop solo volvería a
        // chequear ShutdownRequested() cuando finalmente se alcanzara
        // el *próximo* deadline real (hasta el intervalo pasivo de
        // ~300s). Despertar una vez por segundo y no hacer nada más que
        // volver a chequear un puñado de condiciones ya baratas antes
        // de volver a dormir no es la regresión de "sin busy-wait"/
        // "static-idle sleeping" que este bloque debe evitar — no hay
        // redraw, no hay reconstrucción de hit-mask, no hay I/O de
        // disco en un despertar salvo que haya llegado realmente un
        // deadline; ver docs/PERFORMANCE_BUDGETS.md, que reconfirma que
        // el CPU en idle no se ve afectado por este cap.
        constexpr double kMaxWaitMs = 1000.0;
        waitMs = std::min(waitMs, kMaxWaitMs);
        const Sint32 timeoutMs = static_cast<Sint32>(waitMs);

        // Blocks (no busy-wait) until either a real input/window event
        // arrives or the next scheduled deadline is due. A pending
        // SIGINT/SIGTERM is picked up at most one timeout period later
        // (see ShutdownRequested()).
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

        if (afterMs >= nextPassiveDeadlineMs_) {
            // Only actually triggers while idle (see
            // AnimationController::TriggerPassiveAction()'s doc comment:
            // passive action never interrupts anything) — but the
            // deadline is always rescheduled either way, so a passive
            // action that arrives mid-click-reaction is simply skipped
            // for this cycle rather than queued or fired late.
            if (animController_->State() == content::ControllerState::kIdle && !pet_.passiveActions.empty()) {
                animController_->TriggerPassiveAction(nextPassiveActionIndex_, afterMs);
                nextPassiveActionIndex_ = (nextPassiveActionIndex_ + 1) % pet_.passiveActions.size();
                needsRedraw_ = true;
            }
            nextPassiveDeadlineMs_ = afterMs + passiveIntervalSecondsEffective_ * 1000.0;
        }

        if (const std::optional<double> flushDeadline = persistenceScheduler_.NextFlushDeadlineMs();
            flushDeadline && afterMs >= *flushDeadline) {
            FlushPersistedState();
        }

        if (needsRedraw_) {
#ifndef NDEBUG
            // Transition-only diagnostic for the animation lifecycle (state
            // machine + scheduler) — see docs/ANIMATION_RUNTIME.md §3 and
            // the Block 02 report's "passive action technical verification"
            // section. Fires only when a redraw is actually about to
            // happen, i.e. only on a real state/frame change — never on a
            // fixed cadence, same "log transitions, not every tick"
            // discipline as PollHover()'s click-through instrumentation
            // (see its doc comment). Logs the current frame's own address
            // (not its pixel content) as a cheap, allocation-free way to
            // show *which distinct frame* is on screen across consecutive
            // log lines, without adding a new public accessor to
            // AnimationController just for this.
            const char* stateName = "Idle";
            if (animController_->State() == content::ControllerState::kClickReaction) {
                stateName = "ClickReaction";
            } else if (animController_->State() == content::ControllerState::kPassiveAction) {
                stateName = "PassiveAction";
            }
            SDL_Log(
                "nimvlets: [diag] animation redraw: state=%s frame=%p t=%.0fms",
                stateName, static_cast<const void*>(&animController_->CurrentFrame()), afterMs);
#endif  // NDEBUG
            RenderFrame();
            ApplyCurrentHitMask();
            needsRedraw_ = false;
        }

        if (!usingNativeShapeHitTest_ && afterMs >= hoverScheduler_.NextFrameDeadline(afterMs)) {
            PollHover();
        }
    }

    Shutdown();
    return 0;
}

}  // namespace nimvlets::app
