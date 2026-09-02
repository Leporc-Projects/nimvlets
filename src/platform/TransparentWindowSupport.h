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

// Trae la aplicación al frente y le da el foco de teclado (macOS:
// -[NSApplication activateIgnoringOtherApps:]). Nimvlets corre como
// accessory app (sin ícono en el Dock) para el pet; cuando se abre la
// ventana de producto (block brief §6: "a normal focusable application
// window") hay que activar la app para que esa ventana reciba teclado
// y clicks de contenido sin el "primer click activa" del sistema.
// Windows/Linux: no-op declarado por ahora (el Product UI solo se
// valida en macOS este bloque — ver docs/PRODUCT_UI.md §9).
void BringApplicationToForeground();

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
// This is the click-through mechanism Nimvlets drives itself, used on
// every platform where NativeShapeHitTestIsRenderSafe() is false (see
// its doc comment): Windows, and macOS under the software renderer that
// DEC-083 made the macOS visual baseline. src/app calls this once per
// state change (never every frame) with the result of
// core::EvaluateClickThrough() over the current animation frame's real
// alpha-derived hit region (core::AlphaMask::Contains(), see
// docs/ANIMATION_RUNTIME.md) plus a sampled global cursor position.
//
// On macOS this is only authoritative once MakeClickThroughAuthoritative()
// below has run — otherwise SDL's own Cocoa backend overwrites it on
// every real mouse-moved event. Call that first, then this.
//
// Returns the *actual* resulting state, read back from the native
// property immediately after setting it (on macOS:
// `NSWindow.ignoresMouseEvents` read right after the assignment) rather
// than just echoing the requested value — so a caller instrumenting the
// click-through pipeline can tell "we asked for X" apart from "the OS
// actually applied X" (see docs/PLATFORM_SPIKE.md's click-through
// instrumentation).
bool SetWindowClickThrough(SDL_Window* window, bool clickThrough);

// Lee el estado de click-through REAL del sistema para `window` SIN
// escribirlo (en macOS: `NSWindow.ignoresMouseEvents`). Existe separada
// de SetWindowClickThrough() justamente para poder instrumentar
// "pedimos X" contra "el OS realmente tiene X" DESPUÉS de actividad
// real de mouse de Cocoa/SDL, no solo en el instante de la asignación
// — ver docs/PLATFORM_SPIKE.md, sección de instrumentación de
// click-through. En una plataforma sin mecanismo propio devuelve el
// último valor que este adaptador aplicó (Linux/Wayland: siempre
// false).
bool ReadWindowClickThrough(SDL_Window* window);

// Hace que la decisión per-pixel de Nimvlets sea AUTORITATIVA sobre el
// estado nativo de click-through de `window`: después de esta llamada,
// ninguna otra parte del proceso (incluido el propio backend Cocoa de
// SDL) puede cambiar ese estado por la espalda.
//
// Por qué existe (Block 05, pasada de resolución de click-through —
// ver DEC-086 en docs/DECISION_LOG.md; la causa raíz se probó leyendo
// la fuente pineada de SDL 3.4.12 y con un repro nativo mínimo, no por
// suposición):
//
//   En SDL 3.4.12 hay EXACTAMENTE dos escritores de
//   `NSWindow.ignoresMouseEvents` (`grep -rn ignoresMouseEvents src/`
//   sobre la fuente pineada):
//     1. `Cocoa_UpdateWindowShape()` (src/video/cocoa/SDL_cocoashape.m:50)
//        — solo se alcanza desde `SDL_SetWindowShape()`.
//     2. `-[Cocoa_WindowListener updateIgnoreMouseState:]`
//        (src/video/cocoa/SDL_cocoawindow.m:1073) — se llama SOLO desde
//        `-mouseMoved:` (misma unidad, línea 1893), bajo la guarda
//        `(window->flags & SDL_WINDOW_TRANSPARENT)`; `-mouseDragged:`,
//        `-rightMouseDragged:` y `-otherMouseDragged:` reenvían todos a
//        `-mouseMoved:`.
//
//   El caso (2) lee la superficie de forma desde la propiedad pública
//   `SDL_PROP_WINDOW_SHAPE_POINTER` y, cuando NO hay ninguna instalada,
//   asigna `ignoresMouseEvents = NO` INCONDICIONALMENTE. Nuestra
//   ventana es SDL_WINDOW_TRANSPARENT y — bajo el renderer de software,
//   que es el baseline visual de macOS desde DEC-083 — no puede tener
//   forma instalada (ver NativeShapeHitTestIsRenderSafe() abajo), así
//   que SDL pisa nuestra decisión en cada evento de mouse-moved real.
//   Medido con un repro nativo mínimo con los flags de ventana de
//   producción: UN solo NSEventTypeMouseMoved entregado a nuestra
//   propia ventana da vuelta el valor de YES a NO.
//
//   Eso es exactamente la carrera que la nota histórica de este archivo
//   describía como "poll-driven fallback interactivamente probado poco
//   confiable": el poll no estaba mal medido, estaba PELEANDO contra
//   otro escritor. Subir la frecuencia del poll nunca podía ganar esa
//   carrera, solo achicarla.
//
// Qué hace la implementación de macOS: intercepta
// `-setIgnoresMouseEvents:` para ESTA ventana (override agregado a su
// propia clase vía el runtime de Objective-C, con swizzle in-place solo
// si esa clase ya lo implementaba) y descarta cualquier escritura que
// no venga de este adaptador. Nimvlets sigue aplicando el valor
// llamando a la IMP original directamente, así que el comportamiento de
// AppKit no cambia en nada más. No hay hook global de input, no hay
// permiso de TCC, no hay captura de pantalla: es configuración de
// nuestra propia ventana dentro de nuestro propio proceso.
//
// Devuelve true si la plataforma necesitaba (y pudo instalar) esa
// garantía. Windows/Linux devuelven false: ahí nadie más escribe el
// estado de click-through, así que no hay nada que proteger — y
// SetWindowClickThrough() sigue siendo autoritativo por sí solo.
bool MakeClickThroughAuthoritative(SDL_Window* window);

// Cuántas escrituras EXTERNAS al estado de click-through se
// interceptaron y descartaron desde el arranque del proceso (macOS:
// intentos de SDL de pisar `ignoresMouseEvents`). Siempre 0 en una
// plataforma donde MakeClickThroughAuthoritative() devolvió false.
//
// Es un número de DIAGNÓSTICO, no de control: un valor creciente es la
// prueba directa de que el mecanismo descripto arriba sigue vivo en
// esta versión de SDL — y de que la política de Nimvlets sigue ganando.
unsigned long long ForeignClickThroughWriteCount();

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
// - macOS, `usingSoftwareRenderer` true: FALSE — Block 05's
//   renderer-resolution pass found the white-silhouette corruption
//   (DEC-083's addendum) and this stabilization pass then found its
//   ACTUAL root cause, which is neither Cocoa nor "SDL_video.c
//   bookkeeping" as that addendum guessed. It is a plain, deterministic
//   consequence of the RENDER layer, reproduced from the pinned source:
//
//     `SDL_RenderPresent()` calls `SDL_RenderApplyWindowShape()`
//     (src/render/SDL_render.c:5463-5488) for any transparent window.
//     That function builds a texture from the shape surface and sets a
//     CUSTOM blend mode on it —
//     `SDL_ComposeCustomBlendMode(ZERO, SRC_ALPHA, ADD, ZERO,
//     SRC_ALPHA, ADD)`, i.e. "multiply the destination by the shape's
//     alpha". The software renderer implements NO `SupportsBlendMode`
//     hook, so `IsSupportedBlendMode()` (SDL_render.c:1409) rejects
//     every custom mode and the call FAILS. SDL ignores that failure by
//     design ("There's nothing we can do if this fails, so just keep on
//     going"), which leaves the shape texture on its DEFAULT
//     `SDL_BLENDMODE_BLEND` — so instead of masking our content, the
//     white shape bitmap is PAINTED OVER it, every present.
//
//   Measured with a standalone SDL3 program using the production window
//   flags (no app code): on "software" the custom blend mode is
//   rejected ("That operation is not supported") and the read-back
//   centre pixel goes from (0,0,0,0) to (255,255,255,255) the moment a
//   shape is installed; on "metal" the same blend mode is accepted and
//   the read-back never changes. `SDL_SetWindowShape(window, NULL)`
//   restores rendering immediately — so the corruption is NOT
//   permanent, correcting the addendum's "permanently corrupts every
//   subsequent present" claim. It is simply active for exactly as long
//   as a shape is installed.
//
//   Consequence for this function: the shape path is unusable while the
//   software renderer is the macOS visual baseline, not because of
//   anything Cocoa does, but because SDL's own renderer draws the mask
//   as visible content there. Nimvlets does not patch vendored SDL3 to
//   work around it; it stops installing shapes and takes ownership of
//   the hit-test policy itself — see MakeClickThroughAuthoritative()
//   above.
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

// True if it's worth running the cursor-sampled click-through path
// (src/app/SpikeApp.cpp's hoverScheduler_ / PollHover() /
// UpdateClickThrough(), which calls SetWindowClickThrough() above) on
// this platform (with this renderer driver). Only ever consulted when
// NativeShapeHitTestIsRenderSafe() is false for the same
// `usingSoftwareRenderer` value — irrelevant otherwise, since that
// path isn't used at all when the native shape path is render-safe.
//
// "Meaningful" here answers only "can this mechanism ever change what
// the OS does on this platform?". WHEN the sampling actually runs is a
// separate, finer question answered by core::EvaluateClickThrough():
// since Block 05's click-through resolution pass, sampling is armed
// only while the cursor is inside the window rect, not permanently —
// see that header for why that is both sufficient and much cheaper.
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
// - macOS, `usingSoftwareRenderer` true: true — the finding above
//   means the native shape path is not safe there, so SpikeApp drives
//   click-through itself through SetWindowClickThrough() (independently
//   confirmed safe under the software renderer: it never installs a
//   shape, so SDL_RenderApplyWindowShape() has nothing to draw). Since
//   Block 05's click-through resolution pass this is no longer a
//   "fallback that races SDL" — MakeClickThroughAuthoritative() above
//   makes Nimvlets the only writer of the native state first.
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

// True if, on this platform/backend, SDL_SetWindowPosition() can really
// move a normal toplevel window to an absolute screen position — the
// operation Settings' "Reset position" recovery action needs (Block
// 11B). This is a static platform fact, not runtime state; src/app
// queries it once to decide whether to offer the action, and
// SpikeApp::ResetPetPositionToSafeDefault() re-checks it (and the real
// return value of SDL_SetWindowPosition()) rather than assuming.
//
// - macOS: true — Cocoa always lets a window move itself
//   (-[NSWindow setFrameOrigin:], reached by SDL's Cocoa_SetWindowPosition).
// - Windows: true — SetWindowPos, the standard Win32 mechanism (not run
//   on real Windows hardware in this project — see docs/PLATFORM_SPIKE.md
//   — but there is no protocol-level restriction the way Wayland has one).
// - Linux/X11: true — XMoveWindow; X11 always let a client reposition
//   itself.
// - Linux/Wayland: false — xdg-shell has no client-requestable absolute
//   position for a plain toplevel; SDL returns "wayland cannot position
//   non-popup windows" (see docs/LINUX_PLATFORM.md §3.3/§6). Settings
//   disables "Reset position" on that backend instead of faking it
//   (brief §9). Delegated to the pure, unit-tested
//   platform::LinuxBackendSupportsPositionRestore() table.
bool AbsoluteWindowPositioningSupported();

}  // namespace nimvlets::platform
