#pragma once

#include <string>
#include <unordered_map>

#include "platform/TextRasterizer.h"
#include "productui/TextLayout.h"
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
// (HAlign vive en productui/TextLayout.h — puro y testeable.)

// Dibuja `utf8` con su BASELINE en la y lógica `baselineY`, alineado en
// x respecto de `anchorX`. `maxWidthLogical > 0` recorta con "…".
// Devuelve el ancho lógico dibujado. No-op silencioso si la plataforma
// no rasteriza texto (Windows/Linux por ahora). `family` / `tracking`
// (Block 12A refinement) por defecto sans / natural — los callers de
// cuerpo no cambian.
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
    int maxWidthLogical = 0,
    platform::TextFamily family = platform::TextFamily::kSans,
    double tracking = 0.0);

// Overload por ROL de fuente tokenizado (Block 12A refinement): saca
// tamaño / peso / familia / interletraje de `role`. Es el camino que
// prefieren los rótulos de display (marca, hero, sección, nav).
float DrawText(
    UiPainter& painter,
    TextCache& cache,
    const std::string& utf8,
    const type::FontRole& role,
    UiColor color,
    float anchorX,
    float baselineY,
    HAlign align,
    int maxWidthLogical = 0);

// Ancho lógico que tendría `utf8` sin recortar — para pasadas de layout
// fino que la capa pura de CollectionLayout no cubre (dimensionar un
// botón al texto exacto, etc.).
float MeasureText(
    TextCache& cache, const std::string& utf8, double pointSize, platform::TextWeight weight,
    float scale, platform::TextFamily family = platform::TextFamily::kSans, double tracking = 0.0);

float MeasureText(TextCache& cache, const std::string& utf8, const type::FontRole& role, float scale);

// Dibuja `utf8` con ajuste de línea por palabras (greedy) dentro de
// `maxWidthLogical`, todas las líneas alineadas a la izquierda contra
// `x`. La PRIMERA línea tiene su baseline en `firstBaselineY`; cada
// línea siguiente `lineHeightLogical` más abajo. Corta a `maxLines`
// líneas — si aún queda texto, la última termina en "…". Devuelve la
// cantidad de líneas dibujadas. No-op (devuelve 0) si la plataforma no
// rasteriza texto. Las descripciones editoriales de Block 07 son de un
// par de frases: esto las hace envolver limpio en la columna del hero
// (brief §19) sin un motor de tipografía completo.
int DrawTextWrapped(
    UiPainter& painter,
    TextCache& cache,
    const std::string& utf8,
    double pointSize,
    platform::TextWeight weight,
    UiColor color,
    float x,
    float firstBaselineY,
    float maxWidthLogical,
    float lineHeightLogical,
    int maxLines);

}  // namespace nimvlets::productui
