#include "platform/SystemShell.h"

#include <memory>

// Adapter no-op de Windows. La bandeja del sistema (Shell_NotifyIcon +
// un HMENU de contexto) es la implementación futura — NO se finge acá.
// El Product UI de Block 06 solo se valida en macOS (block brief §24).
// src/app trata Install()==false como "esta plataforma todavía no tiene
// menú rápido nativo": las mismas acciones siguen disponibles cuando se
// construya la bandeja, sin cambiar la interfaz. Ver docs/PRODUCT_UI.md §6.

namespace nimvlets::platform {

namespace {

class NoopShell final : public SystemShell {
 public:
    bool Install(std::uint32_t /*userEventType*/) override { return false; }
    void SetState(const ShellState& /*state*/) override {}
    void Shutdown() override {}
};

}  // namespace

std::unique_ptr<SystemShell> CreateSystemShell() {
    return std::make_unique<NoopShell>();
}

}  // namespace nimvlets::platform
