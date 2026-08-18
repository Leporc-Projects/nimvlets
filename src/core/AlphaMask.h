#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/Geometry.h"

namespace nimvlets::core {

// A simple, pure, rasterized hit-test region: for each pixel in a
// width x height grid, whether it counts as "visible/interactive" or
// "transparent/click-through".
//
// Deliberately source-agnostic — callers build one either from
// core::BlobSilhouette (the analytic placeholder shape, still used by
// tests/SilhouetteTest.cpp) or from a real image/animation frame's
// alpha channel at a configurable threshold (see FromAlphaChannel()
// below, used by content::AnimationController-driven rendering — see
// docs/ANIMATION_RUNTIME.md and docs/DECISION_LOG.md), through this one
// Contains() interface.
class AlphaMask {
public:
    AlphaMask(int width, int height);

    int Width() const { return width_; }
    int Height() const { return height_; }

    void SetOpaque(int x, int y, bool opaque);

    // True if `point` (grid/pixel coordinates, same units the mask was
    // built at) falls on an opaque cell. Out-of-bounds points are
    // never opaque.
    bool Contains(Point point) const;

    // Builds a mask at `targetWidth` x `targetHeight` from an RGBA8
    // pixel buffer (`rgba`, row-major, `srcWidth` x `srcHeight`,
    // `srcWidth * srcHeight * 4` bytes) by nearest-neighbor sampling
    // each target cell back to its source pixel and comparing that
    // pixel's alpha channel against `alphaThreshold` (inclusive: alpha
    // >= threshold counts as opaque/interactive, matching
    // docs/ANIMATION_RUNTIME.md's threshold semantics). This is the one
    // place per-frame hit-test masks are derived from real pixel data —
    // both the macOS SDL_SetWindowShape path and the Windows poll-driven
    // fallback read the result of this same function, so rendering and
    // click-through can never disagree, for any frame of any pet.
    //
    // `rgba` may be a different resolution than the destination the
    // frame is rendered at (`targetWidth`/`targetHeight`, typically the
    // pet's logical canvas size) — sampling handles the scale
    // difference; it does not resample colors, only alpha, and never
    // fabricates coverage the source pixel doesn't have.
    static AlphaMask FromAlphaChannel(
        const std::uint8_t* rgba,
        int srcWidth,
        int srcHeight,
        int targetWidth,
        int targetHeight,
        std::uint8_t alphaThreshold);

private:
    int width_;
    int height_;
    std::vector<bool> opaque_;
};

}  // namespace nimvlets::core
