#pragma once

#include <cstdint>
#include <string>

#include "core/Localization.h"
#include "productui/SectionNav.h"
#include "productui/TextCache.h"
#include "productui/UiPaint.h"

namespace nimvlets::productui {

// Dibuja la cabecera compartida (título "Nimvlets" + balance de clics +
// pestañas "Collection · Shop · Settings") a partir de un
// SectionHeaderLayout ya calculado. La usan por igual las CUATRO
// secciones para que la navegación se vea idéntica. Se llama FUERA del
// clip de scroll de la vista (igual que la cabecera de Block 06).
//
// **El balance de clics ya viene formateado en `header.clicksText`**
// (Block 10, corrección de QA del owner): `BuildSectionHeaderLayout` lo
// calcula a partir del balance CANÓNICO que `ProductWindow` posee, así
// NINGUNA sección puede dibujar un valor propio / obsoleto. Antes de
// esta corrección Settings pasaba `clickBalance = 0` hard-codeado.
//
// `hoverFocusId` / `keyboardFocusId` son los ids de widget bajo el mouse
// y con foco de teclado (o "" si ninguno) — el wash de hover y el
// anillo de foco de una pestaña siguen el mismo patrón "focus-visible
// por modalidad" que el resto del Product UI (Block 06.2).
// Todo el texto (título, `clicksText`, etiquetas de pestaña) ya viene
// localizado en `header` — DrawSectionHeader no necesita el idioma.
void DrawSectionHeader(
    UiPainter& painter,
    TextCache& text,
    const SectionHeaderLayout& header,
    const std::string& hoverFocusId,
    const std::string& keyboardFocusId);

}  // namespace nimvlets::productui
