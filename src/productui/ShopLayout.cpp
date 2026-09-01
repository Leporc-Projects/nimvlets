#include "productui/ShopLayout.h"

#include <algorithm>
#include <cmath>

#include "productui/Format.h"
#include "productui/PetEditorial.h"

namespace nimvlets::productui {

using catalog::ShopItem;
using catalog::ShopItemStatus;
using catalog::ShopModel;
using core::Language;
using core::Localized;
using core::StringKey;

namespace {

// --- Métricas comunes ---------------------------------------------------
constexpr float kMargin = 40.0f;

// --- Rejilla de browse (brief §4/§5) ---------------------------------
// El arte manda; la rejilla se centra en el espacio disponible y
// envuelve limpio para 1..8 personajes sin scroll horizontal (brief
// §16). El alto de la línea revelada SIEMPRE se reserva -> el hover no
// reordena nada (redibujo event-driven, sin reflow — brief §15).
constexpr float kBrowseHeadingH = 18.0f;
constexpr float kBrowseHeadingGap = 22.0f;   // encabezado -> rejilla
constexpr float kBrowseColGap = 24.0f;
constexpr float kBrowseRowGap = 20.0f;
constexpr float kBrowseTileMaxW = 200.0f;
constexpr float kBrowseTileMinW = 112.0f;
constexpr float kBrowseArtMax = 132.0f;
constexpr float kBrowsePadTop = 10.0f;
constexpr float kBrowseArtToName = 10.0f;
constexpr float kBrowseNameH = 18.0f;
constexpr float kBrowseNameToReveal = 5.0f;
constexpr float kBrowseRevealH = 14.0f;
constexpr float kBrowsePadBottom = 10.0f;

// --- Hero (kSelected) — mismas proporciones que la Collection --------
constexpr float kHeroArt = 216.0f;
constexpr float kHeroTextGap = 40.0f;
constexpr float kHeroNameH = 32.0f;
constexpr float kHeroRuleGap = 12.0f;
constexpr float kHeroRuleW = 46.0f;
constexpr float kHeroRuleH = 2.0f;
constexpr float kSpeciesGap = 12.0f;
constexpr float kSpeciesH = 16.0f;
constexpr float kDescGap = 8.0f;
constexpr float kDescLineH = 17.0f;
constexpr int kDescMaxLines = 3;
constexpr float kBlockGap = 14.0f;
constexpr float kPriceH = 17.0f;
constexpr float kPriceToAction = 12.0f;
constexpr float kHeroButtonH = 36.0f;
constexpr float kHeroButtonPadX = 22.0f;
constexpr float kHeroStatusH = 18.0f;

constexpr float kConfirmPromptH = 34.0f;   // hasta 2 líneas
constexpr float kConfirmGap = 12.0f;
constexpr float kConfirmButtonH = 32.0f;
constexpr float kConfirmButtonPadX = 18.0f;
constexpr float kConfirmButtonGap = 12.0f;

// --- Rail compacto (kSelected) — la estantería de browse, más chica --
constexpr float kRailDividerGap = 24.0f;
constexpr float kRailTopGap = 20.0f;         // divisor -> primera tarjeta del rail
constexpr float kRailGap = 18.0f;
constexpr float kRailTileMaxW = 96.0f;
constexpr float kRailTileMinW = 60.0f;
constexpr float kRailArtMax = 60.0f;
constexpr float kRailPadTop = 10.0f;
constexpr float kRailArtToName = 8.0f;
constexpr float kRailNameH = 14.0f;
constexpr float kRailPadBottom = 10.0f;
constexpr float kRailRowGap = 14.0f;

constexpr float kHoverLift = 2.0f;
constexpr float kApproxCharW = 8.0f;

std::string RevealTextFor(const ShopItem& item, Language lang) {
    switch (item.status) {
        case ShopItemStatus::kOwned:
            return Localized(StringKey::kInYourCollection, lang);
        case ShopItemStatus::kInsufficientBalance:
        case ShopItemStatus::kAffordable:
            return FormatClickCount(item.priceClicks, lang);
    }
    return "";
}

// Columnas para `n` tarjetas de browse: hasta 4. 1..4 caben en una sola
// fila que compone bien a 800x560; 5/6 envuelven a dos filas de <=3;
// 7/8 a dos filas de 4. Nunca scroll horizontal (brief §16).
int BrowseColumns(int n) {
    if (n <= 4) return std::max(1, n);
    if (n <= 6) return 3;
    return 4;
}

// Métricas de la rejilla de browse — calculadas UNA vez y compartidas
// por el centrado vertical y la colocación real (no pueden divergir).
struct BrowseGridMetrics {
    int cols = 1;
    int rows = 1;
    float tileW = 0.0f;
    float artSize = 0.0f;
    float tileH = 0.0f;
    float blockH = 0.0f;  // alto total de la rejilla (sin encabezado)
};

BrowseGridMetrics ComputeBrowseGrid(int n, float contentW) {
    BrowseGridMetrics m;
    m.cols = std::min(BrowseColumns(n), std::max(1, n));
    m.rows = (n + m.cols - 1) / m.cols;
    const float fitW =
        (contentW - static_cast<float>(m.cols - 1) * kBrowseColGap) / static_cast<float>(m.cols);
    m.tileW = std::clamp(fitW, kBrowseTileMinW, kBrowseTileMaxW);
    // El arte no se estira a lo bruto en rejillas densas: 4 col -> más
    // chico, para que dos filas de 4 respiren en la ventana por defecto.
    const float artCap = m.cols >= 4 ? 108.0f : m.cols == 3 ? 122.0f : kBrowseArtMax;
    m.artSize = std::min({artCap, kBrowseArtMax, m.tileW - 24.0f});
    m.tileH = kBrowsePadTop + m.artSize + kBrowseArtToName + kBrowseNameH + kBrowseNameToReveal +
              kBrowseRevealH + kBrowsePadBottom;
    m.blockH = static_cast<float>(m.rows) * m.tileH + static_cast<float>(m.rows - 1) * kBrowseRowGap;
    return m;
}

ShopTile MakeTile(const ShopItem& item, Language lang, bool selected) {
    ShopTile t;
    t.petId = item.petId;
    t.displayName = item.displayName;
    t.status = item.status;
    t.revealText = RevealTextFor(item, lang);
    const PetAccent accent = PetAccentFor(item.petId);
    t.accentLine = accent.line;
    t.pedestalTint = accent.shapeTint;
    t.selected = selected;
    t.focusId = "shopitem:" + item.petId;
    return t;
}

}  // namespace

const ShopTile* ShopLayout::FindTile(const std::string& petId) const {
    for (const ShopTile& t : tiles) {
        if (t.petId == petId) {
            return &t;
        }
    }
    for (const ShopTile& t : rail) {
        if (t.petId == petId) {
            return &t;
        }
    }
    return nullptr;
}

std::string ShopLayout::HitTest(float x, float y) const {
    for (const SectionTab& tab : header.tabs) {
        if (tab.hitRect.Contains(x, y)) {
            return tab.focusId;
        }
    }
    if (presentation == ShopPresentation::kSelected) {
        if (hero.confirm.visible) {
            if (hero.confirm.cancelButton.Contains(x, y)) {
                return hero.confirm.cancelFocusId;
            }
            if (hero.confirm.confirmButton.Contains(x, y)) {
                return hero.confirm.confirmFocusId;
            }
        } else if (hero.actionEnabled && hero.actionButton.Contains(x, y)) {
            return hero.actionFocusId;
        }
        for (const ShopTile& t : rail) {
            if (t.cell.Contains(x, y)) {
                return t.focusId;
            }
        }
        return "";
    }
    for (const ShopTile& t : tiles) {
        if (t.cell.Contains(x, y)) {
            return t.focusId;
        }
    }
    return "";
}

float ClampShopScroll(float scrollY, float contentHeight, float viewportH) {
    return std::clamp(scrollY, 0.0f, std::max(0.0f, contentHeight - viewportH));
}

namespace {

// Coloca la rejilla de browse: filas UNIFORMES (mismo ancho de tarjeta),
// cada fila centrada horizontalmente en el viewport (así una última fila
// parcial queda centrada, no pegada a la izquierda). Devuelve el bottom
// de la última fila.
float LayoutBrowseGrid(
    std::vector<ShopTile>& tiles, float viewportW, const BrowseGridMetrics& m, float gridTop) {
    const int n = static_cast<int>(tiles.size());
    if (n == 0) {
        return gridTop;
    }
    for (int i = 0; i < n; ++i) {
        const int row = i / m.cols;
        const int col = i % m.cols;
        const int inRow = std::min(m.cols, n - row * m.cols);
        const float rowW =
            static_cast<float>(inRow) * m.tileW + static_cast<float>(inRow - 1) * kBrowseColGap;
        const float rowLeft = (viewportW - rowW) * 0.5f;
        const float x = rowLeft + static_cast<float>(col) * (m.tileW + kBrowseColGap);
        const float y = gridTop + static_cast<float>(row) * (m.tileH + kBrowseRowGap);

        ShopTile& t = tiles[static_cast<std::size_t>(i)];
        t.cell = UiRect{x, y, m.tileW, m.tileH};
        t.art = UiRect{x + (m.tileW - m.artSize) * 0.5f, y + kBrowsePadTop, m.artSize, m.artSize};
        t.name = UiRect{x, t.art.Bottom() + kBrowseArtToName, m.tileW, kBrowseNameH};
        t.revealAnchor = UiRect{x, t.name.Bottom() + kBrowseNameToReveal, m.tileW, kBrowseRevealH};
    }
    return gridTop + m.blockH;
}

// Coloca el rail compacto bajo el hero: una o más filas de tarjetas
// chicas, cada fila centrada. Devuelve el bottom de la última fila.
float LayoutRail(std::vector<ShopTile>& rail, float viewportW, float contentW, float railTop,
                 const std::string& hoverPetId) {
    const int n = static_cast<int>(rail.size());
    if (n == 0) {
        return railTop;
    }
    const float fitW = (contentW - static_cast<float>(n - 1) * kRailGap) / static_cast<float>(n);
    const float tileW = std::clamp(fitW, kRailTileMinW, kRailTileMaxW);
    const int perRow = std::max(1, static_cast<int>(std::floor((contentW + kRailGap) / (tileW + kRailGap))));
    const float artSize = std::min(kRailArtMax, tileW - 12.0f);
    const float tileH =
        kRailPadTop + artSize + kRailArtToName + kRailNameH + kRailPadBottom;
    const int rows = (n + perRow - 1) / perRow;

    for (int i = 0; i < n; ++i) {
        const int row = i / perRow;
        const int col = i % perRow;
        const int inRow = std::min(perRow, n - row * perRow);
        const float rowW = static_cast<float>(inRow) * tileW + static_cast<float>(inRow - 1) * kRailGap;
        const float rowLeft = (viewportW - rowW) * 0.5f;
        const float lift = (rail[static_cast<std::size_t>(i)].petId == hoverPetId) ? kHoverLift : 0.0f;
        const float x = rowLeft + static_cast<float>(col) * (tileW + kRailGap);
        const float y = railTop + static_cast<float>(row) * (tileH + kRailRowGap) - lift;

        ShopTile& t = rail[static_cast<std::size_t>(i)];
        t.cell = UiRect{x, y, tileW, tileH};
        t.art = UiRect{x + (tileW - artSize) * 0.5f, y + kRailPadTop, artSize, artSize};
        t.name = UiRect{x, t.art.Bottom() + kRailArtToName, tileW, kRailNameH};
        t.revealAnchor = UiRect{x, t.name.y, tileW, kRailNameH};  // el rail no revela línea aparte
    }
    return railTop + static_cast<float>(rows) * tileH + static_cast<float>(rows - 1) * kRailRowGap;
}

// El hero + su hero stage + su bloque de texto/acción/confirmación.
// Devuelve el y del bottom del hero (para colocar el divisor + rail).
// Idéntico en espíritu al hero de la Collection / del Shop de Block 07.
float LayoutHero(
    ShopLayout& out, const ShopItem& heroItem, const ShopLayoutInput& in, float contentW) {
    const Language lang = in.language;
    ShopHero& h = out.hero;
    h.petId = heroItem.petId;
    h.displayName = heroItem.displayName;
    h.speciesText = Species(heroItem.petId, lang);
    h.descriptionText = ShortDescription(heroItem.petId, lang);
    h.status = heroItem.status;
    h.entitlementTarget = heroItem.entitlementTarget;
    h.accent = PetAccentFor(heroItem.petId);
    h.priceClicks = heroItem.priceClicks;
    h.priceText = FormatClickCount(heroItem.priceClicks, lang);

    const float heroTop = out.header.bodyTop;
    h.art = UiRect{kMargin, heroTop, kHeroArt, kHeroArt};

    const float textX = h.art.Right() + kHeroTextGap;
    const float textW = std::max(160.0f, kMargin + contentW - textX);

    const bool owned = heroItem.status == ShopItemStatus::kOwned;
    const bool affordable = heroItem.status == ShopItemStatus::kAffordable;
    const bool confirming = in.confirming && affordable;

    // Estado / acción / confirmación — mutuamente excluyentes.
    if (owned) {
        h.showStatusLine = true;
        h.statusText = Localized(StringKey::kInYourCollection, lang);
    } else if (confirming) {
        h.confirm.visible = true;
        h.confirm.prompt = FormatSpendPrompt(heroItem.priceClicks, heroItem.displayName, lang);
        h.confirm.cancelLabel = Localized(StringKey::kCancel, lang);
        h.confirm.cancelFocusId = "purchase:cancel";
        h.confirm.confirmLabel = Localized(StringKey::kConfirm, lang);
        h.confirm.confirmFocusId = "purchase:confirm";
    } else if (affordable) {
        h.actionEnabled = true;
        h.actionLabel = std::string(Localized(StringKey::kGetPetPrefix, lang)) + heroItem.displayName;
        h.actionFocusId = "get";
    } else {  // kInsufficientBalance
        h.showStatusLine = true;
        h.statusText = FormatNeedMoreClicks(heroItem.clicksShort, lang);
    }

    const bool hasSpecies = !h.speciesText.empty();
    const bool hasDesc = !h.descriptionText.empty();

    float blockH = kHeroNameH + kHeroRuleGap + kHeroRuleH;
    if (hasSpecies) {
        blockH += kSpeciesGap + kSpeciesH;
    }
    if (hasDesc) {
        blockH += kDescGap + kDescLineH * static_cast<float>(kDescMaxLines);
    }
    blockH += kBlockGap;
    if (!owned) {
        blockH += kPriceH + kPriceToAction;
    }
    if (h.confirm.visible) {
        blockH += kConfirmPromptH + kConfirmGap + kConfirmButtonH;
    } else if (h.actionEnabled) {
        blockH += kHeroButtonH;
    } else {
        blockH += kHeroStatusH;
    }

    const float blockTop = heroTop + std::max(0.0f, (kHeroArt - blockH) * 0.5f);

    const float stageRight = textX - 6.0f;
    h.stagePrimary = UiRect{h.art.x - 40.0f, h.art.y - 10.0f, stageRight - (h.art.x - 40.0f), h.art.h + 30.0f};
    h.stageSecondary = UiRect{h.art.x + h.art.w * 0.34f, h.art.y + h.art.h * 0.42f, h.art.w * 0.84f, h.art.h * 0.70f};

    float y = blockTop;
    h.nameAnchor = UiRect{textX, y, textW, kHeroNameH};
    y += kHeroNameH + kHeroRuleGap;
    h.nameRule = UiRect{textX, y, kHeroRuleW, kHeroRuleH};
    y += kHeroRuleH;
    if (hasSpecies) {
        y += kSpeciesGap;
        h.speciesAnchor = UiRect{textX, y, textW, kSpeciesH};
        y += kSpeciesH;
    }
    if (hasDesc) {
        y += kDescGap;
        h.descriptionAnchor = UiRect{textX, y, textW, kDescLineH * static_cast<float>(kDescMaxLines)};
        y += kDescLineH * static_cast<float>(kDescMaxLines);
    }
    y += kBlockGap;

    if (!owned) {
        h.priceAnchor = UiRect{textX, y, textW, kPriceH};
        y += kPriceH + kPriceToAction;
    }

    if (h.confirm.visible) {
        h.confirm.promptAnchor = UiRect{textX, y, textW, kConfirmPromptH};
        y += kConfirmPromptH + kConfirmGap;
        const float cancelW =
            kConfirmButtonPadX * 2.0f + static_cast<float>(h.confirm.cancelLabel.size()) * kApproxCharW;
        const float confirmW =
            kConfirmButtonPadX * 2.0f + static_cast<float>(h.confirm.confirmLabel.size()) * kApproxCharW;
        h.confirm.cancelButton = UiRect{textX, y, cancelW, kConfirmButtonH};
        h.confirm.confirmButton = UiRect{textX + cancelW + kConfirmButtonGap, y, confirmW, kConfirmButtonH};
        out.focusOrder.push_back(h.confirm.cancelFocusId);
        out.focusOrder.push_back(h.confirm.confirmFocusId);
        y += kConfirmButtonH;
    } else if (h.actionEnabled) {
        const float buttonW =
            kHeroButtonPadX * 2.0f + static_cast<float>(h.actionLabel.size()) * kApproxCharW;
        h.actionButton = UiRect{textX, y, buttonW, kHeroButtonH};
        out.focusOrder.push_back(h.actionFocusId);
        y += kHeroButtonH;
    } else {
        h.statusAnchor = UiRect{textX, y, textW, kHeroStatusH};
        y += kHeroStatusH;
    }

    return std::max(h.art.Bottom(), y);
}

}  // namespace

ShopLayout BuildShopLayout(const ShopModel& model, const ShopLayoutInput& in) {
    const Language lang = in.language;
    const float sy = in.scrollY;

    ShopLayout out;
    out.viewport = UiRect{0.0f, 0.0f, in.viewportW, in.viewportH};
    out.header = BuildSectionHeaderLayout(in.viewportW, kMargin, sy, ProductSection::kShop, lang);
    for (const SectionTab& tab : out.header.tabs) {
        out.focusOrder.push_back(tab.focusId);
    }

    const float contentW = std::max(160.0f, in.viewportW - 2.0f * kMargin);
    const float bodyTop = out.header.bodyTop;

    // --- Shop vacío (catálogo DEV sintético, o estados futuros) ------
    if (model.items.empty()) {
        out.empty = true;
        out.presentation = ShopPresentation::kBrowse;
        out.emptyText = Localized(StringKey::kShopEmpty, lang);
        out.emptyAnchor = UiRect{kMargin, bodyTop + (in.viewportH - bodyTop) * 0.36f, contentW, 24.0f};
        out.contentHeight = std::max(in.viewportH, bodyTop + sy + kMargin);
        return out;
    }

    // ¿Hay un personaje seleccionado y sigue en el Shop?
    const ShopItem* heroItem =
        in.selectedPetId.empty() ? nullptr : model.Find(in.selectedPetId);

    // ============================ BROWSE ============================
    if (heroItem == nullptr) {
        out.presentation = ShopPresentation::kBrowse;
        out.browseHeading = Localized(StringKey::kShopBrowseHeading, lang);

        std::vector<ShopTile> tiles;
        tiles.reserve(model.items.size());
        for (const ShopItem& item : model.items) {
            tiles.push_back(MakeTile(item, lang, /*selected=*/false));
        }

        // Centra [encabezado + rejilla] en el espacio bajo la cabecera de
        // navegación cuando entra holgado (brief §16: "composed rather
        // than merely fit" a 800x560); si no entra, top-align + scroll.
        const BrowseGridMetrics m = ComputeBrowseGrid(static_cast<int>(tiles.size()), contentW);
        const float wholeH = kBrowseHeadingH + kBrowseHeadingGap + m.blockH;
        const float freeSpace = (in.viewportH - kMargin - (bodyTop + sy)) - wholeH;
        const float topPad = freeSpace > 0.0f ? std::min(freeSpace * 0.42f, 96.0f) : 0.0f;

        const float headingTop = bodyTop + topPad;
        out.browseHeadingAnchor = UiRect{kMargin, headingTop, contentW, kBrowseHeadingH};
        const float gridTop = headingTop + kBrowseHeadingH + kBrowseHeadingGap;

        const float gridBottom = LayoutBrowseGrid(tiles, in.viewportW, m, gridTop);
        out.tiles = std::move(tiles);
        for (const ShopTile& t : out.tiles) {
            out.focusOrder.push_back(t.focusId);
        }
        out.contentHeight = std::max(in.viewportH, gridBottom + kMargin + sy);
        return out;
    }

    // =========================== SELECTED ==========================
    out.presentation = ShopPresentation::kSelected;

    const float heroBottom = LayoutHero(out, *heroItem, in, contentW);
    out.dividerRect = UiRect{kMargin, heroBottom + kRailDividerGap, contentW, 1.0f};

    // El rail es la estantería completa (TODOS los pets del Shop), con la
    // tarjeta del hero marcada — así "otro personaje se puede seleccionar
    // sin salir del Shop" (brief §7) y la estantería nunca colapsa a una
    // sola tarjeta con el catálogo real de 2 entradas.
    std::vector<ShopTile> rail;
    rail.reserve(model.items.size());
    for (const ShopItem& item : model.items) {
        rail.push_back(MakeTile(item, lang, /*selected=*/item.petId == heroItem->petId));
    }
    const float railBottom =
        LayoutRail(rail, in.viewportW, contentW, out.dividerRect.Bottom() + kRailTopGap, in.hoverPetId);
    out.rail = std::move(rail);
    for (const ShopTile& t : out.rail) {
        out.focusOrder.push_back(t.focusId);
    }

    out.contentHeight = std::max(in.viewportH, railBottom + kMargin + sy);
    out.shelfBackground = UiRect{0.0f, out.dividerRect.y, in.viewportW,
                                 (out.contentHeight - out.dividerRect.y) + in.viewportH};
    return out;
}

}  // namespace nimvlets::productui
