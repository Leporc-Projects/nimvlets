#include "platform/TransparentWindowSupport.h"

#include "platform/LinuxBackendPolicy.h"
#include "platform/RendererPolicy.h"

#include <SDL3/SDL.h>

// IMPORTANT — read before trusting this file's behavior:
//
// This implementation has been compiled via CI (.github/workflows) on
// an ubuntu-24.04 x64 runner as part of Block 04.1, including a
// non-interactive Xvfb GUI smoke test on X11. It has NOT been run on
// a real Linux desktop with a human watching it, on either backend —
// see docs/LINUX_PLATFORM.md for exactly what is and isn't confirmed,
// and the manual QA list required before this can be trusted the way
// docs/PLATFORM_SPIKE.md's macOS section already is.
//
// Both X11 and Wayland are detected at runtime via
// SDL_GetCurrentVideoDriver() — never assumed, never forced (block
// brief: "Do not force X11 globally") — and every capability decision
// below is delegated to the pure, unit-tested tables in
// LinuxBackendPolicy.h/.cpp, each entry cited against the pinned SDL
// 3.4.12 source. See docs/LINUX_PLATFORM.md for the full investigation
// this file's shape is based on (mirrors
// src/platform/macos/TransparentWindowSupport.mm's own "read the
// actual pinned source before rejecting/accepting a mechanism"
// methodology, established in Block 01).

namespace nimvlets::platform {

namespace {

LinuxVideoBackend DetectBackend() {
    return ParseLinuxVideoBackend(SDL_GetCurrentVideoDriver());
}

const char* BackendName(LinuxVideoBackend backend) {
    switch (backend) {
        case LinuxVideoBackend::kX11:
            return "x11";
        case LinuxVideoBackend::kWayland:
            return "wayland";
        case LinuxVideoBackend::kOther:
            return "unknown";
    }
    return "unknown";
}

}  // namespace

void ConfigureCompanionWindow(SDL_Window* window) {
    // A diferencia de macOS (necesitó NSWindow.opaque/backgroundColor/
    // hasShadow/level/collectionBehavior explícitos) y Windows
    // (necesitó WS_EX_LAYERED/NOACTIVATE + HWND_TOPMOST explícitos),
    // la investigación de este bloque (ver docs/LINUX_PLATFORM.md)
    // confirmó -- leyendo la fuente pineada, no asumiendo -- que tanto
    // X11 como Wayland ya aplican transparencia real
    // (SDL_WINDOW_TRANSPARENT), always-on-top donde el protocolo lo
    // permite (SDL_WINDOW_ALWAYS_ON_TOP, honrado en la creación de
    // ventana de X11_CreateWindow; sin equivalente en xdg-shell para
    // Wayland -- limitación del protocolo, no de este código) y
    // not-focusable (SDL_WINDOW_NOT_FOCUSABLE, vía WM hints de ICCCM
    // en X11) directamente a partir de las flags cross-platform que
    // SpikeApp.cpp ya pasa a SDL_CreateWindow -- sin necesitar ningún
    // llamado nativo Xlib/Wayland adicional acá. Esta función solo
    // deja constancia en el log de qué backend se resolvió y de la
    // limitación de Wayland, para que quede visible sin tener que leer
    // el código fuente.
    const LinuxVideoBackend backend = DetectBackend();
    if (window == nullptr) {
        SDL_Log("nimvlets: platform/linux ConfigureCompanionWindow called with a null window; skipping");
        return;
    }

    SDL_Log("nimvlets: platform/linux active SDL video driver = '%s'", BackendName(backend));
    if (backend == LinuxVideoBackend::kWayland) {
        SDL_Log(
            "nimvlets: Wayland: always-on-top is not requested for a plain xdg_toplevel -- the "
            "xdg-shell protocol has no client-requestable stacking hint (see docs/LINUX_PLATFORM.md); "
            "this is a window-system limitation, not a bug");
    } else if (backend == LinuxVideoBackend::kOther) {
        SDL_Log(
            "nimvlets: unrecognized Linux video driver -- only x11/wayland are in scope for this "
            "block (see docs/LINUX_PLATFORM.md); proceeding with SDL's cross-platform defaults only");
    }
}

bool SetWindowClickThrough(SDL_Window* window, bool clickThrough) {
    // En el wiring actual de SpikeApp (ver usingPollDrivenClickThrough_
    // en src/app/SpikeApp.h/.cpp) esta función nunca se invoca en
    // Linux: X11 usa el path nativo de shape (ver
    // NativeShapeHitTestIsRenderSafe() abajo) y Wayland desactiva
    // enteramente el polling fallback vía ClickThroughPollingIsMeaningful()
    // == false, precisamente porque no existe ningún mecanismo nativo
    // equivalente a NSWindow.ignoresMouseEvents/WS_EX_TRANSPARENT
    // alcanzable con la API pública de SDL 3.4.12 en ningún backend
    // Linux (ver LinuxBackendPolicy.h para la evidencia completa). Se
    // implementa de todos modos, de forma honesta -- nunca finge haber
    // aplicado click-through -- por si el wiring de arriba cambia en
    // un bloque futuro.
    (void)window;
    static bool loggedOnce = false;
    if (!loggedOnce) {
        SDL_Log(
            "nimvlets: platform/linux SetWindowClickThrough(%s) called, but no native Linux "
            "click-through mechanism is wired (see docs/LINUX_PLATFORM.md) -- returning false "
            "(not click-through) rather than claiming it was applied",
            clickThrough ? "true" : "false");
        loggedOnce = true;
    }
    return false;
}

bool ReadWindowClickThrough(SDL_Window* window) {
    // SetWindowClickThrough() nunca aplica nada en Linux (ver arriba),
    // así que el estado real es, honestamente, siempre "no
    // click-through" -- nunca se finge lo contrario.
    (void)window;
    return false;
}

bool MakeClickThroughAuthoritative(SDL_Window* window) {
    // X11 usa la ruta nativa de shape (donde el hit-test lo maneja el
    // servidor X, no una propiedad nuestra que alguien pueda pisar) y
    // Wayland no tiene mecanismo alguno -- en ninguno de los dos casos
    // existe una propiedad de click-through propia que defender. Ver
    // docs/LINUX_PLATFORM.md.
    (void)window;
    return false;
}

unsigned long long ForeignClickThroughWriteCount() {
    return 0;  // ver MakeClickThroughAuthoritative() arriba
}

bool NativeShapeHitTestIsRenderSafe(bool usingSoftwareRenderer) {
    // El hallazgo de Block 05 (ver platform/TransparentWindowSupport.h
    // y macos/TransparentWindowSupport.mm) de que SDL_SetWindowShape()
    // corrompe el present bajo el driver "software" fue confirmado SOLO
    // en macOS (no hay máquina Linux disponible en este bloque), y
    // platform::RendererPolicy nunca fuerza "software" en Linux salvo
    // vía el override NIMVLETS_DEV_RENDERER_DRIVER. Se ignora el
    // parámetro deliberadamente en vez de asumir que el mismo bug
    // aplica acá sin evidencia -- ver el informe final de Block 05 para
    // esto como deuda/límite documentado si alguna vez se ejercita ese
    // override en Linux.
    (void)usingSoftwareRenderer;
    return LinuxBackendSupportsNativeShapeHitTest(DetectBackend());
}

bool ClickThroughPollingIsMeaningful(bool usingSoftwareRenderer) {
    (void)usingSoftwareRenderer;
    return LinuxBackendClickThroughPollingIsMeaningful(DetectBackend());
}


RendererPlatform CurrentRendererPlatform() {
    return RendererPlatform::kLinux;
}

}  // namespace nimvlets::platform
