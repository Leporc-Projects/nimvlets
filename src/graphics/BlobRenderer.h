#pragma once

#include "core/Silhouette.h"

struct SDL_Renderer;

namespace nimvlets::graphics {

// Renders a core::BlobSilhouette into an SDL_Renderer whose window was
// created with SDL_WINDOW_TRANSPARENT: clears the target to fully
// transparent, then paints the silhouette as a flat-colored dev
// placeholder (no SDL_image, no textures, no geometry/triangulation
// dependency — see docs/DECISION_LOG.md DEC-005).
class BlobRenderer {
public:
    // Clears `renderer` to transparent and paints `blob` at animation
    // phase `phaseSeconds`, then presents the frame.
    void Render(SDL_Renderer* renderer, const core::BlobSilhouette& blob, double phaseSeconds) const;

private:
    void FillCircle(SDL_Renderer* renderer, core::Point center, double radius) const;
};

}  // namespace nimvlets::graphics
