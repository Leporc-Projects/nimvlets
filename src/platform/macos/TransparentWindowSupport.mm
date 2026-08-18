#include "platform/TransparentWindowSupport.h"

#include <SDL3/SDL.h>

#import <AppKit/AppKit.h>

// See docs/PLATFORM_SPIKE.md for the full click-through investigation
// this file's current shape is based on. Summary of the journey (kept
// here because it's exactly the kind of thing a future reader will
// re-discover the hard way otherwise):
//
// 1. SDL_SetWindowShape() was evaluated first, per the block brief, and
//    initially rejected based on community reports (libsdl-org/SDL#12683,
//    #11199) suggesting it couples the click-through mask to rendering.
// 2. The fallback — manually polling SDL_GetGlobalMouseState() and
//    toggling ignoresMouseEvents ourselves (SetWindowClickThrough(),
//    below) — was shipped instead, and was interactively confirmed
//    broken during macOS QA: SDL's own Cocoa backend
//    (src/video/cocoa/SDL_cocoawindow.m, `-mouseMoved:` →
//    `updateIgnoreMouseState:`) resets `ignoresMouseEvents` to NO on
//    every real mouse-moved NSEvent whenever no SDL_SetWindowShape
//    surface is set — silently undoing our own assignment moments after
//    we made it, race-losing against the user's actual click.
// 3. Reading the pinned SDL 3.4.12 Cocoa source directly (not
//    community reports) showed step 1's rejection was wrong *for
//    macOS specifically*: `Cocoa_UpdateWindowShape()`
//    (SDL_cocoashape.m) only ever touches `ignoresMouseEvents`; it does
//    not composite, clip, or otherwise touch rendered pixels on this
//    platform. So `SDL_SetWindowShape()` is what src/app now uses on
//    macOS (see NativeShapeHitTestIsRenderSafe()) — it hands hit-testing
//    to SDL's own event-driven internals entirely, needing zero polling
//    from us.
//
// SetWindowClickThrough() below is kept as the fallback mechanism for
// platforms where NativeShapeHitTestIsRenderSafe() is false (Windows,
// unverified) — see that function's doc comment in the shared header.
// Neither mechanism requires Accessibility, Input Monitoring, or any
// TCC prompt: both are ordinary window-server/cursor-position queries,
// not global input hooks.

namespace nimvlets::platform {

namespace {

NSWindow* CocoaWindowFor(SDL_Window* window) {
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    void* ptr = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
    return (__bridge NSWindow*)ptr;
}

}  // namespace

void ConfigureCompanionWindow(SDL_Window* window) {
    NSWindow* nsWindow = CocoaWindowFor(window);
    if (nsWindow == nil) {
        SDL_Log("nimvlets: platform/macos could not resolve NSWindow; skipping native window configuration");
        return;
    }

    nsWindow.opaque = NO;
    nsWindow.backgroundColor = [NSColor clearColor];
    nsWindow.hasShadow = NO;

    // NSFloatingWindowLevel: stays above normal document/app windows
    // (satisfies "always on top over a normal window") without asking
    // for the fullscreen-space presence that SDL_WINDOW_ALWAYS_ON_TOP
    // alone does not grant and that this block explicitly keeps
    // out of scope (see NON-SCOPE: "fullscreen detection").
    nsWindow.level = NSFloatingWindowLevel;

    // Follow the user across ordinary Spaces (a desktop companion should
    // not vanish when you switch desktops) but do not participate in
    // Mission Control window shuffling. This is a small locally-made
    // presentation choice, not specified in the block brief — recorded
    // in the final report's "Decisions taken outside prompt" section.
    nsWindow.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorStationary;
}

bool SetWindowClickThrough(SDL_Window* window, bool clickThrough) {
    NSWindow* nsWindow = CocoaWindowFor(window);
    if (nsWindow == nil) {
        return clickThrough;
    }
    nsWindow.ignoresMouseEvents = clickThrough ? YES : NO;
    // Read the property back rather than trusting the assignment stuck —
    // this is the ground truth src/app's click-through instrumentation
    // compares against "what we asked for" (see
    // docs/PLATFORM_SPIKE.md's click-through instrumentation section).
    return nsWindow.ignoresMouseEvents == YES;
}

bool NativeShapeHitTestIsRenderSafe() {
    return true;
}

bool ClickThroughPollingIsMeaningful() {
    // Nunca se consulta en la práctica -- NativeShapeHitTestIsRenderSafe()
    // ya es true en macOS, así que SpikeApp jamás entra a la rama de
    // polling. Retorna false por documentación/consistencia, no
    // porque se haya medido nada acá.
    return false;
}

}  // namespace nimvlets::platform
