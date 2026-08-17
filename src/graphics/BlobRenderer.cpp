#include "graphics/BlobRenderer.h"

#include <SDL3/SDL.h>

#include <cmath>

namespace nimvlets::graphics {

void BlobRenderer::FillCircle(SDL_Renderer* renderer, core::Point center, double radius) const {
    const int top = static_cast<int>(std::ceil(center.y - radius));
    const int bottom = static_cast<int>(std::floor(center.y + radius));

    for (int y = top; y <= bottom; ++y) {
        const double dy = static_cast<double>(y) - center.y;
        const double underSqrt = (radius * radius) - (dy * dy);
        if (underSqrt <= 0.0) {
            continue;
        }
        const double halfWidth = std::sqrt(underSqrt);
        const SDL_FRect row{
            static_cast<float>(center.x - halfWidth),
            static_cast<float>(y),
            static_cast<float>(halfWidth * 2.0),
            1.0f,
        };
        SDL_RenderFillRect(renderer, &row);
    }
}

void BlobRenderer::Render(SDL_Renderer* renderer, const core::BlobSilhouette& blob, double phaseSeconds) const {
    // Fully transparent clear. SDL_RenderClear writes the draw color
    // directly (it is not affected by blend mode), so this is a real
    // alpha=0 clear regardless of the blend mode used below.
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    // Flat, single-color, dev-placeholder fill. Deliberately not final
    // art — see docs/PET_CONTENT_SPEC.md.
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 90, 200, 170, 255);
    FillCircle(renderer, blob.BodyCenter(), blob.bodyRadius);
    FillCircle(renderer, blob.HeadCenter(phaseSeconds), blob.headRadius);

    SDL_RenderPresent(renderer);
}

}  // namespace nimvlets::graphics
