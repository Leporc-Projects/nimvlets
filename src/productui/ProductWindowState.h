#pragma once

#include <cstdint>

namespace nimvlets::productui {

// El estado de ProductWindow que NO necesita SDL, extraído acá para que
// tests/ pueda fijarlo (corrección de QA del owner, Block 11A). Son las
// dos decisiones que la ventana toma sin dibujar nada:
//
//   1. WalletDisplay      — cuándo un balance nuevo obliga a repintar.
//   2. ResolveWindowPresentStep — qué hace falta para poner la ventana
//                           delante del owner cuando pide "Collection…".
//
// Nada de esto es una abstracción nueva sobre la ventana: ProductWindow
// los USA (no son un modelo paralelo), y siguen siendo pocas líneas
// porque las dos preguntas son chicas. Ver docs/PRODUCT_UI.md.

// --- 1. El wallet MOSTRADO ------------------------------------------
//
// ProductWindow es la única autoridad del balance que se dibuja
// (Block 10). El bug de QA del owner en Block 11A fue el eslabón que
// faltaba entre "el balance canónico cambió" y "hay que repintar":
// Collection y Shop se ensuciaban solas al recibir su modelo, Settings
// —que no recibe ninguno— no, así que un clic contado sobre el Nimvlet
// mutaba el AppState pero la cabecera de Settings seguía mostrando el
// número viejo hasta cambiar de sección. El balance se dibuja en la
// cabecera COMPARTIDA, así que la invalidación no depende de la sección:
// si el número cambió, hay que repintar lo que esté visible.
class WalletDisplay {
 public:
    std::uint64_t Value() const { return value_; }

    // Devuelve true si el valor CAMBIÓ — es decir, si la sección visible
    // tiene que repintarse ya. Un push con el mismo número no invalida
    // nada: la invalidación mínima necesaria y ni una más (brief §5).
    bool Set(std::uint64_t balance) {
        if (balance == value_) {
            return false;
        }
        value_ = balance;
        return true;
    }

 private:
    std::uint64_t value_ = 0;
};

// --- 2. Traer la ventana al frente ----------------------------------

// Qué hace falta para que el owner vuelva a VER la ventana del Product
// UI tras elegir "Collection…" en el menú rápido.
enum class WindowPresentStep : std::uint8_t {
    // No hay ventana que presentar (todavía no existe / ya se cerró):
    // el que llama la crea.
    kNone,
    // Está MINIMIZADA. Ni SDL_ShowWindow ni SDL_RaiseWindow alcanzan:
    // el primero corta antes porque una ventana minimizada no es
    // SDL_WINDOW_HIDDEN, y el backend de Cocoa de la SDL pineada ignora
    // el raise mientras `[nswindow isMiniaturized]` (leído de la fuente,
    // AGENTS.md §4). Hay que restaurarla ANTES de subirla.
    kRestoreThenRaise,
    // Visible: alcanza con subirla y activar la app.
    kRaise,
};

constexpr WindowPresentStep ResolveWindowPresentStep(bool exists, bool minimized) {
    if (!exists) {
        return WindowPresentStep::kNone;
    }
    return minimized ? WindowPresentStep::kRestoreThenRaise : WindowPresentStep::kRaise;
}

}  // namespace nimvlets::productui
