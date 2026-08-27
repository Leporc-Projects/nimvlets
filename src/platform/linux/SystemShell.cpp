#include "platform/SystemShell.h"

#include <memory>

// Adapter no-op de Linux. El equivalente (un StatusNotifierItem de
// freedesktop / bandeja de AppIndicator donde el entorno lo soporte) es
// trabajo futuro — NO se finge acá. El Product UI de Block 06 solo se
// valida en macOS (block brief §24). src/app trata Install()==false como
// "esta plataforma todavía no tiene menú rápido nativo". Ver
// docs/PRODUCT_UI.md §6 y docs/LINUX_PLATFORM.md.

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
