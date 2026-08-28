#include "productui/TextCache.h"

#include <SDL3/SDL.h>

#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

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
    // Colocación acotada a píxel entero del dispositivo: el bitmap ya
    // está a densidad nativa y se dibuja 1:1, así que un origen
    // fraccionario (típico en runs centrados/derechos) lo dejaría
    // blando en Retina (Block 06.2 §9). Ver productui/TextLayout.h.
    const GlyphOrigin origin =
        GlyphBlitOrigin(anchorX, baselineY, scale, g.width, g.baseline, align);
    painter.BlitPixels(g.texture, origin.x, origin.y, color.a);
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

namespace {

// Parte `s` en palabras separadas por espacios ASCII (colapsa espacios
// repetidos). Suficiente para copy editorial en EN/ES.
std::vector<std::string> SplitWords(const std::string& s) {
    std::vector<std::string> words;
    std::string cur;
    for (const char c : s) {
        if (c == ' ' || c == '\t' || c == '\n') {
            if (!cur.empty()) {
                words.push_back(std::move(cur));
                cur.clear();
            }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) {
        words.push_back(std::move(cur));
    }
    return words;
}

}  // namespace

int DrawTextWrapped(
    UiPainter& painter,
    TextCache& cache,
    const std::string& utf8,
    double pointSize,
    platform::TextWeight weight,
    UiColor color,
    float x,
    float firstBaselineY,
    float maxWidthLogical,
    float lineHeightLogical,
    int maxLines) {
    if (utf8.empty() || maxLines <= 0 || maxWidthLogical <= 0.0f) {
        return 0;
    }
    const float scale = painter.Scale();

    const std::vector<std::string> words = SplitWords(utf8);
    std::vector<std::string> lines;
    std::string line;
    for (std::size_t i = 0; i < words.size(); ++i) {
        const std::string candidate = line.empty() ? words[i] : line + " " + words[i];
        if (line.empty() ||
            MeasureText(cache, candidate, pointSize, weight, scale) <= maxWidthLogical) {
            line = candidate;
        } else {
            lines.push_back(line);
            line = words[i];
            if (static_cast<int>(lines.size()) == maxLines) {
                break;  // el resto se marca con "…" abajo
            }
        }
    }
    if (static_cast<int>(lines.size()) < maxLines && !line.empty()) {
        lines.push_back(line);
        line.clear();
    }

    // ¿Quedó texto sin colocar? (o porque se llenó maxLines, o porque la
    // última línea pendiente no cupo.) Marcar la última con una elipsis.
    const bool truncated = !line.empty();
    if (truncated && !lines.empty()) {
        std::string& last = lines.back();
        while (!last.empty() &&
               MeasureText(cache, last + "…", pointSize, weight, scale) > maxWidthLogical) {
            const std::size_t sp = last.find_last_of(' ');
            if (sp == std::string::npos) {
                last.pop_back();
            } else {
                last.erase(sp);
            }
        }
        last += "…";
    }

    for (std::size_t i = 0; i < lines.size(); ++i) {
        const float baseline = firstBaselineY + static_cast<float>(i) * lineHeightLogical;
        DrawText(painter, cache, lines[i], pointSize, weight, color, x, baseline, HAlign::kLeft);
    }
    return static_cast<int>(lines.size());
}

}  // namespace nimvlets::productui
