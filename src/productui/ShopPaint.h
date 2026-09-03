#pragma once

#include <string>

#include "productui/PetPreviewCache.h"
#include "productui/ShopLayout.h"
#include "productui/TextCache.h"
#include "productui/UiPaint.h"

namespace nimvlets::productui {

// Primitivas de dibujo COMPARTIDAS por el Shop público (ShopView) y el
// Starter Shop oculto (StarterShopView) — Block 10. Los dos storefronts
// tienen la misma jerarquía visual (estantería de browse -> hover revela
// -> hero + detalle -> compra), así que la lógica de PINTADO de una
// tarjeta y de un hero es idéntica: vive acá una sola vez (brief §12
// "avoid duplication of large layout/paint logic"). Lo que cambia entre
// los dos es el MODELO (identidades exactas vs. filas por pet lógico) y
// el chrome de la sección (encabezado, back affordance) — eso se queda
// en cada vista.

// Óvalo (o round-rect si el acento del pet es "angular") del hero stage.
void FillShopStagePrimitive(UiPainter& painter, const UiRect& r, bool angular, UiColor color);

// Dibuja UNA tarjeta de personaje coherente (rejilla de browse o rail).
// Resuelve la preview `.nvprev` por (petId, variantId). `nameRole` = rol
// de fuente del nombre (serif). `revealVisible` pinta la línea de info
// liviana (precio / propiedad); `selectedMark` da a la tarjeta el
// tratamiento de identidad del pet (borde + tinte + regla de acento).
// Sin pedestal-caja detrás del arte (convergencia DEC-147).
void DrawShopTile(
    UiPainter& painter, TextCache& text, PetPreviewCache& previews, const ShopTile& t,
    const type::FontRole& nameRole, bool hovered, bool focused, bool revealVisible, bool selectedMark);

// Dibuja el hero (stage + arte + nombre + regla + especie + descripción
// + precio + acción/estado/confirmación). `focusedId` dibuja el anillo
// de foco del control correspondiente (o "" si el último input fue de
// mouse). No dibuja el rail ni el divisor — eso lo hace la vista.
void DrawShopHero(
    UiPainter& painter, TextCache& text, PetPreviewCache& previews, const ShopHero& h,
    const std::string& focusedId);

}  // namespace nimvlets::productui
