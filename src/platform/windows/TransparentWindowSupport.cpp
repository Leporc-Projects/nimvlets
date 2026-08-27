#include "platform/TransparentWindowSupport.h"

#include "platform/RendererPolicy.h"

#include <SDL3/SDL.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// IMPORTANT — read before trusting this file's behavior:
//
// This implementation has been compiled via CI (.github/workflows) on a
// Windows x64 runner as part of this block, but it has NOT been run on
// real Windows hardware with a display: configure/build success is the
// only thing verified here. See docs/PLATFORM_SPIKE.md, "Windows"
// section, for exactly what is and isn't confirmed, and the manual smoke
// test list required before this can be trusted.
//
// The technique below (toggling WS_EX_TRANSPARENT on the extended window
// style) is the standard Win32 mechanism for per-pixel mouse
// click-through on a layered window and mirrors
// src/platform/macos/TransparentWindowSupport.mm's use of
// NSWindow.ignoresMouseEvents: both are driven by the same
// core::AlphaMask::Contains() decision (the current animation frame's
// real alpha-derived hit region — see docs/ANIMATION_RUNTIME.md)
// computed once in src/app/SpikeApp.cpp, so the platform layer only
// ever reacts to a bool — it never reimplements hit-testing.

namespace nimvlets::platform {

void BringApplicationToForeground() {
    // No-op por ahora: el Product UI de Block 06 solo se valida en macOS
    // (block brief 24). Una implementacion real usaria SetForegroundWindow.
    // Ver docs/PRODUCT_UI.md.
}

#if defined(_WIN32)

namespace {

HWND Win32WindowFor(SDL_Window* window) {
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    void* ptr = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    return static_cast<HWND>(ptr);
}

}  // namespace

void ConfigureCompanionWindow(SDL_Window* window) {
    HWND hwnd = Win32WindowFor(window);
    if (hwnd == nullptr) {
        SDL_Log("nimvlets: platform/windows could not resolve HWND; skipping native window configuration");
        return;
    }

    const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED | WS_EX_NOACTIVATE);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

bool SetWindowClickThrough(SDL_Window* window, bool clickThrough) {
    HWND hwnd = Win32WindowFor(window);
    if (hwnd == nullptr) {
        return clickThrough;
    }

    const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    const LONG_PTR next = clickThrough ? (exStyle | WS_EX_TRANSPARENT) : (exStyle & ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT));
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, next);

    // Read back rather than trusting the assignment stuck, mirroring
    // src/platform/macos's ignoresMouseEvents readback — see
    // docs/PLATFORM_SPIKE.md's click-through instrumentation section.
    const LONG_PTR after = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    return (after & WS_EX_TRANSPARENT) != 0;
}

bool ReadWindowClickThrough(SDL_Window* window) {
    HWND hwnd = Win32WindowFor(window);
    if (hwnd == nullptr) {
        return false;
    }
    return (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TRANSPARENT) != 0;
}

bool MakeClickThroughAuthoritative(SDL_Window* window) {
    // Nada que hacer en Windows: WS_EX_TRANSPARENT lo escribe
    // exclusivamente SetWindowClickThrough() de arriba. El backend Win32
    // de SDL 3.4.12 nunca lo toca (a diferencia de su backend Cocoa con
    // NSWindow.ignoresMouseEvents — ver
    // platform/TransparentWindowSupport.h), así que no hay ningún otro
    // escritor contra el que proteger la política de Nimvlets.
    //
    // NOTA HONESTA: eso se afirma por lectura de la fuente pineada, no
    // por QA en hardware Windows real (no hubo máquina Windows en este
    // bloque — ver docs/PLATFORM_SPIKE.md).
    (void)window;
    return false;
}

unsigned long long ForeignClickThroughWriteCount() {
    return 0;  // ver MakeClickThroughAuthoritative() arriba
}

bool NativeShapeHitTestIsRenderSafe(bool usingSoftwareRenderer) {
    // Conservative "no" — not verified in this block (no Windows
    // machine available). See this function's doc comment in
    // platform/TransparentWindowSupport.h for the community reports
    // (libsdl-org/SDL#11199) this default is based on. SetWindowClickThrough()
    // above is the mechanism src/app actually uses as a result.
    //
    // `usingSoftwareRenderer` doesn't change this answer: it's already
    // false unconditionally, and platform::RendererPolicy never forces
    // "software" on Windows outside of the NIMVLETS_DEV_RENDERER_DRIVER
    // override (see Block 05/DEC-083) — the parameter exists only to
    // keep this function's signature the same across all three
    // platform adapters.
    (void)usingSoftwareRenderer;
    return false;
}

bool ClickThroughPollingIsMeaningful(bool usingSoftwareRenderer) {
    // True: SetWindowClickThrough() above (WS_EX_TRANSPARENT) is a
    // real, working mechanism here — this is the platform this
    // fallback was built for (see its doc comment in
    // platform/TransparentWindowSupport.h). Added in Block 04.1 when
    // Linux/Wayland needed a way to say "false" for the same question
    // (see src/platform/linux/TransparentWindowSupport.cpp) without
    // changing Windows' own behavior at all. Independent of
    // `usingSoftwareRenderer` for the same reason as above.
    (void)usingSoftwareRenderer;
    return true;
}

#else

// This translation unit is only ever added to the build when
// CMAKE_SYSTEM_NAME is Windows (see root CMakeLists.txt); the branch
// below exists solely so the file is not silently empty if that
// invariant is ever broken.
void ConfigureCompanionWindow(SDL_Window*) {}
bool SetWindowClickThrough(SDL_Window*, bool clickThrough) { return clickThrough; }
bool NativeShapeHitTestIsRenderSafe(bool) { return false; }
bool ClickThroughPollingIsMeaningful(bool) { return false; }

#endif

// Único, fuera del #if/#else de arriba -- el valor no depende de cuál
// rama compiló (esta traducción unit SOLO se agrega al build en
// Windows, ver el comentario junto al #else), así que no necesita
// duplicarse en las dos ramas.
RendererPlatform CurrentRendererPlatform() {
    return RendererPlatform::kWindows;
}

}  // namespace nimvlets::platform
