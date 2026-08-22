#include "graphics/ActiveFrameTexture.h"

#include <SDL3/SDL.h>

namespace nimvlets::graphics {

ActiveFrameTexture::~ActiveFrameTexture() {
    Reset();
}

void ActiveFrameTexture::Reset() {
    if (texture_ != nullptr) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    width_ = 0;
    height_ = 0;
    uploaded_ = nullptr;
}

bool ActiveFrameTexture::SetFrame(SDL_Renderer* renderer, const content::FrameDefinition& frame) {
    if (frame.pixels.empty() || frame.width <= 0 || frame.height <= 0) {
        SDL_Log("nimvlets: ActiveFrameTexture: frame has no pixel data (%dx%d, %zu bytes)",
                frame.width, frame.height, frame.pixels.size());
        return false;
    }
    if (frame.pixels.size() != static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height) * 4) {
        SDL_Log("nimvlets: ActiveFrameTexture: pixel buffer size mismatch for %dx%d frame", frame.width, frame.height);
        return false;
    }

    if (texture_ == nullptr || width_ != frame.width || height_ != frame.height) {
        if (texture_ != nullptr) {
            SDL_DestroyTexture(texture_);
            texture_ = nullptr;
        }
        // STREAMING: el patrón correcto para una textura cuyo contenido
        // se reemplaza seguido desde la CPU. RGBA32 coincide exactamente
        // con el layout de FrameDefinition::pixels (el mismo que
        // SDL_CreateSurfaceFrom usaba en el camino por-frame), así que
        // SDL_UpdateTexture es una copia directa sin conversión.
        texture_ = SDL_CreateTexture(
            renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, frame.width, frame.height);
        if (texture_ == nullptr) {
            SDL_Log("nimvlets: ActiveFrameTexture: SDL_CreateTexture failed: %s", SDL_GetError());
            width_ = height_ = 0;
            uploaded_ = nullptr;
            return false;
        }
        // EXPLÍCITOS, no heredados de un default: alpha directo (no
        // premultiplicado) es exactamente lo que compila el pipeline, y
        // SDL_BLENDMODE_BLEND es la ecuación que le corresponde. El
        // camino por-frame dependía del default implícito de
        // SDL_CreateTextureFromSurface (que resulta ser el mismo, ver
        // SDL_render.c: blendMode = ISPIXELFORMAT_ALPHA ? BLEND : NONE)
        // -- acá se fija a mano para que no dependa de ese detalle.
        SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_LINEAR);
        width_ = frame.width;
        height_ = frame.height;
        uploaded_ = nullptr;  // textura nueva -- lo que hubiera antes ya no está
        ++creationCount_;
        // Log deliberadamente NO condicionado a NDEBUG: debe ocurrir
        // UNA sola vez por pet (o por cambio real de dimensiones), así
        // que es barato, y hace que el invariante "no se crean texturas
        // por frame" sea verificable contra el binario Release real.
        SDL_Log("nimvlets: ActiveFrameTexture: created %dx%d streaming texture (creation #%d for this session)",
                frame.width, frame.height, creationCount_);
    }

    if (uploaded_ == &frame) {
        return true;  // ya está subido este mismo frame -- nada que hacer
    }

    if (!SDL_UpdateTexture(texture_, nullptr, frame.pixels.data(), frame.width * 4)) {
        SDL_Log("nimvlets: ActiveFrameTexture: SDL_UpdateTexture failed: %s", SDL_GetError());
        return false;
    }
    uploaded_ = &frame;
    return true;
}

}  // namespace nimvlets::graphics
