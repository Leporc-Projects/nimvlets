#include "productui/CollectionLayout.h"

#include <algorithm>
#include <cctype>

namespace nimvlets::productui {

using catalog::CollectionItem;
using catalog::CollectionModel;
using catalog::OwnershipStatus;

namespace {

// Métricas en PUNTOS lógicos. Deliberadamente sobrias (block brief §3):
// márgenes amplios, jerarquía chica, sin cards fuertes.
constexpr float kMargin = 36.0f;
constexpr float kTitleTop = 30.0f;
constexpr float kTitleH = 24.0f;
constexpr float kSectionLabelTop = 78.0f;
constexpr float kSectionLabelH = 16.0f;
constexpr float kGridTop = 112.0f;
constexpr int kMaxCols = 3;
constexpr float kArtMax = 148.0f;
constexpr float kNameH = 18.0f;
constexpr float kStatusH = 15.0f;
constexpr float kArtToName = 14.0f;
constexpr float kNameToStatus = 5.0f;
constexpr float kRowGap = 26.0f;

constexpr float kDetailTopGap = 30.0f;
constexpr float kDetailArt = 176.0f;
constexpr float kDetailNameH = 24.0f;
constexpr float kChipW = 74.0f;
constexpr float kChipH = 30.0f;
constexpr float kChipGap = 10.0f;
constexpr float kButtonH = 34.0f;
constexpr float kButtonPadX = 22.0f;
// Ancho aproximado por carácter para dimensionar el botón sin medir
// texto real acá (la vista lo puede ajustar; alcanza para el hit-test).
constexpr float kApproxCharW = 8.0f;

std::string Capitalize(const std::string& s) {
    if (s.empty()) {
        return s;
    }
    std::string out = s;
    out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
    return out;
}

}  // namespace

const char* StatusShortLabel(OwnershipStatus status) {
    switch (status) {
        case OwnershipStatus::kActive:
            return "On desktop";
        case OwnershipStatus::kOwnedInactive:
            return "Use";
        case OwnershipStatus::kLocked:
            return "Not in your collection";
    }
    return "";
}

const CollectionItemBox* CollectionLayout::FindItem(const std::string& petId) const {
    for (const CollectionItemBox& box : items) {
        if (box.petId == petId) {
            return &box;
        }
    }
    return nullptr;
}

std::string CollectionLayout::HitTest(float x, float y) const {
    if (detail.open) {
        for (const VariantChip& chip : detail.variants) {
            if (chip.rect.Contains(x, y)) {
                return chip.focusId;
            }
        }
        if (detail.actionEnabled && detail.actionButton.Contains(x, y)) {
            return detail.actionFocusId;
        }
    }
    for (const CollectionItemBox& box : items) {
        if (box.cell.Contains(x, y)) {
            return box.focusId;
        }
    }
    return "";
}

float ClampScroll(float scrollY, float contentHeight, float viewportH) {
    const float maxScroll = std::max(0.0f, contentHeight - viewportH);
    return std::clamp(scrollY, 0.0f, maxScroll);
}

CollectionLayout BuildCollectionLayout(const CollectionModel& model, const CollectionLayoutInput& in) {
    CollectionLayout out;
    out.viewport = UiRect{0.0f, 0.0f, in.viewportW, in.viewportH};

    const float sy = in.scrollY;
    const float contentW = std::max(120.0f, in.viewportW - 2.0f * kMargin);

    out.titleAnchor = UiRect{kMargin, kTitleTop - sy, contentW, kTitleH};
    out.clicksAnchorRight = UiRect{in.viewportW - kMargin, kTitleTop - sy, 0.0f, kTitleH};
    out.sectionLabelAnchor = UiRect{kMargin, kSectionLabelTop - sy, contentW, kSectionLabelH};

    const int count = static_cast<int>(model.items.size());
    const int cols = std::max(1, std::min(kMaxCols, count));
    const float colW = contentW / static_cast<float>(cols);
    const float art = std::min(kArtMax, colW - 22.0f);
    const float rowH = art + kArtToName + kNameH + kNameToStatus + kStatusH + kRowGap;

    float lastRowBottom = kGridTop - sy;
    for (int i = 0; i < count; ++i) {
        const CollectionItem& item = model.items[static_cast<std::size_t>(i)];
        const int row = i / cols;
        const int col = i % cols;

        CollectionItemBox box;
        box.petId = item.petId;
        box.displayName = item.displayName;
        box.status = item.status;
        box.hasVariants = item.HasVariants();
        box.focusId = "item:" + item.petId;

        const float cellX = kMargin + static_cast<float>(col) * colW;
        const float cellY = (kGridTop - sy) + static_cast<float>(row) * rowH;
        box.cell = UiRect{cellX, cellY, colW, rowH - kRowGap * 0.4f};

        box.art = UiRect{cellX + (colW - art) * 0.5f, cellY, art, art};
        box.name = UiRect{cellX, box.art.Bottom() + kArtToName, colW, kNameH};
        box.status_ = UiRect{cellX, box.name.Bottom() + kNameToStatus, colW, kStatusH};

        lastRowBottom = std::max(lastRowBottom, box.status_.Bottom());
        out.items.push_back(box);
        out.focusOrder.push_back(box.focusId);
    }

    float bottom = lastRowBottom + kRowGap;

    // --- Panel de detalle ---
    if (in.detailOpen) {
        const CollectionItem* item = model.Find(in.detailPetId);
        if (item != nullptr) {
            CollectionDetail& d = out.detail;
            d.open = true;
            d.petId = item->petId;
            d.displayName = item->displayName;
            d.status = item->status;

            const float panelY = bottom + kDetailTopGap;
            d.panel = UiRect{kMargin, panelY, contentW, kDetailArt + 8.0f};
            d.art = UiRect{kMargin, panelY, kDetailArt, kDetailArt};

            const float rightX = d.art.Right() + 30.0f;
            const float rightW = std::max(120.0f, kMargin + contentW - rightX);
            d.nameAnchor = UiRect{rightX, panelY + 2.0f, rightW, kDetailNameH};

            std::string selected = in.detailSelectedVariantId;
            const bool selectedValid =
                std::any_of(item->variants.begin(), item->variants.end(),
                            [&](const catalog::CollectionVariant& v) { return v.variantId == selected; });
            if (!selectedValid) {
                selected = item->selectedVariantId;
            }
            d.selectedVariantId = selected;

            float cursorY = d.nameAnchor.Bottom() + 14.0f;
            if (item->HasVariants()) {
                float chipX = rightX;
                for (const catalog::CollectionVariant& v : item->variants) {
                    VariantChip chip;
                    chip.variantId = v.variantId;
                    chip.label = Capitalize(v.variantId);
                    chip.rect = UiRect{chipX, cursorY, kChipW, kChipH};
                    chip.focusId = "variant:" + v.variantId;
                    chip.selected = (v.variantId == selected);
                    d.variants.push_back(chip);
                    out.focusOrder.push_back(chip.focusId);
                    chipX += kChipW + kChipGap;
                }
                cursorY += kChipH + 16.0f;
            }

            // Etiqueta y habilitación del botón de acción.
            const bool activePet = item->status == OwnershipStatus::kActive;
            const bool variantWouldChange =
                item->HasVariants() && activePet && selected != model.activeVariantId;
            if (item->status == OwnershipStatus::kLocked) {
                d.actionLabel = "Not in your collection";
                d.actionEnabled = false;
            } else if (activePet && !variantWouldChange) {
                d.actionLabel = "On desktop";
                d.actionEnabled = false;
            } else {
                d.actionLabel = "Use " + item->displayName;
                d.actionEnabled = true;
            }
            d.actionFocusId = "use";

            const float buttonW =
                kButtonPadX * 2.0f + static_cast<float>(d.actionLabel.size()) * kApproxCharW;
            d.actionButton = UiRect{rightX, cursorY, buttonW, kButtonH};
            if (d.actionEnabled) {
                out.focusOrder.push_back(d.actionFocusId);
            }

            bottom = std::max(d.panel.Bottom(), d.actionButton.Bottom()) + kMargin;
        }
    }

    // contentHeight se calcula SIN el scroll ya aplicado: se re-suma sy
    // porque `bottom` está en coordenadas ya desplazadas.
    out.contentHeight = bottom + sy;
    return out;
}

}  // namespace nimvlets::productui
