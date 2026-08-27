#include "productui/CollectionView.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <utility>

#include "productui/Format.h"

namespace nimvlets::productui {

using catalog::CollectionItem;
using catalog::OwnershipStatus;
using platform::TextWeight;

namespace {

constexpr float kHeaderClipTop = 100.0f;  // el grid arranca en 112; por debajo de acá se recorta el scroll
constexpr float kWheelStep = 48.0f;       // puntos por "línea" de rueda

bool StartsWith(const std::string& s, const char* prefix) {
    return s.rfind(prefix, 0) == 0;
}

std::string PetIdFromItemFocusId(const std::string& focusId) {
    return StartsWith(focusId, "item:") ? focusId.substr(5) : std::string();
}

std::string VariantFromFocusId(const std::string& focusId) {
    return StartsWith(focusId, "variant:") ? focusId.substr(8) : std::string();
}

// Sub-línea bajo el arte en el grid (block brief §8/§11).
std::string GridSubLabel(const CollectionItem& item) {
    switch (item.status) {
        case OwnershipStatus::kActive:
            return "On desktop";
        case OwnershipStatus::kLocked:
            return "Not in your collection";
        case OwnershipStatus::kOwnedInactive:
            break;
    }
    if (item.HasVariants()) {
        std::string joined;
        for (const auto& v : item.variants) {
            if (!joined.empty()) {
                joined += " · ";  // " · "
            }
            std::string label = v.variantId;
            if (!label.empty()) {
                label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
            }
            joined += label;
        }
        return joined;
    }
    return "Use";
}

}  // namespace

void CollectionView::SetModel(catalog::CollectionModel model, std::uint64_t clickBalance) {
    model_ = std::move(model);
    clickBalance_ = clickBalance;

    if (detailOpen_ && model_.Find(detailPetId_) == nullptr) {
        CloseDetail();
    }
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    dirty_ = true;
}

CollectionLayout CollectionView::BuildLayout(float w, float h) const {
    CollectionLayoutInput in;
    in.viewportW = w;
    in.viewportH = h;
    in.scrollY = ClampScroll(scrollY_, lastContentHeight_, h);
    in.detailOpen = detailOpen_;
    in.detailPetId = detailPetId_;
    in.detailSelectedVariantId = detailVariantId_;
    return BuildCollectionLayout(model_, in);
}

void CollectionView::SyncFocusList(const CollectionLayout& layout) {
    focus_.SetItems(layout.focusOrder);
}

std::string CollectionView::ResolvedDetailVariant() const {
    const CollectionItem* item = model_.Find(detailPetId_);
    if (item == nullptr) {
        return std::string();
    }
    const bool valid = std::any_of(item->variants.begin(), item->variants.end(),
                                   [&](const catalog::CollectionVariant& v) { return v.variantId == detailVariantId_; });
    return valid ? detailVariantId_ : item->selectedVariantId;
}

void CollectionView::OpenDetail(const std::string& petId) {
    const CollectionItem* item = model_.Find(petId);
    if (item == nullptr) {
        return;
    }
    detailOpen_ = true;
    detailPetId_ = petId;
    detailVariantId_ = item->selectedVariantId;
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    focus_.Focus("item:" + petId);
    dirty_ = true;
}

void CollectionView::CloseDetail() {
    const std::string was = detailPetId_;
    detailOpen_ = false;
    detailPetId_.clear();
    detailVariantId_.clear();
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    if (!was.empty()) {
        focus_.Focus("item:" + was);
    }
    dirty_ = true;
}

CollectionViewResult CollectionView::ActivateWidget(const std::string& focusId) {
    CollectionViewResult r;
    if (focusId.empty()) {
        return r;
    }
    if (StartsWith(focusId, "item:")) {
        OpenDetail(PetIdFromItemFocusId(focusId));
        r.dirty = true;
        return r;
    }
    if (StartsWith(focusId, "variant:")) {
        detailVariantId_ = VariantFromFocusId(focusId);
        SyncFocusList(BuildLayout(viewportW_, viewportH_));
        focus_.Focus(focusId);
        r.dirty = true;
        return r;
    }
    if (focusId == "use") {
        const CollectionLayout layout = BuildLayout(viewportW_, viewportH_);
        if (layout.detail.open && layout.detail.actionEnabled) {
            r.hasActivate = true;
            r.activate.petId = detailPetId_;
            r.activate.variantId = ResolvedDetailVariant();
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
        // Click en el vacío: si hay un detalle abierto, se cierra.
        CollectionViewResult r;
        if (detailOpen_) {
            CloseDetail();
            r.dirty = true;
        }
        return r;
    }
    if (StartsWith(hit, "item:")) {
        focus_.Focus(hit);
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
            if (shiftHeld) {
                focus_.Prev();
            } else {
                focus_.Next();
            }
            dirty_ = true;
            r.dirty = true;
            return r;
        case SDLK_RIGHT:
        case SDLK_DOWN:
            focus_.Next();
            dirty_ = true;
            r.dirty = true;
            return r;
        case SDLK_LEFT:
        case SDLK_UP:
            focus_.Prev();
            dirty_ = true;
            r.dirty = true;
            return r;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_SPACE:
            return ActivateWidget(focus_.FocusedId());
        case SDLK_ESCAPE:
            if (detailOpen_) {
                CloseDetail();
                r.dirty = true;
            } else {
                r.requestClose = true;
            }
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
    const std::string focusedId = focus_.FocusedId();

    painter.Clear(theme::kBackground);

    // --- Cabecera (sin recorte de scroll) ---
    const float titleBaseline = layout.titleAnchor.y + 17.0f;
    DrawText(painter, text, "Nimvlets", type::kTitle, TextWeight::kSemibold, theme::kText,
             layout.titleAnchor.x, titleBaseline, HAlign::kLeft);
    DrawText(painter, text, FormatClickCount(clickBalance_), type::kClicks, TextWeight::kRegular,
             theme::kTextMuted, layout.clicksAnchorRight.x, titleBaseline, HAlign::kRight);
    DrawText(painter, text, "Collection", type::kSectionLabel, TextWeight::kMedium, theme::kTextFaint,
             layout.sectionLabelAnchor.x, layout.sectionLabelAnchor.y + 12.0f, HAlign::kLeft);

    // --- Cuerpo scrolleable ---
    painter.PushClip(UiRect{0.0f, kHeaderClipTop, viewportW, std::max(0.0f, viewportH - kHeaderClipTop)});

    for (std::size_t i = 0; i < layout.items.size() && i < model_.items.size(); ++i) {
        const CollectionItemBox& box = layout.items[i];
        const CollectionItem& item = model_.items[i];
        const bool selected = detailOpen_ && detailPetId_ == box.petId;
        const bool hovered = hoverId_ == box.focusId;

        if (selected) {
            painter.FillRoundRect(box.cell.Inset(2.0f), 12.0f, theme::kSelectedWash);
        } else if (hovered) {
            painter.FillRoundRect(box.cell.Inset(2.0f), 12.0f, theme::kHoverWash);
        }
        if (focusedId == box.focusId) {
            painter.RoundRectBorder(box.cell.Inset(2.0f), 12.0f, theme::kAccent, UiColor{0, 0, 0, 0});
        }

        if (item.status != OwnershipStatus::kLocked) {
            painter.FillRoundRect(box.art.Inset(-6.0f), 14.0f, theme::kArtBed);
            SDL_Texture* art = previews.Peek(box.petId, item.selectedVariantId);
            if (art == nullptr) {
                art = previews.Acquire(catalog, box.petId, item.selectedVariantId);
            }
            painter.DrawTextureContained(art, box.art, 255);
        }

        DrawText(painter, text, item.displayName, type::kPetName, TextWeight::kMedium, theme::kText,
                 box.name.CenterX(), box.name.y + 14.0f, HAlign::kCenter,
                 static_cast<int>(box.cell.w - 8.0f));

        const std::string sub = GridSubLabel(item);
        const UiColor subColor = item.status == OwnershipStatus::kLocked
                                     ? theme::kTextFaint
                                     : (item.status == OwnershipStatus::kOwnedInactive && !item.HasVariants()
                                            ? theme::kAccent
                                            : theme::kTextMuted);
        DrawText(painter, text, sub, type::kStatus, TextWeight::kRegular, subColor, box.status_.CenterX(),
                 box.status_.y + 12.0f, HAlign::kCenter, static_cast<int>(box.cell.w - 8.0f));
    }

    // --- Panel de detalle ---
    if (layout.detail.open) {
        const CollectionDetail& d = layout.detail;
        painter.FillRect(UiRect{d.panel.x, d.panel.y - 18.0f, d.panel.w, 1.0f}, theme::kHairline);

        if (d.status != OwnershipStatus::kLocked) {
            painter.FillRoundRect(d.art.Inset(-8.0f), 16.0f, theme::kArtBed);
            const std::string variant = ResolvedDetailVariant();
            SDL_Texture* art = previews.Peek(d.petId, variant);
            if (art == nullptr) {
                art = previews.Acquire(catalog, d.petId, variant);
            }
            painter.DrawTextureContained(art, d.art, 255);
        }

        DrawText(painter, text, d.displayName, type::kDetailName, TextWeight::kSemibold, theme::kText,
                 d.nameAnchor.x, d.nameAnchor.y + 20.0f, HAlign::kLeft);

        for (const VariantChip& chip : d.variants) {
            if (chip.selected) {
                painter.RoundRectBorder(chip.rect, 9.0f, theme::kAccent, theme::kAccentSoft);
            } else {
                painter.RoundRectBorder(chip.rect, 9.0f, theme::kHairline, theme::kBackground);
            }
            if (focusedId == chip.focusId) {
                painter.RoundRectBorder(chip.rect.Inset(-3.0f), 12.0f, theme::kAccent, UiColor{0, 0, 0, 0});
            }
            DrawText(painter, text, chip.label, type::kChip, TextWeight::kMedium,
                     chip.selected ? theme::kText : theme::kTextMuted, chip.rect.CenterX(),
                     chip.rect.CenterY() + 4.5f, HAlign::kCenter);
        }

        if (d.actionEnabled) {
            painter.FillRoundRect(d.actionButton, 9.0f, theme::kButtonFill);
            if (focusedId == d.actionFocusId) {
                painter.RoundRectBorder(d.actionButton.Inset(-3.0f), 12.0f, theme::kAccent, UiColor{0, 0, 0, 0});
            }
            DrawText(painter, text, d.actionLabel, type::kButton, TextWeight::kSemibold, theme::kButtonText,
                     d.actionButton.CenterX(), d.actionButton.CenterY() + 4.5f, HAlign::kCenter);
        } else {
            painter.RoundRectBorder(d.actionButton, 9.0f, theme::kHairline, theme::kBackground);
            DrawText(painter, text, d.actionLabel, type::kButton, TextWeight::kMedium, theme::kTextMuted,
                     d.actionButton.CenterX(), d.actionButton.CenterY() + 4.5f, HAlign::kCenter,
                     static_cast<int>(d.actionButton.w - 6.0f));
        }
    }

    painter.PopClip();
}

}  // namespace nimvlets::productui
