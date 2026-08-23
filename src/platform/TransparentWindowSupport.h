#pragma once

// Shared declaration for the platform adapter seam described in
// AGENTS.md ("core compartido y platform adapters, no dos codebases").
//
// Exactly one of src/platform/macos/TransparentWindowSupport.mm,
// src/platform/windows/TransparentWindowSupport.cpp, or
// src/platform/linux/TransparentWindowSupport.cpp is compiled into any
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
// core::AlphaMask::Contains() — the current animation frame's real
// alpha-derived hit region, see docs/ANIMATION_RUNTIME.md — against a
// polled global cursor position.
//
// Returns the *actual* resulting state, read back from the native
// property immediately after setting it (on macOS:
// `NSWindow.ignoresMouseEvents` read right after the assignment) rather
// than just echoing the requested value — so a caller instrumenting the
// click-through pipeline can tell "we asked for X" apart from "the OS
// actually applied X" (see docs/PLATFORM_SPIKE.md's click-through
// instrumentation).
bool SetWindowClickThrough(SDL_Window* window, bool clickThrough);

// True if, on this platform (and with the ACTIVE renderer driver —
// `usingSoftwareRenderer` says whether that's SDL's "software" driver,
// see platform::RendererPolicy/DEC-083), SDL_SetWindowShape() only
// affects native hit-testing (which pixels ignore the mouse) and does
// NOT touch or replace the window's actually-rendered pixel content —
// i.e. it's safe to use as the primary click-through mechanism
// alongside our own SDL_Renderer-drawn content (src/graphics/BlobRenderer).
//
// - macOS, accelerated driver (Metal/GL/GPU — the historical case,
//   still SDL's own default absent DEC-083's macOS override): true.
//   Confirmed by reading the pinned SDL 3.4.12 Cocoa backend source
//   directly (not assumed): `Cocoa_UpdateWindowShape()`
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
// - macOS, `usingSoftwareRenderer` true: FALSE — a new finding from
//   Block 05's renderer-resolution pass (see DEC-083's addendum and the
//   final report's diagnostic section). Interactive QA under DEC-083's
//   new macOS software-renderer default showed every animation frame
//   after the very first one presenting as a solid opaque white
//   silhouette; a minimal ~50-line standalone SDL3 program (no app code
//   involved at all) isolated the cause to `SDL_SetWindowShape()`
//   itself — a single call, in either order relative to rendering,
//   permanently corrupts every subsequent `SDL_RenderPresent()` on the
//   "software" driver on this dev machine. A second standalone repro
//   proved this is NOT about `Cocoa_UpdateWindowShape()`'s
//   `ignoresMouseEvents` toggle (the claim just above is still true in
//   isolation: toggling `NSWindow.ignoresMouseEvents` directly, the
//   exact mechanism SetWindowClickThrough() below already uses, was
//   run 4 times in a row against the software renderer with zero
//   corruption) — the breakage lives somewhere in SDL_video.c's
//   platform-independent `SDL_SetWindowShape()` wrapper (the
//   `SDL_ConvertSurface`/`SDL_SetSurfaceProperty` bookkeeping that runs
//   before it ever reaches Cocoa's code), not in anything platform.mm
//   controls or could work around by re-ordering calls. Filed as an
//   honest, reproducible SDL3-on-macOS limitation — not something this
//   codebase patches around inside vendored SDL3 source.
// - Windows: false (conservative default; not verified in this block —
//   no Windows machine was available). Community reports
//   (libsdl-org/SDL#11199) describe `SDL_SetWindowShape` behaving
//   differently on Windows than macOS, consistent with the classic
//   Win32 `UpdateLayeredWindow` technique using the shape bitmap *as*
//   the window's rendered content rather than purely for hit-testing —
//   which would blank out our own SDL_Renderer output. Windows keeps
//   using SetWindowClickThrough() above instead until this is verified
//   on real hardware. `usingSoftwareRenderer` changes nothing here:
//   already false regardless, and RendererPolicy never forces
//   "software" on Windows outside of the DEV override anyway.
bool NativeShapeHitTestIsRenderSafe(bool usingSoftwareRenderer);

// True if it's worth running the poll-driven click-through fallback
// (src/app/SpikeApp.cpp's hoverScheduler_ / PollHover() /
// UpdateClickThrough(), which calls SetWindowClickThrough() above) on
// this platform (with this renderer driver). Only ever consulted when
// NativeShapeHitTestIsRenderSafe() is false for the same
// `usingSoftwareRenderer` value — irrelevant otherwise, since that
// fallback isn't used at all when the native shape path is render-safe.
//
// This exists because "no native shape path" and "poll-driven
// click-through would actually work" turned out NOT to always be the
// same fact once Linux (Block 04.1) added a third platform: on
// Windows they coincide (no shape path, but the poll fallback does
// work — WS_EX_TRANSPARENT genuinely changes OS-level click delivery),
// but on Linux/Wayland neither the native shape path NOR the poll
// fallback can do anything (see
// src/platform/linux/TransparentWindowSupport.cpp and
// docs/LINUX_PLATFORM.md for the pinned-SDL3-source evidence). Running
// a ~60Hz wakeup loop forever, knowing in advance it can never change
// anything, would be exactly the kind of permanent polling loop
// AGENTS.md §2 ("event-driven scheduling") and this block's brief §8
// forbid — so this function lets src/app skip starting that loop at
// all on a platform where it would be pointless, without src/app ever
// needing to know *why* per-platform.
//
// - macOS, accelerated driver: never actually consulted
//   (NativeShapeHitTestIsRenderSafe() is already true there), returns
//   false for documentation purposes.
// - macOS, `usingSoftwareRenderer` true: true — Block 05's finding
//   above means the native shape path is no longer safe there, so
//   SpikeApp needs this poll-driven fallback to actually run (it uses
//   SetWindowClickThrough(), independently confirmed safe under the
//   software renderer — see this function's doc comment above).
// - Windows: true — same poll-driven mechanism this project has always
//   used there, independent of `usingSoftwareRenderer` (RendererPolicy
//   doesn't touch Windows's renderer choice outside the DEV override).
// - Linux/X11: never actually consulted (native shape path is
//   render-safe there too — see the Linux adapter). Whether it would
//   stay render-safe if `usingSoftwareRenderer` were ever forced true
//   there via the DEV override is NOT verified in this block (no Linux
//   machine available) — see docs/LINUX_PLATFORM.md.
// - Linux/Wayland: false — see docs/LINUX_PLATFORM.md.
bool ClickThroughPollingIsMeaningful(bool usingSoftwareRenderer);

}  // namespace nimvlets::platform
