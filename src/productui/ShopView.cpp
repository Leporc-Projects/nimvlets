#include "productui/ShopView.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <utility>

#include "productui/Format.h"
#include "productui/SectionHeaderView.h"
#include "productui/ShopPaint.h"
#include "productui/UiTheme.h"

namespace nimvlets::productui {

using catalog::ShopItemStatus;
using platform::TextWeight;

namespace {

constexpr float kHeaderClipTop = 106.0f;
constexpr float kWheelStep = 48.0f;

constexpr unsigned char kPedestalAlpha = 52;
constexpr unsigned char kRailPedestalAlpha = 44;

bool StartsWith(const std::string& s, const char* prefix) { return s.rfind(prefix, 0) == 0; }

std::string PetIdFromShopFocusId(const std::string& focusId) {
    return StartsWith(focusId, "shopitem:") ? focusId.substr(9) : std::string();
}

}  // namespace

void ShopView::SetModel(catalog::ShopModel model, std::uint64_t clickBalance) {
    model_ = std::move(model);
    clickBalance_ = clickBalance;

    if (!selectedPetId_.empty() && model_.Find(selectedPetId_) == nullptr) {
        selectedPetId_.clear();
    }
    // Una confirmación abierta se cierra si su pet ya NO es asequible
    // (típicamente porque la compra tuvo éxito y ahora está poseído), o
    // si de algún modo ya no hay un personaje seleccionado.
    if (confirming_) {
        const ShopLayout layout = BuildLayout(viewportW_, viewportH_);
        if (layout.presentation != ShopPresentation::kSelected ||
            layout.hero.status != ShopItemStatus::kAffordable) {
            confirming_ = false;
        }
    }
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    dirty_ = true;
}

void ShopView::SetLanguage(core::Language language) {
    if (language == language_) {
        return;
    }
    language_ = language;
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    dirty_ = true;
}

void ShopView::SetStarterAffordanceVisible(bool visible) {
    if (visible == starterAffordanceVisible_) {
        return;
    }
    starterAffordanceVisible_ = visible;
    // La afordancia entra / sale del anillo de foco ("starter:enter"),
    // así que hay que re-sincronizar la FocusList.
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    dirty_ = true;
}

void ShopView::OnEnterSection() {
    // Entrar a la sección arranca en modo BROWSE (sin selección), con el
    // foco en la pestaña "Shop" y sin confirmación ni hover — un punto de
    // partida predecible (brief §13/§17). Ninguna selección del Shop se
    // recuerda entre visitas.
    selectedPetId_.clear();
    confirming_ = false;
    hoverId_.clear();
    keyboardFocus_ = false;
    scrollY_ = 0.0f;
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    focus_.Focus("nav:shop");
    dirty_ = true;
}

ShopLayout ShopView::BuildLayout(float w, float h) const {
    ShopLayoutInput in;
    in.viewportW = w;
    in.viewportH = h;
    in.scrollY = ClampShopScroll(scrollY_, lastContentHeight_, h);
    in.language = language_;
    in.selectedPetId = selectedPetId_;
    in.hoverPetId = HoverPetId();
    in.confirming = confirming_;
    in.starterAffordanceVisible = starterAffordanceVisible_;
    return BuildShopLayout(model_, in);
}

void ShopView::SyncFocusList(const ShopLayout& layout) {
    focus_.SetItems(layout.focusOrder);
}

std::string ShopView::HoverPetId() const {
    return StartsWith(hoverId_, "shopitem:") ? hoverId_.substr(9) : std::string();
}

namespace {

// Afordancia quieta "Starter choices…" cerca del pie del Shop público
// (Block 10, brief §10). UNA línea de texto en el tono más tenue, sin
// caja, sin flecha, sin badge — solo un subrayado corto de foco de
// teclado. No se dibuja si el layout no la trae (0 ofertas de starter).
void DrawStarterAffordance(
    UiPainter& painter, TextCache& text, const ShopLayout& layout, bool focused) {
    if (!layout.starterAffordanceVisible) {
        return;
    }
    const UiRect& a = layout.starterAffordanceAnchor;
    DrawText(painter, text, layout.starterAffordanceText, type::kGalleryStatus, TextWeight::kRegular,
             theme::kTextFaint, a.CenterX(), a.y + 11.0f, HAlign::kCenter, static_cast<int>(a.w));
    if (focused) {
        const float uw = 108.0f;
        painter.FillRect(UiRect{a.CenterX() - uw * 0.5f, a.y + 15.0f, uw, 1.0f}, theme::kTextFaint);
    }
}

}  // namespace

void ShopView::SelectHero(const std::string& petId) {
    // Un click / Enter en una tarjeta SELECCIONA (browse -> selected), o
    // cambia de personaje seleccionado — NUNCA compra, NUNCA muta el
    // wallet o la propiedad (brief §7/§8). Lo único que cambia es qué
    // personaje es el hero.
    if (model_.Find(petId) == nullptr || petId == selectedPetId_) {
        return;
    }
    selectedPetId_ = petId;
    confirming_ = false;  // cambiar de hero descarta cualquier confirmación
    scrollY_ = 0.0f;
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    dirty_ = true;
}

void ShopView::SelectHeroForQA(const std::string& petId) {
    SelectHero(petId);
}

ShopViewResult ShopView::ActivateWidget(const std::string& focusId) {
    ShopViewResult r;
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
    if (StartsWith(focusId, "shopitem:")) {
        // Selecciona el personaje. El foco se queda en esta MISMA tarjeta
        // (ahora en el rail) — la selección nunca salta el foco a la
        // confirmación de compra (brief §12).
        SelectHero(PetIdFromShopFocusId(focusId));
        r.dirty = true;
        return r;
    }
    if (focusId == "starter:enter") {
        // Afordancia quieta "Starter choices…": src/app entra al submodo
        // del Starter Shop oculto (Block 10, brief §11). El Shop público
        // no cambia de estado acá.
        r.enterStarterSubmode = true;
        r.dirty = true;
        return r;
    }
    if (focusId == "get") {
        // Primer paso deliberado: abre la confirmación inline. NO compra.
        confirming_ = true;
        SyncFocusList(BuildLayout(viewportW_, viewportH_));
        focus_.Focus("purchase:cancel");  // el foco arranca en Cancelar (no gastar por accidente)
        r.dirty = true;
        return r;
    }
    if (focusId == "purchase:cancel") {
        confirming_ = false;
        SyncFocusList(BuildLayout(viewportW_, viewportH_));
        focus_.Focus("get");
        r.dirty = true;
        return r;
    }
    if (focusId == "purchase:confirm") {
        // Segundo paso deliberado: compra de verdad. El objetivo es la
        // identidad de catálogo del ítem (entitlementTarget), NO una
        // suposición sobre petId.
        const ShopLayout layout = BuildLayout(viewportW_, viewportH_);
        if (layout.hero.confirm.visible && !layout.hero.petId.empty()) {
            r.hasPurchase = true;
            r.purchase.petId = layout.hero.entitlementTarget.petId;
            r.purchase.variantId = layout.hero.entitlementTarget.variantId;
        }
        confirming_ = false;  // src/app re-empuja el modelo con el estado final
        r.dirty = true;
        return r;
    }
    return r;
}

ShopViewResult ShopView::OnMouseMove(float x, float y) {
    ShopViewResult r;
    const ShopLayout layout = BuildLayout(viewportW_, viewportH_);
    const std::string hit = layout.HitTest(x, y);
    // Redibuja SOLO si el objetivo de hover cambió de verdad (brief §15):
    // el Shop en reposo es efectivamente event-driven.
    if (hit != hoverId_) {
        hoverId_ = hit;
        dirty_ = true;
        r.dirty = true;
    }
    return r;
}

ShopViewResult ShopView::OnMouseDown(float x, float y) {
    keyboardFocus_ = false;
    const ShopLayout layout = BuildLayout(viewportW_, viewportH_);
    const std::string hit = layout.HitTest(x, y);
    if (hit.empty()) {
        dirty_ = true;
        return ShopViewResult{};
    }
    if (StartsWith(hit, "shopitem:") || StartsWith(hit, "nav:")) {
        focus_.Focus(hit);
    }
    ShopViewResult r = ActivateWidget(hit);
    r.dirty = true;
    dirty_ = true;
    return r;
}

ShopViewResult ShopView::OnWheel(float dyLines) {
    ShopViewResult r;
    const float before = scrollY_;
    scrollY_ = ClampShopScroll(scrollY_ - dyLines * kWheelStep, lastContentHeight_, viewportH_);
    if (scrollY_ != before) {
        dirty_ = true;
        r.dirty = true;
    }
    return r;
}

ShopViewResult ShopView::OnKey(int sdlKeycode, bool shiftHeld) {
    ShopViewResult r;
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
            if (confirming_) {
                // Esc cancela la confirmación, no cierra la ventana
                // (semántica preservada de Block 07, brief §12).
                confirming_ = false;
                SyncFocusList(BuildLayout(viewportW_, viewportH_));
                focus_.Focus("get");
                keyboardFocus_ = true;
                dirty_ = true;
                r.dirty = true;
                return r;
            }
            r.requestClose = true;
            return r;
        default:
            return r;
    }
}

ShopViewResult ShopView::OnViewportChanged() {
    ShopViewResult r;
    dirty_ = true;
    r.dirty = true;
    return r;
}

void ShopView::Render(
    UiPainter& painter, TextCache& text, PetPreviewCache& previews, float viewportW, float viewportH) {
    viewportW_ = viewportW;
    viewportH_ = viewportH;

    const ShopLayout layout = BuildLayout(viewportW, viewportH);
    lastContentHeight_ = layout.contentHeight;
    SyncFocusList(layout);
    const std::string focusedId = keyboardFocus_ ? focus_.FocusedId() : std::string();

    painter.Clear(theme::kBackground);
    DrawSectionHeader(painter, text, layout.header, clickBalance_, language_, hoverId_, focusedId);

    painter.PushClip(UiRect{0.0f, kHeaderClipTop, viewportW, std::max(0.0f, viewportH - kHeaderClipTop)});

    // --- Shop vacío ------------------------------------------------
    if (layout.empty) {
        DrawText(painter, text, layout.emptyText, type::kSectionTitle, TextWeight::kRegular,
                 theme::kTextFaint, layout.emptyAnchor.CenterX(), layout.emptyAnchor.y + 16.0f,
                 HAlign::kCenter, static_cast<int>(layout.emptyAnchor.w));
        DrawStarterAffordance(painter, text, layout, focusedId == "starter:enter");
        painter.PopClip();
        return;
    }

    // --- BROWSE: la estantería de personajes es todo el contenido ---
    if (layout.presentation == ShopPresentation::kBrowse) {
        DrawText(painter, text, layout.browseHeading, type::kSectionSub, TextWeight::kRegular,
                 theme::kTextMuted, layout.browseHeadingAnchor.CenterX(),
                 layout.browseHeadingAnchor.y + 13.0f, HAlign::kCenter,
                 static_cast<int>(layout.browseHeadingAnchor.w));

        for (const ShopTile& t : layout.tiles) {
            const bool hovered = hoverId_ == t.focusId;
            const bool focused = focusedId == t.focusId;
            // El hover / foco revela la info liviana (precio o propiedad);
            // sin hover, la tarjeta es solo arte + nombre (brief §5/§6).
            DrawShopTile(painter, text, previews, t, type::kGalleryName, kPedestalAlpha, hovered,
                         focused, /*revealVisible=*/hovered || focused, /*selectedMark=*/false);
        }
        DrawStarterAffordance(painter, text, layout, focusedId == "starter:enter");
        painter.PopClip();
        return;
    }

    // --- SELECTED: hero grande + rail compacto de la estantería ----
    painter.FillRect(layout.shelfBackground, theme::kGalleryShelf);

    DrawShopHero(painter, text, previews, layout.hero, focusedId);

    painter.FillRect(layout.dividerRect, theme::kHairline);

    for (const ShopTile& t : layout.rail) {
        const bool hovered = hoverId_ == t.focusId;
        const bool focused = focusedId == t.focusId;
        DrawShopTile(painter, text, previews, t, type::kGalleryStatus, kRailPedestalAlpha, hovered,
                     focused, /*revealVisible=*/false, /*selectedMark=*/t.selected);
    }

    DrawStarterAffordance(painter, text, layout, focusedId == "starter:enter");
    painter.PopClip();
}

}  // namespace nimvlets::productui
