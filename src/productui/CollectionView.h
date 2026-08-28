#pragma once

#include <cstdint>
#include <string>

#include "catalog/CollectionModel.h"
#include "catalog/PetCatalog.h"
#include "core/Localization.h"
#include "productui/CollectionLayout.h"
#include "productui/FocusList.h"
#include "productui/PetPreviewCache.h"
#include "productui/TextCache.h"
#include "productui/UiPaint.h"

namespace nimvlets::productui {

// Lo que la vista le pide a src/app hacer como consecuencia de un input.
struct ActivateRequest {
    std::string petId;
    std::string variantId;
};

struct CollectionViewResult {
    bool dirty = false;         // hay que redibujar
    bool requestClose = false;  // Escape -> cerrar la ventana
    bool hasActivate = false;   // el owner pidió activar un pet
    ActivateRequest activate;
};

// La Collection interactiva con composición HERO + GALLERY (Block
// 06.1): el Nimvlet seleccionado es el protagonista visual; el resto
// vive en una gallery discreta debajo. Junta el modelo
// (catalog::CollectionModel), el layout puro (CollectionLayout), el
// foco de teclado (FocusList), los caches de texto/arte, y la capa de
// dibujo. Sin ventana ni event loop propios — ProductWindow la maneja.
//
// Redibuja SOLO ante un cambio real (hover, foco, selección de hero,
// cambio de variante/modelo/idioma): `Dirty()` lo indica, no hay ningún
// loop continuo (block brief §19/§20).
class CollectionView {
 public:
    void SetModel(catalog::CollectionModel model, std::uint64_t clickBalance);

    // Idioma de TODO el texto. Redibuja de inmediato; el id del widget
    // con foco es semántico, así que el foco se conserva (brief §21).
    void SetLanguage(core::Language language);

    const catalog::CollectionModel& Model() const { return model_; }

    // Entradas. Coordenadas en PUNTOS lógicos de la ventana.
    CollectionViewResult OnMouseMove(float x, float y);
    CollectionViewResult OnMouseDown(float x, float y);
    CollectionViewResult OnWheel(float dyLines);
    CollectionViewResult OnKey(int sdlKeycode, bool shiftHeld);
    CollectionViewResult OnViewportChanged();

    bool Dirty() const { return dirty_; }
    void ClearDirty() { dirty_ = false; }

    // Solo-DEV (QA / capturas): elige el hero / la variante del hero sin
    // un click real. No-op si el pet no está en el modelo.
    void SelectHeroForQA(const std::string& petId) { SelectHero(petId); }
    void SetHeroVariantForQA(const std::string& variantId) {
        selectedVariantId_ = variantId;
        dirty_ = true;
    }
    // Solo-DEV: fija el estado de hover + foco visible sobre una entrada
    // de la gallery (para la captura de "hover/focus state").
    void SetGalleryHoverForQA(const std::string& petId) {
        hoverId_ = petId.empty() ? std::string() : ("item:" + petId);
        focus_.Focus(hoverId_);
        focusVisible_ = !petId.empty();
        dirty_ = true;
    }

    void Render(
        UiPainter& painter, TextCache& text, PetPreviewCache& previews, const catalog::PetCatalog& catalog,
        float viewportW, float viewportH);

 private:
    CollectionLayout BuildLayout(float w, float h) const;
    void SyncFocusList(const CollectionLayout& layout);
    void SelectHero(const std::string& petId);
    std::string HoverPetId() const;  // "item:<x>" -> "<x>", si no ""
    CollectionViewResult ActivateWidget(const std::string& focusId);

    catalog::CollectionModel model_;
    std::uint64_t clickBalance_ = 0;
    core::Language language_ = core::Language::kEn;

    FocusList focus_;
    // "" => el hero sigue al pet activo del modelo.
    std::string selectedPetId_;
    // "" => variante por defecto del hero.
    std::string selectedVariantId_;

    float scrollY_ = 0.0f;
    float lastContentHeight_ = 0.0f;
    float viewportW_ = 800.0f;
    float viewportH_ = 560.0f;

    std::string hoverId_;
    // El anillo de foco solo se dibuja después de la primera navegación
    // por teclado (o un click que enfoca) — patrón "focus-visible":
    // así, al abrir, la atención va al hero, no a un anillo sobre el
    // primer item de la gallery (brief §21/§26).
    bool focusVisible_ = false;
    bool dirty_ = true;
};

}  // namespace nimvlets::productui
