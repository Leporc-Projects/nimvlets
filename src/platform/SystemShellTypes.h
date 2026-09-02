#pragma once

#include <string>

#include "core/Localization.h"

// Tipos puros del "System Shell" de Block 06 — la presencia nativa de
// Nimvlets fuera de sus ventanas (en macOS: un NSStatusItem con menú
// rápido; en Windows: un icono de bandeja, futuro; en Linux: el
// equivalente donde tenga sentido). Sin SDL, sin AppKit: viven acá para
// que la CONSTRUCCIÓN del modelo de menú (platform::BuildQuickMenuModel)
// sea testeable en cualquier host, igual que LinuxBackendPolicy /
// RendererPolicy. Ver docs/PRODUCT_UI.md §6.

namespace nimvlets::platform {

// Todo lo que el menú rápido puede pedirle a la app. Se entrega a
// src/app como un SDL_EVENT_USER con .code == int(ShellAction).
enum class ShellAction {
    kTogglePetVisibility,   // Show/Hide del pet — NO es quit (block brief §17)
    // Abre/enfoca el Product UI (Collection · Shop · Settings). Antes
    // `kOpenCollection`: el nombre quedó estrecho cuando la ventana ganó
    // Shop (Block 07) y Settings (Block 08), y Block 11A fijó que
    // invocarlo con la ventana ya abierta restaura LA MISMA ventana y su
    // sección actual — no siempre "Collection". Renombrado en Block 11B
    // junto con la etiqueta visible "Open Nimvlets…". Ver
    // docs/PRODUCT_UI.md §4/§4.1/§9 y DEC-141.
    kOpenProductUi,
    kToggleLockPosition,    // bloquea/desbloquea el arrastre del pet (block brief §16)
    kSetSizeSmall,
    kSetSizeMedium,
    kSetSizeLarge,
    kSetOpacity100,
    kSetOpacity85,
    kSetOpacity70,
    kSetOpacity55,
    kSetLanguageEn,         // Block 06.1 — elección explícita de idioma
    kSetLanguageEs,
    kQuit,                  // termina toda la aplicación, limpio (block brief §14)
};

// Lo que el menú necesita mostrar (checkmarks, "Show" vs "Hide", nombre
// del pet activo, idioma). src/app lo empuja al shell cada vez que algo
// cambia; el shell reconstruye el NSMenu.
struct ShellState {
    std::string currentPetName;      // "" => se muestra "Nimvlets"
    bool petHidden = false;          // gobierna "Show Nimvlet" vs "Hide Nimvlet"
    bool lockPosition = false;
    std::string sizeChoiceId = "medium";  // "small" / "medium" / "large"
    int opacityPercent = 100;             // se compara contra {100,85,70,55}
    core::Language language = core::Language::kEn;  // idioma de TODAS las etiquetas del menú
};

}  // namespace nimvlets::platform
