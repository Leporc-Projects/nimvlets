#include "platform/TextRasterizer.h"

// Stub de Linux. El Product UI de Block 06 solo se valida en macOS
// (block brief §24). Una implementación real usaría fontconfig (para
// resolver la fuente sans del sistema) + FreeType/HarfBuzz para
// rasterizar — NO se finge acá. src/productui trata `false` como "esta
// plataforma todavía no dibuja texto de producto" y no crashea. Ver
// docs/PRODUCT_UI.md §9 y docs/LINUX_PLATFORM.md.

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
