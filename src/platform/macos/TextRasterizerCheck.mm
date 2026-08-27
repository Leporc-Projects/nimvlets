// Verificación de desarrollo del rasterizador de texto de macOS
// (Block 06 — ver platform/TextRasterizer.h y docs/PRODUCT_UI.md §9).
//
// Por qué no es un test de CTest normal: platform::RasterizeText usa
// Core Text / AppKit, y todo tests/ es deliberadamente SDL-free y
// AppKit-free (AGENTS.md §3/§12). Se construye SOLO en macOS y SOLO con
// -DNIMVLETS_ENABLE_GUI_CHECKS=ON, junto al check de click-through, y se
// registra como test de CTest únicamente en esa configuración — la CI
// de cuatro plataformas queda igual. Core Text sí funciona sin
// WindowServer, así que este check corre headless.
//
// Qué prueba, contra el código de adaptador REAL que se envía:
//   1. Disponibilidad reportada = true en macOS.
//   2. Un string simple produce un bitmap de área > 0 con al menos un
//      pixel de alpha alto (se dibujó algo) y straight alpha (un pixel
//      opaco tinta = el color pedido, no premultiplicado).
//   3. maxWidthPx fuerza recorte con "…": el ancho resultante no supera
//      el límite y es menor que el ancho natural del texto largo.
//   4. MeasureTextWidth coincide (± margen) con el ancho del bitmap sin
//      recortar, y crece con el tamaño de punto.
//
// Uso:
//   cmake --preset macos-debug -DNIMVLETS_ENABLE_GUI_CHECKS=ON
//   cmake --build --preset macos-debug --target nimvlets_macos_text_check
//   ./build/macos-debug/src/platform/macos/nimvlets_macos_text_check

#include "platform/TextRasterizer.h"

#include <cstdio>
#include <string>

using nimvlets::platform::MeasureTextWidth;
using nimvlets::platform::RasterizedText;
using nimvlets::platform::RasterizeText;
using nimvlets::platform::TextRasterizationAvailable;
using nimvlets::platform::TextRasterRequest;
using nimvlets::platform::TextWeight;

namespace {

int g_failures = 0;

void Check(bool cond, const char* what) {
    std::printf("%s %s\n", cond ? "[PASS]" : "[FAIL]", what);
    if (!cond) {
        ++g_failures;
    }
}

bool HasHighAlphaPixel(const RasterizedText& t) {
    for (size_t i = 3; i < t.pixels.size(); i += 4) {
        if (t.pixels[i] > 200) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main() {
    Check(TextRasterizationAvailable(), "text rasterization reported available on macOS");

    TextRasterRequest req;
    req.utf8 = "Collection";
    req.pointSize = 15.0;
    req.scale = 2.0;
    req.r = 20;
    req.g = 18;
    req.b = 16;

    RasterizedText basic;
    const bool ok = RasterizeText(req, basic);
    Check(ok, "RasterizeText succeeded for a simple string");
    Check(basic.width > 0 && basic.height > 0, "bitmap has positive area");
    Check(basic.pixels.size() == static_cast<size_t>(basic.width) * static_cast<size_t>(basic.height) * 4,
          "pixel buffer size matches");
    Check(basic.baseline > 0 && basic.baseline <= basic.height, "baseline within bitmap");
    Check(HasHighAlphaPixel(basic), "at least one near-opaque glyph pixel drawn");

    // Straight alpha: en el pixel de cobertura máxima, el canal de color
    // debe seguir siendo el color oscuro pedido (~20), no atenuado por
    // el alpha (que sería ~20*a/255).
    {
        size_t bestIdx = 0;
        std::uint8_t bestA = 0;
        bool found = false;
        for (size_t i = 0; i + 3 < basic.pixels.size(); i += 4) {
            if (basic.pixels[i + 3] >= bestA) {
                bestA = basic.pixels[i + 3];
                bestIdx = i;
                found = true;
            }
        }
        Check(found && bestA > 200 && basic.pixels[bestIdx + 0] < 60,
              "opaque pixel keeps the requested dark colour (straight alpha)");
    }

    // Recorte con puntos suspensivos.
    TextRasterRequest longReq = req;
    longReq.utf8 = "This is a deliberately long companion name that will not fit";
    RasterizedText natural;
    RasterizeText(longReq, natural);

    TextRasterRequest clippedReq = longReq;
    clippedReq.maxWidthPx = 120;
    RasterizedText clipped;
    const bool clipOk = RasterizeText(clippedReq, clipped);
    Check(clipOk, "RasterizeText succeeded with maxWidthPx");
    Check(clipped.width <= 120 + 4, "clipped width respects maxWidthPx (+ AA margin)");
    Check(clipped.width < natural.width, "clipped bitmap is narrower than the natural one");

    // MeasureTextWidth vs. ancho de bitmap.
    const int measured = MeasureTextWidth(req);
    Check(measured > 0, "MeasureTextWidth positive");
    Check(measured <= basic.width && measured >= basic.width - 4, "MeasureTextWidth ~= bitmap width");

    TextRasterRequest big = req;
    big.pointSize = 40.0;
    Check(MeasureTextWidth(big) > measured, "larger point size measures wider");

    std::printf("%s\n", g_failures == 0 ? "all text-rasterizer checks passed" : "text-rasterizer checks FAILED");
    return g_failures == 0 ? 0 : 1;
}
