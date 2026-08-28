#pragma once

#include <cstdint>
#include <string>

#include "core/Localization.h"
#include "productui/SectionNav.h"
#include "productui/TextCache.h"
#include "productui/UiPaint.h"

namespace nimvlets::productui {

// Dibuja la cabecera compartida (título "Nimvlets" + balance de clics +
// pestañas "Collection · Shop") a partir de un SectionHeaderLayout ya
// calculado. La usan por igual CollectionView y ShopView para que la
// navegación se vea idéntica en ambas secciones. Se llama FUERA del clip
// de scroll de la vista (igual que la cabecera de Block 06).
//
// `hoverFocusId` / `keyboardFocusId` son los ids de widget bajo el mouse
// y con foco de teclado (o "" si ninguno) — el wash de hover y el
// anillo de foco de una pestaña siguen el mismo patrón "focus-visible
// por modalidad" que el resto del Product UI (Block 06.2).
void DrawSectionHeader(
    UiPainter& painter,
    TextCache& text,
    const SectionHeaderLayout& header,
    std::uint64_t clickBalance,
    core::Language language,
    const std::string& hoverFocusId,
    const std::string& keyboardFocusId);

}  // namespace nimvlets::productui
