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

// Rótulo editorial simple con un rombo chico A CADA LADO, centrado:
//   ◇  label  ◇
// SIN líneas laterales (owner QA, DEC-146). Ligero y centrado. `role`
// da tamaño / peso / familia / interletraje; el texto se dibuja con su
// baseline en `baselineY`, centrado en `centerX`; los rombos se
// alinean con el centro visual del texto. Consistente en EN/ES —
// mide el ancho real de la etiqueta.
void DrawFlankedLabel(
    UiPainter& p, TextCache& text, const std::string& label, const type::FontRole& role,
    UiColor labelColor, UiColor ornamentColor, float centerX, float baselineY);

// Panel enmarcado suave reusable (referencia D, "subtle inner frame"):
// superficie cálida + borde exterior fino + highlight interior
// casi-blanco opcional. Sin drop shadow, sin glass.
void DrawSoftPanel(
    UiPainter& p, const UiRect& r, float radius, UiColor surface, UiColor border, bool innerHighlight);

// Dibuja un botón semántico: relleno / borde / tinta de
// ResolveButtonVisual, etiqueta centrada, anillo de foco desplazado en
// tokens::kFocus si `v.drawFocusRing`. `role` = rol de fuente
// tokenizado (las vistas pasan type::role::kButton, o
// .WithWeight(kMedium) para Secondary / Quiet).
void DrawButton(
    UiPainter& p, TextCache& text, const UiRect& r, const std::string& label, const ButtonVisual& v,
    const type::FontRole& role);

}  // namespace nimvlets::productui
