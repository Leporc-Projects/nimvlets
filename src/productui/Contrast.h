#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "productui/UiColor.h"

namespace nimvlets::productui {

// Helpers PUROS de luminancia / contraste (Block 12A — DEC-143). Chicos
// a propósito: no es un framework de accesibilidad. Sirven para que la
// resolución de estilo de botón (ButtonStyle) y los tests puedan
// garantizar que un acento POR PET nunca produzca una etiqueta
// ilegible, sin "ojímetro" pet por pet. Header-only, sin SDL — vive en
// nimvlets_productui_core.

// Canal sRGB (0-255) -> lineal, según la definición de WCAG 2.x.
inline double SrgbChannelToLinear(std::uint8_t c) {
    const double s = static_cast<double>(c) / 255.0;
    return s <= 0.04045 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
}

// Luminancia relativa WCAG en [0, 1].
inline double RelativeLuminance(UiColor c) {
    return 0.2126 * SrgbChannelToLinear(c.r) + 0.7152 * SrgbChannelToLinear(c.g) +
           0.0722 * SrgbChannelToLinear(c.b);
}

// Ratio de contraste WCAG en [1, 21]. Simétrico en sus argumentos.
inline double ContrastRatio(UiColor a, UiColor b) {
    const double la = RelativeLuminance(a);
    const double lb = RelativeLuminance(b);
    const double hi = std::max(la, lb);
    const double lo = std::min(la, lb);
    return (hi + 0.05) / (lo + 0.05);
}

// Mezcla lineal: `k` 0 -> `a`, 1 -> `b`. Conserva el alpha de `a`.
inline UiColor Mix(UiColor a, UiColor b, double k) {
    k = std::clamp(k, 0.0, 1.0);
    const auto lerp = [&](std::uint8_t x, std::uint8_t y) {
        return static_cast<std::uint8_t>(
            std::lround(static_cast<double>(x) + (static_cast<double>(y) - static_cast<double>(x)) * k));
    };
    return UiColor{lerp(a.r, b.r), lerp(a.g, b.g), lerp(a.b, b.b), a.a};
}

// Oscurece `c` moviéndolo `amount` (0..1) hacia el negro.
inline UiColor Darken(UiColor c, double amount) {
    return Mix(c, UiColor{0, 0, 0, c.a}, amount);
}

// Devuelve `fg` oscurecido lo justo para alcanzar `minRatio` contra
// `bg` (pasos hacia el negro), o el paso más oscuro probado si ni el
// negro alcanza. NUNCA aclara, nunca devuelve algo MENOS legible que la
// entrada: si `fg` ya cumple, se devuelve tal cual.
inline UiColor EnsureContrastOn(UiColor fg, UiColor bg, double minRatio) {
    if (ContrastRatio(fg, bg) >= minRatio) {
        return fg;
    }
    UiColor best = fg;
    for (int i = 1; i <= 20; ++i) {
        best = Darken(fg, static_cast<double>(i) * 0.05);
        if (ContrastRatio(best, bg) >= minRatio) {
            return best;
        }
    }
    return best;  // fg vs bg imposible (negro sobre negro) — mejor esfuerzo
}

}  // namespace nimvlets::productui
