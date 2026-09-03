#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Rasterización de texto del sistema para el Product UI de Block 06.
// Costura de plataforma, exactamente igual que
// platform/TransparentWindowSupport.h: uno solo de
// src/platform/macos/TextRasterizer.mm (Core Text — real),
// src/platform/windows/TextRasterizer.cpp o
// src/platform/linux/TextRasterizer.cpp (stubs por ahora) se compila
// según CMAKE_SYSTEM_NAME, así que src/productui nunca contiene un
// #ifdef para esto.
//
// Por qué acá y no en src/productui: dibujar una string con la
// tipografía del sistema (SF Pro en macOS) sin una dependencia de
// terceros (no SDL_ttf — AGENTS.md §10) requiere APIs nativas del OS
// (Core Text / DirectWrite / fontconfig+HarfBuzz). El resultado es un
// bitmap RGBA8 puro que src/productui sube como cualquier otra textura
// — nada de Core Text sale de esta costura.
//
// Block 06 solo valida el Product UI en macOS (block brief §24). Las
// otras dos plataformas COMPILAN (los stubs devuelven false), así que
// la arquitectura no impide una implementación futura, pero no se
// finge una — ver docs/PRODUCT_UI.md §9.

namespace nimvlets::platform {

// Pesos expuestos (los únicos que el Product UI necesita). Se mapean al
// peso nativo más cercano; una plataforma sin control fino de peso
// puede colapsarlos.
enum class TextWeight {
    kRegular,
    kMedium,
    kSemibold,
};

// Familia tipográfica SEMÁNTICA (Block 12A refinement — DEC-146). NO se
// nombra una fuente concreta: `kSans` = la sans de UI del sistema (SF
// Pro en macOS), `kSerif` = el *diseño serif* del sistema (New York en
// macOS, resuelto por NSFontDescriptor — NINGÚN asset empacado). Una
// plataforma sin variante serif cae a la sans. Windows/Linux todavía no
// dibujan texto de producto (los stubs devuelven false), así que no hay
// divergencia real: cuando lleguen DirectWrite / fontconfig, `kSerif`
// mapea al serif de esa plataforma.
enum class TextFamily {
    kSans,
    kSerif,
};

struct TextRasterRequest {
    // UTF-8. Una sola línea (los '\n' se tratan como espacio) — el
    // Product UI de este bloque no tiene ningún texto multilínea.
    std::string utf8;

    // Tamaño tipográfico en PUNTOS lógicos. El bitmap se rasteriza a
    // `pointSize * scale` píxeles para quedar nítido en alto-DPI.
    double pointSize = 13.0;

    // Factor de escala de pantalla (2.0 en Retina). pixeles =
    // pointSize * scale.
    double scale = 1.0;

    TextWeight weight = TextWeight::kRegular;

    // Familia tipográfica (ver TextFamily). `kSans` por defecto — el
    // texto de cuerpo, metadata, botones y wallet no cambia.
    TextFamily family = TextFamily::kSans;

    // Interletraje EXTRA en PUNTOS lógicos (se escala a píxeles con
    // `scale` internamente). 0 = natural. Un toque chico (~0.2-0.5 pt)
    // sobre los rótulos serif chicos les da un aire editorial sin
    // volverlos temáticos (Block 12A refinement).
    double tracking = 0.0;

    // Color del texto. Los glyphs salen tintados de este color con
    // alpha = cobertura del glyph (straight alpha, para encajar con
    // SDL_BLENDMODE_BLEND / SDL_PIXELFORMAT_RGBA32 como el resto del
    // runtime).
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;

    // 0 = sin límite (ancho natural). > 0 = ancho máximo en PÍXELES: el
    // texto que no entra se recorta al final con puntos suspensivos
    // ("…"), siempre en una sola línea.
    int maxWidthPx = 0;
};

struct RasterizedText {
    int width = 0;     // píxeles
    int height = 0;    // píxeles
    int baseline = 0;  // píxeles desde el borde superior hasta la baseline

    // RGBA8, row-major, top-to-bottom, straight alpha. Tamaño exacto
    // width * height * 4.
    std::vector<std::uint8_t> pixels;
};

// true si esta plataforma sabe rasterizar texto del sistema. macOS:
// true. Windows/Linux: false por ahora.
bool TextRasterizationAvailable();

// Rasteriza `request` en `out`. Devuelve false (y deja `out` intacto)
// si la plataforma no lo soporta, o si `utf8` está vacío / produce un
// bitmap de área cero. Nunca lanza, nunca crashea.
bool RasterizeText(const TextRasterRequest& request, RasterizedText& out);

// Ancho en píxeles que RasterizeText produciría para `request` SIN
// recortar (para pasadas de layout que no quieren pagar el rasterizado
// completo). Ignora `maxWidthPx`. 0 si no está disponible o el texto es
// vacío.
int MeasureTextWidth(const TextRasterRequest& request);

}  // namespace nimvlets::platform
