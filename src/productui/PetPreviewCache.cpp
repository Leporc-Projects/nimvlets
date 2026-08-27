#include "productui/PetPreviewCache.h"

#include <SDL3/SDL.h>

#include <string>

#include "content/PetPackLoader.h"

namespace nimvlets::productui {

namespace {

// Frame de reposo canónico de un pack ya cargado: frame 0 de la pose
// base del primer estado, en Direction::kRight (la dirección canónica
// del runtime — ver content::Direction). nullptr si el pack está
// estructuralmente vacío (no debería pasar con un pack válido).
const content::FrameDefinition* RestFrameOf(const content::PetDefinition& pet) {
    if (pet.states.empty()) {
        return nullptr;
    }
    const content::BehaviorState& state = pet.states.front();
    const content::AnimationDefinition& anim = content::ResolveAnimation(
        state.baseAnimation, state.baseAnimationDirectionOverrides, content::Direction::kRight);
    if (anim.frames.empty()) {
        return nullptr;
    }
    return &anim.frames.front();
}

}  // namespace

PetPreviewCache::~PetPreviewCache() {
    Clear();
}

void PetPreviewCache::Clear() {
    for (auto& [key, tex] : textures_) {
        if (tex != nullptr) {
            SDL_DestroyTexture(tex);
        }
    }
    textures_.clear();
}

std::string PetPreviewCache::KeyOf(const std::string& petId, const std::string& variantId) {
    return petId + "/" + variantId;
}

SDL_Texture* PetPreviewCache::UploadFrame(const content::FrameDefinition& frame) {
    if (frame.width <= 0 || frame.height <= 0 || frame.pixels.empty()) {
        return nullptr;
    }
    if (frame.pixels.size() != static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height) * 4) {
        return nullptr;
    }
    SDL_Texture* tex = SDL_CreateTexture(
        renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, frame.width, frame.height);
    if (tex == nullptr) {
        SDL_Log("nimvlets: PetPreviewCache: SDL_CreateTexture failed: %s", SDL_GetError());
        return nullptr;
    }
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
    SDL_UpdateTexture(tex, nullptr, frame.pixels.data(), frame.width * 4);
    return tex;
}

void PetPreviewCache::SetActive(
    const std::string& petId, const std::string& variantId, const content::FrameDefinition& frame) {
    const std::string key = KeyOf(petId, variantId);
    if (const auto it = textures_.find(key); it != textures_.end()) {
        if (it->second != nullptr) {
            SDL_DestroyTexture(it->second);
        }
        textures_.erase(it);
    }
    if (SDL_Texture* tex = UploadFrame(frame); tex != nullptr) {
        textures_.emplace(key, tex);
    }
}

SDL_Texture* PetPreviewCache::Peek(const std::string& petId, const std::string& variantId) const {
    const auto it = textures_.find(KeyOf(petId, variantId));
    return it != textures_.end() ? it->second : nullptr;
}

SDL_Texture* PetPreviewCache::Acquire(
    const catalog::PetCatalog& catalog, const std::string& petId, const std::string& variantId) {
    const std::string key = KeyOf(petId, variantId);
    if (const auto it = textures_.find(key); it != textures_.end()) {
        return it->second;
    }

    const catalog::CatalogEntry* entry = catalog.Find(catalog::PetIdentity{petId, variantId});
    if (entry == nullptr) {
        textures_.emplace(key, nullptr);  // no reintentar
        return nullptr;
    }

    content::PetDefinition pet;
    std::string error;
    SDL_Texture* tex = nullptr;
    if (content::LoadPetPackFromFile(entry->packPath, pet, error)) {
        if (const content::FrameDefinition* frame = RestFrameOf(pet)) {
            tex = UploadFrame(*frame);
        }
        // `pet` (y todos sus frames decodificados) se libera al salir de
        // este scope — solo sobrevive la textura chica.
    } else {
        SDL_Log("nimvlets: PetPreviewCache: could not load pack for preview '%s': %s", petId.c_str(), error.c_str());
    }

    textures_.emplace(key, tex);
    return tex;
}

}  // namespace nimvlets::productui
