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
using nimvlets::platform::TextFamily;
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

    // Familia SERIF (Block 12A refinement — DEC-146): pedir el diseño
    // serif del sistema produce un bitmap con métricas distintas a la
    // sans para el mismo texto/tamaño (prueba que el descriptor tomó
    // efecto), y nunca vacío ni degenerado.
    {
        TextRasterRequest serif = req;
        serif.utf8 = "Collection";
        serif.family = TextFamily::kSerif;
        RasterizedText serifBmp;
        const bool serifOk = RasterizeText(serif, serifBmp);
        Check(serifOk, "RasterizeText succeeded with TextFamily::kSerif");
        Check(serifBmp.width > 0 && serifBmp.height > 0 && HasHighAlphaPixel(serifBmp),
              "serif bitmap has positive area and drew glyphs");
        Check(serifBmp.width != basic.width || serifBmp.height != basic.height,
              "serif metrics differ from sans for the same string (design descriptor took effect)");
    }

    // Interletraje (tracking): un valor positivo ensancha la línea sin
    // romperla.
    {
        TextRasterRequest tracked = req;
        tracked.utf8 = "Collection";
        tracked.tracking = 2.0;
        Check(MeasureTextWidth(tracked) > measured, "positive tracking measures wider");
    }

    // Densidad de backing Retina (Block 06.2 §8/§27): el mismo texto
    // lógico se rasteriza a ~2x los píxeles cuando scale = 2.0. Prueba
    // que RasterizeText escala con la densidad de pantalla (pixeles =
    // pointSize * scale) — la base del arreglo de nitidez; si esto
    // fallara, el texto estaría rasterizado a 1x y agrandado por SDL.
    {
        TextRasterRequest at1 = req;
        at1.scale = 1.0;
        at1.utf8 = "Companions";
        TextRasterRequest at2 = at1;
        at2.scale = 2.0;
        RasterizedText r1;
        RasterizedText r2;
        const bool ok1 = RasterizeText(at1, r1);
        const bool ok2 = RasterizeText(at2, r2);
        Check(ok1 && ok2, "RasterizeText succeeded at scale 1.0 and 2.0");
        // ~2x en ambos ejes. Las métricas de Core Text no son
        // perfectamente lineales con el tamaño de punto (hinting,
        // redondeo de advance), así que la tolerancia es un porcentaje
        // del doble exacto, no un ±px fijo: basta con demostrar que el
        // bitmap se rasteriza a densidad de backing, no a 1x agrandado.
        const double wRatio = static_cast<double>(r2.width) / static_cast<double>(r1.width);
        const double hRatio = static_cast<double>(r2.height) / static_cast<double>(r1.height);
        Check(wRatio > 1.85 && wRatio < 2.15,
              "scale 2.0 glyph bitmap is ~2x wider than scale 1.0 (native backing density)");
        Check(hRatio > 1.85 && hRatio < 2.15, "scale 2.0 glyph bitmap is ~2x taller than scale 1.0");
        Check(r2.baseline > r1.baseline, "scale 2.0 baseline sits lower in pixels than scale 1.0");
    }

    std::printf("%s\n", g_failures == 0 ? "all text-rasterizer checks passed" : "text-rasterizer checks FAILED");
    return g_failures == 0 ? 0 : 1;
}
