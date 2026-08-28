#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

#include "catalog/PetCatalog.h"
#include "content/AnimationDefinition.h"

struct SDL_Renderer;
struct SDL_Texture;

namespace nimvlets::productui {

// Texturas de vista previa del arte de un pet para la Collection.
//
// Block 06.2 (§4-§7): la Collection ya NO abre ni parsea el pack de
// animación completo de un pet (~46-76 MB c/u) para mostrar una preview
// estática. Al abrir la ventana, LoadBundle() carga de una sola vez el
// artefacto liviano ".nvprev" ("NVPREV1", ~0.3-0.4 MB c/u) de cada
// entrada del catálogo — el frame de reposo canónico ya extraído por el
// pipeline de assets (tools/compile_pet_preview.py). Total en memoria:
// unos pocos MB de texturas chicas, no cientos.
//
// El pet ACTIVO se inyecta además desde src/app (SetActive) con su frame
// de reposo real a resolución completa — su pack ya está en RAM, así que
// su preview es la más nítida posible sin ningún costo extra; sobre-
// escribe la entrada del bundle para esa identidad.
//
// Cambiar de variante (Frin Macho <-> Hembra) o de hero es entonces un
// lookup en un mapa sobre texturas ya residentes — sin I/O, sin parseo,
// dentro del redraw event-driven normal. Clear() libera todo al cerrar
// la ventana; sin hilos, sin carga perezosa de packs.
class PetPreviewCache {
 public:
    explicit PetPreviewCache(SDL_Renderer* renderer) : renderer_(renderer) {}
    ~PetPreviewCache();

    PetPreviewCache(const PetPreviewCache&) = delete;
    PetPreviewCache& operator=(const PetPreviewCache&) = delete;

    // Carga (eager, una sola vez al abrir la ventana) la preview liviana
    // de cada entrada del catálogo, por la convención de nombres
    // PreviewPathForPack(packPath). Una preview faltante/corrupta se
    // registra y se saltea — su entrada simplemente no dibuja arte. Idem-
    // potente: volver a llamar no recarga lo que ya está.
    void LoadBundle(const catalog::PetCatalog& catalog);

    // Sube el frame de reposo del pet activo (pack ya cargado por
    // src/app) a resolución completa. Reemplaza cualquier entrada previa
    // para esa identidad (incluida la del bundle).
    void SetActive(const std::string& petId, const std::string& variantId, const content::FrameDefinition& frame);

    // Textura de preview para (petId, variantId), o nullptr si no hay
    // ninguna cargada. La textura la posee el cache — no destruirla.
    SDL_Texture* Get(const std::string& petId, const std::string& variantId) const;

    void Clear();

    // Solo para logging/diagnóstico (informe de performance del bloque).
    std::size_t BundleImageCount() const { return bundleImageCount_; }
    std::size_t BundleBytes() const { return bundleBytes_; }

 private:
    static std::string KeyOf(const std::string& petId, const std::string& variantId);
    SDL_Texture* UploadRgba(int width, int height, const std::uint8_t* pixels, std::size_t byteCount);

    SDL_Renderer* renderer_ = nullptr;
    std::unordered_map<std::string, SDL_Texture*> textures_;
    bool bundleLoaded_ = false;
    std::size_t bundleImageCount_ = 0;
    std::size_t bundleBytes_ = 0;
};

}  // namespace nimvlets::productui
