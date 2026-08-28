#include "productui/PetPreviewCache.h"

#include <SDL3/SDL.h>

#include <string>

#include "productui/PreviewArtifact.h"

namespace nimvlets::productui {

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
    bundleLoaded_ = false;
    bundleImageCount_ = 0;
    bundleBytes_ = 0;
}

std::string PetPreviewCache::KeyOf(const std::string& petId, const std::string& variantId) {
    return petId + "/" + variantId;
}

SDL_Texture* PetPreviewCache::UploadRgba(
    int width, int height, const std::uint8_t* pixels, std::size_t byteCount) {
    if (width <= 0 || height <= 0 || pixels == nullptr) {
        return nullptr;
    }
    if (byteCount != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4) {
        return nullptr;
    }
    SDL_Texture* tex =
        SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
    if (tex == nullptr) {
        SDL_Log("nimvlets: PetPreviewCache: SDL_CreateTexture failed: %s", SDL_GetError());
        return nullptr;
    }
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
    SDL_UpdateTexture(tex, nullptr, pixels, width * 4);
    return tex;
}

void PetPreviewCache::LoadBundle(const catalog::PetCatalog& catalog) {
    if (bundleLoaded_) {
        return;
    }
    bundleLoaded_ = true;

    for (const catalog::CatalogEntry& entry : catalog.Entries()) {
        const std::string key = KeyOf(entry.identity.petId, entry.identity.variantId);
        if (textures_.find(key) != textures_.end()) {
            continue;  // ya lo puso SetActive
        }
        const std::string previewPath = PreviewPathForPack(entry.packPath);
        PetPreviewImage image;
        std::string error;
        if (!LoadPetPreviewFromFile(previewPath, image, error)) {
            SDL_Log("nimvlets: PetPreviewCache: no preview for '%s' (%s): %s", key.c_str(),
                    previewPath.c_str(), error.c_str());
            continue;
        }
        if (image.petId != entry.identity.petId || image.variantId != entry.identity.variantId) {
            SDL_Log("nimvlets: PetPreviewCache: preview '%s' self-identifies as '%s/%s' but catalog says '%s' — using it anyway",
                    previewPath.c_str(), image.petId.c_str(), image.variantId.c_str(), key.c_str());
        }
        SDL_Texture* tex = UploadRgba(image.width, image.height, image.rgba.data(), image.rgba.size());
        if (tex == nullptr) {
            continue;
        }
        textures_.emplace(key, tex);
        ++bundleImageCount_;
        bundleBytes_ += image.rgba.size();
    }

    SDL_Log("nimvlets: PetPreviewCache: loaded %zu preview image(s), %.2f MB of texture data",
            bundleImageCount_, static_cast<double>(bundleBytes_) / (1024.0 * 1024.0));
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
    if (frame.pixels.empty()) {
        return;
    }
    if (SDL_Texture* tex = UploadRgba(frame.width, frame.height, frame.pixels.data(), frame.pixels.size());
        tex != nullptr) {
        textures_.emplace(key, tex);
    }
}

SDL_Texture* PetPreviewCache::Get(const std::string& petId, const std::string& variantId) const {
    const auto it = textures_.find(KeyOf(petId, variantId));
    return it != textures_.end() ? it->second : nullptr;
}

}  // namespace nimvlets::productui
