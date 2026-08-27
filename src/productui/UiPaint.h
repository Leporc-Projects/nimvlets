#pragma once

#include "productui/UiGeometry.h"
#include "productui/UiTheme.h"

struct SDL_Renderer;
struct SDL_Texture;

namespace nimvlets::productui {

// Capa de dibujo delgada sobre SDL_Renderer para el Product UI. Trabaja
// en coordenadas LÓGICAS (puntos): cada método multiplica por `scale`
// (densidad de píxeles de la ventana) antes de llamar a SDL, así el
// layout es alto-DPI-correcto sin SDL_SetRenderLogicalPresentation (el
// texto se rasteriza aparte a tamaño de píxel real — ver TextCache).
//
// No es un framework de UI: solo los primitivos que la Collection de
// Block 06 necesita (block brief §5, "Implement only the components
// Nimvlets actually needs").
class UiPainter {
 public:
    UiPainter(SDL_Renderer* renderer, float scale) : renderer_(renderer), scale_(scale) {}

    SDL_Renderer* Renderer() const { return renderer_; }
    float Scale() const { return scale_; }

    void Clear(UiColor color);

    void FillRect(const UiRect& r, UiColor color);

    // Rectángulo con esquinas redondeadas (radio en puntos lógicos).
    // Implementado por spans horizontales — barato en un redraw
    // event-driven, y evita depender de SDL_RenderGeometry.
    void FillRoundRect(const UiRect& r, float radius, UiColor color);

    // Solo el CONTORNO de un round-rect, `thickness` puntos de grosor
    // hacia adentro. No rellena el interior (a diferencia de un fill).
    // Para anillos de foco y chips sin seleccionar.
    void StrokeRoundRect(const UiRect& r, float radius, float thickness, UiColor color);

    // Relleno + contorno en una sola llamada: primero FillRoundRect(fill),
    // luego StrokeRoundRect(border). Para chips seleccionados / botones
    // con borde.
    void RoundRectBorder(const UiRect& r, float radius, UiColor border, UiColor fill);

    // Dibuja `texture` (RGBA) escalada para CONTENER dentro de `box`
    // preservando el aspecto, centrada. `alpha` 0-255 modula todo.
    void DrawTextureContained(SDL_Texture* texture, const UiRect& box, unsigned char alpha = 255);

    // Dibuja `texture` estirada exactamente a `dst` (para bitmaps de
    // texto, que ya vienen al tamaño correcto).
    void DrawTextureExact(SDL_Texture* texture, const UiRect& dst, unsigned char alpha = 255);

    // Dibuja `texture` a su tamaño NATIVO en píxeles, con la esquina
    // superior izquierda en (pxX, pxY) — ya en píxeles, sin aplicar
    // `scale`. Para bitmaps de texto, que se rasterizan a tamaño de
    // píxel real.
    void BlitPixels(SDL_Texture* texture, float pxX, float pxY, unsigned char alpha = 255);

    // Recorte rectangular (para la lista scrolleable). Anidar no está
    // soportado — un push/pop por frame alcanza.
    void PushClip(const UiRect& r);
    void PopClip();

 private:
    SDL_Renderer* renderer_ = nullptr;
    float scale_ = 1.0f;
    bool clipActive_ = false;
};

}  // namespace nimvlets::productui
