#pragma once

#include <cmath>

#include "productui/UiGeometry.h"

namespace nimvlets::productui {

// Geometría PURA de los ornamentos procedurales de primera parte
// (Block 12A — DEC-144). El dibujo real (Ornaments.cpp / UiPainter)
// usa estas funciones, así que los tests fijan sus límites, su
// simetría y su centrado sin ventana ni renderer. Coordenadas en
// PUNTOS lógicos. No es un motor de gráficos vectoriales: solo lo que
// Nimvlets usa (spark, rombo, divisor ornamental, regla de acento,
// motivo de encabezado).

// Caja contenedora de un spark de radio `radius` centrado en (cx, cy).
// El spark son dos rombos finos cruzados: sus puntas llegan justo a
// ±radius en los dos ejes.
inline UiRect SparkleBounds(float cx, float cy, float radius) {
    return UiRect{cx - radius, cy - radius, radius * 2.0f, radius * 2.0f};
}

// Semi-ancho de un rombo inscrito en una caja de ancho `w`, a la
// posición vertical normalizada `t` en [0, 1]: 0 en las puntas
// (t == 0 y t == 1), w/2 en el medio (t == 0.5). Nunca negativo.
inline float DiamondHalfWidthAt(float w, float t) {
    const float k = 1.0f - std::fabs(2.0f * t - 1.0f);
    return 0.5f * w * (k < 0.0f ? 0.0f : k);
}

// Largo de cada uno de los dos segmentos de regla de un divisor
// ornacional (── ◇ ──) para una banda de ancho `bandW` con un hueco
// `gap` a cada lado del rombo central. Los dos segmentos son iguales
// (simétrico). Nunca negativo.
inline float OrnamentalDividerRuleLen(float bandW, float gap) {
    const float half = bandW * 0.5f - gap;
    return half < 0.0f ? 0.0f : half;
}

// Avance en x (puntos lógicos) que ocupa el motivo de encabezado
// contextual (una marca chica: tick + rombo) antes de la etiqueta,
// para `diamond` = lado del rombo. El caller coloca el texto en
// `x + HeadingMotifAdvance(d)`.
inline float HeadingMotifAdvance(float diamond) {
    return 7.0f + diamond + 6.0f;  // tick(5)+aire(2) · rombo · aire(6)
}

}  // namespace nimvlets::productui
