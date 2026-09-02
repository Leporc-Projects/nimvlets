#pragma once

#include <string>

#include "platform/TextRasterizer.h"
#include "productui/ButtonStyle.h"
#include "productui/TextCache.h"
#include "productui/UiColor.h"
#include "productui/UiGeometry.h"
#include "productui/UiPaint.h"

namespace nimvlets::productui {

// Conjunto CHICO de primitivas de ornamento PROCEDURALES de primera
// parte (Block 12A — DEC-144). Coordenadas en puntos lógicos,
// Retina-safe (componen los fills por-scanline de UiPainter),
// deterministas, baratas, ESTÁTICAS — sin textura de imagen, sin
// animación. Solo lo que Nimvlets usa; no es un motor de gráficos
// vectoriales. La geometría pura testeable está en OrnamentGeometry.h.

// Spark de 4 puntas (✦) centrado en (cx, cy), `radius` = puntos hasta
// la punta. Dos rombos finos cruzados.
void DrawSparkle(UiPainter& p, float cx, float cy, float radius, UiColor color);

// Rombo lleno inscrito en `r` (thin wrapper de UiPainter::FillDiamond,
// para simetría de API con el resto de los ornamentos).
void DrawDiamond(UiPainter& p, const UiRect& r, UiColor color);

// Regla hairline con un rombo chico centrado y un hueco alrededor:
//   ─────── ◇ ───────
// El motivo del divisor de detalle (referencia D). `band` da el ancho
// total y el centro vertical; la regla es de 1 pt.
void DrawOrnamentalDivider(UiPainter& p, const UiRect& band, UiColor line, UiColor ornament);

// Regla de acento corta de 2 pt (p. ej. bajo el nombre del hero).
// Wrapper nombrado para un tratamiento consistente.
void DrawAccentRule(UiPainter& p, const UiRect& r, UiColor color);

// Motivo editorial chico a la izquierda de un encabezado contextual
// ("Nimvlets you can meet", referencia E): un tick corto + un rombo.
// Se dibuja con la punta izquierda en `x`, centrado en `centerY`.
// Devuelve el avance en x (puntos) — el caller pone la etiqueta en
// `x + <avance>`.
float DrawHeadingMotif(UiPainter& p, float x, float centerY, UiColor color);

// Panel enmarcado suave reusable (referencia D, "subtle inner frame"):
// superficie cálida + borde exterior fino + highlight interior
// casi-blanco opcional. Sin drop shadow, sin glass.
void DrawSoftPanel(
    UiPainter& p, const UiRect& r, float radius, UiColor surface, UiColor border, bool innerHighlight);

// Dibuja un botón semántico: relleno / borde / tinta de
// ResolveButtonVisual, etiqueta centrada, anillo de foco desplazado en
// tokens::kFocus si `v.drawFocusRing`. `weight` por defecto semibold
// (Primary / Confirm); las vistas pasan kMedium para Secondary / Quiet.
void DrawButton(
    UiPainter& p, TextCache& text, const UiRect& r, const std::string& label, const ButtonVisual& v,
    double labelSize, platform::TextWeight weight = platform::TextWeight::kSemibold);

}  // namespace nimvlets::productui
