#pragma once

#include "core/DragClassifier.h"
#include "core/FrameScheduler.h"
#include "core/Silhouette.h"
#include "graphics/BlobRenderer.h"

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
    void RenderAndPollHover();
    void UpdateClickThrough(bool cursorOverOpaque);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    graphics::BlobRenderer blobRenderer_;

    core::BlobSilhouette blob_ = core::BlobSilhouette::Default();
    core::DragClassifier dragClassifier_;
    core::FrameScheduler frameScheduler_{1000.0 / 12.0};  // ~12 fps idle animation, not 60/144

    int clickCount_ = 0;
    bool currentlyClickThrough_ = false;

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
