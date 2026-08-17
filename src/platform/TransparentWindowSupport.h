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
// Called once per state change (not every frame) by src/app/SpikeApp
// with the result of evaluating core::BlobSilhouette::Contains() against
// the current global cursor position (see PLATFORM_SPIKE.md for why
// SDL_SetWindowShape was evaluated and not used for this).
void SetWindowClickThrough(SDL_Window* window, bool clickThrough);

}  // namespace nimvlets::platform
