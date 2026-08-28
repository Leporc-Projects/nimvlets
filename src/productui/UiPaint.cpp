#include "productui/UiPaint.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

namespace nimvlets::productui {

namespace {

SDL_FRect Scaled(const UiRect& r, float s) {
    return SDL_FRect{r.x * s, r.y * s, r.w * s, r.h * s};
}

void SetColor(SDL_Renderer* renderer, UiColor c) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
}

}  // namespace

void UiPainter::Clear(UiColor color) {
    SetColor(renderer_, color);
    SDL_RenderClear(renderer_);
}

void UiPainter::FillRect(const UiRect& r, UiColor color) {
    if (color.a == 0 || r.w <= 0.0f || r.h <= 0.0f) {
        return;
    }
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SetColor(renderer_, color);
    const SDL_FRect rect = Scaled(r, scale_);
    SDL_RenderFillRect(renderer_, &rect);
}

void UiPainter::FillRoundRect(const UiRect& r, float radius, UiColor color) {
    if (color.a == 0 || r.w <= 0.0f || r.h <= 0.0f) {
        return;
    }
    const float rad = std::clamp(radius, 0.0f, std::min(r.w, r.h) * 0.5f);
    if (rad < 0.75f) {
        FillRect(r, color);
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SetColor(renderer_, color);

    // Spans horizontales en espacio de PÍXELES para que el borde
    // redondeado quede nítido en alto-DPI.
    const float px = r.x * scale_;
    const float py = r.y * scale_;
    const float pw = r.w * scale_;
    const float ph = r.h * scale_;
    const float pr = rad * scale_;
    const int rows = static_cast<int>(std::lround(ph));

    for (int i = 0; i < rows; ++i) {
        const float yTop = static_cast<float>(i);
        float inset = 0.0f;
        // Distancia vertical al centro del arco de esquina más cercano.
        float dy = -1.0f;
        if (yTop < pr) {
            dy = pr - (yTop + 0.5f);
        } else if (yTop > ph - pr) {
            dy = (yTop + 0.5f) - (ph - pr);
        }
        if (dy > 0.0f && dy < pr) {
            inset = pr - std::sqrt(std::max(0.0f, pr * pr - dy * dy));
        } else if (dy >= pr) {
            continue;  // fuera del arco (esquina puntiaguda recortada)
        }
        const SDL_FRect span{px + inset, py + yTop, pw - 2.0f * inset, 1.0f};
        if (span.w > 0.0f) {
            SDL_RenderFillRect(renderer_, &span);
        }
    }
}

void UiPainter::FillEllipse(const UiRect& r, UiColor color) {
    if (color.a == 0 || r.w <= 0.0f || r.h <= 0.0f) {
        return;
    }
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SetColor(renderer_, color);

    const float px = r.x * scale_;
    const float py = r.y * scale_;
    const float pw = r.w * scale_;
    const float ph = r.h * scale_;
    const float cx = px + pw * 0.5f;
    const float ry = ph * 0.5f;
    const float cyPix = py + ry;
    const int rows = static_cast<int>(std::lround(ph));

    for (int i = 0; i < rows; ++i) {
        const float yc = py + static_cast<float>(i) + 0.5f;
        const float t = (yc - cyPix) / ry;
        if (t <= -1.0f || t >= 1.0f) {
            continue;
        }
        const float halfSpan = (pw * 0.5f) * std::sqrt(std::max(0.0f, 1.0f - t * t));
        const SDL_FRect span{cx - halfSpan, py + static_cast<float>(i), 2.0f * halfSpan, 1.0f};
        if (span.w > 0.0f) {
            SDL_RenderFillRect(renderer_, &span);
        }
    }
}

void UiPainter::StrokeRoundRect(const UiRect& r, float radius, float thickness, UiColor color) {
    if (color.a == 0 || r.w <= 0.0f || r.h <= 0.0f || thickness <= 0.0f) {
        return;
    }
    const float rad = std::clamp(radius, 0.0f, std::min(r.w, r.h) * 0.5f);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SetColor(renderer_, color);

    const float px = r.x * scale_;
    const float py = r.y * scale_;
    const float pw = r.w * scale_;
    const float ph = r.h * scale_;
    const float pr = rad * scale_;
    const float pt = std::max(1.0f, thickness * scale_);
    const int rows = static_cast<int>(std::lround(ph));

    for (int i = 0; i < rows; ++i) {
        const float yTop = static_cast<float>(i);
        float inset = 0.0f;
        float dy = -1.0f;
        if (yTop < pr) {
            dy = pr - (yTop + 0.5f);
        } else if (yTop > ph - pr) {
            dy = (yTop + 0.5f) - (ph - pr);
        }
        if (dy > 0.0f && dy < pr) {
            inset = pr - std::sqrt(std::max(0.0f, pr * pr - dy * dy));
        } else if (dy >= pr) {
            continue;
        }
        const bool edgeRow = yTop < pt || yTop >= ph - pt;
        if (edgeRow) {
            const SDL_FRect span{px + inset, py + yTop, pw - 2.0f * inset, 1.0f};
            if (span.w > 0.0f) {
                SDL_RenderFillRect(renderer_, &span);
            }
        } else {
            const SDL_FRect left{px + inset, py + yTop, pt, 1.0f};
            const SDL_FRect right{px + pw - inset - pt, py + yTop, pt, 1.0f};
            SDL_RenderFillRect(renderer_, &left);
            SDL_RenderFillRect(renderer_, &right);
        }
    }
}

void UiPainter::RoundRectBorder(const UiRect& r, float radius, UiColor border, UiColor fill) {
    if (fill.a != 0) {
        FillRoundRect(r, radius, fill);
    }
    StrokeRoundRect(r, radius, 1.0f, border);
}

void UiPainter::DrawTextureContained(SDL_Texture* texture, const UiRect& box, unsigned char alpha) {
    if (texture == nullptr) {
        return;
    }
    float tw = 0.0f;
    float th = 0.0f;
    SDL_GetTextureSize(texture, &tw, &th);
    if (tw <= 0.0f || th <= 0.0f) {
        return;
    }
    const float boxPw = box.w * scale_;
    const float boxPh = box.h * scale_;
    const float k = std::min(boxPw / tw, boxPh / th);
    const float w = tw * k;
    const float h = th * k;
    const SDL_FRect dst{
        box.x * scale_ + (boxPw - w) * 0.5f,
        box.y * scale_ + (boxPh - h) * 0.5f,
        w,
        h,
    };
    SDL_SetTextureAlphaMod(texture, alpha);
    SDL_RenderTexture(renderer_, texture, nullptr, &dst);
    SDL_SetTextureAlphaMod(texture, 255);
}

void UiPainter::DrawTextureExact(SDL_Texture* texture, const UiRect& dst, unsigned char alpha) {
    if (texture == nullptr) {
        return;
    }
    const SDL_FRect rect = Scaled(dst, scale_);
    SDL_SetTextureAlphaMod(texture, alpha);
    SDL_RenderTexture(renderer_, texture, nullptr, &rect);
    SDL_SetTextureAlphaMod(texture, 255);
}

void UiPainter::BlitPixels(SDL_Texture* texture, float pxX, float pxY, unsigned char alpha) {
    if (texture == nullptr) {
        return;
    }
    float tw = 0.0f;
    float th = 0.0f;
    SDL_GetTextureSize(texture, &tw, &th);
    const SDL_FRect dst{pxX, pxY, tw, th};
    SDL_SetTextureAlphaMod(texture, alpha);
    SDL_RenderTexture(renderer_, texture, nullptr, &dst);
    SDL_SetTextureAlphaMod(texture, 255);
}

void UiPainter::PushClip(const UiRect& r) {
    const SDL_FRect f = Scaled(r, scale_);
    const SDL_Rect clip{
        static_cast<int>(std::floor(f.x)),
        static_cast<int>(std::floor(f.y)),
        static_cast<int>(std::ceil(f.w)),
        static_cast<int>(std::ceil(f.h)),
    };
    SDL_SetRenderClipRect(renderer_, &clip);
    clipActive_ = true;
}

void UiPainter::PopClip() {
    if (clipActive_) {
        SDL_SetRenderClipRect(renderer_, nullptr);
        clipActive_ = false;
    }
}

}  // namespace nimvlets::productui
