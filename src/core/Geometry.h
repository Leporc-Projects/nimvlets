#pragma once

namespace nimvlets::core {

// Window-local, DPI-independent 2D point. Plain data, no SDL dependency —
// this header is included by both the platform-agnostic core and the
// SDL-aware graphics/platform layers.
struct Point {
    double x = 0.0;
    double y = 0.0;
};

inline double DistanceSquared(Point a, Point b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return dx * dx + dy * dy;
}

}  // namespace nimvlets::core
