#pragma once

#include <string>
#include <unordered_map>

#include "catalog/PetCatalog.h"
#include "content/AnimationDefinition.h"

struct SDL_Renderer;
struct SDL_Texture;

namespace nimvlets::productui {

// Texturas de vista previa del arte de un pet para la Collection: el
// frame 0 de la pose base del primer estado, en dirección canónica.
//
// El pet ACTIVO se inyecta desde src/app (SetActive) — su pack ya está
// en memoria, no se recarga. Cualquier OTRO pet visible (poseído-
// inactivo o, desde Block 06.1, también LOCKED — que ahora muestra su
// arte más callada, brief §12) se carga perezosamente la primera vez
// que se dibuja su entrada (Acquire): se abre su pack, se copia el
// frame 0, y el PetDefinition se descarta de inmediato — solo queda la
// textura chica.
//
// Costo: una carga de pack (~decenas de MB, transitoria) por pet no
// activo visible, pagada una sola vez mientras la Collection está
// abierta. Clear() libera todas las texturas al cerrar la ventana. Ver
// docs/PRODUCT_UI.md §11 (modelo de performance).
class PetPreviewCache {
 public:
    explicit PetPreviewCache(SDL_Renderer* renderer) : renderer_(renderer) {}
    ~PetPreviewCache();

    PetPreviewCache(const PetPreviewCache&) = delete;
    PetPreviewCache& operator=(const PetPreviewCache&) = delete;

    // Sube el frame de reposo del pet activo (pack ya cargado por
    // src/app). Reemplaza cualquier entrada previa para esa identidad.
    void SetActive(const std::string& petId, const std::string& variantId, const content::FrameDefinition& frame);

    // Devuelve la textura de preview para (petId, variantId), cargando
    // el pack si hace falta. nullptr si el pack no se pudo abrir o no
    // hay entrada en el catálogo. La textura la posee el cache.
    SDL_Texture* Acquire(const catalog::PetCatalog& catalog, const std::string& petId, const std::string& variantId);

    // Igual que Acquire pero SIN cargar nada: solo devuelve lo que ya
    // esté cacheado (para dibujar sin bloquear en el primer frame).
    SDL_Texture* Peek(const std::string& petId, const std::string& variantId) const;

    void Clear();

 private:
    static std::string KeyOf(const std::string& petId, const std::string& variantId);
    SDL_Texture* UploadFrame(const content::FrameDefinition& frame);

    SDL_Renderer* renderer_ = nullptr;
    std::unordered_map<std::string, SDL_Texture*> textures_;
};

}  // namespace nimvlets::productui
