#include "productui/TextCache.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <utility>

namespace nimvlets::productui {

TextCache::~TextCache() {
    Clear();
}

void TextCache::Clear() {
    for (auto& [key, entry] : entries_) {
        if (entry.texture != nullptr) {
            SDL_DestroyTexture(entry.texture);
        }
    }
    entries_.clear();
}

std::string TextCache::KeyOf(const platform::TextRasterRequest& request) {
    char buf[96];
    std::snprintf(
        buf, sizeof(buf), "|%d|%d|%d|%d|%d|%d|%d|",
        static_cast<int>(request.pointSize * 4.0),   // 0.25pt de resolución
        static_cast<int>(request.scale * 100.0),
        static_cast<int>(request.weight),
        request.r, request.g, request.b,
        request.maxWidthPx);
    return request.utf8 + buf;
}

TextCache::Glyphs TextCache::Acquire(const platform::TextRasterRequest& request) {
    const std::string key = KeyOf(request);
    if (const auto it = entries_.find(key); it != entries_.end()) {
        const Entry& e = it->second;
        return Glyphs{e.texture, e.width, e.height, e.baseline};
    }

    platform::RasterizedText raster;
    Entry entry;
    if (platform::RasterizeText(request, raster) && raster.width > 0 && raster.height > 0) {
        SDL_Texture* tex = SDL_CreateTexture(
            renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, raster.width, raster.height);
        if (tex != nullptr) {
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);  // ya está a tamaño de píxel real
            SDL_UpdateTexture(tex, nullptr, raster.pixels.data(), raster.width * 4);
            entry.texture = tex;
            entry.width = raster.width;
            entry.height = raster.height;
            entry.baseline = raster.baseline;
        }
    }
    // Se cachea incluso el resultado vacío (plataforma sin rasterizador)
    // para no reintentar en cada frame.
    entries_.emplace(key, entry);
    return Glyphs{entry.texture, entry.width, entry.height, entry.baseline};
}

namespace {

platform::TextRasterRequest MakeRequest(
    const std::string& utf8, double pointSize, platform::TextWeight weight, UiColor color, float scale,
    int maxWidthPx) {
    platform::TextRasterRequest req;
    req.utf8 = utf8;
    req.pointSize = pointSize;
    req.scale = scale;
    req.weight = weight;
    req.r = color.r;
    req.g = color.g;
    req.b = color.b;
    req.maxWidthPx = maxWidthPx;
    return req;
}

}  // namespace

float DrawText(
    UiPainter& painter,
    TextCache& cache,
    const std::string& utf8,
    double pointSize,
    platform::TextWeight weight,
    UiColor color,
    float anchorX,
    float baselineY,
    HAlign align,
    int maxWidthLogical) {
    const float scale = painter.Scale();
    const int maxWidthPx = maxWidthLogical > 0 ? static_cast<int>(static_cast<float>(maxWidthLogical) * scale) : 0;
    const TextCache::Glyphs g =
        cache.Acquire(MakeRequest(utf8, pointSize, weight, color, scale, maxWidthPx));
    if (g.texture == nullptr) {
        return 0.0f;
    }

    const float widthLogical = static_cast<float>(g.width) / scale;
    float leftPx = anchorX * scale;
    if (align == HAlign::kCenter) {
        leftPx = anchorX * scale - static_cast<float>(g.width) * 0.5f;
    } else if (align == HAlign::kRight) {
        leftPx = anchorX * scale - static_cast<float>(g.width);
    }
    const float topPx = baselineY * scale - static_cast<float>(g.baseline);
    painter.BlitPixels(g.texture, leftPx, topPx, color.a);
    return widthLogical;
}

float MeasureText(
    TextCache& /*cache*/, const std::string& utf8, double pointSize, platform::TextWeight weight, float scale) {
    platform::TextRasterRequest req;
    req.utf8 = utf8;
    req.pointSize = pointSize;
    req.scale = scale;
    req.weight = weight;
    const int px = platform::MeasureTextWidth(req);
    return static_cast<float>(px) / scale;
}

}  // namespace nimvlets::productui
