#pragma once

#include <cstdint>
#include <memory>

#include "platform/SystemShellTypes.h"

namespace nimvlets::platform {

// La presencia de Nimvlets en el System Shell (macOS: NSStatusItem con
// menú rápido — block brief §14). Interfaz; la implementación real vive
// en src/platform/<os>/. Un adapter no-op es una implementación válida
// (Windows/Linux por ahora).
//
// Las acciones del menú se entregan a src/app como SDL_EVENT_USER
// empujados en el hilo principal: .type == el tipo que src/app registró
// con SDL_RegisterEvents(1) y pasó a Install(), .code == int(ShellAction).
// Así el menú nativo se integra en el MISMO event loop que todo lo
// demás, sin callbacks entre hilos ni estado global compartido.
class SystemShell {
 public:
    virtual ~SystemShell() = default;

    // Instala la presencia nativa. Devuelve false si la plataforma no la
    // soporta (el adapter no-op devuelve false pero no es un error).
    virtual bool Install(std::uint32_t userEventType) = 0;

    // Actualiza lo que el menú muestra (checkmarks, "Show"/"Hide",
    // nombre del pet). Barato de llamar seguido.
    virtual void SetState(const ShellState& state) = 0;

    // Quita la presencia nativa. Idempotente.
    virtual void Shutdown() = 0;
};

// Nunca devuelve nullptr. macOS: NSStatusItem real. Windows/Linux: un
// adapter no-op (la bandeja de Windows y su equivalente en Linux son
// trabajo futuro — ver docs/PRODUCT_UI.md §6/§9).
std::unique_ptr<SystemShell> CreateSystemShell();

}  // namespace nimvlets::platform
