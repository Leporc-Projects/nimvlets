#pragma once

#include <string>

#include "catalog/OnboardingPolicy.h"
#include "catalog/PetIdentity.h"
#include "core/Localization.h"
#include "productui/FocusList.h"
#include "productui/OnboardingLayout.h"
#include "productui/PetPreviewCache.h"
#include "productui/TextCache.h"
#include "productui/UiPaint.h"

namespace nimvlets::productui {

// Lo que la vista de onboarding le pide a src/app: aplicar la selección
// de starter CONFIRMADA. `selection` es la identidad EXACTA (para Frin,
// la variante concreta ya elegida). src/app la pasa por
// catalog::EvaluateOnboardingSelection y, si es kOk, aplica la
// transacción de completitud (brief §14) y saca a la ventana del modo
// onboarding. NUNCA hay `requestClose` ni `switchSection`: onboarding no
// es una sección y Esc no lo puede saltear (brief §19/§25).
struct OnboardingViewResult {
    bool dirty = false;
    bool hasSelection = false;
    catalog::PetIdentity selection;
};

// La pantalla de ONBOARDING de primer arranque (Block 09A). Misma
// arquitectura que las otras vistas del Product UI: input
// (mouse/teclado/rueda) sobre un layout puro (OnboardingLayout) + un
// anillo de foco (FocusList) + un flag `dirty_` (event-driven, sin
// loop). Reusa TextCache / PetPreviewCache (para el arte `.nvprev` de
// los candidatos — NO abre ningún `.nvpack`, brief §26). Sin ventana ni
// event loop propios — ProductWindow la maneja.
//
// La confirmación y la sub-elección de variante de Frin son ESTADO
// LOCAL de la vista (`stage_`): un click en un candidato normal abre la
// confirmación; un click en Frin abre "Which Frin?"; sólo "Choose
// <name>" emite `hasSelection`. Esc cancela una confirmación / vuelve
// del sub-menú, NUNCA cierra ni saltea (brief §25).
class OnboardingView {
 public:
    // src/app empuja el offer (candidatos + si el secreto ya está
    // revelado) al ENTRAR al modo onboarding. Resetea la etapa a
    // kBrowse.
    void SetOffer(catalog::OnboardingOffer offer);

    // src/app la llama UNA vez, en el deadline monotónico de los 44 s
    // (brief §10/§12): revela el candidato secreto discretamente. No
    // reordena la fila ni arrebata el foco (Frin entra al final).
    void RevealSecret();

    void SetLanguage(core::Language language);

    const catalog::OnboardingOffer& Offer() const { return offer_; }
    bool SecretRevealed() const { return offer_.secretRevealed; }
    OnboardingStage Stage() const { return stage_; }

    // Coordenadas en PUNTOS lógicos de la ventana.
    OnboardingViewResult OnMouseMove(float x, float y);
    OnboardingViewResult OnMouseDown(float x, float y);
    OnboardingViewResult OnWheel(float dyLines);
    OnboardingViewResult OnKey(int sdlKeycode, bool shiftHeld);
    OnboardingViewResult OnViewportChanged();

    bool Dirty() const { return dirty_; }
    void ClearDirty() { dirty_ = false; }

    // Al ENTRAR al modo onboarding: foco en el primer candidato, sin
    // hover, sin chrome de foco de teclado.
    void OnEnter();

    // Solo-DEV (QA / capturas): fuerza la etapa / la confirmación.
    void RevealSecretForQA() { RevealSecret(); }
    void SetStageForQA(OnboardingStage stage, const std::string& focusId);

    void Render(UiPainter& painter, TextCache& text, PetPreviewCache& previews, float viewportW,
                float viewportH);

 private:
    OnboardingLayout BuildLayout(float w, float h) const;
    void SyncFocusList(const OnboardingLayout& layout);
    OnboardingViewResult ActivateWidget(const std::string& focusId);
    std::string DisplayNameForNormal(const std::string& petId) const;
    std::string FrinVariantDisplayName(const std::string& variantId) const;

    catalog::OnboardingOffer offer_;
    core::Language language_ = core::Language::kEn;

    OnboardingStage stage_ = OnboardingStage::kBrowse;
    catalog::PetIdentity pendingSelection_;    // válido en kConfirm
    std::string pendingDisplayName_;

    FocusList focus_;
    float scrollY_ = 0.0f;
    float lastContentHeight_ = 0.0f;
    float viewportW_ = 800.0f;
    float viewportH_ = 560.0f;

    std::string hoverId_;
    bool keyboardFocus_ = false;
    bool dirty_ = true;
};

}  // namespace nimvlets::productui
