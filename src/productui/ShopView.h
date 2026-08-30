#pragma once

#include <cstdint>
#include <string>

#include "catalog/ShopModel.h"
#include "core/Localization.h"
#include "productui/FocusList.h"
#include "productui/PetPreviewCache.h"
#include "productui/SectionNav.h"
#include "productui/ShopLayout.h"
#include "productui/TextCache.h"
#include "productui/UiPaint.h"

namespace nimvlets::productui {

// Lo que la vista del Shop le pide a src/app: comprar la IDENTIDAD de
// catálogo `{petId, variantId}` (tras la confirmación inline). Simétrico
// a ActivateRequest. En Block 07 `variantId` siempre viene "" (el Shop
// solo ofrece pets sin variantes), pero el campo existe para que la
// compra por variante (shop oculto, futuro) no necesite un rediseño —
// sale de ShopItem::entitlementTarget, no de una suposición sobre petId.
struct PurchaseRequest {
    std::string petId;
    std::string variantId;
};

struct ShopViewResult {
    bool dirty = false;
    bool requestClose = false;   // Escape (sin confirmación abierta) -> cerrar
    bool hasPurchase = false;    // el owner confirmó una compra
    PurchaseRequest purchase;
    bool switchSection = false;  // tocó una pestaña de navegación
    ProductSection targetSection = ProductSection::kCollection;
};

// La sección SHOP del Product UI (Block 07). Misma arquitectura que
// CollectionView: input (mouse/teclado/rueda) sobre un layout puro
// (ShopLayout) + un anillo de foco (FocusList) + un flag `dirty_`
// (event-driven, sin loop). Reusa TextCache / PetPreviewCache /
// UiPainter y el hero stage + acento por pet de la Collection. Sin
// ventana ni event loop propios — ProductWindow la maneja.
//
// La confirmación de compra es un estado LOCAL de la vista
// (`confirming_`): un click en "Get <pet>" la abre; un segundo click
// deliberado en "Confirmar" emite hasPurchase; "Cancelar" o Escape la
// cierran sin tocar nada (brief §12).
class ShopView {
 public:
    void SetModel(catalog::ShopModel model, std::uint64_t clickBalance);
    void SetLanguage(core::Language language);

    const catalog::ShopModel& Model() const { return model_; }

    // Coordenadas en PUNTOS lógicos de la ventana.
    ShopViewResult OnMouseMove(float x, float y);
    ShopViewResult OnMouseDown(float x, float y);
    ShopViewResult OnWheel(float dyLines);
    ShopViewResult OnKey(int sdlKeycode, bool shiftHeld);
    ShopViewResult OnViewportChanged();

    bool Dirty() const { return dirty_; }
    void ClearDirty() { dirty_ = false; }

    // Llamado por ProductWindow al ENTRAR a la sección Shop: reinicia el
    // foco a la primera pestaña y limpia el hover (para que el foco de
    // teclado arranque en un lugar predecible — brief §17).
    void OnEnterSection();

    bool Confirming() const { return confirming_; }

    // Solo-DEV (QA / capturas).
    void SelectHeroForQA(const std::string& petId);
    void SetConfirmingForQA(bool confirming) {
        confirming_ = confirming;
        dirty_ = true;
    }
    void SetGalleryHoverForQA(const std::string& petId) {
        hoverId_ = petId.empty() ? std::string() : ("shopitem:" + petId);
        dirty_ = true;
    }

    void Render(UiPainter& painter, TextCache& text, PetPreviewCache& previews, float viewportW, float viewportH);

 private:
    ShopLayout BuildLayout(float w, float h) const;
    void SyncFocusList(const ShopLayout& layout);
    std::string HoverPetId() const;
    ShopViewResult ActivateWidget(const std::string& focusId);
    void SelectHero(const std::string& petId);

    catalog::ShopModel model_;
    std::uint64_t clickBalance_ = 0;
    core::Language language_ = core::Language::kEn;

    FocusList focus_;
    std::string selectedPetId_;  // "" => primer pet del Shop
    bool confirming_ = false;

    float scrollY_ = 0.0f;
    float lastContentHeight_ = 0.0f;
    float viewportW_ = 800.0f;
    float viewportH_ = 560.0f;

    std::string hoverId_;
    bool keyboardFocus_ = false;
    bool dirty_ = true;
};

}  // namespace nimvlets::productui
