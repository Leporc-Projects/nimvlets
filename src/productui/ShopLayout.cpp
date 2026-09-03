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

// --- Rejilla de browse (brief 09C §4/§5) ----------------------------
// El arte manda; la rejilla se centra en el espacio disponible y
// envuelve limpio para 1..8 personajes sin scroll horizontal. El alto de
// la línea revelada SIEMPRE se reserva -> el hover no reordena nada
// (redibujo event-driven, sin reflow).
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
constexpr float kHeroNameH = 34.0f;
constexpr float kHeroRuleGap = 14.0f;   // nombre -> divisor 1
constexpr float kHeroRuleH = 2.0f;
constexpr float kSpeciesGap = 12.0f;
constexpr float kSpeciesH = 16.0f;
constexpr float kDescGap = 10.0f;
constexpr float kDescLineH = 17.0f;
constexpr int kDescMaxLines = 3;
// Espacio "identidad -> economía": divisor 2 con aire a ambos lados
// (solo si hay descripción; si no, un gap chico).
constexpr float kPreDivider2 = 16.0f;
constexpr float kDivider2H = 10.0f;
constexpr float kPostDivider2 = 16.0f;
constexpr float kBlockGapNoDesc = 14.0f;
constexpr float kPriceH = 22.0f;
constexpr float kPriceToAction = 12.0f;
constexpr float kHeroButtonH = 42.0f;   // pill más generosa (referencia CTA)
constexpr float kHeroButtonPadX = 30.0f;
constexpr float kHeroButtonSparkRoom = 18.0f;  // spark a la derecha de la CTA
constexpr float kHeroStatusH = 18.0f;
// Panel enmarcado del hero: aire alrededor de todo el bloque.
constexpr float kHeroPanelPad = 16.0f;

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

// --- Hotspot INVISIBLE del Shop oculto de starters (Block 10) --------
// Región cuadrada anclada a la esquina INFERIOR DERECHA del viewport del
// Shop (coords de viewport, sin scroll — es un elemento de "chrome
// secreto", no contenido que scrollea). No se dibuja. Ver ShopLayout.h.
constexpr float kStarterHotspotSize = 48.0f;

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
// 7/8 a dos filas de 4. Nunca scroll horizontal.
int BrowseColumns(int n) {
    if (n <= 4) return std::max(1, n);
    if (n <= 6) return 3;
    return 4;
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
    t.accentSecondary = accent.secondary;
    t.emphasis = accent.emphasis;
    t.selected = selected;
    t.focusId = "shopitem:" + item.petId;
    return t;
}

// Ancla el hotspot INVISIBLE a la esquina inf-der del viewport (Block
// 10, corrección de QA del owner). NO dibuja, NO toca `focusOrder`, NO
// toca `contentHeight`. Solo existe si `in.starterHotspotArmed` (>= 1
// oferta legítima del Starter Shop oculto). Recalculado en cada
// BuildShopLayout, así el resize lo reubica.
void ArmStarterHotspot(ShopLayout& out, const ShopLayoutInput& in) {
    if (!in.starterHotspotArmed) {
        return;
    }
    out.starterHotspotArmed = true;
    out.starterHotspotRect = UiRect{
        in.viewportW - kStarterHotspotSize, in.viewportH - kStarterHotspotSize,
        kStarterHotspotSize, kStarterHotspotSize};
}

}  // namespace

// --- Helpers compartidos (declarados en ShopLayout.h) ----------------

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

float LayoutShopRail(
    std::vector<ShopTile>& rail, float viewportW, float contentW, float railTop,
    const std::string& hoverFocusId) {
    const int n = static_cast<int>(rail.size());
    if (n == 0) {
        return railTop;
    }
    const float fitW = (contentW - static_cast<float>(n - 1) * kRailGap) / static_cast<float>(n);
    const float tileW = std::clamp(fitW, kRailTileMinW, kRailTileMaxW);
    const int perRow =
        std::max(1, static_cast<int>(std::floor((contentW + kRailGap) / (tileW + kRailGap))));
    const float artSize = std::min(kRailArtMax, tileW - 12.0f);
    const float tileH = kRailPadTop + artSize + kRailArtToName + kRailNameH + kRailPadBottom;
    const int rows = (n + perRow - 1) / perRow;

    for (int i = 0; i < n; ++i) {
        const int row = i / perRow;
        const int col = i % perRow;
        const int inRow = std::min(perRow, n - row * perRow);
        const float rowW = static_cast<float>(inRow) * tileW + static_cast<float>(inRow - 1) * kRailGap;
        const float rowLeft = (viewportW - rowW) * 0.5f;
        const float lift = (rail[static_cast<std::size_t>(i)].focusId == hoverFocusId) ? kHoverLift : 0.0f;
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

float LayoutShopHero(
    ShopHero& h, std::vector<std::string>& focusOrder, const ShopHeroContent& c,
    float headerBodyTop, float contentW, Language lang, bool confirming) {
    h.petId = c.petId;
    h.variantId = c.variantId;
    h.displayName = c.displayName;
    h.speciesText = c.speciesText;
    h.descriptionText = c.descriptionText;
    h.status = c.status;
    h.entitlementTarget = c.entitlementTarget;
    h.accent = PetAccentFor(c.petId);
    h.priceClicks = c.priceClicks;
    h.priceText = FormatClickCount(c.priceClicks, lang);

    const float heroTop = headerBodyTop;
    h.art = UiRect{kMargin, heroTop, kHeroArt, kHeroArt};

    const float textX = h.art.Right() + kHeroTextGap;
    const float textW = std::max(160.0f, kMargin + contentW - textX);

    const bool owned = c.status == ShopItemStatus::kOwned;
    const bool affordable = c.status == ShopItemStatus::kAffordable;
    const bool confirmingNow = confirming && affordable;

    // Estado / acción / confirmación — mutuamente excluyentes.
    if (owned) {
        h.showStatusLine = true;
        h.statusText = Localized(StringKey::kInYourCollection, lang);
    } else if (confirmingNow) {
        h.confirm.visible = true;
        h.confirm.prompt = FormatSpendPrompt(c.priceClicks, c.displayName, lang);
        h.confirm.cancelLabel = Localized(StringKey::kCancel, lang);
        h.confirm.cancelFocusId = "purchase:cancel";
        h.confirm.confirmLabel = Localized(StringKey::kConfirm, lang);
        h.confirm.confirmFocusId = "purchase:confirm";
    } else if (affordable) {
        h.actionEnabled = true;
        h.actionLabel = std::string(Localized(StringKey::kGetPetPrefix, lang)) + c.displayName;
        h.actionFocusId = "get";
    } else {  // kInsufficientBalance
        h.showStatusLine = true;
        h.statusText = FormatNeedMoreClicks(c.clicksShort, lang);
    }

    const bool hasSpecies = !h.speciesText.empty();
    const bool hasDesc = !h.descriptionText.empty();
    const float blockGap2 = hasDesc ? (kPreDivider2 + kDivider2H + kPostDivider2) : kBlockGapNoDesc;

    float blockH = kHeroNameH + kHeroRuleGap + kHeroRuleH;
    if (hasSpecies) {
        blockH += kSpeciesGap + kSpeciesH;
    }
    if (hasDesc) {
        blockH += kDescGap + kDescLineH * static_cast<float>(kDescMaxLines);
    }
    blockH += blockGap2;
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
    h.stagePrimary =
        UiRect{h.art.x - 40.0f, h.art.y - 10.0f, stageRight - (h.art.x - 40.0f), h.art.h + 30.0f};
    h.stageSecondary =
        UiRect{h.art.x + h.art.w * 0.34f, h.art.y + h.art.h * 0.42f, h.art.w * 0.84f, h.art.h * 0.70f};

    float y = blockTop;
    h.nameAnchor = UiRect{textX, y, textW, kHeroNameH};
    y += kHeroNameH + kHeroRuleGap;
    // Divisor 1: ancho de la columna (rombo central = acento del pet).
    h.nameRule = UiRect{textX, y, textW, kHeroRuleH};
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
    // Divisor 2: identidad/descripción -> precio/acción (rombo neutro).
    if (hasDesc) {
        y += kPreDivider2;
        h.detailDividerRect = UiRect{textX, y, textW, kDivider2H};
        y += kDivider2H + kPostDivider2;
    } else {
        y += kBlockGapNoDesc;
    }

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
        h.confirm.confirmButton =
            UiRect{textX + cancelW + kConfirmButtonGap, y, confirmW, kConfirmButtonH};
        focusOrder.push_back(h.confirm.cancelFocusId);
        focusOrder.push_back(h.confirm.confirmFocusId);
        y += kConfirmButtonH;
    } else if (h.actionEnabled) {
        const float buttonW = kHeroButtonPadX * 2.0f +
                              static_cast<float>(h.actionLabel.size()) * kApproxCharW +
                              kHeroButtonSparkRoom;
        h.actionButton = UiRect{textX, y, buttonW, kHeroButtonH};
        focusOrder.push_back(h.actionFocusId);
        y += kHeroButtonH;
    } else {
        h.statusAnchor = UiRect{textX, y, textW, kHeroStatusH};
        y += kHeroStatusH;
    }

    const float heroBottom = std::max(h.art.Bottom(), y);
    // Panel enmarcado suave alrededor de TODO el hero (arte + detalle).
    // El borde SUPERIOR se mantiene por debajo del clip de la vista
    // (kHeaderClipTop): un pad chico arriba, generoso abajo/lados.
    constexpr float kHeroPanelTopPad = 4.0f;
    h.heroPanel = UiRect{kMargin - 6.0f, heroTop - kHeroPanelTopPad, contentW + 12.0f,
                         (heroBottom - heroTop) + kHeroPanelTopPad + kHeroPanelPad};
    return heroBottom;
}

// --- Miembros de ShopLayout -----------------------------------------

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
    // NB: el hotspot invisible del Starter Shop NO se resuelve acá — es
    // una consulta aparte (HitStarterHotspot), sin focusId.
    return "";
}

float ClampShopScroll(float scrollY, float contentHeight, float viewportH) {
    return std::clamp(scrollY, 0.0f, std::max(0.0f, contentHeight - viewportH));
}

ShopLayout BuildShopLayout(const ShopModel& model, const ShopLayoutInput& in) {
    const Language lang = in.language;
    const float sy = in.scrollY;

    ShopLayout out;
    out.viewport = UiRect{0.0f, 0.0f, in.viewportW, in.viewportH};
    out.header = BuildSectionHeaderLayout(
        in.viewportW, kMargin, sy, ProductSection::kShop, lang, in.clickBalance);
    for (const SectionTab& tab : out.header.tabs) {
        out.focusOrder.push_back(tab.focusId);
    }
    // El hotspot invisible se ancla igual en las 3 presentaciones
    // (browse / selected / vacío) — es chrome de esquina, no contenido.
    ArmStarterHotspot(out, in);

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
    const ShopItem* heroItem = in.selectedPetId.empty() ? nullptr : model.Find(in.selectedPetId);

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
        // navegación cuando entra holgado; si no entra, top-align + scroll.
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

    ShopHeroContent hc;
    hc.petId = heroItem->petId;
    hc.displayName = heroItem->displayName;
    hc.speciesText = Species(heroItem->petId, lang);
    hc.descriptionText = ShortDescription(heroItem->petId, lang);
    hc.status = heroItem->status;
    hc.entitlementTarget = heroItem->entitlementTarget;
    hc.priceClicks = heroItem->priceClicks;
    hc.clicksShort = heroItem->clicksShort;
    const float heroBottom =
        LayoutShopHero(out.hero, out.focusOrder, hc, out.header.bodyTop, contentW, lang, in.confirming);
    out.dividerRect = UiRect{kMargin, heroBottom + kRailDividerGap, contentW, 1.0f};

    // El rail es la estantería completa (TODOS los pets del Shop), con la
    // tarjeta del hero marcada.
    std::vector<ShopTile> rail;
    rail.reserve(model.items.size());
    for (const ShopItem& item : model.items) {
        rail.push_back(MakeTile(item, lang, /*selected=*/item.petId == heroItem->petId));
    }
    const std::string hoverFocusId = in.hoverPetId.empty() ? std::string() : ("shopitem:" + in.hoverPetId);
    const float railBottom = LayoutShopRail(
        rail, in.viewportW, contentW, out.dividerRect.Bottom() + kRailTopGap, hoverFocusId);
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
