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

void UiPainter::RoundRectBorder(const UiRect& r, float radius, UiColor border, UiColor fill) {
    FillRoundRect(r, radius, border);
    const UiRect inner = r.Inset(1.0f);
    if (fill.a != 0 && inner.w > 0.0f && inner.h > 0.0f) {
        FillRoundRect(inner, std::max(0.0f, radius - 1.0f), fill);
    }
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
