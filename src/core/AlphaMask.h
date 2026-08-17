#pragma once

#include <vector>

#include "core/Geometry.h"

namespace nimvlets::core {

// A simple, pure, rasterized hit-test region: for each pixel in a
// width x height grid, whether it counts as "visible/interactive" or
// "transparent/click-through".
//
// Deliberately source-agnostic — src/app builds one either from
// core::BlobSilhouette (the analytic placeholder shape, still used by
// tests/DragClassifierTest.cpp and friends) or from a real image's
// alpha channel at an explicit threshold (see graphics::DevSprite, the
// Block 01 "Bunny" QA fixture loader — see docs/DECISION_LOG.md and
// docs/PLATFORM_SPIKE.md), through this one Contains() interface.
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

private:
    int width_;
    int height_;
    std::vector<bool> opaque_;
};

}  // namespace nimvlets::core
