#include "productui/OnboardingLayout.h"

#include <algorithm>
#include <string>

#include "core/Localization.h"
#include "productui/Format.h"
#include "productui/PetEditorial.h"

namespace nimvlets::productui {

using catalog::OnboardingOffer;
using catalog::OnboardingStarter;
using catalog::PetIdentity;
using core::Language;
using core::Localized;
using core::StringKey;

namespace {

// Métricas en PUNTOS lógicos. Pantalla de elección: encabezado arriba,
// una fila de tarjetas centrada, el arte manda (brief §20). Sin
// cabecera de navegación (brief §19).
constexpr float kMargin = 40.0f;
constexpr float kHeadingTop = 56.0f;
constexpr float kHeadingH = 30.0f;
constexpr float kHeadingToRow = 44.0f;

constexpr float kCardMaxW = 208.0f;
constexpr float kCardMinW = 132.0f;
constexpr float kCardGap = 24.0f;
constexpr float kCardMaxArt = 150.0f;
constexpr float kCardArtToName = 14.0f;
constexpr float kCardNameH = 20.0f;
constexpr float kCardNameToSpecies = 4.0f;
constexpr float kCardSpeciesH = 16.0f;
constexpr float kCardPadY = 16.0f;   // aire clickeable arriba/abajo del contenido

constexpr float kConfirmPromptTop = 150.0f;
constexpr float kConfirmPromptH = 60.0f;   // hasta 2 líneas, centrado
constexpr float kConfirmGap = 28.0f;
constexpr float kConfirmButtonH = 40.0f;
constexpr float kConfirmButtonPadX = 24.0f;
constexpr float kConfirmButtonGap = 16.0f;

constexpr float kApproxCharW = 8.0f;

std::string FrinVariantLabel(const std::string& variantId, Language lang) {
    if (variantId == "male") {
        return Localized(StringKey::kMale, lang);
    }
    if (variantId == "female") {
        return Localized(StringKey::kFemale, lang);
    }
    return variantId;
}

// Coloca una fila de `count` tarjetas centrada horizontalmente, con la
// caja de cada tarjeta arrancando en `rowTop`. Devuelve el bottom de la
// fila. El ancho de tarjeta se ajusta al espacio disponible para
// `count`: 3 candidatos -> ~208 pt; cuando el secreto revela un 4º, las
// tarjetas se achican un poco para que la fila SIGA cabiendo sin scroll
// (un reajuste contenido, no una reorganización violenta — brief §21).
float LayoutCardRow(std::vector<OnboardingCandidate>& cards, float viewportW, float rowTop) {
    const int count = static_cast<int>(cards.size());
    if (count == 0) {
        return rowTop;
    }
    const float avail = viewportW - 2.0f * kMargin;
    const float fitW = (avail - static_cast<float>(count - 1) * kCardGap) / static_cast<float>(count);
    const float cardW = std::clamp(fitW, kCardMinW, kCardMaxW);
    const float cardArt = std::min(kCardMaxArt, cardW - 24.0f);

    const float totalW = static_cast<float>(count) * cardW + static_cast<float>(count - 1) * kCardGap;
    const float left = (viewportW - totalW) * 0.5f;
    const float cardH = kCardPadY + cardArt + kCardArtToName + kCardNameH + kCardNameToSpecies +
                        kCardSpeciesH + kCardPadY;

    for (int i = 0; i < count; ++i) {
        OnboardingCandidate& c = cards[static_cast<std::size_t>(i)];
        const float x = left + static_cast<float>(i) * (cardW + kCardGap);
        c.cell = UiRect{x, rowTop, cardW, cardH};
        c.art = UiRect{x + (cardW - cardArt) * 0.5f, rowTop + kCardPadY, cardArt, cardArt};
        c.name = UiRect{x, c.art.Bottom() + kCardArtToName, cardW, kCardNameH};
        c.species_ = UiRect{x, c.name.Bottom() + kCardNameToSpecies, cardW, kCardSpeciesH};
    }
    return rowTop + cardH;
}

}  // namespace

const OnboardingCandidate* OnboardingLayout::FindCandidate(const std::string& focusId) const {
    for (const OnboardingCandidate& c : candidates) {
        if (c.focusId == focusId) {
            return &c;
        }
    }
    return nullptr;
}

std::string OnboardingLayout::HitTest(float x, float y) const {
    for (const OnboardingCandidate& c : candidates) {
        if (c.cell.Contains(x, y)) {
            return c.focusId;
        }
    }
    if (confirm.visible) {
        if (confirm.cancelButton.Contains(x, y)) {
            return confirm.cancelFocusId;
        }
        if (confirm.chooseButton.Contains(x, y)) {
            return confirm.chooseFocusId;
        }
    }
    return "";
}

float ClampOnboardingScroll(float scrollY, float contentHeight, float viewportH) {
    return std::clamp(scrollY, 0.0f, std::max(0.0f, contentHeight - viewportH));
}

OnboardingLayout BuildOnboardingLayout(const OnboardingLayoutInput& in) {
    const Language lang = in.language;
    const float sy = in.scrollY;

    OnboardingLayout out;
    out.viewport = UiRect{0.0f, 0.0f, in.viewportW, in.viewportH};
    out.stage = in.stage;

    if (in.stage == OnboardingStage::kConfirm) {
        out.heading.clear();
        OnboardingConfirm& cf = out.confirm;
        cf.visible = true;
        cf.prompt = FormatOnboardingConfirmPrompt(in.pendingDisplayName, lang);
        cf.promptAnchor = UiRect{kMargin, kConfirmPromptTop - sy, in.viewportW - 2.0f * kMargin,
                                 kConfirmPromptH};

        cf.cancelLabel = Localized(StringKey::kCancel, lang);
        cf.cancelFocusId = "onb:cancel";
        cf.chooseLabel = std::string(Localized(StringKey::kOnboardingConfirmChoosePrefix, lang)) +
                         in.pendingDisplayName;
        cf.chooseFocusId = "onb:choose";

        const float cancelW =
            kConfirmButtonPadX * 2.0f + static_cast<float>(cf.cancelLabel.size()) * kApproxCharW;
        const float chooseW =
            kConfirmButtonPadX * 2.0f + static_cast<float>(cf.chooseLabel.size()) * kApproxCharW;
        const float rowW = cancelW + kConfirmButtonGap + chooseW;
        const float rowLeft = (in.viewportW - rowW) * 0.5f;
        const float btnY = cf.promptAnchor.Bottom() + kConfirmGap;
        cf.cancelButton = UiRect{rowLeft, btnY, cancelW, kConfirmButtonH};
        cf.chooseButton = UiRect{rowLeft + cancelW + kConfirmButtonGap, btnY, chooseW, kConfirmButtonH};

        // El foco ARRANCA en Cancel (brief §23): un click perdido nunca
        // termina el onboarding.
        out.focusOrder.push_back(cf.cancelFocusId);
        out.focusOrder.push_back(cf.chooseFocusId);
        out.contentHeight = cf.chooseButton.Bottom() + kMargin + sy;
        return out;
    }

    // --- kBrowse / kFrinVariant: encabezado + fila de tarjetas ---
    out.heading = in.stage == OnboardingStage::kFrinVariant
                      ? Localized(StringKey::kOnboardingWhichVariant, lang)
                      : Localized(StringKey::kOnboardingChooseFirst, lang);
    out.headingAnchor = UiRect{kMargin, kHeadingTop - sy, in.viewportW - 2.0f * kMargin, kHeadingH};

    std::vector<OnboardingCandidate> cards;
    if (in.stage == OnboardingStage::kFrinVariant && in.offer.secret.has_value()) {
        for (const PetIdentity& v : in.offer.secret->variants) {
            OnboardingCandidate c;
            c.identity = v;
            c.previewVariantId = v.variantId;
            c.displayName = in.offer.secret->displayName;  // "Frin" — el arte / la variante lo distinguen
            c.speciesText = FrinVariantLabel(v.variantId, lang);
            c.focusId = "var:" + v.variantId;
            cards.push_back(std::move(c));
        }
    } else {  // kBrowse
        for (const OnboardingStarter& s : in.offer.normal) {
            OnboardingCandidate c;
            c.identity = s.identity;
            c.previewVariantId = s.identity.variantId;  // "" para un pet sin variantes
            c.displayName = s.displayName;
            c.speciesText = Species(s.identity.petId, lang);
            c.focusId = "cand:" + s.identity.petId;
            cards.push_back(std::move(c));
        }
        // Frin, SOLO si el secreto fue revelado — y SIEMPRE al final, así
        // el reveal no reordena ni arrebata el foco (brief §21).
        if (in.offer.secret.has_value() && in.offer.secretRevealed) {
            OnboardingCandidate c;
            c.identity = in.offer.secret->identity;  // {frin, ""}
            // No hay preview para {frin, ""}: se usa la primera variante.
            c.previewVariantId = in.offer.secret->variants.empty()
                                     ? std::string()
                                     : in.offer.secret->variants.front().variantId;
            c.displayName = in.offer.secret->displayName;
            c.speciesText = Species(in.offer.secret->identity.petId, lang);
            c.focusId = "cand:" + in.offer.secret->identity.petId;
            cards.push_back(std::move(c));
        }
    }

    const float rowBottom =
        LayoutCardRow(cards, in.viewportW, (kHeadingTop + kHeadingH + kHeadingToRow) - sy);
    out.candidates = std::move(cards);
    for (const OnboardingCandidate& c : out.candidates) {
        out.focusOrder.push_back(c.focusId);
    }

    out.contentHeight = rowBottom + kMargin + sy;
    return out;
}

}  // namespace nimvlets::productui
