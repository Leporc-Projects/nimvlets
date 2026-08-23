// Verificación NATIVA de la resolución de click-through de macOS
// (Block 05, pasada de estabilización -- ver DEC-086 en
// docs/DECISION_LOG.md y platform::MakeClickThroughAuthoritative()).
//
// Por qué esto no es un test de CTest normal: necesita una ventana real
// y una sesión de WindowServer, y todo el resto de tests/ es
// deliberadamente SDL-free/display-free (ver tests/CMakeLists.txt y
// AGENTS.md §12). Así que se construye como un ejecutable aparte,
// SOLO en macOS y SOLO con -DNIMVLETS_ENABLE_GUI_CHECKS=ON, y se
// registra como test de CTest únicamente en esa configuración -- la CI
// de cuatro plataformas queda exactamente igual que antes.
//
// Vive bajo src/platform/macos/ y no bajo tests/ a propósito: incluye
// AppKit, y AGENTS.md §3 dice que nada fuera de src/platform/* lo hace.
//
// Qué prueba, contra el código de adaptador REAL que se envía (no una
// reimplementación):
//   1. Que instalar la propiedad exclusiva funciona.
//   2. Que el mecanismo histórico que rompía el click-through -- SDL
//      reescribiendo NSWindow.ignoresMouseEvents desde
//      -[Cocoa_WindowListener mouseMoved:] -- ya NO puede deshacer la
//      decisión de Nimvlets. Los eventos son NSEvent reales entregados
//      a NUESTRA PROPIA ventana vía -[NSApplication sendEvent:], que es
//      exactamente el camino de AppKit que dispara ese código de SDL:
//      no hay hook global, no hay evento sintético hacia otra app, no
//      hace falta ningún permiso.
//   3. Que transparente -> opaco -> transparente aguanta repetido.
//   4. Que la ventana visual renderizada por software NUNCA recibe
//      SDL_SetWindowShape() -- y que por lo tanto su contenido no se
//      corrompe (se lee el pixel realmente presentado, sin captura de
//      pantalla: SDL_RenderReadPixels sobre nuestro propio renderer).
//
// Uso:
//   cmake --preset macos-debug -DNIMVLETS_ENABLE_GUI_CHECKS=ON
//   cmake --build --preset macos-debug --target nimvlets_macos_clickthrough_check
//   ./build/macos-debug/src/platform/macos/nimvlets_macos_clickthrough_check

#include "core/ClickThroughPolicy.h"
#include "platform/TransparentWindowSupport.h"

#include <SDL3/SDL.h>

#import <AppKit/AppKit.h>

#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
    std::printf("  [%s] %s\n", condition ? "PASS" : "FAIL", what);
    if (!condition) {
        ++g_failures;
    }
}

NSWindow* CocoaWindowFor(SDL_Window* window) {
    return (__bridge NSWindow*)SDL_GetPointerProperty(
        SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
}

// Entrega un NSEventTypeMouseMoved REAL a nuestra propia ventana. Este
// es el camino exacto que ejecuta -[Cocoa_WindowListener mouseMoved:] ->
// -updateIgnoreMouseState:, el escritor que históricamente pisaba
// nuestro valor (ver platform/TransparentWindowSupport.h).
void DeliverRealMouseMovedEvent(NSWindow* nsWindow) {
    NSEvent* moved = [NSEvent mouseEventWithType:NSEventTypeMouseMoved
                                        location:NSMakePoint(nsWindow.frame.size.width / 2.0,
                                                             nsWindow.frame.size.height / 2.0)
                                   modifierFlags:0
                                       timestamp:[[NSProcessInfo processInfo] systemUptime]
                                    windowNumber:[nsWindow windowNumber]
                                         context:nil
                                     eventNumber:0
                                      clickCount:0
                                        pressure:0];
    [NSApp sendEvent:moved];
}

bool CenterPixelIsTransparent(SDL_Renderer* renderer) {
    SDL_Surface* raw = SDL_RenderReadPixels(renderer, nullptr);
    if (raw == nullptr) {
        std::printf("    (SDL_RenderReadPixels failed: %s)\n", SDL_GetError());
        return false;
    }
    SDL_Surface* rgba = raw->format == SDL_PIXELFORMAT_RGBA32 ? raw : SDL_ConvertSurface(raw, SDL_PIXELFORMAT_RGBA32);
    Uint8 r = 0, g = 0, b = 0, a = 0;
    SDL_ReadSurfacePixel(rgba, rgba->w / 2, rgba->h / 2, &r, &g, &b, &a);
    std::printf("    presented centre pixel RGBA = (%u,%u,%u,%u)\n", r, g, b, a);
    const bool transparent = a == 0;
    if (rgba != raw) {
        SDL_DestroySurface(rgba);
    }
    SDL_DestroySurface(raw);
    return transparent;
}

}  // namespace

int main() {
    SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP, "1");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    // Exactamente los flags de producción de SpikeApp::Init().
    const SDL_WindowFlags flags = SDL_WINDOW_TRANSPARENT | SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP |
                                  SDL_WINDOW_UTILITY | SDL_WINDOW_NOT_FOCUSABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window* window = SDL_CreateWindow("nimvlets click-through check", 160, 160, flags);
    if (window == nullptr) {
        std::printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, SDL_SOFTWARE_RENDERER);
    if (renderer == nullptr) {
        std::printf("SDL_CreateRenderer(software) failed: %s\n", SDL_GetError());
        return 1;
    }
    nimvlets::platform::ConfigureCompanionWindow(window);
    NSWindow* nsWindow = CocoaWindowFor(window);
    std::printf("renderer='%s'  window class='%s'\n", SDL_GetRendererName(renderer),
                nsWindow != nil ? object_getClassName(nsWindow) : "(nil)");

    std::printf("\n[1] the software renderer must NOT be given a window shape\n");
    Check(!nimvlets::platform::NativeShapeHitTestIsRenderSafe(true),
          "NativeShapeHitTestIsRenderSafe(software) == false");
    Check(nimvlets::platform::ClickThroughPollingIsMeaningful(true),
          "ClickThroughPollingIsMeaningful(software) == true");
    Check(SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_SHAPE_POINTER, nullptr) == nullptr,
          "no SDL_PROP_WINDOW_SHAPE_POINTER installed on the visual window");
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    Check(CenterPixelIsTransparent(renderer), "presented content is not the white-silhouette corruption");

    std::printf("\n[2] Nimvlets takes ownership of the native click-through state\n");
    Check(nimvlets::platform::MakeClickThroughAuthoritative(window), "MakeClickThroughAuthoritative() succeeded");
    Check(nimvlets::platform::ForeignClickThroughWriteCount() == 0, "no foreign writes intercepted yet");

    std::printf("\n[3] the historical reset (SDL's -updateIgnoreMouseState:) can no longer undo us\n");
    Check(nimvlets::platform::SetWindowClickThrough(window, true), "SetWindowClickThrough(true) reads back true");
    for (int i = 0; i < 25; ++i) {
        DeliverRealMouseMovedEvent(nsWindow);
    }
    Check(nimvlets::platform::ReadWindowClickThrough(window), "still click-through after 25 real mouse-moved events");
    const unsigned long long intercepted = nimvlets::platform::ForeignClickThroughWriteCount();
    std::printf("    foreign writes intercepted: %llu\n", intercepted);
    Check(intercepted > 0, "SDL really did attempt to overwrite it (so the guard is what is holding)");

    std::printf("\n[4] repeated transparent -> opaque -> transparent transitions\n");
    for (int cycle = 1; cycle <= 4; ++cycle) {
        nimvlets::platform::SetWindowClickThrough(window, true);
        for (int i = 0; i < 5; ++i) {
            DeliverRealMouseMovedEvent(nsWindow);
        }
        const bool transparentOk = nimvlets::platform::ReadWindowClickThrough(window);

        nimvlets::platform::SetWindowClickThrough(window, false);
        for (int i = 0; i < 5; ++i) {
            DeliverRealMouseMovedEvent(nsWindow);
        }
        const bool opaqueOk = !nimvlets::platform::ReadWindowClickThrough(window);

        char label[96];
        std::snprintf(label, sizeof(label), "cycle %d: transparent=%s opaque=%s", cycle,
                      transparentOk ? "click-through" : "WRONG", opaqueOk ? "interactive" : "WRONG");
        Check(transparentOk && opaqueOk, label);
    }

    std::printf("\n[5] drag semantics (core::EvaluateClickThrough, the same policy src/app applies)\n");
    Check(!nimvlets::core::EvaluateClickThrough(true, false, true).clickThrough,
          "a drag over a transparent pixel is NOT click-through (the drag keeps the pet)");
    Check(!nimvlets::core::EvaluateClickThrough(true, false, true).samplingRequired,
          "a drag needs no periodic sampling (real events keep flowing)");
    Check(nimvlets::core::EvaluateClickThrough(true, false, false).clickThrough,
          "releasing the drag restores per-pixel click-through");

    std::printf("\n[6] rendering is still intact after all of the above\n");
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    Check(CenterPixelIsTransparent(renderer), "still no white-silhouette corruption");
    Check(SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_SHAPE_POINTER, nullptr) == nullptr,
          "still no window shape was ever installed");

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL CHECKS PASSED" : "CHECKS FAILED", g_failures,
                g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
