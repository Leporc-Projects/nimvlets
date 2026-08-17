#include "platform/TransparentWindowSupport.h"

#include <SDL3/SDL.h>

#import <AppKit/AppKit.h>

// See docs/PLATFORM_SPIKE.md for the evaluation this implementation is
// based on. Summary: SDL_SetWindowShape() was evaluated first (per the
// block brief) and rejected for the shipped path because on SDL 3.4.12
// it couples the click-through mask to what gets rendered — pixels
// outside the shape surface are not drawn at all, which conflicts with
// wanting our own renderer (src/graphics/BlobRenderer) to own drawing.
// This is also a known upstream limitation, not a Nimvlets-specific bug
// (libsdl-org/SDL#12683). Instead this file drives two plain,
// permission-free AppKit/Win32 mechanisms:
//   - NSWindow.ignoresMouseEvents, toggled per-state (not per-frame);
//   - polling SDL_GetGlobalMouseState() from src/app's existing idle
//     animation tick to decide when to toggle it.
// Neither requires Accessibility, Input Monitoring, or any TCC prompt:
// both are ordinary window-server/cursor-position queries, not global
// input hooks.

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

void SetWindowClickThrough(SDL_Window* window, bool clickThrough) {
    NSWindow* nsWindow = CocoaWindowFor(window);
    if (nsWindow == nil) {
        return;
    }
    nsWindow.ignoresMouseEvents = clickThrough ? YES : NO;
}

}  // namespace nimvlets::platform
