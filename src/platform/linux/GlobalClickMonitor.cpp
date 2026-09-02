#include "platform/GlobalClickMonitor.h"

#include <memory>

// Adapter de Linux para el monitor de clics globales (Block 11A):
// reporta HONESTAMENTE `kUnavailable`, en X11 y en Wayland. Sin
// cabeceras X11/XInput2 acá, sin protocolo Wayland, sin dependencias
// nuevas.
//
// **X11** (brief §17). `XI_RawButtonPress` de XInput2 sobre la ventana
// raíz sí daría, en teoría, una notificación pasiva del botón primario
// sin grabs, sin suprimir input y sin root. Pero este proyecto **no usa
// Xlib directamente en ningún lado**: XInput2 está activado dentro de la
// SDL pineada (`SDL_X11_XINPUT`, ver cmake/FetchSDL3.cmake), no en
// nuestro código, y `src/platform/linux/` no enlaza ni `libX11` ni
// `libXi`. Implementarlo exigiría (a) dependencias de desarrollo nuevas
// y paquetes nuevos en CI, (b) una conexión X propia en paralelo a la
// que SDL ya administra, o hurgar en la interna de SDL, y (c) un bucle
// de eventos aparte — todo eso es exactamente el tipo de pieza que
// AGENTS.md §10 pide no agregar sin una razón concreta, y que AGENTS.md
// §4 prohíbe declarar verificada sin haberla corrido. Este bloque no
// tuvo hardware Linux ni QA interactiva de Linux (docs/LINUX_PLATFORM.md
// §13 ya registra esa brecha). Queda diseñado, no fingido.
//
// **Wayland** (brief §17). No hay camino legítimo, y no es una
// limitación de este proyecto sino del diseño del protocolo: un cliente
// Wayland ordinario solo recibe input cuando el compositor le da foco al
// puntero sobre su propia superficie. Las únicas rutas para "ver clics
// en cualquier lado" serían capturar la pantalla (prohibido de forma
// permanente — AGENTS.md §5), leer `/dev/input` (root/grupo `input`,
// prohibido), o un portal de captura de entrada, cuya semántica
// **desvía/captura** el puntero en vez de observarlo pasivamente — que
// rompería el uso normal del escritorio y no es lo que esta feature
// hace. El brief §17 lo dice sin vueltas: honest limitation > fake
// parity. Wayland: el modo global no está disponible.
//
// Consecuencia de producto en Linux, hoy: Settings dibuja el control
// "Click counting" con "Anywhere" NO elegible y la línea de estado "Not
// available on this system". El modo "Nimvlet only" funciona
// exactamente igual que siempre. Ver docs/GLOBAL_CLICK_MODE.md §9 y
// docs/LINUX_PLATFORM.md §14.

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
