#pragma once

#include <string>
#include <unordered_map>

#include "platform/TextRasterizer.h"
#include "productui/UiPaint.h"
#include "productui/UiTheme.h"

struct SDL_Renderer;
struct SDL_Texture;

namespace nimvlets::productui {

// Cachea bitmaps de texto ya rasterizados (platform::RasterizeText) como
// SDL_Texture, indexados por contenido + tamaño + peso + color + ancho
// máximo + escala. El Product UI redibuja solo ante un cambio real
// (event-driven), así que la mayoría de los frames son cache hits.
// Clear() destruye todo — se llama al cerrar la ventana (block brief
// §18: liberar recursos de UI al cerrar).
class TextCache {
 public:
    explicit TextCache(SDL_Renderer* renderer) : renderer_(renderer) {}
    ~TextCache();

    TextCache(const TextCache&) = delete;
    TextCache& operator=(const TextCache&) = delete;

    struct Glyphs {
        SDL_Texture* texture = nullptr;  // nullptr si la plataforma no rasteriza texto
        int width = 0;                   // píxeles
        int height = 0;                  // píxeles
        int baseline = 0;                // píxeles desde el borde superior
    };

    // Devuelve (creando si hace falta) el bitmap para `request`. La
    // textura la posee el cache — no destruirla.
    Glyphs Acquire(const platform::TextRasterRequest& request);

    void Clear();

 private:
    struct Entry {
        SDL_Texture* texture = nullptr;
        int width = 0;
        int height = 0;
        int baseline = 0;
    };

    static std::string KeyOf(const platform::TextRasterRequest& request);

    SDL_Renderer* renderer_ = nullptr;
    std::unordered_map<std::string, Entry> entries_;
};

// --- Helpers de dibujo de texto ----------------------------------

enum class HAlign { kLeft, kCenter, kRight };

// Dibuja `utf8` con su BASELINE en la y lógica `baselineY`, alineado en
// x respecto de `anchorX`. `maxWidthLogical > 0` recorta con "…".
// Devuelve el ancho lógico dibujado. No-op silencioso si la plataforma
// no rasteriza texto (Windows/Linux por ahora).
float DrawText(
    UiPainter& painter,
    TextCache& cache,
    const std::string& utf8,
    double pointSize,
    platform::TextWeight weight,
    UiColor color,
    float anchorX,
    float baselineY,
    HAlign align,
    int maxWidthLogical = 0);

// Ancho lógico que tendría `utf8` sin recortar — para pasadas de layout
// fino que la capa pura de CollectionLayout no cubre (dimensionar un
// botón al texto exacto, etc.).
float MeasureText(TextCache& cache, const std::string& utf8, double pointSize, platform::TextWeight weight, float scale);

}  // namespace nimvlets::productui
