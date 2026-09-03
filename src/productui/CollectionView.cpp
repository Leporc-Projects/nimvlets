#include "productui/CollectionView.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <utility>

#include "productui/ButtonStyle.h"
#include "productui/Ornaments.h"
#include "productui/SectionHeaderView.h"
#include "productui/UiTheme.h"

namespace nimvlets::productui {

using catalog::OwnershipStatus;
using platform::TextWeight;

namespace {

constexpr float kHeaderClipTop = 100.0f;  // deja lugar al borde superior del panel enmarcado del hero (convergencia DEC-147)
constexpr float kWheelStep = 48.0f;

// Alpha de las primitivas del hero stage: el stage "apoya" el arte,
// nunca compite (brief §11). La primaria un poco más presente que en
// Block 06.1 (52) porque el owner la reportó demasiado sutil (§5).
constexpr unsigned char kStagePrimaryAlpha = 92;
constexpr unsigned char kStageSecondaryAlpha = 52;

// Alpha del arte de un pet locked: visible pero más callado, NO
// grayscale/opacidad agresiva (brief §12/§13).
constexpr unsigned char kLockedArtAlpha = 150;

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

void CollectionView::SetModel(catalog::CollectionModel model) {
    model_ = std::move(model);

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
    in.clickBalance = clickBalance_;  // cache empujado por ProductWindow en cada Render
    in.selectedPetId = selectedPetId_;
    in.selectedVariantId = selectedVariantId_;
    in.hoverPetId = HoverPetId();
    CollectionLayout layout = BuildCollectionLayout(model_, in);
    // Coloca las pestañas con anchos MEDIDOS (serif) — no-op si Render
    // todavía no midió (navLabelWidths_ en 0). kMargin = 40 (compartido).
    ReflowNavTabs(layout.header, navLabelWidths_, w, 40.0f);
    return layout;
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
    UiPainter& painter, TextCache& text, PetPreviewCache& previews, float viewportW, float viewportH,
    std::uint64_t clickBalance) {
    viewportW_ = viewportW;
    viewportH_ = viewportH;
    clickBalance_ = clickBalance;  // balance CANÓNICO de ProductWindow — la única fuente
    MeasureNavLabels(text, language_, painter.Scale(), navLabelWidths_);

    const CollectionLayout layout = BuildLayout(viewportW, viewportH);
    lastContentHeight_ = layout.contentHeight;
    SyncFocusList(layout);
    const std::string focusedId = keyboardFocus_ ? focus_.FocusedId() : std::string();

    painter.Clear(theme::kBackground);

    // --- Cabecera compartida (título + balance + pestañas), sin recorte
    //     de scroll. El balance ya viene formateado en layout.header.clicksText.
    DrawSectionHeader(painter, text, layout.header, hoverId_, focusedId);

    painter.PushClip(UiRect{0.0f, kHeaderClipTop, viewportW, std::max(0.0f, viewportH - kHeaderClipTop)});

    // --- Segundo plano: la zona de la gallery (brief §12). Con un solo
    //     Nimvlet poseído no hay gallery -> no se dibuja el segundo plano
    //     (DEC-136). ---
    const bool hasGallery = !layout.gallery.empty();
    if (hasGallery) {
        painter.FillRect(layout.galleryShelf, theme::kGalleryShelf);
    }

    // --- Hero ---
    const CollectionHero& h = layout.hero;
    if (!h.petId.empty()) {
        // Panel enmarcado suave alrededor de TODO el hero (convergencia
        // DEC-147) — "esto es mi compañero".
        DrawSoftPanel(painter, h.heroPanel, 16.0f, tokens::kSurfaceRaised, tokens::kBorder,
                      /*innerHighlight=*/true);

        // Hero stage: primaria grande + secundaria descentrada, teñidas
        // con el shapeTint del pet a alpha bajo (brief §11).
        FillStagePrimitive(painter, h.stageSecondary, h.accent.angularShape,
                           h.accent.shapeTint.WithAlpha(kStageSecondaryAlpha));
        FillStagePrimitive(painter, h.stagePrimary, h.accent.angularShape,
                           h.accent.shapeTint.WithAlpha(kStagePrimaryAlpha));

        SDL_Texture* art = previews.Get(h.petId, h.selectedVariantId);
        const unsigned char artAlpha = h.status == OwnershipStatus::kLocked ? kLockedArtAlpha : 255;
        painter.DrawTextureContained(art, h.art, artAlpha);

        DrawText(painter, text, h.displayName, type::role::kHeroTitle, tokens::kTextPrimary,
                 h.nameAnchor.x, h.nameAnchor.y + 24.0f, HAlign::kLeft);
        // DIVISOR 1: bajo el nombre, rombo central = acento del pet
        // (identidad). Acotado para no estirarse por el lado vacío.
        DrawOrnamentalDivider(
            painter,
            UiRect{h.nameRule.x, h.nameRule.y - 3.5f, std::min(h.nameRule.w, 360.0f), 8.0f},
            tokens::kBorder, h.accent.line);

        if (!h.speciesText.empty()) {
            // Especie en el TONO del pet (metadata, pero especial).
            DrawText(painter, text, h.speciesText,
                     type::role::kMetadata.WithWeight(TextWeight::kMedium), h.accent.deepInk,
                     h.speciesAnchor.x, h.speciesAnchor.y + 12.0f, HAlign::kLeft);
        }
        if (!h.descriptionText.empty()) {
            // Block 07: la descripción es un par de frases -> word-wrap
            // dentro de la columna del hero, hasta 3 líneas (brief §19).
            DrawTextWrapped(painter, text, h.descriptionText, type::kHeroBody, TextWeight::kRegular,
                            theme::kText, h.descriptionAnchor.x, h.descriptionAnchor.y + 13.0f,
                            h.descriptionAnchor.w, 17.0f, 3);
        }

        // DIVISOR 2: identidad/descripción -> acción (rombo neutro).
        if (h.detailDividerRect.w > 0.0f) {
            DrawOrnamentalDivider(painter,
                                  UiRect{h.detailDividerRect.x, h.detailDividerRect.CenterY() - 4.0f,
                                         std::min(h.detailDividerRect.w, 360.0f), 8.0f},
                                  tokens::kBorder, tokens::kOrnamentNeutral);
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
            // "Use <pet>": la MISMA familia de CTA que el Shop (pill
            // generosa, relleno de acento, filo de oro) pero SIN el spark
            // de economía — Collection no es una tienda (brief §12 /
            // convergencia DEC-147).
            ButtonVisual v = ResolveButtonVisual(
                ButtonRole::kPrimaryCta, &h.accent,
                ButtonStateFlags{false, false, focusedId == h.actionFocusId, false});
            v.sparkle = false;
            DrawButton(painter, text, h.actionButton, h.actionLabel, v, type::role::kButton);
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

    // --- Un solo Nimvlet: línea quieta hacia el Shop, sin divisor
    //     (DEC-136 / brief §4.A) ---
    if (!hasGallery) {
        if (!layout.emptyGalleryText.empty()) {
            // Línea editorial quieta hacia el Shop, con el mismo rótulo
            // flanqueado que el encabezado del Shop (owner QA — DEC-146).
            DrawFlankedLabel(painter, text, layout.emptyGalleryText, type::role::kSectionLabel,
                             tokens::kTextMuted, tokens::kOrnamentNeutral,
                             layout.emptyGalleryAnchor.CenterX(),
                             layout.emptyGalleryAnchor.y + 13.0f);
        }
        painter.PopClip();
        return;
    }

    // --- Divisor ornamental (── ◇ ──) entre el hero y la gallery
    //     (referencia D) — sobre el borde del segundo plano. ---
    DrawOrnamentalDivider(
        painter,
        UiRect{layout.dividerRect.x, layout.dividerRect.y - 3.5f, layout.dividerRect.w, 8.0f},
        tokens::kBorder, tokens::kOrnamentNeutral);

    // --- Gallery — UNA card coherente por Nimvlet poseído ---------
    // Sin pedestal-caja detrás del arte (convergencia DEC-147): el arte
    // va directo sobre la superficie de la card. Nombre en serif; el
    // hint "Use" / "On desktop" en sans, con el tono del pet.
    for (const GalleryItem& g : layout.gallery) {
        const bool hovered = hoverId_ == g.focusId;
        const bool focused = focusedId == g.focusId;
        const UiRect card = g.cell.Inset(2.0f);
        painter.FillRoundRect(card, 12.0f, hovered ? tokens::kHoverWash : tokens::kSurfaceRaised);
        painter.StrokeRoundRect(card, 12.0f, 1.0f, tokens::kBorder);
        if (focused) {
            painter.StrokeRoundRect(card.Inset(-3.5f), 15.5f, 2.0f, tokens::kFocus);
        }

        SDL_Texture* art = previews.Get(g.petId, g.previewVariantId);
        painter.DrawTextureContained(
            art, g.art, g.status == OwnershipStatus::kLocked ? kLockedArtAlpha : 255);

        DrawText(painter, text, g.displayName, type::role::kCardName, theme::kText, g.name.CenterX(),
                 g.name.y + 13.0f, HAlign::kCenter, static_cast<int>(card.w - 8.0f));
        DrawText(painter, text, g.statusText, type::role::kCaption,
                 StatusColor(g.status, g.accentLine), g.status_.CenterX(), g.status_.y + 11.0f,
                 HAlign::kCenter, static_cast<int>(card.w - 8.0f));
    }

    painter.PopClip();
}

}  // namespace nimvlets::productui
