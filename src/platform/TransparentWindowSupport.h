#pragma once

// Shared declaration for the platform adapter seam described in
// AGENTS.md ("core compartido y platform adapters, no dos codebases").
//
// Exactly one of src/platform/macos/TransparentWindowSupport.mm or
// src/platform/windows/TransparentWindowSupport.cpp is compiled into any
// given build (selected in the root CMakeLists.txt by CMAKE_SYSTEM_NAME),
// so src/app never contains an #ifdef for this.

struct SDL_Window;

namespace nimvlets::platform {

// Configures `window` (already created with SDL_WINDOW_TRANSPARENT |
// SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_UTILITY |
// SDL_WINDOW_NOT_FOCUSABLE) for the desktop-companion presentation: no
// window shadow/opaque backing behind our alpha-blended content, and
// floats above normal windows. Native code is required here because
// neither "no shadow behind a transparent window" nor "float above
// normal windows without joining fullscreen Spaces" is fully exposed
// through cross-platform SDL3 window flags alone — see
// docs/PLATFORM_SPIKE.md.
void ConfigureCompanionWindow(SDL_Window* window);

// Applies (or removes) OS-level mouse click-through for `window` as a
// whole. When `clickThrough` is true, clicks are NOT delivered to this
// window and fall through to whatever is beneath it on screen; when
// false, the window receives clicks normally.
//
// This is the FALLBACK, poll-driven click-through mechanism, only used
// on platforms where NativeShapeHitTestIsRenderSafe() is false (see its
// doc comment) — currently Windows. src/app calls this once per state
// change (not every frame) with the result of evaluating
// core::BlobSilhouette::Contains() against a polled global cursor
// position.
//
// Returns the *actual* resulting state, read back from the native
// property immediately after setting it (on macOS:
// `NSWindow.ignoresMouseEvents` read right after the assignment) rather
// than just echoing the requested value — so a caller instrumenting the
// click-through pipeline can tell "we asked for X" apart from "the OS
// actually applied X" (see docs/PLATFORM_SPIKE.md's click-through
// instrumentation).
bool SetWindowClickThrough(SDL_Window* window, bool clickThrough);

// True if, on this platform, SDL_SetWindowShape() only affects native
// hit-testing (which pixels ignore the mouse) and does NOT touch or
// replace the window's actually-rendered pixel content — i.e. it's safe
// to use as the primary click-through mechanism alongside our own
// SDL_Renderer-drawn content (src/graphics/BlobRenderer).
//
// - macOS: true. Confirmed by reading the pinned SDL 3.4.12 Cocoa
//   backend source directly (not assumed): `Cocoa_UpdateWindowShape()`
//   (src/video/cocoa/SDL_cocoashape.m) only toggles
//   `NSWindow.ignoresMouseEvents`; nothing in that path touches
//   rendering. Even better: once a shape is set, SDL's own
//   `-[Cocoa_WindowListener mouseMoved:]` (SDL_cocoawindow.m) calls
//   `updateIgnoreMouseState:` on every real mouse-moved NSEvent, which
//   re-reads the shape's alpha at the current cursor position and
//   updates `ignoresMouseEvents` accordingly — a correct, event-driven
//   mechanism requiring zero polling from us. See
//   docs/PLATFORM_SPIKE.md's click-through investigation for how this
//   was found and confirmed on this block's dev machine.
// - Windows: false (conservative default; not verified in this block —
//   no Windows machine was available). Community reports
//   (libsdl-org/SDL#11199) describe `SDL_SetWindowShape` behaving
//   differently on Windows than macOS, consistent with the classic
//   Win32 `UpdateLayeredWindow` technique using the shape bitmap *as*
//   the window's rendered content rather than purely for hit-testing —
//   which would blank out our own SDL_Renderer output. Windows keeps
//   using SetWindowClickThrough() above instead until this is verified
//   on real hardware.
bool NativeShapeHitTestIsRenderSafe();

}  // namespace nimvlets::platform
