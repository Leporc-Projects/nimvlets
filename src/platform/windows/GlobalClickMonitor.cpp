#include "platform/GlobalClickMonitor.h"

#include <memory>

// Adapter de Windows para el monitor de clics globales (Block 11A):
// reporta HONESTAMENTE `kUnavailable`. No hay código Win32 acá — ni
// `windows.h`, ni `SetWindowsHookEx`, ni `WH_MOUSE_LL` — y eso es
// deliberado, no un olvido.
//
// **Investigación** (brief §16). El camino moderno soportado sería un
// hook de bajo nivel de MOUSE únicamente:
// `SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, hInstance, 0)`,
// filtrando `WM_LBUTTONDOWN`, devolviendo siempre `CallNextHookEx(...)`
// (nunca suprimir el input del usuario), reenviando al hilo principal y
// desinstalando con `UnhookWindowsHookEx`. Nunca `WH_KEYBOARD_LL`. Sin
// admin. Esa forma encaja perfectamente detrás de esta misma interfaz.
//
// **Por qué NO se escribe todavía.** AGENTS.md §4 prohíbe afirmar una
// conducta de plataforma que no se corrió realmente en ese OS, y el
// brief §16 es explícito: "do NOT write speculative unvalidated Win32
// code merely to claim support". Un hook global de input es justo el
// tipo de código cuya corrección NO se demuestra compilando: que no
// trague ni retrase clics, que el hilo que lo instala tenga la bomba de
// mensajes que `WH_MOUSE_LL` exige para despachar, el timeout de
// `LowLevelHooksTimeout`, y la desinstalación limpia al apagar, son
// todos hechos de RUNTIME. Este bloque no tuvo máquina Windows ni QA
// interactiva de Windows disponible, así que la capacidad se reporta
// como ausente en vez de fingida.
//
// Consecuencia de producto en Windows, hoy: Settings dibuja el control
// "Click counting" con "Anywhere" NO elegible y la línea de estado
// "Not available on this system". El modo "Nimvlet only" funciona
// exactamente igual que siempre. Ver docs/GLOBAL_CLICK_MODE.md §8.

namespace nimvlets::platform {

namespace {

class UnavailableGlobalClickMonitor final : public GlobalClickMonitor {
 public:
    GlobalClickStatus QueryStatus() const override {
        GlobalClickStatus status;  // kUnavailable / kNotRequired / inactivo
        return status;
    }

    bool RequestPermission() override { return false; }
    bool Start(GlobalPrimaryClickCallback /*callback*/, void* /*userData*/) override { return false; }
    void Stop() override {}
    bool IsActive() const override { return false; }
};

}  // namespace

std::unique_ptr<GlobalClickMonitor> CreateGlobalClickMonitor() {
    return std::make_unique<UnavailableGlobalClickMonitor>();
}

}  // namespace nimvlets::platform
