#include "productui/CollectionView.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <utility>

#include "productui/Format.h"
#include "productui/SectionHeaderView.h"
#include "productui/UiTheme.h"

namespace nimvlets::productui {

using catalog::OwnershipStatus;
using platform::TextWeight;

namespace {

constexpr float kHeaderClipTop = 106.0f;  // el hero stage arranca en ~96; arriba de acá no se recorta
constexpr float kWheelStep = 48.0f;

// Alpha de las primitivas del hero stage: el stage "apoya" el arte,
// nunca compite (brief §11). La primaria un poco más presente que en
// Block 06.1 (52) porque el owner la reportó demasiado sutil (§5).
constexpr unsigned char kStagePrimaryAlpha = 92;
constexpr unsigned char kStageSecondaryAlpha = 52;

// Alpha del arte de un pet locked: visible pero más callado, NO
// grayscale/opacidad agresiva (brief §12/§13).
constexpr unsigned char kLockedArtAlpha = 150;

// Pedestal del arte en la gallery: un tinte de identidad MUY tenue en
// vez del mismo cuadrado neutro para todos (brief §20).
constexpr unsigned char kPedestalAlpha = 52;
constexpr unsigned char kPedestalAlphaLocked = 30;

bool StartsWith(const std::string& s, const char* prefix) {
    return s.rfind(prefix, 0) == 0;
}

std::string PetIdFromItemFocusId(const std::string& focusId) {
    return StartsWith(focusId, "item:") ? focusId.substr(5) : std::string();
}

std::string VariantFromFocusId(const std::string& focusId) {
    return StartsWith(focusId, "variant:") ? focusId.substr(8) : std::string();
}

// Color del texto de estado. Locked usa kTextMuted (no kTextFaint): el
// pet locked se siente NO DISPONIBLE, no muerto (brief §13).
UiColor StatusColor(OwnershipStatus status, UiColor accentLine) {
    switch (status) {
        case OwnershipStatus::kActive:
            return theme::kTextMuted;
        case OwnershipStatus::kOwnedInactive:
            return accentLine;  // el hint "Use" de la gallery lleva el tono del pet
        case OwnershipStatus::kLocked:
            return theme::kTextMuted;
    }
    return theme::kTextMuted;
}

// Dibuja una primitiva del hero stage: óvalo, o round-rect si el acento
// del pet pide una forma más angular (Nidir).
void FillStagePrimitive(UiPainter& painter, const UiRect& r, bool angular, UiColor color) {
    if (angular) {
        painter.FillRoundRect(r, std::min(r.w, r.h) * 0.32f, color);
    } else {
        painter.FillEllipse(r, color);
    }
}

}  // namespace

void CollectionView::SetModel(catalog::CollectionModel model, std::uint64_t clickBalance) {
    model_ = std::move(model);
    clickBalance_ = clickBalance;

    if (!selectedPetId_.empty() && model_.Find(selectedPetId_) == nullptr) {
        selectedPetId_.clear();
        selectedVariantId_.clear();
    }
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    dirty_ = true;
}

void CollectionView::SetLanguage(core::Language language) {
    if (language == language_) {
        return;
    }
    language_ = language;
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    dirty_ = true;
}

CollectionLayout CollectionView::BuildLayout(float w, float h) const {
    CollectionLayoutInput in;
    in.viewportW = w;
    in.viewportH = h;
    in.scrollY = ClampScroll(scrollY_, lastContentHeight_, h);
    in.language = language_;
    in.selectedPetId = selectedPetId_;
    in.selectedVariantId = selectedVariantId_;
    in.hoverPetId = HoverPetId();
    return BuildCollectionLayout(model_, in);
}

void CollectionView::SyncFocusList(const CollectionLayout& layout) {
    focus_.SetItems(layout.focusOrder);
}

std::string CollectionView::HoverPetId() const {
    return StartsWith(hoverId_, "item:") ? hoverId_.substr(5) : std::string();
}

void CollectionView::SelectHero(const std::string& petId) {
    if (model_.Find(petId) == nullptr || petId == selectedPetId_) {
        return;
    }
    selectedPetId_ = petId;
    selectedVariantId_.clear();  // que el layout resuelva la variante por defecto del nuevo hero
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    dirty_ = true;
}

CollectionViewResult CollectionView::ActivateWidget(const std::string& focusId) {
    CollectionViewResult r;
    if (focusId.empty()) {
        return r;
    }
    if (ProductSection target; NavTargetSection(focusId, target)) {
        // Las TRES pestañas (Collection · Shop · Settings) se rutean por
        // la misma tabla — ver productui::NavTargetSection.
        r.switchSection = true;
        r.targetSection = target;
        r.dirty = true;
        return r;
    }
    if (StartsWith(focusId, "item:")) {
        SelectHero(PetIdFromItemFocusId(focusId));
        r.dirty = true;
        return r;
    }
    if (StartsWith(focusId, "variant:")) {
        selectedVariantId_ = VariantFromFocusId(focusId);
        SyncFocusList(BuildLayout(viewportW_, viewportH_));
        focus_.Focus(focusId);
        r.dirty = true;
        return r;
    }
    if (focusId == "use") {
        const CollectionLayout layout = BuildLayout(viewportW_, viewportH_);
        if (layout.hero.actionEnabled) {
            r.hasActivate = true;
            r.activate.petId = layout.hero.petId;
            r.activate.variantId = layout.hero.selectedVariantId;
            selectedPetId_ = layout.hero.petId;
            selectedVariantId_ = layout.hero.selectedVariantId;
        }
        r.dirty = true;
        return r;
    }
    return r;
}

CollectionViewResult CollectionView::OnMouseMove(float x, float y) {
    CollectionViewResult r;
    const CollectionLayout layout = BuildLayout(viewportW_, viewportH_);
    const std::string hit = layout.HitTest(x, y);
    if (hit != hoverId_) {
        hoverId_ = hit;
        dirty_ = true;
        r.dirty = true;
    }
    return r;
}

CollectionViewResult CollectionView::OnMouseDown(float x, float y) {
    // Un click de mouse sale del "modo teclado": el chrome de foco no se
    // dibuja hasta la próxima navegación por teclado (brief §19).
    keyboardFocus_ = false;
    const CollectionLayout layout = BuildLayout(viewportW_, viewportH_);
    const std::string hit = layout.HitTest(x, y);
    if (hit.empty()) {
        dirty_ = true;  // por si había chrome de foco visible que ahora se apaga
        return CollectionViewResult{};
    }
    if (StartsWith(hit, "item:") || StartsWith(hit, "nav:")) {
        focus_.Focus(hit);
    }
    CollectionViewResult r = ActivateWidget(hit);
    r.dirty = true;
    dirty_ = true;
    return r;
}

CollectionViewResult CollectionView::OnWheel(float dyLines) {
    CollectionViewResult r;
    const float before = scrollY_;
    scrollY_ = ClampScroll(scrollY_ - dyLines * kWheelStep, lastContentHeight_, viewportH_);
    if (scrollY_ != before) {
        dirty_ = true;
        r.dirty = true;
    }
    return r;
}

CollectionViewResult CollectionView::OnKey(int sdlKeycode, bool shiftHeld) {
    CollectionViewResult r;
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
            r.requestClose = true;
            return r;
        default:
            return r;
    }
}

CollectionViewResult CollectionView::OnViewportChanged() {
    CollectionViewResult r;
    dirty_ = true;
    r.dirty = true;
    return r;
}

void CollectionView::Render(
    UiPainter& painter, TextCache& text, PetPreviewCache& previews, float viewportW, float viewportH) {
    viewportW_ = viewportW;
    viewportH_ = viewportH;

    const CollectionLayout layout = BuildLayout(viewportW, viewportH);
    lastContentHeight_ = layout.contentHeight;
    SyncFocusList(layout);
    const std::string focusedId = keyboardFocus_ ? focus_.FocusedId() : std::string();

    painter.Clear(theme::kBackground);

    // --- Cabecera compartida (título + balance + pestañas), sin recorte
    //     de scroll ---
    DrawSectionHeader(painter, text, layout.header, clickBalance_, language_, hoverId_, focusedId);

    painter.PushClip(UiRect{0.0f, kHeaderClipTop, viewportW, std::max(0.0f, viewportH - kHeaderClipTop)});

    // --- Segundo plano: la zona de la gallery (brief §12) ---
    painter.FillRect(layout.galleryShelf, theme::kGalleryShelf);

    // --- Hero ---
    const CollectionHero& h = layout.hero;
    if (!h.petId.empty()) {
        // Hero stage: primaria grande + secundaria descentrada, teñidas
        // con el shapeTint del pet a alpha bajo (brief §11).
        FillStagePrimitive(painter, h.stageSecondary, h.accent.angularShape,
                           h.accent.shapeTint.WithAlpha(kStageSecondaryAlpha));
        FillStagePrimitive(painter, h.stagePrimary, h.accent.angularShape,
                           h.accent.shapeTint.WithAlpha(kStagePrimaryAlpha));

        SDL_Texture* art = previews.Get(h.petId, h.selectedVariantId);
        const unsigned char artAlpha = h.status == OwnershipStatus::kLocked ? kLockedArtAlpha : 255;
        painter.DrawTextureContained(art, h.art, artAlpha);

        DrawText(painter, text, h.displayName, type::kHeroName, TextWeight::kSemibold, theme::kText,
                 h.nameAnchor.x, h.nameAnchor.y + 24.0f, HAlign::kLeft);
        // Regla de acento fina bajo el nombre — el tono del pet, sin caja.
        painter.FillRect(h.nameRule, h.accent.line);

        if (!h.speciesText.empty()) {
            DrawText(painter, text, h.speciesText, type::kHeroSpecies, TextWeight::kRegular,
                     theme::kTextMuted, h.speciesAnchor.x, h.speciesAnchor.y + 12.0f, HAlign::kLeft);
        }
        if (!h.descriptionText.empty()) {
            // Block 07: la descripción es un par de frases -> word-wrap
            // dentro de la columna del hero, hasta 3 líneas (brief §19).
            DrawTextWrapped(painter, text, h.descriptionText, type::kHeroBody, TextWeight::kRegular,
                            theme::kText, h.descriptionAnchor.x, h.descriptionAnchor.y + 13.0f,
                            h.descriptionAnchor.w, 17.0f, 3);
        }

        // Selector de variante tipográfico: "Male · Female" con
        // subrayado de acento bajo la seleccionada; el chrome de foco de
        // teclado es un pill sutil, NO un recuadro de control (brief §19).
        for (std::size_t i = 0; i < h.variants.size(); ++i) {
            const HeroVariantChip& chip = h.variants[i];
            if (focusedId == chip.focusId) {
                painter.FillRoundRect(chip.rect.Inset(-4.0f), 9.0f, h.accent.shapeTint.WithAlpha(150));
            }
            if (i > 0) {
                const float dotX = 0.5f * (h.variants[i - 1].rect.Right() + chip.rect.x);
                DrawText(painter, text, "·", type::kHeroVariant, TextWeight::kRegular, theme::kTextFaint,
                         dotX, chip.rect.CenterY() + 5.0f, HAlign::kCenter);
            }
            // Una variante NO poseída se muestra atenuada (kTextFaint):
            // visible en el selector para que la elección sea clara,
            // pero se lee como no disponible — sin ninguna ruta de
            // compra (brief §6).
            const UiColor chipColor = !chip.owned ? theme::kTextFaint
                                      : chip.selected ? theme::kText
                                                      : theme::kTextMuted;
            DrawText(painter, text, chip.label, type::kHeroVariant,
                     chip.selected && chip.owned ? TextWeight::kSemibold : TextWeight::kRegular,
                     chipColor, chip.rect.CenterX(), chip.rect.CenterY() + 5.0f, HAlign::kCenter);
            if (chip.selected) {
                painter.FillRect(chip.underline, chip.owned ? h.accent.line : theme::kHairline);
            }
        }

        if (h.actionEnabled) {
            // Botón primario con la identidad del pet: relleno tenue +
            // borde y texto oscuros legibles — NUNCA casi-negro (§17).
            painter.FillRoundRect(h.actionButton, 9.0f, h.accent.softFill);
            painter.StrokeRoundRect(h.actionButton, 9.0f, 1.5f, h.accent.line);
            if (focusedId == h.actionFocusId) {
                painter.StrokeRoundRect(h.actionButton.Inset(-3.0f), 12.0f, 2.0f, h.accent.line);
            }
            DrawText(painter, text, h.actionLabel, type::kButton, TextWeight::kSemibold, h.accent.deepInk,
                     h.actionButton.CenterX(), h.actionButton.CenterY() + 4.5f, HAlign::kCenter);
        } else if (h.showStatusLine) {
            // ACTIVO: "● On desktop". LOCKED: "Not in your collection",
            // sin punto. Sin botón, sin duplicar (brief §18).
            float statusX = h.statusAnchor.x;
            if (h.status == OwnershipStatus::kActive) {
                painter.FillEllipse(UiRect{statusX, h.statusAnchor.y + 3.0f, 7.0f, 7.0f}, h.accent.line);
                statusX += 14.0f;
            }
            DrawText(painter, text, h.statusText, type::kHeroStatus, TextWeight::kMedium,
                     StatusColor(h.status, h.accent.line), statusX, h.statusAnchor.y + 12.0f, HAlign::kLeft);
        }
    }

    // --- Divisor (sobre el borde del segundo plano) ---
    painter.FillRect(layout.dividerRect, theme::kHairline);

    // --- Gallery ---
    for (const GalleryItem& g : layout.gallery) {
        const bool hovered = hoverId_ == g.focusId;
        const bool focused = focusedId == g.focusId;
        if (hovered) {
            painter.FillRoundRect(g.cell.Inset(2.0f), 12.0f, theme::kHoverWash);
        }
        if (focused) {
            painter.StrokeRoundRect(g.cell.Inset(2.0f), 12.0f, 2.0f, g.accentLine);
        }

        const unsigned char pedAlpha =
            g.status == OwnershipStatus::kLocked ? kPedestalAlphaLocked : kPedestalAlpha;
        painter.FillRoundRect(g.art.Inset(-5.0f), 12.0f, g.pedestalTint.WithAlpha(pedAlpha));

        SDL_Texture* art = previews.Get(g.petId, g.previewVariantId);
        painter.DrawTextureContained(
            art, g.art, g.status == OwnershipStatus::kLocked ? kLockedArtAlpha : 255);

        DrawText(painter, text, g.displayName, type::kGalleryName, TextWeight::kMedium, theme::kText,
                 g.name.CenterX(), g.name.y + 13.0f, HAlign::kCenter, static_cast<int>(g.cell.w - 6.0f));
        DrawText(painter, text, g.statusText, type::kGalleryStatus, TextWeight::kRegular,
                 StatusColor(g.status, g.accentLine), g.status_.CenterX(), g.status_.y + 11.0f,
                 HAlign::kCenter, static_cast<int>(g.cell.w - 6.0f));
    }

    painter.PopClip();
}

}  // namespace nimvlets::productui
