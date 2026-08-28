#pragma once

#include <cstdint>

namespace nimvlets::productui {

// Color RGBA straight-alpha (0-255). POD puro, sin SDL — vive aparte de
// UiTheme.h para que la capa pura (nimvlets_productui_core:
// CollectionLayout, PetAccent) pueda cargar colores sin arrastrar el
// resto del tema, y para que los tests los verifiquen.
struct UiColor {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

    UiColor WithAlpha(std::uint8_t alpha) const { return UiColor{r, g, b, alpha}; }

    friend bool operator==(UiColor x, UiColor y) {
        return x.r == y.r && x.g == y.g && x.b == y.b && x.a == y.a;
    }
};

}  // namespace nimvlets::productui
