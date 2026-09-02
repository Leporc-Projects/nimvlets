#pragma once

#include <memory>

#include "platform/GlobalClickTypes.h"

namespace nimvlets::platform {

// El monitor OPT-IN de clics primarios globales (Block 11A). Una
// frontera de privacidad PROPIA, deliberadamente separada de
// TransparentWindowSupport / SystemShell / el input normal de SDL
// (brief §6): nada de lo que hace el camino estándar de interacción
// necesita esto, y nada de esto se activa jamás salvo que el owner lo
// pida explícitamente desde Settings.
//
// Reglas que el header mismo hace cumplir, no solo la documentación:
//
//   - **La única salida funcional es "pasó un clic primario".** El
//     callback no recibe coordenadas, ni botón, ni timestamp, ni
//     ventana, ni aplicación, ni modificadores — la FIRMA no tiene
//     dónde ponerlos. No se puede filtrar lo que no se puede
//     transportar.
//   - **Sin SDL acá.** Es la misma disciplina que
//     LinuxBackendPolicy/QuickMenuModel: el seam se compila y se
//     ejercita en cualquier host, y tests/ puede sustituirlo por un
//     doble puro (tests/FakeGlobalClickMonitor.h) sin ventana, sin
//     display y sin permiso del OS.
//   - **Ninguna consulta pide permiso.** Solo `RequestPermission()`
//     puede provocar un diálogo del sistema, y src/app solo la llama
//     tras un "Continue" explícito del owner sobre la explicación de
//     primera parte (brief §8). Arrancar la app NUNCA la llama.
//
// Ver docs/GLOBAL_CLICK_MODE.md para la implementación por plataforma y
// docs/PRIVACY_SECURITY.md §H para el contrato de privacidad.

// Lo ÚNICO que el monitor nativo reporta: ocurrió una presión del botón
// primario. Sin payload, a propósito (ver arriba).
//
// **Puede invocarse desde un hilo que NO es el principal** (macOS: el
// hilo dedicado del event tap — ver docs/GLOBAL_CLICK_MODE.md §5). La
// implementación que se pase acá debe ser mínima y thread-safe; la
// mutación canónica del wallet ocurre siempre después, en el hilo
// principal (src/app reenvía un SDL_EVENT_USER, que SDL documenta como
// seguro desde cualquier hilo).
using GlobalPrimaryClickCallback = void (*)(void* userData);

class GlobalClickMonitor {
 public:
    virtual ~GlobalClickMonitor() = default;

    // Consulta NO INTRUSIVA del estado completo: capacidad, permiso
    // (preflight — nunca dispara un diálogo), si el monitor corre ahora,
    // si un arranque falló, y el nombre del permiso tal como lo llama el
    // OS. Barata; src/app la llama al abrir Settings y tras cada acción.
    virtual GlobalClickStatus QueryStatus() const = 0;

    // Puede mostrar el diálogo de permiso del sistema. **Solo se llama
    // desde una acción explícita del owner** (el "Continue" de la
    // explicación), nunca al arrancar, nunca desde onboarding/Shop/
    // Collection/menú rápido (brief §8). Debe llamarse en el hilo
    // principal. Devuelve el estado del permiso INMEDIATAMENTE DESPUÉS
    // del pedido, que en macOS normalmente sigue siendo "no concedido":
    // el usuario tiene que activarlo en Ajustes del Sistema. No es un
    // fallo — ver docs/GLOBAL_CLICK_MODE.md §6.
    virtual bool RequestPermission() = 0;

    // Arranca la observación. Falla (false) si no hay capacidad, si
    // falta el permiso, o si el mecanismo nativo no pudo instalarse.
    // Idempotente: llamarla con el monitor ya activo es un no-op
    // exitoso. `callback` debe seguir siendo válido hasta Stop().
    virtual bool Start(GlobalPrimaryClickCallback callback, void* userData) = 0;

    // Detiene y desinstala TODO lo nativo. Idempotente. Tras volver,
    // garantiza que el callback ya no puede invocarse — src/app depende
    // de eso para el orden de shutdown (brief §19).
    virtual void Stop() = 0;

    virtual bool IsActive() const = 0;

    // Conveniencias con el vocabulario del brief §6; leen QueryStatus().
    GlobalClickCapability QueryCapability() const { return QueryStatus().capability; }
    GlobalClickPermission QueryPermission() const { return QueryStatus().permission; }
};

// Nunca devuelve nullptr. macOS: un event tap CoreGraphics listen-only
// sobre kCGEventLeftMouseDown. Windows/Linux: un adapter honesto que
// reporta kUnavailable — NO se finge soporte (AGENTS.md §4). Ver
// docs/GLOBAL_CLICK_MODE.md §8/§9.
std::unique_ptr<GlobalClickMonitor> CreateGlobalClickMonitor();

}  // namespace nimvlets::platform
