#pragma once

#include "core/Geometry.h"

namespace nimvlets::core {

// Analytic (not bitmap) placeholder "creature" silhouette used by the
// foundation spike: two overlapping filled circles (a "body" and a
// "head") in window-local coordinates, centered in the window.
//
// This is a development placeholder, not final art — see
// docs/PET_CONTENT_SPEC.md and NON-SCOPE in the block brief. It exists to
// prove out non-rectangular hit-testing, click-through, and a real
// (non-zero) idle animation without depending on image loading (no
// SDL_image) or final assets.
//
// Both the renderer (src/graphics/BlobRenderer.cpp) and the platform
// click-through code (src/platform/*/TransparentWindowSupport) evaluate
// this exact same Contains() with the same phase value each frame, so
// what's drawn and what's clickable can never disagree.
struct BlobSilhouette {
    double windowWidth = 160.0;
    double windowHeight = 160.0;

    double bodyRadius = 46.0;
    double headRadius = 27.0;

    // Head center offset from the body center, before animation bobbing.
    double headOffsetX = 0.0;
    double headOffsetY = -52.0;

    // Vertical bob amplitude (pixels) / angular speed (radians per
    // second) for the idle animation.
    double bobAmplitudePx = 4.0;
    double bobAngularSpeed = 2.4;

    Point BodyCenter() const;
    Point HeadCenter(double phaseSeconds) const;

    // True if `point` (window-local pixel coordinates) falls inside the
    // body or the (animated) head circle at animation phase
    // `phaseSeconds`.
    bool Contains(Point point, double phaseSeconds) const;

    static BlobSilhouette Default();
};

}  // namespace nimvlets::core
