#pragma once

#include "content/AnimationController.h"
#include "content/AnimationDefinition.h"
#include "core/AlphaMask.h"
#include "core/DragClassifier.h"
#include "core/FrameScheduler.h"

#include <SDL3/SDL.h>

#include <optional>

namespace nimvlets::app {

// Owns the SDL window/renderer lifecycle and the main event loop for the
// small, data-driven content+animation runtime built in Block 02 on top
// of Block 01's platform-feasibility spike.
//
// This is still explicitly a foundation/spike executable, not the
// product — see docs/PLATFORM_SPIKE.md, docs/ANIMATION_RUNTIME.md, and
// the block briefs' NON-SCOPE lists for what it deliberately does not do
// (Shop, Collection, onboarding, persistence, audio, global click mode,
// final content for all 8 Nimvlets, ...).
//
// The one pet this runtime shows is entirely data (a compiled
// content::PetDefinition loaded from a ".nvpack" file at startup, see
// Init()) — this class contains no pet-specific branches, no hardcoded
// shapes, and no knowledge of "Bunny" beyond the path it happens to load
// in this block. Swapping which pack loads swaps the pet.
class SpikeApp {
public:
    // Runs until the window is closed. Returns a process exit code (0 on
    // clean shutdown, non-zero if SDL initialization or content loading
    // failed).
    int Run();

private:
    bool Init();
    void Shutdown();

    void HandleEvent(const SDL_Event& event, bool& running);

    // Draws content::AnimationController::CurrentFrame()'s texture.
    // Called only when needsRedraw_ is set — see that field's doc
    // comment — never on a fixed per-frame tick.
    void RenderFrame();

    // Rebuilds activeHitMask_ from the controller's current frame (see
    // core::AlphaMask::FromAlphaChannel) and, on platforms where it's
    // render-safe (see platform::NativeShapeHitTestIsRenderSafe()),
    // pushes it to the OS via SDL_SetWindowShape. Called alongside
    // RenderFrame() only when the displayed frame actually changed —
    // never every tick — so the window shape surface is rebuilt exactly
    // as often as the picture it hit-tests against.
    void ApplyCurrentHitMask();

    void PollHover();
    void UpdateClickThrough(bool cursorOverOpaque);

    // True if `localPoint` (window-local, logical coordinates) is over
    // the currently active pet frame's real alpha-derived hit region.
    // Every hit-test in this file (the MOUSE_BUTTON_DOWN defense-in-
    // depth check, the poll-driven fallback's hover check, and the
    // SDL_SetWindowShape surface built in ApplyCurrentHitMask()) goes
    // through this one predicate.
    bool IsPointInteractive(core::Point localPoint) const;

    // Creates/releases one SDL_Texture per frame across pet_'s idle,
    // click-reaction, and every passive action — see
    // graphics::AttachFrameTexture()/ReleaseFrameTexture().
    void AttachAllTextures();
    void ReleaseAllTextures();

    // Reads NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS (if set to a valid
    // positive number) to shorten the passive-action wait for manual QA,
    // without ever touching pet_.passiveIntervalSeconds itself — the
    // pack's authored ~300s default stays the production value in every
    // build; this is purely an opt-in override for *this run*. See
    // docs/ANIMATION_RUNTIME.md, "DEV passive-interval override".
    double ComputeEffectivePassiveIntervalSeconds() const;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;

    // The one active pet's data-driven content — loaded once in Init()
    // from assets/dev/bunny_pack.nvpack (see kPetPackPath in
    // SpikeApp.cpp) via content::LoadPetPackFromFile(). Declared before
    // animController_ so it exists (default-constructed, if not yet
    // loaded) at the point animController_'s member-initializer binds a
    // reference to it — see animController_'s doc comment for why that
    // ordering matters and why it's still safe.
    content::PetDefinition pet_;

    // Constructed only after pet_ is successfully loaded (Init() calls
    // animController_.emplace(pet_)) — std::optional rather than binding
    // a reference to pet_ before it holds real content, so there is
    // never a window where a reference exists to not-yet-loaded data.
    std::optional<content::AnimationController> animController_;

    // The rasterized hit-test region for animController_'s *current*
    // frame — rebuilt by ApplyCurrentHitMask() only when the frame
    // changes. 1x1 placeholder until Init() populates real content
    // (never used as such: Init() always calls ApplyCurrentHitMask()
    // before the window becomes interactive).
    core::AlphaMask activeHitMask_{1, 1};

    core::DragClassifier dragClassifier_;
    int clickCount_ = 0;

    // Set whenever something that affects the displayed picture happens
    // (a frame-advance from animController_->Advance(), a
    // TriggerClick()/TriggerPassiveAction() call, or an
    // SDL_EVENT_WINDOW_EXPOSED asking us to repaint) and cleared right
    // after RenderFrame()+ApplyCurrentHitMask() run for it. This is the
    // mechanism that makes static idle render/redraw *nothing* — no
    // fixed tick exists anymore (contrast Block 01's frameScheduler_).
    // Starts true so the very first frame renders.
    bool needsRedraw_ = true;

    // Effective seconds between sparse passive actions for this run —
    // see ComputeEffectivePassiveIntervalSeconds(). Computed once in
    // Init(); pet_.passiveIntervalSeconds (the pack's authored default)
    // is never mutated.
    double passiveIntervalSecondsEffective_ = 300.0;
    double nextPassiveDeadlineMs_ = 0.0;
    std::size_t nextPassiveActionIndex_ = 0;

    // True once Init() has successfully handed hit-testing to the
    // platform's own native mechanism (SDL_SetWindowShape on macOS —
    // see platform::NativeShapeHitTestIsRenderSafe()). When true, the
    // poll-driven fallback below (hoverScheduler_, PollHover(),
    // UpdateClickThrough()) is never invoked — the OS handles
    // click-through on every real mouse event, with zero polling.
    bool usingNativeShapeHitTest_ = false;

    // --- poll-driven click-through FALLBACK, used only when
    // usingNativeShapeHitTest_ is false (currently: Windows; see
    // platform::NativeShapeHitTestIsRenderSafe()'s doc comment for why
    // macOS doesn't need this anymore). Unchanged in spirit from
    // Block 01 — still a bounded SDL_WaitEventTimeout wakeup at ~60 Hz,
    // never a busy-wait, still a cursor-*position* poll
    // (SDL_GetGlobalMouseState), not a global input hook. See
    // AGENTS.md's privacy rules. ---
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
