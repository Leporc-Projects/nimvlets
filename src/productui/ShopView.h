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
// a ActivateRequest. Hoy `variantId` siempre viene "" (el Shop solo
// ofrece pets sin variantes), pero el campo existe para que la compra
// por variante (shop oculto, futuro) no necesite un rediseño — sale de
// ShopItem::entitlementTarget, no de una suposición sobre petId.
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
    // El owner tocó la afordancia quieta "Starter choices…" (Block 10):
    // entrar al submodo del Starter Shop oculto (que la propia sección
    // Shop posee — NO una cuarta pestaña de nav).
    bool enterStarterSubmode = false;
};

// La sección SHOP del Product UI. Misma arquitectura que CollectionView:
// input (mouse/teclado/rueda) sobre un layout puro (ShopLayout) + un
// anillo de foco (FocusList) + un flag `dirty_` (event-driven, sin
// loop). Reusa TextCache / PetPreviewCache / UiPainter y el hero stage +
// acento por pet de la Collection. Sin ventana ni event loop propios —
// ProductWindow la maneja.
//
// Block 09C — BROWSE-FIRST (DEC-135). Tres cosas distintas, nunca una
// sola variable:
//   - `hoverId_`      : la tarjeta bajo el mouse — revela info liviana,
//                       NO selecciona, NO compra (brief §6).
//   - `selectedPetId_`: "" => modo BROWSE (estantería de personajes, sin
//                       hero); un petId => modo SELECTED (ese personaje
//                       es el hero grande + rail compacto). Un click /
//                       Enter en una tarjeta SELECCIONA — no compra.
//   - `confirming_`   : sub-estado de SELECTED — la confirmación de
//                       compra inline. Un click en "Get <pet>" la abre;
//                       un segundo click deliberado en "Confirmar" emite
//                       hasPurchase; "Cancelar" o Escape la cierran sin
//                       tocar nada (brief §8).
// Ninguna selección se persiste: cerrar/reabrir el Product UI, o volver
// a entrar a la sección, devuelve el Shop a modo BROWSE (brief §13).
class ShopView {
 public:
    void SetModel(catalog::ShopModel model, std::uint64_t clickBalance);
    void SetLanguage(core::Language language);

    // Block 10: ¿mostrar la afordancia quieta "Starter choices…" cerca
    // del pie? src/app la pone en true SOLO cuando el modelo del Starter
    // Shop oculto tiene >= 1 oferta (lifecycle kCompleted + una variante
    // de Frin no poseída, o un starter normal priced). Con 0 ofertas la
    // afordancia NO existe (brief §10).
    void SetStarterAffordanceVisible(bool visible);

    const catalog::ShopModel& Model() const { return model_; }

    // Coordenadas en PUNTOS lógicos de la ventana.
    ShopViewResult OnMouseMove(float x, float y);
    ShopViewResult OnMouseDown(float x, float y);
    ShopViewResult OnWheel(float dyLines);
    ShopViewResult OnKey(int sdlKeycode, bool shiftHeld);
    ShopViewResult OnViewportChanged();

    bool Dirty() const { return dirty_; }
    void ClearDirty() { dirty_ = false; }

    // Llamado por ProductWindow al ENTRAR a la sección Shop: vuelve a
    // modo BROWSE, reinicia el foco a la pestaña "Shop" y limpia hover /
    // confirmación (punto de partida predecible — brief §13/§17).
    void OnEnterSection();

    bool Confirming() const { return confirming_; }
    bool Browsing() const { return selectedPetId_.empty(); }

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
    // Solo-DEV: pone el foco de TECLADO sobre una tarjeta (browse o rail)
    // para la captura de "keyboard-focused candidate" (brief §24).
    void SetTileKeyboardFocusForQA(const std::string& petId) {
        keyboardFocus_ = true;
        focus_.Focus(petId.empty() ? std::string() : ("shopitem:" + petId));
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
    std::string selectedPetId_;  // "" => modo BROWSE
    bool confirming_ = false;

    float scrollY_ = 0.0f;
    float lastContentHeight_ = 0.0f;
    float viewportW_ = 800.0f;
    float viewportH_ = 560.0f;

    std::string hoverId_;
    bool keyboardFocus_ = false;
    bool starterAffordanceVisible_ = false;
    bool dirty_ = true;
};

}  // namespace nimvlets::productui
