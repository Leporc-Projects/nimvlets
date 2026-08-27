#include "platform/TextRasterizer.h"

// Stub de Windows. El Product UI de Block 06 solo se valida en macOS
// (block brief §24). Una implementación real usaría DirectWrite +
// Direct2D (o GDI) para rasterizar Segoe UI — NO se finge acá. El
// código de src/productui trata `false` como "esta plataforma todavía
// no dibuja texto de producto" y no crashea. Ver docs/PRODUCT_UI.md §9.

namespace nimvlets::platform {

bool TextRasterizationAvailable() {
    return false;
}

bool RasterizeText(const TextRasterRequest& /*request*/, RasterizedText& /*out*/) {
    return false;
}

int MeasureTextWidth(const TextRasterRequest& /*request*/) {
    return 0;
}

}  // namespace nimvlets::platform
