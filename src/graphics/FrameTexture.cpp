#include "graphics/FrameTexture.h"

#include <SDL3/SDL.h>

namespace nimvlets::graphics {

bool AttachFrameTexture(SDL_Renderer* renderer, content::FrameDefinition& frame) {
    if (frame.rendererHandle != nullptr) {
        return true;
    }
    if (frame.pixels.empty() || frame.width <= 0 || frame.height <= 0) {
        SDL_Log("nimvlets: AttachFrameTexture: frame has no pixel data (%dx%d, %zu bytes)",
                frame.width, frame.height, frame.pixels.size());
        return false;
    }
    if (frame.pixels.size() != static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height) * 4) {
        SDL_Log("nimvlets: AttachFrameTexture: pixel buffer size mismatch for %dx%d frame", frame.width, frame.height);
        return false;
    }

    // SDL_CreateSurfaceFrom does not take ownership/copy eagerly on
    // creation — it wraps frame.pixels.data() directly — so the surface
    // must not outlive this function call. SDL_CreateTextureFromSurface
    // below uploads the pixel data to the renderer's own texture memory
    // immediately, so the surface (and the source buffer it points at)
    // can be safely discarded right after.
    SDL_Surface* surface = SDL_CreateSurfaceFrom(
        frame.width, frame.height, SDL_PIXELFORMAT_RGBA32,
        frame.pixels.data(), frame.width * 4);
    if (surface == nullptr) {
        SDL_Log("nimvlets: AttachFrameTexture: SDL_CreateSurfaceFrom failed: %s", SDL_GetError());
        return false;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (texture == nullptr) {
        SDL_Log("nimvlets: AttachFrameTexture: SDL_CreateTextureFromSurface failed: %s", SDL_GetError());
        return false;
    }

    // Frames are drawn scaled into the pet's logical canvas rect (see
    // SpikeApp::RenderFrame()); linear filtering avoids visible blocking
    // when a frame's native resolution doesn't exactly match the canvas.
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);

    frame.rendererHandle = texture;
    return true;
}

void ReleaseFrameTexture(content::FrameDefinition& frame) {
    if (frame.rendererHandle != nullptr) {
        SDL_DestroyTexture(static_cast<SDL_Texture*>(frame.rendererHandle));
        frame.rendererHandle = nullptr;
    }
}

}  // namespace nimvlets::graphics
