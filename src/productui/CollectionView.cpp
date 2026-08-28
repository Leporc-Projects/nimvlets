#include "productui/CollectionView.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <utility>

#include "productui/Format.h"
#include "productui/PetEditorial.h"
#include "productui/UiTheme.h"

namespace nimvlets::productui {

using catalog::OwnershipStatus;
using platform::TextWeight;

namespace {

constexpr float kHeaderClipTop = 112.0f;  // el hero arranca en 120; arriba de acá no se recorta
constexpr float kWheelStep = 48.0f;

bool StartsWith(const std::string& s, const char* prefix) {
    return s.rfind(prefix, 0) == 0;
}

std::string PetIdFromItemFocusId(const std::string& focusId) {
    return StartsWith(focusId, "item:") ? focusId.substr(5) : std::string();
}

std::string VariantFromFocusId(const std::string& focusId) {
    return StartsWith(focusId, "variant:") ? focusId.substr(8) : std::string();
}

// Color del texto de estado según el estado de propiedad.
UiColor StatusColor(OwnershipStatus status, UiColor accentLine) {
    switch (status) {
        case OwnershipStatus::kActive:
            return theme::kTextMuted;
        case OwnershipStatus::kOwnedInactive:
            return accentLine;  // el hint "Use" lleva el tono del pet
        case OwnershipStatus::kLocked:
            return theme::kTextFaint;
    }
    return theme::kTextMuted;
}

// Alpha del arte de un pet locked: visible pero más callado, NO
// grayscale/opacidad agresiva (brief §12).
constexpr unsigned char kLockedArtAlpha = 150;

// Alpha de la forma orgánica detrás del arte del hero: muy sutil — la
// forma "apoya" el arte, nunca compite (brief §10).
constexpr unsigned char kHeroShapeAlpha = 52;

}  // namespace

void CollectionView::SetModel(catalog::CollectionModel model, std::uint64_t clickBalance) {
    model_ = std::move(model);
    clickBalance_ = clickBalance;

    // Si el hero seleccionado ya no existe, se vuelve a "seguir al pet
    // activo" (selectedPetId_ vacío).
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
            // El hero sigue mostrando el pet que se está activando.
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
    const CollectionLayout layout = BuildLayout(viewportW_, viewportH_);
    const std::string hit = layout.HitTest(x, y);
    if (hit.empty()) {
        return CollectionViewResult{};
    }
    if (StartsWith(hit, "item:")) {
        focus_.Focus(hit);
        focusVisible_ = true;
    }
    return ActivateWidget(hit);
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
            focusVisible_ = true;
            dirty_ = true;
            r.dirty = true;
            return r;
        case SDLK_LEFT:
        case SDLK_UP:
            focus_.Prev();
            focusVisible_ = true;
            dirty_ = true;
            r.dirty = true;
            return r;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_SPACE:
            focusVisible_ = true;
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
    UiPainter& painter, TextCache& text, PetPreviewCache& previews, const catalog::PetCatalog& catalog,
    float viewportW, float viewportH) {
    viewportW_ = viewportW;
    viewportH_ = viewportH;

    const CollectionLayout layout = BuildLayout(viewportW, viewportH);
    lastContentHeight_ = layout.contentHeight;
    SyncFocusList(layout);
    // El anillo de foco solo se muestra tras la primera navegación por
    // teclado / click que enfoca (focus-visible).
    const std::string focusedId = focusVisible_ ? focus_.FocusedId() : std::string();

    painter.Clear(theme::kBackground);

    // --- Cabecera (sin recorte de scroll) ---
    const float titleBaseline = layout.titleAnchor.y + 18.0f;
    DrawText(painter, text, "Nimvlets", type::kTitle, TextWeight::kSemibold, theme::kText,
             layout.titleAnchor.x, titleBaseline, HAlign::kLeft);
    DrawText(painter, text, FormatClickCount(clickBalance_, language_), type::kClicks, TextWeight::kRegular,
             theme::kTextMuted, layout.clicksAnchorRight.x, titleBaseline, HAlign::kRight);
    DrawText(painter, text, core::Localized(core::StringKey::kCollection, language_), type::kSectionTitle,
             TextWeight::kSemibold, theme::kText, layout.sectionTitleAnchor.x,
             layout.sectionTitleAnchor.y + 12.0f, HAlign::kLeft);
    DrawText(painter, text, core::Localized(core::StringKey::kYourCompanions, language_), type::kSectionSub,
             TextWeight::kRegular, theme::kTextFaint, layout.sectionSubtitleAnchor.x,
             layout.sectionSubtitleAnchor.y + 12.0f, HAlign::kLeft);

    painter.PushClip(UiRect{0.0f, kHeaderClipTop, viewportW, std::max(0.0f, viewportH - kHeaderClipTop)});

    // --- Hero ---
    const CollectionHero& h = layout.hero;
    if (!h.petId.empty()) {
        const UiColor shapeCol = h.accent.shapeTint.WithAlpha(kHeroShapeAlpha);
        if (h.accent.angularShape) {
            painter.FillRoundRect(h.shape, 34.0f, shapeCol);
        } else {
            painter.FillEllipse(h.shape, shapeCol);
        }

        const std::string variant = h.selectedVariantId;
        SDL_Texture* art = previews.Peek(h.petId, variant);
        if (art == nullptr) {
            art = previews.Acquire(catalog, h.petId, variant);
        }
        const unsigned char artAlpha = h.status == OwnershipStatus::kLocked ? kLockedArtAlpha : 255;
        painter.DrawTextureContained(art, h.art, artAlpha);

        DrawText(painter, text, h.displayName, type::kHeroName, TextWeight::kSemibold, theme::kText,
                 h.nameAnchor.x, h.nameAnchor.y + 24.0f, HAlign::kLeft);
        if (!h.speciesText.empty()) {
            DrawText(painter, text, h.speciesText, type::kHeroMeta, TextWeight::kRegular, theme::kTextMuted,
                     h.speciesAnchor.x, h.speciesAnchor.y + 12.0f, HAlign::kLeft);
        }
        DrawText(painter, text, h.statusText, type::kHeroMeta, TextWeight::kMedium,
                 StatusColor(h.status, h.accent.line), h.statusAnchor.x, h.statusAnchor.y + 12.0f,
                 HAlign::kLeft);

        // Selector de variante tipográfico: "Male · Female" con
        // subrayado de acento bajo la seleccionada (brief §13).
        for (std::size_t i = 0; i < h.variants.size(); ++i) {
            const HeroVariantChip& chip = h.variants[i];
            if (i > 0) {
                const float dotX = 0.5f * (h.variants[i - 1].rect.Right() + chip.rect.x);
                DrawText(painter, text, "·", type::kHeroVariant, TextWeight::kRegular, theme::kTextFaint,
                         dotX, chip.rect.CenterY() + 5.0f, HAlign::kCenter);
            }
            DrawText(painter, text, chip.label, type::kHeroVariant,
                     chip.selected ? TextWeight::kSemibold : TextWeight::kRegular,
                     chip.selected ? theme::kText : theme::kTextMuted, chip.rect.CenterX(),
                     chip.rect.CenterY() + 5.0f, HAlign::kCenter);
            if (chip.selected) {
                painter.FillRect(chip.underline, h.accent.line);
            }
            if (focusedId == chip.focusId) {
                painter.StrokeRoundRect(chip.rect.Inset(-3.0f), 8.0f, 2.0f, h.accent.line);
            }
        }

        // Botón de acción.
        if (h.actionEnabled) {
            painter.FillRoundRect(h.actionButton, 9.0f, theme::kButtonFill);
            if (focusedId == h.actionFocusId) {
                painter.StrokeRoundRect(h.actionButton.Inset(-3.0f), 12.0f, 2.0f, h.accent.line);
            }
            DrawText(painter, text, h.actionLabel, type::kButton, TextWeight::kSemibold, theme::kButtonText,
                     h.actionButton.CenterX(), h.actionButton.CenterY() + 4.5f, HAlign::kCenter);
        } else {
            painter.StrokeRoundRect(h.actionButton, 9.0f, 1.0f, theme::kHairline);
            DrawText(painter, text, h.actionLabel, type::kButton, TextWeight::kMedium, theme::kTextMuted,
                     h.actionButton.CenterX(), h.actionButton.CenterY() + 4.5f, HAlign::kCenter,
                     static_cast<int>(h.actionButton.w - 6.0f));
        }
    }

    // --- Divisor ---
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

        painter.FillRoundRect(g.art.Inset(-5.0f), 12.0f,
                              g.status == OwnershipStatus::kLocked ? theme::kArtBed.WithAlpha(120)
                                                                  : theme::kArtBed);
        SDL_Texture* art = previews.Peek(g.petId, g.previewVariantId);
        if (art == nullptr) {
            art = previews.Acquire(catalog, g.petId, g.previewVariantId);
        }
        painter.DrawTextureContained(
            art, g.art, g.status == OwnershipStatus::kLocked ? kLockedArtAlpha : 255);

        const bool emphasize = hovered || focused;
        DrawText(painter, text, g.displayName, type::kGalleryName, TextWeight::kMedium,
                 emphasize ? theme::kText : theme::kText, g.name.CenterX(), g.name.y + 13.0f,
                 HAlign::kCenter, static_cast<int>(g.cell.w - 6.0f));
        DrawText(painter, text, g.statusText, type::kGalleryStatus, TextWeight::kRegular,
                 StatusColor(g.status, g.accentLine), g.status_.CenterX(), g.status_.y + 11.0f,
                 HAlign::kCenter, static_cast<int>(g.cell.w - 6.0f));
    }

    painter.PopClip();
}

}  // namespace nimvlets::productui
