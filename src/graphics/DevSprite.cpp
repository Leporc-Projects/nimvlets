#include "graphics/DevSprite.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstring>
#include <fstream>

namespace nimvlets::graphics {

bool DevSprite::LoadFromFile(const std::string& path) {
    width_ = 0;
    height_ = 0;
    pixels_.clear();

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        SDL_Log("nimvlets: DevSprite: could not open %s", path.c_str());
        return false;
    }

    char header[12];
    file.read(header, sizeof(header));
    if (!file || std::memcmp(header, "NVR1", 4) != 0) {
        SDL_Log("nimvlets: DevSprite: %s is not a valid .rgba fixture (bad header)", path.c_str());
        return false;
    }

    std::uint32_t w = 0;
    std::uint32_t h = 0;
    // Format is little-endian by construction (tools/prep_dev_sprite.py
    // writes it with struct.pack("<II", ...)); a plain memcpy is
    // correct on every platform this block targets (macOS/Windows are
    // both little-endian), so no explicit byte-swap is implemented.
    std::memcpy(&w, header + 4, 4);
    std::memcpy(&h, header + 8, 4);

    const std::size_t expectedBytes = static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4;
    std::vector<std::uint8_t> pixels(expectedBytes);
    file.read(reinterpret_cast<char*>(pixels.data()), static_cast<std::streamsize>(expectedBytes));
    if (!file) {
        SDL_Log("nimvlets: DevSprite: %s is truncated (expected %zu pixel bytes)", path.c_str(), expectedBytes);
        return false;
    }

    width_ = static_cast<int>(w);
    height_ = static_cast<int>(h);
    pixels_ = std::move(pixels);
    return true;
}

SDL_Texture* DevSprite::CreateTexture(SDL_Renderer* renderer) const {
    if (!IsLoaded()) {
        return nullptr;
    }

    SDL_Surface* surface = SDL_CreateSurfaceFrom(
        width_, height_, SDL_PIXELFORMAT_RGBA32,
        const_cast<std::uint8_t*>(pixels_.data()), width_ * 4);
    if (surface == nullptr) {
        SDL_Log("nimvlets: DevSprite: SDL_CreateSurfaceFrom failed: %s", SDL_GetError());
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);  // texture creation copies pixel data; surface not needed after
    if (texture == nullptr) {
        SDL_Log("nimvlets: DevSprite: SDL_CreateTextureFromSurface failed: %s", SDL_GetError());
        return nullptr;
    }

    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    return texture;
}

core::AlphaMask DevSprite::BuildAlphaMask(int targetWidth, int targetHeight) const {
    core::AlphaMask mask(targetWidth, targetHeight);
    if (!IsLoaded() || targetWidth <= 0 || targetHeight <= 0) {
        return mask;
    }

    for (int ty = 0; ty < targetHeight; ++ty) {
        const int sy = std::min(height_ - 1, (ty * height_) / targetHeight);
        for (int tx = 0; tx < targetWidth; ++tx) {
            const int sx = std::min(width_ - 1, (tx * width_) / targetWidth);
            const std::size_t offset =
                (static_cast<std::size_t>(sy) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(sx)) * 4;
            const std::uint8_t alpha = pixels_[offset + 3];
            mask.SetOpaque(tx, ty, alpha >= kHitTestAlphaThreshold);
        }
    }
    return mask;
}

}  // namespace nimvlets::graphics
