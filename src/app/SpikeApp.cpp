#include "app/SpikeApp.h"

#include "platform/TransparentWindowSupport.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <csignal>

namespace nimvlets::app {

namespace {

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

// Rasterizes an already-built core::AlphaMask (either from the Bunny QA
// fixture's real alpha channel, or — failing that — from
// core::BlobSilhouette at rest) into an SDL_Surface for
// SDL_SetWindowShape() — the primary click-through mechanism on
// platforms where platform::NativeShapeHitTestIsRenderSafe() is true
// (currently macOS; see that function's doc comment for the SDL-source-
// level evidence behind this).
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

// Rasterizes core::BlobSilhouette::Contains() (at rest, phase=0) into a
// core::AlphaMask — the fallback hit-test source used only when the
// Bunny QA fixture can't be loaded (see SpikeApp::Init()). Not
// re-rasterized per animation frame; the hit region can lag the ~4px
// idle head-bob by a few pixels at the very top of the head when this
// fallback is active. A documented, accepted limitation for this spike,
// not attempted to be pixel-perfect — see docs/PLATFORM_SPIKE.md.
core::AlphaMask RasterizeBlobIntoAlphaMask(const core::BlobSilhouette& blob, int width, int height) {
    core::AlphaMask mask(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const core::Point sample{static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5};
            mask.SetOpaque(x, y, blob.Contains(sample, /*phaseSeconds=*/0.0));
        }
    }
    return mask;
}

}  // namespace

bool ShutdownRequested() {
    return g_shutdownRequested.load(std::memory_order_relaxed);
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

    const SDL_WindowFlags flags =
        SDL_WINDOW_TRANSPARENT |
        SDL_WINDOW_BORDERLESS |
        SDL_WINDOW_ALWAYS_ON_TOP |
        SDL_WINDOW_UTILITY |
        SDL_WINDOW_NOT_FOCUSABLE |
        SDL_WINDOW_HIGH_PIXEL_DENSITY;

    window_ = SDL_CreateWindow(
        "Nimvlets Foundation Spike",
        static_cast<int>(blob_.windowWidth),
        static_cast<int>(blob_.windowHeight),
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
    // 2x Retina display our BlobSilhouette's logical 160x160 coordinates
    // (see core/Silhouette.h; deliberately DPI-independent) would only
    // cover the top-left quarter of the real 320x320 backbuffer at half
    // the intended size, instead of filling the window. Found by pixel-
    // inspecting an actual captured frame during interactive macOS QA —
    // see docs/PLATFORM_SPIKE.md. LETTERBOX (not STRETCH) because it's
    // correct even if window and blob logical sizes ever diverge; the
    // letterbox fill color is our own transparent clear color, so no
    // visible bars appear in the square case this block actually uses.
    SDL_SetRenderLogicalPresentation(
        renderer_,
        static_cast<int>(blob_.windowWidth),
        static_cast<int>(blob_.windowHeight),
        SDL_LOGICAL_PRESENTATION_LETTERBOX);

    platform::ConfigureCompanionWindow(window_);

    // Block 01 closure QA fixture: try to load "Bunny" — a realistic,
    // non-analytic asset used to validate transparency/hit-testing
    // against real alpha data (see docs/DECISION_LOG.md). Falls back to
    // the analytic placeholder shape (unchanged, still exercised by
    // tests/) if the fixture can't be loaded — e.g. if the process
    // isn't run from the repo root, where this relative path resolves
    // from. Not a content system; see graphics::DevSprite's doc
    // comment.
    static const char* const kBunnyFixturePath = "assets/dev/bunny.rgba";
    if (bunnySprite_.LoadFromFile(kBunnyFixturePath)) {
        bunnyTexture_ = bunnySprite_.CreateTexture(renderer_);
    }
    const int windowW = static_cast<int>(blob_.windowWidth);
    const int windowH = static_cast<int>(blob_.windowHeight);
    if (bunnyTexture_ != nullptr) {
        activeHitMask_ = bunnySprite_.BuildAlphaMask(windowW, windowH);
        SDL_Log(
            "nimvlets: QA fixture loaded: %s (%dx%d native, hit-test alpha threshold=%d/255)",
            kBunnyFixturePath, bunnySprite_.Width(), bunnySprite_.Height(),
            static_cast<int>(graphics::DevSprite::kHitTestAlphaThreshold));
    } else {
        activeHitMask_ = RasterizeBlobIntoAlphaMask(blob_, windowW, windowH);
        SDL_Log(
            "nimvlets: QA fixture %s not loaded; rendering the analytic placeholder shape instead",
            kBunnyFixturePath);
    }

    // Hand click-through hit-testing to the platform's own native
    // mechanism where it's safe to do so (see
    // platform::NativeShapeHitTestIsRenderSafe()'s doc comment — this
    // was added after interactive macOS QA proved the poll-driven
    // fallback below unreliable: SDL's own Cocoa backend silently resets
    // NSWindow.ignoresMouseEvents on every real mouse-moved event unless
    // a shape is set). SDL_SetWindowShape() copies the surface
    // internally, so it's destroyed right after the call either way.
    if (platform::NativeShapeHitTestIsRenderSafe()) {
        SDL_Surface* hitTestShape = BuildHitTestShapeSurface(activeHitMask_);
        if (hitTestShape != nullptr) {
            if (SDL_SetWindowShape(window_, hitTestShape)) {
                usingNativeShapeHitTest_ = true;
            } else {
                SDL_Log("nimvlets: SDL_SetWindowShape failed: %s (falling back to poll-driven click-through)", SDL_GetError());
            }
            SDL_DestroySurface(hitTestShape);
        }
    }
    SDL_Log(
        "nimvlets: click-through mechanism = %s",
        usingNativeShapeHitTest_ ? "native SDL_SetWindowShape (event-driven, no polling)" : "poll-driven fallback");

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
        "nimvlets: spike window ready (%dx%d @ ~%.0f fps idle). Click the shape; drag to move; "
        "close the window to quit. Clicks are counted in-memory only (stdout log), not persisted.",
        static_cast<int>(blob_.windowWidth),
        static_cast<int>(blob_.windowHeight),
        1000.0 / frameScheduler_.FrameIntervalMs());

    return true;
}

void SpikeApp::Shutdown() {
    if (bunnyTexture_ != nullptr) {
        SDL_DestroyTexture(bunnyTexture_);
        bunnyTexture_ = nullptr;
    }
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
    const double nowMs = static_cast<double>(SDL_GetTicks());
    const double phaseSeconds = nowMs / 1000.0;

    if (bunnyTexture_ != nullptr) {
        // Bunny is a static QA fixture (no animation frames — see
        // graphics::DevSprite's doc comment), so this redraws the same
        // texture every tick. Clear-then-draw-then-present mirrors
        // graphics::BlobRenderer::Render()'s own sequence.
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
        SDL_RenderClear(renderer_);
        const SDL_FRect dst{0.0f, 0.0f, static_cast<float>(blob_.windowWidth), static_cast<float>(blob_.windowHeight)};
        SDL_RenderTexture(renderer_, bunnyTexture_, nullptr, &dst);
        SDL_RenderPresent(renderer_);
    } else {
        blobRenderer_.Render(renderer_, blob_, phaseSeconds);
    }
    frameScheduler_.OnFramePresented(nowMs);
}

void SpikeApp::PollHover() {
    // Fallback path only (Windows, or if SDL_SetWindowShape ever fails
    // on macOS) — see usingNativeShapeHitTest_'s doc comment. Originally
    // piggybacked on the ~83ms (12 fps) idle-animation tick; moved to
    // its own ~60Hz schedule after interactive macOS QA found that lag
    // made click-through unreliable there (a click landing shortly
    // after the cursor left the shape could see a stale
    // ignoresMouseEvents=NO). Still a plain, bounded
    // SDL_WaitEventTimeout wakeup, not a busy-wait, and still a
    // cursor-*position* poll (SDL_GetGlobalMouseState), not a global
    // input hook — see AGENTS.md's privacy rules.
    //
    // The diagnostic logging below (six pipeline stages, transition-only
    // — a value is only printed when it differs from the last-logged
    // value for that stage) was added to debug exactly this mechanism
    // during macOS QA — see docs/PLATFORM_SPIKE.md, "click-through
    // instrumentation" — and is compiled out entirely in Release builds
    // (#ifndef NDEBUG) so it can't flood a normal run's log, per that
    // section's closing note. It stays available in Debug builds for
    // whoever eventually brings up the Windows fallback on real
    // hardware.
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
            // docs/PLATFORM_SPIKE.md.
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
                ++clickCount_;
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
    RenderFrame();  // first frame, presented immediately
    if (!usingNativeShapeHitTest_) {
        PollHover();  // establish correct initial click-through state immediately too
    }

    SDL_Event event;
    while (running && !ShutdownRequested()) {
        const double nowMs = static_cast<double>(SDL_GetTicks());
        // The hover-poll fallback isn't part of the wait calculation at
        // all when usingNativeShapeHitTest_ — no extra wakeups are
        // introduced beyond the idle-animation tick on macOS.
        const double waitMs = usingNativeShapeHitTest_
            ? frameScheduler_.MillisUntilNextFrame(nowMs)
            : std::min(frameScheduler_.MillisUntilNextFrame(nowMs), hoverScheduler_.MillisUntilNextFrame(nowMs));
        const Sint32 timeoutMs = static_cast<Sint32>(waitMs > 0.0 ? waitMs : 0.0);

        // Blocks (no busy-wait) until either a real input/window event
        // arrives or the next scheduled tick (render, and — fallback
        // platforms only — hover-poll) is due. A pending SIGINT/SIGTERM
        // is picked up at most one timeout period later (see
        // ShutdownRequested()).
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
        if (afterMs >= frameScheduler_.NextFrameDeadline(afterMs)) {
            RenderFrame();
        }
        if (!usingNativeShapeHitTest_ && afterMs >= hoverScheduler_.NextFrameDeadline(afterMs)) {
            PollHover();
        }
    }

    Shutdown();
    return 0;
}

}  // namespace nimvlets::app
