#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "catalog/CollectionModel.h"
#include "catalog/PetCatalog.h"
#include "content/AnimationDefinition.h"
#include "core/Localization.h"
#include "productui/CollectionView.h"

struct SDL_Window;
struct SDL_Renderer;
union SDL_Event;

namespace nimvlets::productui {

// Resultado de pasarle un SDL_Event a la ventana de producto.
struct ProductWindowEvent {
    bool consumed = false;        // el evento era de esta ventana
    bool closeRequested = false;  // el owner cerró la ventana (X) -> src/app hace Close(), NO quit
    bool hasActivate = false;
    ActivateRequest activate;
};

// Ventana de aplicación NORMAL (con marco, enfocable, redimensionable)
// para el Product UI de Block 06 — separada por completo de la ventana
// transparente del pet. Se puede abrir y cerrar de forma independiente:
// cerrarla NO termina Nimvlets, no resetea el pet activo ni el balance,
// no detiene el runtime del pet (block brief §4/§18). Al cerrar libera
// TODOS sus recursos de GPU (renderer, texturas, caches); reabrir
// reconstruye — barato y correcto (block brief §18).
//
// Event-driven: redibuja solo ante un cambio real de la vista o un
// EXPOSED. Sin loop de render oculto, sin polling (block brief §19).
class ProductWindow {
 public:
    ProductWindow() = default;
    ~ProductWindow();

    ProductWindow(const ProductWindow&) = delete;
    ProductWindow& operator=(const ProductWindow&) = delete;

    bool IsOpen() const { return window_ != nullptr; }

    // Crea ventana + renderer + caches + vista, y carga el bundle de
    // previews livianas (".nvprev") de todas las entradas de `catalog`
    // de una sola vez. `catalog` solo tiene que vivir durante esta
    // llamada. No-op si ya está abierta (solo la trae al frente).
    bool Open(const catalog::PetCatalog& catalog);

    // Destruye todos los recursos. Seguro llamar aunque ya esté cerrada.
    void Close();

    // Trae la ventana al frente y le da foco de teclado.
    void FocusWindow();

    std::uint32_t WindowId() const;

    // Snapshot del modelo + balance. Se llama al abrir y cada vez que
    // cambian el pet activo o la propiedad.
    void SetModel(const catalog::CollectionModel& model, std::uint64_t clickBalance);

    // Idioma de TODO el texto de la Collection. Cambiarlo redibuja de
    // inmediato, sin reiniciar (block brief §5). No-op si está cerrada.
    void SetLanguage(core::Language language);

    // Frame de reposo del pet ACTIVO (su pack ya está cargado por
    // src/app) para la preview, sin recargar nada.
    void SetActivePreview(
        const std::string& petId, const std::string& variantId, const content::FrameDefinition& restFrame);

    // Procesa un SDL_Event. Ignora (consumed=false) los que no son de
    // esta ventana.
    ProductWindowEvent HandleEvent(const SDL_Event& event);

    // Redibuja si la vista está sucia o hay un EXPOSED pendiente. Barato
    // y no-op si no hay nada que hacer.
    void RenderIfNeeded();

    // Solo-DEV (QA / capturas): fuerza un redibujo y vuelca el
    // framebuffer del renderer de la Collection a un BMP en
    // `path` (SDL_RenderReadPixels + SDL_SaveBMP — cómputo local, sin
    // ninguna captura de pantalla del SO, ver AGENTS.md §5). A densidad
    // de píxeles nativa. Devuelve false si la ventana está cerrada o la
    // escritura falla. No forma parte del producto.
    bool CaptureToBmpForQA(const std::string& path);

    // Solo-DEV (QA / capturas): elige el hero / su variante / el hover /
    // el foco de teclado sobre un chip de variante.
    void SelectHeroForQA(const std::string& petId) { view_.SelectHeroForQA(petId); }
    void SetHeroVariantForQA(const std::string& variantId) { view_.SetHeroVariantForQA(variantId); }
    void SetGalleryHoverForQA(const std::string& petId) { view_.SetGalleryHoverForQA(petId); }
    void SetVariantKeyboardFocusForQA(const std::string& variantId) {
        view_.SetVariantKeyboardFocusForQA(variantId);
    }

 private:
    void RecomputeScale();
    void DestroyResources();
    void DrawFrame();  // dibuja la vista al backbuffer, SIN presentar

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;

    std::unique_ptr<TextCache> text_;
    std::unique_ptr<PetPreviewCache> previews_;
    CollectionView view_;

    float scale_ = 1.0f;
    bool pendingExpose_ = true;
};

}  // namespace nimvlets::productui
