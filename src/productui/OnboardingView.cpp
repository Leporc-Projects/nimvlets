#include "productui/OnboardingView.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <utility>

#include "core/Localization.h"
#include "productui/UiTheme.h"

namespace nimvlets::productui {

using catalog::OnboardingOffer;
using catalog::OnboardingStarter;
using catalog::PetIdentity;
using core::Language;
using core::Localized;
using core::StringKey;
using platform::TextWeight;

namespace {

constexpr float kWheelStep = 48.0f;
constexpr unsigned char kCardArtAlpha = 255;

bool StartsWith(const std::string& s, const char* prefix) { return s.rfind(prefix, 0) == 0; }

}  // namespace

void OnboardingView::SetOffer(OnboardingOffer offer) {
    offer_ = std::move(offer);
    stage_ = OnboardingStage::kBrowse;
    pendingSelection_ = PetIdentity{};
    pendingDisplayName_.clear();
    hoverId_.clear();
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    dirty_ = true;
}

void OnboardingView::RevealSecret() {
    if (offer_.secretRevealed) {
        return;
    }
    offer_.secretRevealed = true;
    // El reveal NO reordena la fila (Frin ya se agrega al final en el
    // layout) ni cambia el foco: SyncFocusList conserva el id enfocado.
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    dirty_ = true;
}

void OnboardingView::SetLanguage(Language language) {
    if (language == language_) {
        return;
    }
    language_ = language;
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    dirty_ = true;
}

void OnboardingView::OnEnter() {
    hoverId_.clear();
    keyboardFocus_ = false;
    stage_ = OnboardingStage::kBrowse;
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    if (!focus_.Empty()) {
        focus_.Focus(focus_.Items().front());
    }
    dirty_ = true;
}

void OnboardingView::SetStageForQA(OnboardingStage stage, const std::string& focusId) {
    stage_ = stage;
    if (stage == OnboardingStage::kConfirm && !focusId.empty()) {
        // focusId aquí es un "cand:<petId>" / "var:<variant>" a confirmar.
        if (StartsWith(focusId, "var:")) {
            pendingSelection_ = PetIdentity{offer_.secret ? offer_.secret->identity.petId : std::string(),
                                           focusId.substr(4)};
            pendingDisplayName_ = FrinVariantDisplayName(focusId.substr(4));
        } else if (StartsWith(focusId, "cand:")) {
            const std::string petId = focusId.substr(5);
            pendingSelection_ = PetIdentity{petId, std::string()};
            pendingDisplayName_ = DisplayNameForNormal(petId);
        }
    }
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    dirty_ = true;
}

std::string OnboardingView::DisplayNameForNormal(const std::string& petId) const {
    for (const OnboardingStarter& s : offer_.normal) {
        if (s.identity.petId == petId) {
            return s.displayName;
        }
    }
    return petId;
}

std::string OnboardingView::FrinVariantDisplayName(const std::string& variantId) const {
    const std::string base = offer_.secret ? offer_.secret->displayName : std::string("Frin");
    const StringKey key = variantId == "female" ? StringKey::kFemale : StringKey::kMale;
    // "Frin (Male)" / "Frin (Macho)" — la identidad confirmada incluye
    // la variante elegida (brief §23).
    return base + " (" + Localized(key, language_) + ")";
}

OnboardingLayout OnboardingView::BuildLayout(float w, float h) const {
    OnboardingLayoutInput in;
    in.viewportW = w;
    in.viewportH = h;
    in.scrollY = ClampOnboardingScroll(scrollY_, lastContentHeight_, h);
    in.language = language_;
    in.offer = offer_;
    in.stage = stage_;
    in.pendingSelection = pendingSelection_;
    in.pendingDisplayName = pendingDisplayName_;
    return BuildOnboardingLayout(in);
}

void OnboardingView::SyncFocusList(const OnboardingLayout& layout) {
    focus_.SetItems(layout.focusOrder);
}

OnboardingViewResult OnboardingView::ActivateWidget(const std::string& focusId) {
    OnboardingViewResult r;
    if (focusId.empty()) {
        return r;
    }

    if (focusId == "onb:choose") {
        r.hasSelection = true;
        r.selection = pendingSelection_;
        r.dirty = true;
        return r;
    }
    if (focusId == "onb:cancel") {
        // Volver: de una confirmación de variante -> al sub-menú de Frin;
        // de una confirmación normal -> a la lista.
        const bool wasFrinVariant =
            offer_.secret.has_value() && pendingSelection_.petId == offer_.secret->identity.petId &&
            !pendingSelection_.variantId.empty();
        stage_ = wasFrinVariant ? OnboardingStage::kFrinVariant : OnboardingStage::kBrowse;
        pendingSelection_ = PetIdentity{};
        pendingDisplayName_.clear();
        SyncFocusList(BuildLayout(viewportW_, viewportH_));
        if (!focus_.Empty()) {
            focus_.Focus(focus_.Items().front());
        }
        r.dirty = true;
        return r;
    }
    if (StartsWith(focusId, "var:")) {
        const std::string variantId = focusId.substr(4);
        pendingSelection_ =
            PetIdentity{offer_.secret ? offer_.secret->identity.petId : std::string(), variantId};
        pendingDisplayName_ = FrinVariantDisplayName(variantId);
        stage_ = OnboardingStage::kConfirm;
        SyncFocusList(BuildLayout(viewportW_, viewportH_));
        focus_.Focus("onb:cancel");  // el foco arranca en Cancel (brief §23)
        r.dirty = true;
        return r;
    }
    if (StartsWith(focusId, "cand:")) {
        const std::string petId = focusId.substr(5);
        // ¿Es el candidato SECRETO (Frin lógico)?
        if (offer_.secret.has_value() && petId == offer_.secret->identity.petId) {
            if (!offer_.secretRevealed) {
                return r;  // no debería pasar: la tarjeta no existe si no está revelado
            }
            stage_ = OnboardingStage::kFrinVariant;
            SyncFocusList(BuildLayout(viewportW_, viewportH_));
            if (!focus_.Empty()) {
                focus_.Focus(focus_.Items().front());
            }
            r.dirty = true;
            return r;
        }
        // Candidato normal -> confirmación inline.
        pendingSelection_ = PetIdentity{petId, std::string()};
        pendingDisplayName_ = DisplayNameForNormal(petId);
        stage_ = OnboardingStage::kConfirm;
        SyncFocusList(BuildLayout(viewportW_, viewportH_));
        focus_.Focus("onb:cancel");  // el foco arranca en Cancel (brief §23)
        r.dirty = true;
        return r;
    }
    return r;
}

OnboardingViewResult OnboardingView::OnMouseMove(float x, float y) {
    OnboardingViewResult r;
    const OnboardingLayout layout = BuildLayout(viewportW_, viewportH_);
    const std::string hit = layout.HitTest(x, y);
    if (hit != hoverId_) {
        hoverId_ = hit;
        dirty_ = true;
        r.dirty = true;
    }
    return r;
}

OnboardingViewResult OnboardingView::OnMouseDown(float x, float y) {
    keyboardFocus_ = false;
    const OnboardingLayout layout = BuildLayout(viewportW_, viewportH_);
    const std::string hit = layout.HitTest(x, y);
    if (hit.empty()) {
        dirty_ = true;
        return OnboardingViewResult{};
    }
    focus_.Focus(hit);
    OnboardingViewResult r = ActivateWidget(hit);
    r.dirty = true;
    dirty_ = true;
    return r;
}

OnboardingViewResult OnboardingView::OnWheel(float dyLines) {
    OnboardingViewResult r;
    const float before = scrollY_;
    scrollY_ = ClampOnboardingScroll(scrollY_ - dyLines * kWheelStep, lastContentHeight_, viewportH_);
    if (scrollY_ != before) {
        dirty_ = true;
        r.dirty = true;
    }
    return r;
}

OnboardingViewResult OnboardingView::OnKey(int sdlKeycode, bool shiftHeld) {
    OnboardingViewResult r;
    switch (sdlKeycode) {
        case SDLK_TAB:
        case SDLK_RIGHT:
        case SDLK_DOWN:
            if (sdlKeycode == SDLK_TAB && shiftHeld) {
                focus_.Prev();
            } else {
                focus_.Next();
            }
            keyboardFocus_ = true;
            dirty_ = true;
            r.dirty = true;
            return r;
        case SDLK_LEFT:
        case SDLK_UP:
            focus_.Prev();
            keyboardFocus_ = true;
            dirty_ = true;
            r.dirty = true;
            return r;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_SPACE:
            keyboardFocus_ = true;
            return ActivateWidget(focus_.FocusedId());
        case SDLK_ESCAPE:
            // Esc SÓLO tiene un significado seguro: cancelar una
            // confirmación / volver del sub-menú de Frin. NUNCA saltea el
            // onboarding (brief §25).
            if (stage_ == OnboardingStage::kConfirm) {
                keyboardFocus_ = true;
                return ActivateWidget("onb:cancel");
            }
            if (stage_ == OnboardingStage::kFrinVariant) {
                stage_ = OnboardingStage::kBrowse;
                SyncFocusList(BuildLayout(viewportW_, viewportH_));
                if (!focus_.Empty()) {
                    focus_.Focus(focus_.Items().front());
                }
                keyboardFocus_ = true;
                dirty_ = true;
                r.dirty = true;
            }
            return r;
        default:
            return r;
    }
}

OnboardingViewResult OnboardingView::OnViewportChanged() {
    OnboardingViewResult r;
    dirty_ = true;
    r.dirty = true;
    return r;
}

void OnboardingView::Render(
    UiPainter& painter, TextCache& text, PetPreviewCache& previews, float viewportW, float viewportH) {
    viewportW_ = viewportW;
    viewportH_ = viewportH;

    const OnboardingLayout layout = BuildLayout(viewportW, viewportH);
    lastContentHeight_ = layout.contentHeight;
    SyncFocusList(layout);
    const std::string focusedId = keyboardFocus_ ? focus_.FocusedId() : std::string();

    painter.Clear(theme::kBackground);

    if (!layout.heading.empty()) {
        DrawText(painter, text, layout.heading, type::kHeroName, TextWeight::kSemibold, theme::kText,
                 layout.headingAnchor.CenterX(), layout.headingAnchor.y + 26.0f, HAlign::kCenter);
    }

    for (const OnboardingCandidate& c : layout.candidates) {
        const bool hovered = hoverId_ == c.focusId;
        const bool focused = focusedId == c.focusId;
        if (hovered) {
            painter.FillRoundRect(c.cell.Inset(2.0f), 14.0f, theme::kHoverWash);
        }
        if (focused) {
            painter.StrokeRoundRect(c.cell.Inset(2.0f), 14.0f, 2.0f, theme::kText);
        }
        painter.FillRoundRect(c.art.Inset(-6.0f), 14.0f, theme::kGalleryShelf);

        SDL_Texture* art = previews.Get(c.identity.petId, c.previewVariantId);
        painter.DrawTextureContained(art, c.art, kCardArtAlpha);

        DrawText(painter, text, c.displayName, type::kGalleryName, TextWeight::kMedium, theme::kText,
                 c.name.CenterX(), c.name.y + 15.0f, HAlign::kCenter, static_cast<int>(c.cell.w - 8.0f));
        if (!c.speciesText.empty()) {
            DrawText(painter, text, c.speciesText, type::kGalleryStatus, TextWeight::kRegular,
                     theme::kTextMuted, c.species_.CenterX(), c.species_.y + 12.0f, HAlign::kCenter,
                     static_cast<int>(c.cell.w - 8.0f));
        }
    }

    if (layout.confirm.visible) {
        const OnboardingConfirm& cf = layout.confirm;
        DrawText(painter, text, cf.prompt, type::kSectionTitle, TextWeight::kRegular, theme::kText,
                 cf.promptAnchor.CenterX(), cf.promptAnchor.y + 22.0f, HAlign::kCenter,
                 static_cast<int>(cf.promptAnchor.w));

        painter.StrokeRoundRect(cf.cancelButton, 9.0f, 1.5f, theme::kHairline);
        if (focusedId == cf.cancelFocusId) {
            painter.StrokeRoundRect(cf.cancelButton.Inset(-3.0f), 12.0f, 2.0f, theme::kText);
        }
        DrawText(painter, text, cf.cancelLabel, type::kButton, TextWeight::kMedium, theme::kTextMuted,
                 cf.cancelButton.CenterX(), cf.cancelButton.CenterY() + 4.5f, HAlign::kCenter);

        painter.FillRoundRect(cf.chooseButton, 9.0f, theme::kText);
        if (focusedId == cf.chooseFocusId) {
            painter.StrokeRoundRect(cf.chooseButton.Inset(-3.0f), 12.0f, 2.0f, theme::kText);
        }
        DrawText(painter, text, cf.chooseLabel, type::kButton, TextWeight::kSemibold, theme::kBackground,
                 cf.chooseButton.CenterX(), cf.chooseButton.CenterY() + 4.5f, HAlign::kCenter);
    }
}

}  // namespace nimvlets::productui
