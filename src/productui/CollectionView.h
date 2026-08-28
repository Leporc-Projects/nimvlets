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
    // El owner tocó una pestaña de navegación: ProductWindow cambia de
    // sección (no llega a src/app — el runtime del pet no se toca).
    bool switchSection = false;
    ProductSection targetSection = ProductSection::kCollection;
};

// La Collection interactiva con composición HERO + GALLERY. El Nimvlet
// seleccionado es el protagonista visual (hero stage teñido con su
// acento, nombre/especie/descripción, selector de variante, acción); el
// resto vive en una gallery discreta sobre un segundo plano, debajo de
// un divisor. Junta el modelo, el layout puro (CollectionLayout), el
// foco de teclado (FocusList), los caches de texto/arte, y la capa de
// dibujo. Sin ventana ni event loop propios — ProductWindow la maneja.
//
// Redibuja SOLO ante un cambio real (hover, foco, selección de hero,
// cambio de variante/modelo/idioma): `Dirty()` lo indica, no hay ningún
// loop continuo (brief §20).
class CollectionView {
 public:
    void SetModel(catalog::CollectionModel model, std::uint64_t clickBalance);

    // Idioma de TODO el texto. Redibuja de inmediato; el id del widget
    // con foco es semántico, así que el foco se conserva (brief §21/§28).
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
    // Solo-DEV: fija SOLO el hover sobre una entrada de la gallery
    // (captura de "gallery hover" — sin chrome de foco de teclado, que
    // es una captura aparte).
    void SetGalleryHoverForQA(const std::string& petId) {
        hoverId_ = petId.empty() ? std::string() : ("item:" + petId);
        dirty_ = true;
    }
    // Solo-DEV: pone el foco de TECLADO sobre un chip de variante del
    // hero (captura de "keyboard-focused Frin variant", brief §29).
    void SetVariantKeyboardFocusForQA(const std::string& variantId) {
        focus_.Focus("variant:" + variantId);
        keyboardFocus_ = true;
        dirty_ = true;
    }

    void Render(UiPainter& painter, TextCache& text, PetPreviewCache& previews, float viewportW, float viewportH);

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
    // Modalidad de input (patrón "focus-visible"): el chrome de foco
    // (anillo del botón, pill del chip de variante) se dibuja SOLO
    // mientras el último input fue de teclado. Un click de mouse lo
    // apaga — así una selección de variante con mouse muestra solo el
    // subrayado de acento, no un recuadro tipo control de formulario
    // (brief §19). Arranca en false: al abrir, la atención va al hero.
    bool keyboardFocus_ = false;
    bool dirty_ = true;
};

}  // namespace nimvlets::productui
