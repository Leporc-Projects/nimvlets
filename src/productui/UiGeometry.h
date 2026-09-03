#pragma once

namespace nimvlets::productui {

// Rectángulo en coordenadas LÓGICAS (puntos, no píxeles) — el Product
// UI hace todo su layout en puntos y la capa de dibujo multiplica por
// el factor de escala de pantalla al final (alto-DPI correcto, block
// brief §24). Puro, sin SDL.
struct UiRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    float Right() const { return x + w; }
    float Bottom() const { return y + h; }
    float CenterX() const { return x + w * 0.5f; }
    float CenterY() const { return y + h * 0.5f; }

    bool Contains(float px, float py) const {
        return px >= x && py >= y && px < x + w && py < y + h;
    }

    // Rectángulo encogido `d` puntos por cada lado (o agrandado si `d`
    // es negativo).
    UiRect Inset(float d) const { return UiRect{x + d, y + d, w - 2.0f * d, h - 2.0f * d}; }

    // Intersección con `o`. Si no se solapan devuelve un rect de ancho /
    // alto 0 (los primitivos de dibujo lo tratan como no-op). Se usa para
    // recortar el hero-stage teñido al panel enmarcado del hero, sin
    // depender de un clip de round-rect (DEC-148).
    UiRect ClampedTo(const UiRect& o) const {
        const float nx = x > o.x ? x : o.x;
        const float ny = y > o.y ? y : o.y;
        const float rx = Right() < o.Right() ? Right() : o.Right();
        const float ry = Bottom() < o.Bottom() ? Bottom() : o.Bottom();
        return UiRect{nx, ny, rx > nx ? rx - nx : 0.0f, ry > ny ? ry - ny : 0.0f};
    }
};

}  // namespace nimvlets::productui
