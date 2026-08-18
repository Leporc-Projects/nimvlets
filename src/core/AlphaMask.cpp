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

AlphaMask AlphaMask::FromAlphaChannel(
    const std::uint8_t* rgba,
    int srcWidth,
    int srcHeight,
    int targetWidth,
    int targetHeight,
    std::uint8_t alphaThreshold) {
    AlphaMask mask(targetWidth, targetHeight);
    if (rgba == nullptr || srcWidth <= 0 || srcHeight <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        return mask;
    }

    for (int ty = 0; ty < targetHeight; ++ty) {
        int sy = (ty * srcHeight) / targetHeight;
        if (sy >= srcHeight) {
            sy = srcHeight - 1;
        }
        for (int tx = 0; tx < targetWidth; ++tx) {
            int sx = (tx * srcWidth) / targetWidth;
            if (sx >= srcWidth) {
                sx = srcWidth - 1;
            }
            const std::size_t offset =
                (static_cast<std::size_t>(sy) * static_cast<std::size_t>(srcWidth) + static_cast<std::size_t>(sx)) * 4;
            const std::uint8_t alpha = rgba[offset + 3];
            mask.SetOpaque(tx, ty, alpha >= alphaThreshold);
        }
    }
    return mask;
}

}  // namespace nimvlets::core
