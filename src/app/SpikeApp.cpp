#include "app/SpikeApp.h"

#include "content/PetPackLoader.h"
#include "graphics/FrameTexture.h"
#include "platform/TransparentWindowSupport.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <csignal>
#include <cstdlib>

namespace nimvlets::app {

namespace {

// Where the one pet this runtime shows is loaded from. A relative path,
// resolved from the process's current working directory (matching
// Block 01's asset-loading precedent) — see docs/ANIMATION_RUNTIME.md,
// "running the spike", for the "run from the repo root" requirement this
// implies. Deliberately the *only* pet-specific string literal in this
// entire file: swapping it swaps the pet, with zero other code changes.
constexpr const char* kPetPackPath = "assets/dev/bunny_pack.nvpack";

// Reads NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS — see
// SpikeApp::ComputeEffectivePassiveIntervalSeconds()'s doc comment.
constexpr const char* kDevPassiveIntervalEnvVar = "NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS";

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

    // Load the one pet this runtime shows *before* creating any window —
    // fail loudly and exit non-zero if it can't be loaded, rather than
    // falling back to a hardcoded analytic shape. A data-driven runtime
    // with no content to show is a real problem to surface, not paper
    // over — matching the asset pipeline's own "fail loudly" contract
    // (tools/compile_pet_pack.py) applied consistently at the app level.
    // See docs/ANIMATION_RUNTIME.md and docs/DECISION_LOG.md.
    std::string loadError;
    if (!content::LoadPetPackFromFile(kPetPackPath, pet_, loadError)) {
        SDL_Log("nimvlets: FATAL: could not load pet pack '%s': %s", kPetPackPath, loadError.c_str());
        SDL_Log("nimvlets: (run from the repository root, or regenerate it: python3 tools/generate_bunny_dev_pack.py)");
        return false;
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

    SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

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
        "Clicks are counted in-memory only (stdout log), not persisted.",
        pet_.id.c_str(), pet_.displayName.c_str(), pet_.canvasWidth, pet_.canvasHeight,
        static_cast<int>(pet_.alphaHitThreshold), passiveIntervalSecondsEffective_);

    return true;
}

void SpikeApp::Shutdown() {
    ReleaseAllTextures();
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Log("nimvlets: clean shutdown, %d click(s) recorded this session", clickCount_);
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
            if (gesture == core::PointerGesture::kClick) {
                // Click counting is unconditional and entirely separate
                // from the visual reaction: it always increments, even
                // if a click reaction is already playing and
                // TriggerClick() below is therefore a no-op for
                // *animation* state (see AnimationController's doc
                // comment). Repeated clicks during an active reaction
                // count but never restart the visual.
                ++clickCount_;
                const double nowMs = static_cast<double>(SDL_GetTicks());
                animController_->TriggerClick(nowMs);
                needsRedraw_ = true;
                SDL_Log("nimvlets: click #%d (in-memory only, not persisted)", clickCount_);
            } else {
                SDL_Log("nimvlets: drag ended (correctly not counted as a click)");
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

    bool running = true;

    SDL_Event event;
    while (running && !ShutdownRequested()) {
        const double nowMs = static_cast<double>(SDL_GetTicks());

        // The wait is always bounded by the passive-action deadline (at
        // most ~300s away by default), then tightened by whichever of
        // the animation's own next-frame deadline (nullopt while idle is
        // static — see AnimationController::NextFrameDeadlineMs()) and
        // the Windows-fallback hover-poll schedule are actually in play.
        // A truly static idle with a distant passive deadline can block
        // here for minutes at a stretch: this is the mechanism behind
        // Block 02's static-idle CPU improvement over Block 01's fixed
        // ~12fps tick — see docs/ANIMATION_RUNTIME.md and
        // docs/PERFORMANCE_BUDGETS.md. Real input events (including the
        // mouse-moved events SDL's own Cocoa backend needs to keep
        // click-through correct) wake SDL_WaitEventTimeout immediately
        // regardless of how long this timeout is; the timeout only
        // bounds how long *we* block when nothing happens.
        double waitMs = nextPassiveDeadlineMs_ - nowMs;
        if (const std::optional<double> frameDeadline = animController_->NextFrameDeadlineMs()) {
            waitMs = std::min(waitMs, *frameDeadline - nowMs);
        }
        if (!usingNativeShapeHitTest_) {
            waitMs = std::min(waitMs, hoverScheduler_.MillisUntilNextFrame(nowMs));
        }
        if (waitMs < 0.0) {
            waitMs = 0.0;
        }
        // Defensive cap (well above any real deadline this app computes,
        // including the default ~300s passive interval) so an
        // unreasonable DEV override can't overflow the Sint32 timeout
        // SDL_WaitEventTimeout takes.
        waitMs = std::min(waitMs, 2'000'000'000.0);
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
