#pragma once

// Lógica pura (sin SDL, sin X11, sin Wayland) detrás de las decisiones
// de capacidad por backend que src/platform/linux/TransparentWindowSupport.cpp
// aplica en runtime. Vive fuera de src/platform/linux/ deliberadamente:
// ese directorio solo se compila en Linux (ver el CMakeLists.txt raíz),
// pero esta lógica -- un mapeo de string a enum y tres tablas de
// booleanos -- no necesita ningún header de plataforma, así que puede
// compilarse y testearse en cualquier host (incluido este dev machine
// macOS, donde no hay forma de correr Linux real -- ver el brief de
// Block 04.1 §11). Mismo patrón que separa core::AlphaMask/
// core::DragClassifier de src/app: la geometría/decisión es pura y
// testeable; solo la llamada real a SDL/X11/Wayland vive detrás del
// seam de plataforma.
//
// Cada valor de abajo está citado con la evidencia de código fuente de
// SDL 3.4.12 (la versión pineada, ver cmake/FetchSDL3.cmake) que lo
// respalda -- ver docs/LINUX_PLATFORM.md para la investigación
// completa. Nada acá es una suposición.

namespace nimvlets::platform {

enum class LinuxVideoBackend {
    kX11,
    kWayland,
    // Cualquier otro valor de SDL_GetCurrentVideoDriver() (incluido
    // nullptr) -- este bloque solo soporta x86_64 X11/Wayland (ver el
    // brief), así que un backend Linux no reconocido nunca asume
    // ninguna capacidad.
    kOther,
};

// Mapea el string que retorna SDL_GetCurrentVideoDriver() ("x11",
// "wayland", ...) a LinuxVideoBackend. nullptr o cualquier string no
// reconocido caen en kOther.
LinuxVideoBackend ParseLinuxVideoBackend(const char* sdlVideoDriverName);

// True si SDL_SetWindowShape() es render-safe (solo afecta hit-testing
// nativo, nunca compone ni reemplaza los pixeles ya renderizados) en
// este backend -- el mismo contrato que
// platform::NativeShapeHitTestIsRenderSafe() documenta.
//
// - X11: true. `X11_UpdateWindowShape` (src/video/x11/SDL_x11shape.c
//   en la fuente pineada) llama a XShapeCombineMask/Region con
//   `ShapeInput` -- la extensión XShape aplicada a la región de
//   *entrada*, no a la de renderizado; misma familia que
//   Cocoa_UpdateWindowShape en macOS (ver
//   src/platform/macos/TransparentWindowSupport.mm).
// - Wayland: false. La fuente pineada nunca asigna
//   `device->UpdateWindowShape` para el driver Wayland (grep sobre
//   src/video/wayland/*.c confirma que no existe ningún
//   `SDL_waylandshape.c`) -- `SDL_SetWindowShape()` no tiene ningún
//   efecto ahí en absoluto, así que afirmar "render-safe" sería falso.
// - kOther: false (conservador -- ningún backend Linux fuera de
//   alcance de este bloque se asume compatible).
bool LinuxBackendSupportsNativeShapeHitTest(LinuxVideoBackend backend);

// True si vale la pena correr el fallback de click-through por polling
// (PollHover()/SetWindowClickThrough() en src/app/SpikeApp.cpp) en
// este backend -- es decir, si algún llamado nativo disparado por ese
// polling podría cambiar de verdad la entrega de input a nivel OS.
// Solo se consulta cuando LinuxBackendSupportsNativeShapeHitTest() ya
// es false (X11 nunca llega acá).
//
// - Wayland: false. Dos hechos de la fuente pineada, juntos, hacen que
//   el mecanismo de polling de Windows no tenga ningún análogo Wayland
//   posible con la API pública de SDL 3.4.12:
//   1. `Wayland_GetGlobalMouseState`
//      (src/video/wayland/SDL_waylandmouse.c) solo retorna una
//      posición real "if (mouse->focus)" -- es decir, mientras el
//      cursor ya está sobre nuestra propia ventana; fuera de eso
//      retorna (0,0), inútil para decidir nada.
//   2. No existe ninguna forma pública de restringir la input region
//      de una xdg_toplevel normal: `wl_surface_set_input_region` solo
//      se usa internamente para tooltips
//      (src/video/wayland/SDL_waylandwindow.c), y SDL no expone
//      `wl_compositor` para que un adapter externo cree su propia
//      `wl_region`. No hay ningún equivalente a
//      `NSWindow.ignoresMouseEvents`/`WS_EX_TRANSPARENT` alcanzable
//      sin hablar el protocolo Wayland en crudo, en paralelo a la
//      cola de eventos que SDL ya administra -- ver
//      docs/LINUX_PLATFORM.md para por qué ese camino no se tomó en
//      este bloque.
//   Correr el scheduler de ~60Hz de todos modos, sabiendo que
//   SetWindowClickThrough() nunca podría lograr nada ahí, sería
//   exactamente el "polling loop" que el brief §8 prohíbe -- así que
//   SpikeApp simplemente no lo arranca cuando esto es false (ver
//   usingPollDrivenClickThrough_ en src/app/SpikeApp.h).
// - X11: irrelevante en la práctica (nunca se consulta, ya que
//   LinuxBackendSupportsNativeShapeHitTest(kX11) es true), pero
//   retorna true por completitud/documentación: a diferencia de
//   Wayland, `X11_GetGlobalMouseState` sí puede consultar la posición
//   global real del cursor en cualquier momento (XQueryPointer no
//   depende de que nuestra ventana tenga foco).
bool LinuxBackendClickThroughPollingIsMeaningful(LinuxVideoBackend backend);

// True si SDL_SetWindowPosition() puede reposicionar de verdad una
// ventana toplevel normal en este backend -- ver block brief §3.
//
// - X11: true. `X11_SetWindowPosition`
//   (src/video/x11/SDL_x11window.c) llama a XMoveWindow directamente;
//   el protocolo X11 siempre permitió que un cliente se reposicione a
//   sí mismo.
// - Wayland: false. `Wayland_SetWindowPosition`
//   (src/video/wayland/SDL_waylandwindow.c) retorna literalmente
//   `SDL_SetError("wayland cannot position non-popup windows")` para
//   cualquier xdg_toplevel -- una limitación del protocolo xdg-shell
//   en sí (ningún cliente puede pedir una posición de pantalla
//   absoluta para una toplevel normal), no un bug de SDL ni de este
//   proyecto. src/app/SpikeApp.cpp no necesita consultar esta función
//   en runtime -- reacciona genéricamente al valor de retorno real de
//   SDL_SetWindowPosition() (ver su comentario en Init()) sin ningún
//   #ifdef de plataforma; esta función existe para que la política por
//   backend quede documentada y testeada de forma explícita (block
//   brief §7).
bool LinuxBackendSupportsPositionRestore(LinuxVideoBackend backend);

}  // namespace nimvlets::platform
