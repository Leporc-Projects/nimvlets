#pragma once

#include "core/AlphaMask.h"
#include "core/DragClassifier.h"
#include "core/FrameScheduler.h"
#include "core/Silhouette.h"
#include "graphics/BlobRenderer.h"
#include "graphics/DevSprite.h"

#include <SDL3/SDL.h>

namespace nimvlets::app {

// Owns the SDL window/renderer lifecycle and the main event loop for the
// Block 01 platform-feasibility spike.
//
// This is explicitly a foundation/spike executable, not the product —
// see docs/PLATFORM_SPIKE.md and the block brief's NON-SCOPE list for
// what it deliberately does not do (Shop, Collection, onboarding,
// persistence, audio, global click mode, ...).
class SpikeApp {
public:
    // Runs until the window is closed. Returns a process exit code (0 on
    // clean shutdown, non-zero if SDL initialization failed).
    int Run();

private:
    bool Init();
    void Shutdown();

    void HandleEvent(const SDL_Event& event, bool& running);
    void RenderFrame();
    void PollHover();
    void UpdateClickThrough(bool cursorOverOpaque);

    // True if `localPoint` (window-local, logical coordinates) is over
    // the currently active visible/interactive region — the Bunny QA
    // fixture's real alpha-derived mask when loaded, or the analytic
    // placeholder shape otherwise. Every hit-test in this file (the
    // MOUSE_BUTTON_DOWN defense-in-depth check, the poll-driven
    // fallback's hover check, and the SDL_SetWindowShape surface built
    // in Init()) goes through this one predicate.
    bool IsPointInteractive(core::Point localPoint) const;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    graphics::BlobRenderer blobRenderer_;

    core::BlobSilhouette blob_ = core::BlobSilhouette::Default();
    core::DragClassifier dragClassifier_;
    core::FrameScheduler frameScheduler_{1000.0 / 12.0};  // ~12 fps idle animation, not 60/144

    // Block 01 closure QA fixture ("Bunny") — a realistic, non-analytic
    // asset used to validate hit-testing against real alpha data,
    // replacing the analytic blob as the spike's default visual for
    // this closure pass. See docs/DECISION_LOG.md and
    // docs/PLATFORM_SPIKE.md. NOT the content-loading system
    // docs/PET_CONTENT_SPEC.md describes — see graphics::DevSprite's
    // doc comment. Falls back to the analytic blob (still exercised by
    // tests/) if the fixture file can't be loaded, e.g. because the
    // process wasn't launched from the repo root.
    graphics::DevSprite bunnySprite_;
    SDL_Texture* bunnyTexture_ = nullptr;

    // The rasterized hit-test region actually in effect: built once in
    // Init() from whichever source is active (bunny fixture or,
    // failing that, the analytic blob at rest). Declared after blob_ so
    // its member-initializer can safely reference blob_.windowWidth/
    // windowHeight (C++ initializes members in declaration order) as a
    // valid, if provisional, size before Init() replaces it.
    core::AlphaMask activeHitMask_{static_cast<int>(blob_.windowWidth), static_cast<int>(blob_.windowHeight)};

    int clickCount_ = 0;

    // True once Init() has successfully handed hit-testing to the
    // platform's own native mechanism (SDL_SetWindowShape on macOS —
    // see platform::NativeShapeHitTestIsRenderSafe()). When true, the
    // poll-driven fallback below (hoverScheduler_, PollHover(),
    // UpdateClickThrough()) is never invoked — the OS handles
    // click-through on every real mouse event, with zero polling and
    // less CPU than even the original render-tied approach.
    bool usingNativeShapeHitTest_ = false;

    // --- poll-driven click-through FALLBACK, used only when
    // usingNativeShapeHitTest_ is false (currently: Windows; see
    // platform::NativeShapeHitTestIsRenderSafe()'s doc comment for why
    // macOS doesn't need this anymore). ---
    // Click-through responsiveness needs a much shorter interval than
    // the idle animation does — see PollHover()'s doc comment. Reuses
    // the same tested FrameScheduler class/logic as frameScheduler_,
    // just with a shorter interval; no new scheduling logic.
    core::FrameScheduler hoverScheduler_{1000.0 / 60.0};
    bool currentlyClickThrough_ = false;

    // Click-through pipeline instrumentation (fallback path only — see
    // PollHover()'s and UpdateClickThrough()'s doc comments, and
    // docs/PLATFORM_SPIKE.md's "click-through instrumentation"
    // section). Tracks the last-logged value of each stage of the
    // pipeline so PollHover()/UpdateClickThrough() can log a line ONLY
    // when a stage's value actually changes — never once per poll/frame
    // regardless of change, which would flood the log at 60 Hz.
    // `diagHasValue_` guards the very first sample so it's always
    // logged once, establishing a baseline. The fields themselves (not
    // just the logging that uses them) are compiled out entirely in
    // Release builds (#ifndef NDEBUG) — matching the call sites exactly
    // avoids "unused private field" warnings there instead of just
    // silencing them.
#ifndef NDEBUG
    bool diagHasValue_ = false;
    float diagGlobalX_ = 0.0f;
    float diagGlobalY_ = 0.0f;
    int diagWindowX_ = 0;
    int diagWindowY_ = 0;
    core::Point diagLocalPoint_{};
    bool diagContains_ = false;
    bool diagRequestedClickThrough_ = false;
    bool diagActualIgnoresMouseEvents_ = false;
#endif  // NDEBUG

    // Manual window drag state. We deliberately do not use
    // SDL_HITTEST_DRAGGABLE / OS-level window dragging: that would hand
    // the whole gesture to the window manager and we would never see the
    // button-down/motion/button-up sequence core::DragClassifier needs
    // to tell a click from a drag.
    double dragGrabOffsetX_ = 0.0;
    double dragGrabOffsetY_ = 0.0;
};

// Returns true once a SIGINT/SIGTERM has been received.
//
// The spike window is intentionally borderless, NOT_FOCUSABLE, and
// excluded from the Dock (see Init()) — that's correct for a desktop
// companion, but it also means there is no window-chrome close button
// and no way for it to ever receive Cmd+Q. This is the dev/QA exit path
// (`kill -TERM <pid>` or Ctrl+C in the launching terminal) used to
// verify "clean shutdown, no hung process" without adding any real UI.
// It is not a product feature — see docs/PLATFORM_SPIKE.md.
bool ShutdownRequested();

}  // namespace nimvlets::app
