#pragma once

#include <cmath>

namespace nimvlets::productui {

// Alineación horizontal de un run de texto respecto de su `anchorX`.
enum class HAlign { kLeft, kCenter, kRight };

// Origen (esquina superior izquierda) EN PÍXELES del dispositivo para
// blittear un bitmap de glyphs ya rasterizado a densidad nativa.
struct GlyphOrigin {
    float x = 0.0f;
    float y = 0.0f;
};

// Dónde poner el bitmap de glyphs. `anchorX`/`baselineY` están en PUNTOS
// lógicos; `scale` es la densidad de píxeles de la ventana (2.0 en
// Retina). `glyphW` es el ancho del bitmap en píxeles; `glyphBaseline`
// los píxeles desde el borde superior del bitmap hasta la baseline.
//
// El resultado se ACOTA a píxel entero del dispositivo (std::round): el
// bitmap ya está rasterizado a `pointSize * scale` píxeles y se dibuja
// 1:1 (sin reescalar), así que un origen fraccionario haría que cada
// texel cayera a caballo entre dos píxeles y el texto se viera blando —
// el defecto Retina que reportó el owner, peor en runs centrados/
// derechos donde `- glyphW*0.5` / `- glyphW` mete un medio píxel
// (Block 06.2 §8/§9). La rasterización en sí ya es correcta; esto es un
// arreglo de COLOCACIÓN, no se toca el tamaño del glyph.
//
// Puro (sin SDL): vive acá para que los tests lo verifiquen sin una
// ventana ni un renderer.
inline GlyphOrigin GlyphBlitOrigin(
    float anchorX, float baselineY, float scale, int glyphW, int glyphBaseline, HAlign align) {
    float leftPx = anchorX * scale;
    if (align == HAlign::kCenter) {
        leftPx = anchorX * scale - static_cast<float>(glyphW) * 0.5f;
    } else if (align == HAlign::kRight) {
        leftPx = anchorX * scale - static_cast<float>(glyphW);
    }
    const float topPx = baselineY * scale - static_cast<float>(glyphBaseline);
    return GlyphOrigin{std::round(leftPx), std::round(topPx)};
}

}  // namespace nimvlets::productui
