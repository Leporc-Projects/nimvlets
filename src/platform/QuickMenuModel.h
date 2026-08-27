#pragma once

#include <string>
#include <vector>

#include "platform/SystemShellTypes.h"

namespace nimvlets::platform {

enum class MenuItemKind {
    kHeader,     // texto deshabilitado (el nombre del pet)
    kSeparator,
    kAction,     // dispara `action` al elegirlo
    kCheckable,  // dispara `action`; `checked` refleja el estado actual
    kSubmenu,    // contiene `submenu`
};

struct MenuItem {
    MenuItemKind kind = MenuItemKind::kAction;
    std::string label;
    bool checked = false;
    bool enabled = true;
    ShellAction action = ShellAction::kQuit;  // significativo para kAction/kCheckable
    std::vector<MenuItem> submenu;            // solo para kSubmenu
};

struct QuickMenuModel {
    std::vector<MenuItem> items;
};

// Construye el modelo de menú rápido a partir del estado. PURO y
// determinista — el adapter de macOS (src/platform/macos/QuickMenu.mm)
// construye el NSMenu real a partir de ESटE modelo, así que este test
// cubre las etiquetas y la estructura que se envía (block brief §14/§27).
//
// Estructura (block brief §14):
//   [nombre del pet]        (header, deshabilitado)
//   ----
//   Show Nimvlet / Hide Nimvlet   (según state.petHidden)
//   Collection…
//   ----
//   Size ▸   { Small, Medium, Large }        (checkable, exactamente uno marcado)
//   Opacity ▸{ 100%, 85%, 70%, 55% }         (checkable)
//   Lock Position                            (checkable = state.lockPosition)
//   ----
//   Quit Nimvlets
QuickMenuModel BuildQuickMenuModel(const ShellState& state);

}  // namespace nimvlets::platform
