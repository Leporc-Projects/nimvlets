#pragma once

#include <cstdint>
#include <string>

#include "catalog/StarterShopModel.h"
#include "core/Localization.h"
#include "productui/FocusList.h"
#include "productui/PetPreviewCache.h"
#include "productui/SectionNav.h"
#include "productui/ShopView.h"  // productui::PurchaseRequest (reusado)
#include "productui/StarterShopLayout.h"
#include "productui/TextCache.h"
#include "productui/UiPaint.h"

namespace nimvlets::productui {

struct StarterShopViewResult {
    bool dirty = false;
    // "← Shop" / Esc (en browse) / pestaña "Shop": volver al Shop público
    // (el submodo se cierra; src/app vuelve ShopView a BROWSE).
    bool exitSubmode = false;
    // Pestaña "Collection" / "Settings": cambiar de sección (deja el
    // submodo).
    bool switchSection = false;
    ProductSection targetSection = ProductSection::kCollection;
    // El owner confirmó una compra de starter. `purchase` lleva la
    // IDENTIDAD EXACTA ({petId, variantId}) de la oferta.
    bool hasPurchase = false;
    PurchaseRequest purchase;
};

// La vista del SHOP OCULTO DE STARTERS (Block 10) — el submodo
// contextual que la sección Shop posee. Misma arquitectura que ShopView
// (input sobre un layout puro + FocusList + dirty_ event-driven; reusa
// TextCache / PetPreviewCache / ShopPaint), pero opera en IDENTIDADES
// EXACTAS: dos "Frin" (male / female) conviven como ofertas distintas.
//
// No tiene ventana ni event loop propios — ProductWindow la rutea cuando
// `section_ == kShop && starterSubmode_`. La cabecera compartida sigue
// marcando "Shop" (brief §11). NUNCA cierra la ventana: Esc siempre da
// un paso atrás (confirm -> selected -> browse -> Shop público).
class StarterShopView {
 public:
    // El balance de clics NO viaja con el modelo (corrección de QA del
    // owner, Block 10): ProductWindow lo pasa a Render() como valor
    // CANÓNICO único — ver docs/PRODUCT_UI.md §17.
    void SetModel(catalog::StarterShopModel model);
    void SetLanguage(core::Language language);

    const catalog::StarterShopModel& Model() const { return model_; }

    // Coordenadas en PUNTOS lógicos de la ventana.
    StarterShopViewResult OnMouseMove(float x, float y);
    StarterShopViewResult OnMouseDown(float x, float y);
    StarterShopViewResult OnWheel(float dyLines);
    StarterShopViewResult OnKey(int sdlKeycode, bool shiftHeld);
    StarterShopViewResult OnViewportChanged();

    bool Dirty() const { return dirty_; }
    void ClearDirty() { dirty_ = false; }

    // Llamado por ProductWindow al ENTRAR al submodo: modo BROWSE, foco en
    // "← Shop", sin selección / hover / confirmación.
    void OnEnterSubmode();

    bool Confirming() const { return confirming_; }
    bool Browsing() const { return selectedFocusId_.empty(); }

    // Solo-DEV (QA / capturas).
    void SelectOfferForQA(const std::string& petId, const std::string& variantId);
    void SetConfirmingForQA(bool confirming) {
        confirming_ = confirming;
        dirty_ = true;
    }
    void SetHoverForQA(const std::string& petId, const std::string& variantId) {
        hoverId_ = petId.empty() ? std::string()
                                 : ("starteritem:" + petId + "/" + variantId);
        dirty_ = true;
    }
    void SetKeyboardFocusForQA(const std::string& focusId) {
        keyboardFocus_ = true;
        focus_.Focus(focusId);
        dirty_ = true;
    }

    void Render(
        UiPainter& painter, TextCache& text, PetPreviewCache& previews, float viewportW,
        float viewportH, std::uint64_t clickBalance);

 private:
    StarterShopLayout BuildLayout(float w, float h) const;
    void SyncFocusList(const StarterShopLayout& layout);
    StarterShopViewResult ActivateWidget(const std::string& focusId);
    void SelectHero(const std::string& focusId);

    catalog::StarterShopModel model_;
    // Cache del balance CANÓNICO: lo fija Render() cada frame (valor de
    // ProductWindow) para que BuildLayout() lo pase a la cabecera.
    std::uint64_t clickBalance_ = 0;
    core::Language language_ = core::Language::kEn;
    // Anchos MEDIDOS de los rótulos de nav (convergencia DEC-147).
    float navLabelWidths_[3] = {0.0f, 0.0f, 0.0f};

    FocusList focus_;
    std::string selectedFocusId_;  // "" => modo BROWSE
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
