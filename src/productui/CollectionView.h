#pragma once

#include <cstdint>
#include <string>

#include "catalog/CollectionModel.h"
#include "catalog/PetCatalog.h"
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
    bool requestClose = false;  // Escape sin detalle abierto -> cerrar la ventana
    bool hasActivate = false;   // el owner pidió activar un pet
    ActivateRequest activate;
};

// La Collection interactiva: junta el modelo (catalog::CollectionModel)
// con el layout puro (CollectionLayout), el foco de teclado (FocusList),
// los caches de texto/arte, y la capa de dibujo. Sin ventana ni event
// loop propios — ProductWindow la maneja.
//
// Redibuja SOLO ante un cambio real (hover, foco, scroll, abrir/cerrar
// detalle, cambio de modelo): `Dirty()` lo indica, no hay ningún loop
// continuo (block brief §19).
class CollectionView {
 public:
    // Snapshot del modelo + balance de clicks a mostrar. Se llama al
    // abrir la ventana y cada vez que cambian el pet activo o la
    // propiedad.
    void SetModel(catalog::CollectionModel model, std::uint64_t clickBalance);

    const catalog::CollectionModel& Model() const { return model_; }

    // Entradas. Coordenadas en PUNTOS lógicos de la ventana.
    CollectionViewResult OnMouseMove(float x, float y);
    CollectionViewResult OnMouseDown(float x, float y);
    CollectionViewResult OnWheel(float dyLines);
    CollectionViewResult OnKey(int sdlKeycode, bool shiftHeld);
    CollectionViewResult OnViewportChanged();

    bool Dirty() const { return dirty_; }
    void ClearDirty() { dirty_ = false; }

    // Solo-DEV: abre el panel de detalle de `petId` sin un click real
    // (para QA / capturas). No-op si `petId` no está en el modelo.
    void OpenDetailForQA(const std::string& petId) { OpenDetail(petId); }

    void Render(
        UiPainter& painter, TextCache& text, PetPreviewCache& previews, const catalog::PetCatalog& catalog,
        float viewportW, float viewportH);

 private:
    CollectionLayout BuildLayout(float w, float h) const;
    void SyncFocusList(const CollectionLayout& layout);
    void OpenDetail(const std::string& petId);
    void CloseDetail();
    CollectionViewResult ActivateWidget(const std::string& focusId);
    std::string ResolvedDetailVariant() const;

    catalog::CollectionModel model_;
    std::uint64_t clickBalance_ = 0;

    FocusList focus_;
    bool detailOpen_ = false;
    std::string detailPetId_;
    std::string detailVariantId_;

    float scrollY_ = 0.0f;
    float lastContentHeight_ = 0.0f;
    float viewportW_ = 760.0f;
    float viewportH_ = 540.0f;

    std::string hoverId_;
    bool dirty_ = true;
};

}  // namespace nimvlets::productui
