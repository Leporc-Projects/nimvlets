#include "core/AlphaMask.h"

#include <cmath>

namespace nimvlets::core {

AlphaMask::AlphaMask(int width, int height)
    : width_(width < 0 ? 0 : width),
      height_(height < 0 ? 0 : height),
      opaque_(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_), false) {}

void AlphaMask::SetOpaque(int x, int y, bool opaque) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return;
    }
    opaque_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)] = opaque;
}

bool AlphaMask::Contains(Point point) const {
    const int x = static_cast<int>(std::floor(point.x));
    const int y = static_cast<int>(std::floor(point.y));
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return false;
    }
    return opaque_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)];
}

}  // namespace nimvlets::core
