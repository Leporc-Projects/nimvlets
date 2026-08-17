#include "app/SpikeApp.h"

#include "platform/TransparentWindowSupport.h"

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

core::Point WindowLocalCursor(SDL_Window* window) {
    float globalX = 0.0f;
    float globalY = 0.0f;
    SDL_GetGlobalMouseState(&globalX, &globalY);

    int winX = 0;
    int winY = 0;
    SDL_GetWindowPosition(window, &winX, &winY);

    return core::Point{
        static_cast<double>(globalX) - static_cast<double>(winX),
        static_cast<double>(globalY) - static_cast<double>(winY),
    };
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

void SpikeApp::RenderAndPollHover() {
    const double nowMs = static_cast<double>(SDL_GetTicks());
    const double phaseSeconds = nowMs / 1000.0;

    blobRenderer_.Render(renderer_, blob_, phaseSeconds);
    frameScheduler_.OnFramePresented(nowMs);

    // Piggyback the click-through hover check on this same wake-up so no
    // extra timer wakeups are introduced beyond the idle-animation tick
    // (see docs/PLATFORM_SPIKE.md, "click-through region"). This is a
    // plain cursor-position poll (SDL_GetGlobalMouseState), not a global
    // input hook — see AGENTS.md's privacy rules.
    const core::Point localCursor = WindowLocalCursor(window_);
    const bool cursorOverOpaque = blob_.Contains(localCursor, phaseSeconds);
    UpdateClickThrough(cursorOverOpaque);
}

void SpikeApp::UpdateClickThrough(bool cursorOverOpaque) {
    // Never toggle click-through away mid-gesture: once the user has
    // pressed on the shape, keep receiving events until they release,
    // regardless of exactly which pixel the cursor is over on any given
    // sample (the window itself is also moving under the cursor during a
    // drag, which would otherwise make this noisy).
    const bool shouldBeClickThrough = dragClassifier_.IsActive() ? false : !cursorOverOpaque;
    if (shouldBeClickThrough != currentlyClickThrough_) {
        platform::SetWindowClickThrough(window_, shouldBeClickThrough);
        currentlyClickThrough_ = shouldBeClickThrough;
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
    RenderAndPollHover();  // first frame, presented immediately

    SDL_Event event;
    while (running && !ShutdownRequested()) {
        const double nowMs = static_cast<double>(SDL_GetTicks());
        const double waitMs = frameScheduler_.MillisUntilNextFrame(nowMs);
        const Sint32 timeoutMs = static_cast<Sint32>(waitMs > 0.0 ? waitMs : 0.0);

        // Blocks (no busy-wait) until either a real input/window event
        // arrives or the next idle-animation frame is due, whichever is
        // first. A pending SIGINT/SIGTERM is picked up at most one
        // timeout period later (see ShutdownRequested()).
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
            RenderAndPollHover();
        }
    }

    Shutdown();
    return 0;
}

}  // namespace nimvlets::app
