#include "platform/TransparentWindowSupport.h"
#include "platform/RendererPolicy.h"

#include <SDL3/SDL.h>

#import <AppKit/AppKit.h>
#import <objc/runtime.h>

// See docs/PLATFORM_SPIKE.md for the full click-through investigation
// this file's current shape is based on. Summary of the journey, kept
// here because it is exactly the kind of thing a future reader will
// otherwise re-discover the hard way:
//
// 1. SDL_SetWindowShape() was evaluated first and initially rejected on
//    community reports (libsdl-org/SDL#12683, #11199) suggesting it
//    couples the click-through mask to rendering.
// 2. The alternative — sampling the cursor ourselves and toggling
//    NSWindow.ignoresMouseEvents (SetWindowClickThrough(), below) — was
//    shipped instead, and macOS QA found it unreliable.
// 3. Reading the pinned SDL 3.4.12 Cocoa source showed step 1's
//    rejection was wrong *for macOS with an accelerated renderer*:
//    Cocoa_UpdateWindowShape() (SDL_cocoashape.m) only ever touches
//    ignoresMouseEvents. So the native shape path became the macOS
//    mechanism, event-driven and poll-free.
// 4. DEC-083 then made the SOFTWARE renderer the macOS visual baseline
//    (it is the one that renders Bunny/Frin correctly), and under it a
//    window shape corrupts the picture — so the shape path had to go,
//    and step 2's cursor-sampled mechanism came back, along with step
//    2's original unreliability.
// 5. THIS pass (Block 05 stabilization) finally established WHY each of
//    those two things happens, from the pinned source plus minimal
//    native repros rather than from inference — see
//    NativeShapeHitTestIsRenderSafe() and MakeClickThroughAuthoritative()
//    in platform/TransparentWindowSupport.h for both root causes in
//    full. The short version: the shape corrupts rendering because
//    SDL's *renderer* paints the shape bitmap as content when the
//    driver rejects its custom blend mode, and the cursor-sampled
//    mechanism was unreliable because SDL's Cocoa backend rewrites
//    ignoresMouseEvents on every mouse-moved event when no shape is
//    installed. The first is why we do not install a shape; the second
//    is why we now take ownership of the property instead of racing for
//    it.
//
// Nothing here requires Accessibility, Input Monitoring, or any TCC
// prompt: both mechanisms are ordinary window-server/cursor-position
// queries and configuration of our own window, never a global input
// hook (AGENTS.md §5/§14).

namespace nimvlets::platform {

namespace {

NSWindow* CocoaWindowFor(SDL_Window* window) {
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    void* ptr = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
    return (__bridge NSWindow*)ptr;
}

// --- Propiedad exclusiva de NSWindow.ignoresMouseEvents ---------------
//
// Ver MakeClickThroughAuthoritative() en
// platform/TransparentWindowSupport.h para la causa raíz completa y su
// evidencia. Acá solo el mecanismo.
//
// `g_ownedWindow` es una referencia PRESTADA (AGENTS.md §9: los objetos
// nativos los posee SDL/AppKit, este código solo los configura) y hay
// exactamente una ventana viva por proceso en este producto ("exactly
// one Nimvlet is visible at a time", AGENTS.md §2) -- por eso un único
// puntero alcanza y no hace falta una tabla. Se compara por identidad
// en el interceptor, así que aunque el override termine viviendo en una
// clase compartida, ninguna otra NSWindow del proceso cambia de
// comportamiento.
NSWindow* g_ownedWindow = nil;
IMP g_originalSetIgnoresMouseEvents = nullptr;
unsigned long long g_foreignWriteCount = 0;

void NimvletsSetIgnoresMouseEvents(id self, SEL cmd, BOOL value) {
    if (self == g_ownedWindow) {
        // Escritura EXTERNA (en la práctica: SDL_cocoawindow.m's
        // -updateIgnoreMouseState:). Se cuenta como diagnóstico y se
        // descarta -- la política per-pixel de Nimvlets es la única
        // fuente de verdad de esta propiedad. Nunca se llama a la IMP
        // original acá: eso es justamente lo que la haría perder.
        ++g_foreignWriteCount;
        return;
    }
    if (g_originalSetIgnoresMouseEvents != nullptr) {
        reinterpret_cast<void (*)(id, SEL, BOOL)>(g_originalSetIgnoresMouseEvents)(self, cmd, value);
    }
}

// Única vía por la que Nimvlets escribe la propiedad una vez instalado
// el interceptor: llama a la IMP original DIRECTAMENTE, saltándose el
// despacho normal (y por lo tanto el interceptor). Antes de instalar,
// cae al setter normal.
void ApplyOwnedIgnoresMouseEvents(NSWindow* nsWindow, BOOL value) {
    if (g_originalSetIgnoresMouseEvents != nullptr && nsWindow == g_ownedWindow) {
        reinterpret_cast<void (*)(id, SEL, BOOL)>(g_originalSetIgnoresMouseEvents)(
            nsWindow, @selector(setIgnoresMouseEvents:), value);
        return;
    }
    nsWindow.ignoresMouseEvents = value;
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
    ApplyOwnedIgnoresMouseEvents(nsWindow, clickThrough ? YES : NO);
    // Read the property back rather than trusting the assignment stuck —
    // this is the ground truth src/app's click-through instrumentation
    // compares against "what we asked for" (see
    // docs/PLATFORM_SPIKE.md's click-through instrumentation section).
    return nsWindow.ignoresMouseEvents == YES;
}

bool ReadWindowClickThrough(SDL_Window* window) {
    NSWindow* nsWindow = CocoaWindowFor(window);
    return nsWindow != nil && nsWindow.ignoresMouseEvents == YES;
}

bool MakeClickThroughAuthoritative(SDL_Window* window) {
    NSWindow* nsWindow = CocoaWindowFor(window);
    if (nsWindow == nil) {
        SDL_Log("nimvlets: platform/macos could not resolve NSWindow; click-through ownership NOT installed");
        return false;
    }
    if (g_ownedWindow == nsWindow && g_originalSetIgnoresMouseEvents != nullptr) {
        return true;  // ya instalado para esta misma ventana
    }

    Class cls = [nsWindow class];
    const SEL sel = @selector(setIgnoresMouseEvents:);
    Method method = class_getInstanceMethod(cls, sel);
    if (method == nullptr) {
        SDL_Log("nimvlets: -setIgnoresMouseEvents: not found on %s; click-through ownership NOT installed",
                class_getName(cls));
        return false;
    }

    // Se captura la IMP original ANTES de tocar nada -- es la que
    // ApplyOwnedIgnoresMouseEvents() usará para escribir de verdad.
    g_originalSetIgnoresMouseEvents = method_getImplementation(method);
    g_ownedWindow = nsWindow;

    // Preferido: AGREGAR el override a la clase concreta de la ventana
    // (SDL3Window). class_addMethod() falla si esa clase YA implementa
    // el selector por sí misma; solo entonces se recurre al swizzle
    // in-place. La diferencia importa: agregando, la implementación de
    // NSWindow queda intacta para cualquier otra ventana del proceso.
    const bool added = class_addMethod(
        cls, sel, reinterpret_cast<IMP>(NimvletsSetIgnoresMouseEvents), method_getTypeEncoding(method)) == YES;
    if (!added) {
        method_setImplementation(class_getInstanceMethod(cls, sel), reinterpret_cast<IMP>(NimvletsSetIgnoresMouseEvents));
    }

    SDL_Log(
        "nimvlets: click-through ownership installed on %s (%s) — Nimvlets is now the only writer of "
        "NSWindow.ignoresMouseEvents for this window",
        class_getName(cls), added ? "subclass override" : "in-place swizzle");
    return true;
}

unsigned long long ForeignClickThroughWriteCount() {
    return g_foreignWriteCount;
}

bool NativeShapeHitTestIsRenderSafe(bool usingSoftwareRenderer) {
    // Causa raíz real, probada en esta pasada de estabilización contra
    // la fuente pineada y con un repro nativo mínimo (ver el comentario
    // largo de esta misma función en platform/TransparentWindowSupport.h):
    // instalar una forma hace que SDL_RenderPresent() DIBUJE el bitmap
    // de la forma sobre nuestro contenido cuando el driver activo
    // rechaza el blend mode custom que SDL_RenderApplyWindowShape()
    // intenta usar -- y el driver de software es exactamente uno que lo
    // rechaza (no implementa SupportsBlendMode). De ahí la silueta
    // blanca opaca. No es nada que este archivo pueda evitar
    // reordenando llamadas: la decisión es simplemente no instalar
    // ninguna forma mientras el renderer de software sea el baseline.
    //
    // La regla en sí vive en platform::MacOSNativeShapeIsRenderSafe()
    // (pura, sin SDL/AppKit) para que la CI de las cuatro plataformas
    // pueda fijarla como invariante -- mismo patrón que
    // LinuxBackendPolicy. Acá solo se delega.
    return MacOSNativeShapeIsRenderSafe(usingSoftwareRenderer);
}

bool ClickThroughPollingIsMeaningful(bool usingSoftwareRenderer) {
    // Con el driver acelerado (el histórico), NativeShapeHitTestIsRenderSafe()
    // ya es true en macOS, así que SpikeApp jamás entra a esta rama --
    // false por documentación/consistencia, no porque se haya medido
    // nada acá.
    //
    // Con `usingSoftwareRenderer` true (DEC-083), en cambio, SÍ se
    // consulta de verdad: sin ruta de forma, SpikeApp maneja el
    // click-through él mismo con SetWindowClickThrough() sobre muestras
    // reales del cursor. Eso ya no es "un fallback que pelea contra
    // SDL": MakeClickThroughAuthoritative() corre primero y deja a
    // Nimvlets como único escritor de la propiedad. Y el muestreo ya no
    // es permanente -- core::EvaluateClickThrough() lo arma solo
    // mientras el cursor está DENTRO del rectángulo de la ventana.
    return usingSoftwareRenderer;
}

RendererPlatform CurrentRendererPlatform() {
    return RendererPlatform::kMacOS;
}

}  // namespace nimvlets::platform
