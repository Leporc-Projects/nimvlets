#include "TextLayoutTest.h"

#include <cmath>

#include "productui/TextLayout.h"

using nimvlets::productui::GlyphBlitOrigin;
using nimvlets::productui::GlyphOrigin;
using nimvlets::productui::HAlign;

namespace nimvlets::tests {

namespace {

bool IsWholePixel(float v) {
    return v == std::floor(v);
}

// El origen SIEMPRE cae en píxel entero del dispositivo — para cualquier
// alineación, escala o paridad de ancho de glyph. Ése es el arreglo de
// nitidez Retina de Block 06.2 §9: el bitmap se blittea 1:1, así que un
// origen fraccionario lo dejaría blando.
bool TestOriginIsAlwaysWholePixels() {
    const float scales[] = {1.0f, 2.0f, 1.5f};
    const int widths[] = {50, 51, 64, 87, 99};            // pares e impares
    const float anchors[] = {40.0f, 100.25f, 379.5f};     // enteros y fraccionarios
    const HAlign aligns[] = {HAlign::kLeft, HAlign::kCenter, HAlign::kRight};
    for (float s : scales) {
        for (int w : widths) {
            for (float a : anchors) {
                for (HAlign al : aligns) {
                    const GlyphOrigin o = GlyphBlitOrigin(a, 131.75f, s, w, 27, al);
                    NIMVLETS_CHECK(IsWholePixel(o.x));
                    NIMVLETS_CHECK(IsWholePixel(o.y));
                }
            }
        }
    }
    return true;
}

// El caso exacto del bug del owner: texto CENTRADO con un bitmap de
// ancho IMPAR a escala 2.0 daba leftPx = X.5 (medio píxel) y el run se
// veía blando. Ahora se acota al entero.
bool TestCenteredOddWidthSnapsOffHalfPixel() {
    // "Use Frin" medido: glyphTex 99px, anchorX 307 -> raw 614 - 49.5 = 564.5
    const GlyphOrigin o = GlyphBlitOrigin(307.0f, 280.0f, 2.0f, 99, 26, HAlign::kCenter);
    NIMVLETS_CHECK(IsWholePixel(o.x));
    // Redondea a 565 (no queda en 564.5).
    NIMVLETS_CHECK(o.x == 565.0f);
    return true;
}

// Alineación izquierda con anchor y escala enteros: sin cambio de
// comportamiento respecto de antes (ya caía en entero) — no se rompió
// nada del texto de cabecera que el owner NO reportó como blando.
bool TestLeftAlignIntegerAnchorUnchanged() {
    const GlyphOrigin o = GlyphBlitOrigin(40.0f, 30.0f, 2.0f, 130, 31, HAlign::kLeft);
    NIMVLETS_CHECK(o.x == 80.0f);   // 40 * 2
    NIMVLETS_CHECK(o.y == 29.0f);   // 30 * 2 - 31
    return true;
}

// Escala 1.0 (no-Retina futuro): el helper sigue produciendo enteros y
// centra correctamente.
bool TestNonRetinaScaleStillWhole() {
    const GlyphOrigin o = GlyphBlitOrigin(100.0f, 50.0f, 1.0f, 53, 12, HAlign::kCenter);
    NIMVLETS_CHECK(IsWholePixel(o.x));  // 100 - 26.5 = 73.5 -> 74
    NIMVLETS_CHECK(o.x == 74.0f);
    NIMVLETS_CHECK(o.y == 38.0f);      // 50 - 12
    return true;
}

// Alineación derecha: origen = anchorX*scale - glyphW, acotado a entero.
bool TestRightAlign() {
    const GlyphOrigin o = GlyphBlitOrigin(380.0f, 40.0f, 2.0f, 87, 26, HAlign::kRight);
    NIMVLETS_CHECK(o.x == 673.0f);  // 760 - 87
    return true;
}

}  // namespace

void RegisterTextLayoutTests(testing::TestRunner& runner) {
    runner.Add("TextLayout/OriginIsAlwaysWholePixels", TestOriginIsAlwaysWholePixels);
    runner.Add("TextLayout/CenteredOddWidthSnapsOffHalfPixel", TestCenteredOddWidthSnapsOffHalfPixel);
    runner.Add("TextLayout/LeftAlignIntegerAnchorUnchanged", TestLeftAlignIntegerAnchorUnchanged);
    runner.Add("TextLayout/NonRetinaScaleStillWhole", TestNonRetinaScaleStillWhole);
    runner.Add("TextLayout/RightAlign", TestRightAlign);
}

}  // namespace nimvlets::tests
