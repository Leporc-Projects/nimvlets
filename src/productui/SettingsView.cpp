#include "productui/SettingsView.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdlib>
#include <utility>

#include "productui/SectionHeaderView.h"
#include "productui/UiTheme.h"

namespace nimvlets::productui {

using core::ClickCountingMode;
using core::Language;
using core::PetSizeChoice;
using core::PreferenceField;
using platform::TextWeight;

namespace {

constexpr float kHeaderClipTop = 106.0f;
constexpr float kWheelStep = 48.0f;

bool StartsWith(const std::string& s, const char* prefix) { return s.rfind(prefix, 0) == 0; }

// "opt:size:large" -> field=kSize, value="large".  "row:opacity" -> just
// the field.  Devuelve false si no matchea ningún campo conocido.
bool ParseField(const std::string& token, PreferenceField& outField) {
    if (token == "size") {
        outField = PreferenceField::kSize;
    } else if (token == "opacity") {
        outField = PreferenceField::kOpacity;
    } else if (token == "lock") {
        outField = PreferenceField::kLockPosition;
    } else if (token == "language") {
        outField = PreferenceField::kLanguage;
    } else if (token == "clickcounting") {
        outField = PreferenceField::kClickCounting;
    } else {
        return false;
    }
    return true;
}

}  // namespace

void SettingsView::DrawNotice(
    UiPainter& painter, TextCache& text, const SettingsNotice& notice,
    const std::string& focusedId) const {
    if (!notice.present) {
        return;
    }

    if (!notice.statusLabel.empty()) {
        DrawText(painter, text, notice.statusLabel, type::kGalleryStatus, TextWeight::kMedium,
                 notice.statusIsAlert ? theme::kText : theme::kTextMuted, notice.statusAnchor.x,
                 notice.statusAnchor.y + 11.0f, HAlign::kLeft,
                 static_cast<int>(notice.statusAnchor.w));
    }

    if (!notice.body.empty()) {
        // Envoltura real por palabras: la capa de layout solo ESTIMÓ
        // cuántas líneas reservar (no puede medir texto), así que se
        // corta a las mismas líneas para que el dibujo no se salga del
        // hueco calculado.
        DrawTextWrapped(painter, text, notice.body, type::kGalleryStatus, TextWeight::kRegular,
                        theme::kTextMuted, notice.bodyAnchor.x, notice.bodyAnchor.y + 11.0f,
                        notice.bodyAnchor.w, 15.0f, std::max(1, notice.bodyLines));
    }

    for (const SettingsNoticeButton& b : notice.buttons) {
        const bool hovered = hoverId_ == b.focusId;
        if (hovered) {
            painter.FillRoundRect(b.rect, 7.0f, theme::kHoverWash);
        }
        painter.StrokeRoundRect(b.rect, 7.0f, 1.25f, theme::kHairline);
        if (focusedId == b.focusId) {
            painter.StrokeRoundRect(
                UiRect{b.rect.x - 4.0f, b.rect.y - 4.0f, b.rect.w + 8.0f, b.rect.h + 8.0f}, 10.0f,
                2.0f, theme::kText);
        }
        DrawText(painter, text, b.label, type::kButton, TextWeight::kMedium, theme::kText,
                 b.rect.CenterX(), b.rect.CenterY() + 4.5f, HAlign::kCenter,
                 static_cast<int>(b.rect.w - 4.0f));
    }
}

void SettingsView::SetPreferences(core::Preferences prefs) {
    prefs_ = prefs;
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    dirty_ = true;
}

void SettingsView::SetGlobalClick(
    const platform::GlobalClickUiState& state, bool explanationVisible) {
    globalClick_ = state;
    globalClickExplanationVisible_ = explanationVisible;
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    dirty_ = true;
}

void SettingsView::SetCompanionRuntime(bool petShown, bool positionResetAvailable) {
    if (petShown == petShown_ && positionResetAvailable == positionResetAvailable_) {
        return;
    }
    petShown_ = petShown;
    positionResetAvailable_ = positionResetAvailable;
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    dirty_ = true;
}

void SettingsView::SetLanguage(Language language) {
    if (language == prefs_.language) {
        return;
    }
    prefs_.language = language;
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    dirty_ = true;
}

void SettingsView::OnEnterSection() {
    hoverId_.clear();
    keyboardFocus_ = false;
    SyncFocusList(BuildLayout(viewportW_, viewportH_));
    focus_.Focus("nav:settings");
    dirty_ = true;
}

SettingsLayout SettingsView::BuildLayout(float w, float h) const {
    SettingsLayoutInput in;
    in.viewportW = w;
    in.viewportH = h;
    in.scrollY = ClampSettingsScroll(scrollY_, lastContentHeight_, h);
    in.prefs = prefs_;
    in.globalClick = globalClick_;
    in.globalClickExplanationVisible = globalClickExplanationVisible_;
    in.petShown = petShown_;
    in.positionResetAvailable = positionResetAvailable_;
    in.clickBalance = clickBalance_;  // cache empujado por ProductWindow en cada Render
    return BuildSettingsLayout(in);
}

void SettingsView::SyncFocusList(const SettingsLayout& layout) {
    focus_.SetItems(layout.focusOrder);
}

SettingsChange SettingsView::ChangeForSegment(const std::string& focusId) const {
    // "opt:<field>:<value>"
    SettingsChange c;
    const std::size_t first = focusId.find(':');
    const std::size_t second = focusId.find(':', first + 1);
    const std::string fieldToken = focusId.substr(first + 1, second - first - 1);
    const std::string value = focusId.substr(second + 1);
    ParseField(fieldToken, c.field);
    switch (c.field) {
        case PreferenceField::kSize:
            c.size = value == "small"  ? PetSizeChoice::kSmall
                     : value == "large" ? PetSizeChoice::kLarge
                                        : PetSizeChoice::kMedium;
            break;
        case PreferenceField::kOpacity:
            c.opacityPercent = std::atoi(value.c_str());
            break;
        case PreferenceField::kLockPosition:
            c.lockPosition = (value == "on");
            break;
        case PreferenceField::kLanguage:
            c.language = (value == "es") ? Language::kEs : Language::kEn;
            break;
        case PreferenceField::kClickCounting:
            c.clickCounting = core::ParseClickCountingMode(value);
            break;
    }
    return c;
}

bool SettingsView::ChangeIsAllowed(const SettingsChange& change) const {
    if (change.field == PreferenceField::kClickCounting &&
        change.clickCounting == ClickCountingMode::kAnywhere) {
        return globalClick_.anywhereSelectable;
    }
    return true;
}

SettingsChange SettingsView::ChangeForStep(PreferenceField field, int dir, bool wrap) const {
    SettingsChange c;
    c.field = field;
    const int step = dir >= 0 ? 1 : -1;
    switch (field) {
        case PreferenceField::kSize:
            c.size = core::StepSize(prefs_.size, step, /*clamp=*/!wrap);
            break;
        case PreferenceField::kOpacity:
            c.opacityPercent = core::StepOpacityPercent(prefs_.opacityPercent, step, /*clamp=*/!wrap);
            break;
        case PreferenceField::kLockPosition: {
            // segmentos [On, Off]; On = índice 0.
            const int idx = prefs_.lockPosition ? 0 : 1;
            const int next = wrap ? (idx + step + 2) % 2 : std::clamp(idx + step, 0, 1);
            c.lockPosition = (next == 0);
            break;
        }
        case PreferenceField::kLanguage: {
            const int idx = prefs_.language == Language::kEn ? 0 : 1;
            const int next = wrap ? (idx + step + 2) % 2 : std::clamp(idx + step, 0, 1);
            c.language = (next == 0) ? Language::kEn : Language::kEs;
            break;
        }
        case PreferenceField::kClickCounting: {
            // segmentos [Nimvlet only, Anywhere]; Nimvlet only = índice 0.
            const int idx = prefs_.clickCounting == ClickCountingMode::kNimvletOnly ? 0 : 1;
            const int next = wrap ? (idx + step + 2) % 2 : std::clamp(idx + step, 0, 1);
            c.clickCounting = (next == 0) ? ClickCountingMode::kNimvletOnly : ClickCountingMode::kAnywhere;
            break;
        }
    }
    return c;
}

SettingsViewResult SettingsView::ActivateWidget(const std::string& focusId) {
    SettingsViewResult r;
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
    if (const GlobalClickAction action = ParseGlobalClickAction(focusId);
        action != GlobalClickAction::kNone) {
        // Los botones del flujo de permiso (Block 11A). NO son un cambio
        // de preferencia: src/app decide qué hacer con cada uno —
        // "Continue" es el único camino que puede pedir el permiso
        // nativo (brief §8).
        r.hasGlobalClickAction = true;
        r.globalClickAction = action;
        r.dirty = true;
        return r;
    }
    if (const SettingsCommand cmd = ParseSettingsCommand(focusId);
        cmd != SettingsCommand::kNone) {
        // Click de mouse directo sobre un segmento transitorio de
        // Companion (Block 11B): "Shown" / "Hidden" / "Reset position".
        // NO muta ninguna preferencia — src/app lo enruta a la ruta
        // canónica. El layout ya sacó "Reset position" apagado del
        // hit-test, así que un `cmd:resetpos` acá siempre es válido.
        r.hasCommand = true;
        r.command = cmd;
        r.dirty = true;
        return r;
    }
    if (StartsWith(focusId, "opt:")) {
        const SettingsChange change = ChangeForSegment(focusId);
        if (ChangeIsAllowed(change)) {
            r.hasChange = true;
            r.change = change;
        }
        r.dirty = true;
        return r;
    }
    if (focusId == "row:visibility") {
        // Enter / Espacio sobre la fila Visibility: alterna Shown <-> Hidden.
        r.hasCommand = true;
        r.command = petShown_ ? SettingsCommand::kHidePet : SettingsCommand::kShowPet;
        r.dirty = true;
        return r;
    }
    if (focusId == "row:position") {
        // Enter / Espacio sobre la fila Position: "Reset position". Solo
        // llega acá si la fila está en el anillo de foco, y el layout la
        // saca cuando el backend no puede colocar la ventana (Wayland).
        r.hasCommand = true;
        r.command = SettingsCommand::kResetPosition;
        r.dirty = true;
        return r;
    }
    if (StartsWith(focusId, "row:")) {
        // Enter / Espacio sobre una fila: avanza al siguiente valor,
        // cíclico (un teclado-solo alcanza cualquier opción con una
        // tecla).
        PreferenceField field = PreferenceField::kSize;
        if (ParseField(focusId.substr(4), field)) {
            const SettingsChange change = ChangeForStep(field, +1, /*wrap=*/true);
            if (ChangeIsAllowed(change)) {
                r.hasChange = true;
                r.change = change;
            }
            r.dirty = true;
        }
        return r;
    }
    return r;
}

SettingsViewResult SettingsView::OnMouseMove(float x, float y) {
    SettingsViewResult r;
    const SettingsLayout layout = BuildLayout(viewportW_, viewportH_);
    const std::string hit = layout.HitTest(x, y);
    if (hit != hoverId_) {
        hoverId_ = hit;
        dirty_ = true;
        r.dirty = true;
    }
    return r;
}

SettingsViewResult SettingsView::OnMouseDown(float x, float y) {
    keyboardFocus_ = false;
    const SettingsLayout layout = BuildLayout(viewportW_, viewportH_);
    const std::string hit = layout.HitTest(x, y);
    if (hit.empty()) {
        dirty_ = true;  // por si había chrome de foco visible que ahora se apaga
        return SettingsViewResult{};
    }
    if (StartsWith(hit, "nav:") || StartsWith(hit, "gc:")) {
        focus_.Focus(hit);
    } else if (StartsWith(hit, "opt:")) {
        // El foco de teclado vive en la FILA, no en el segmento.
        const std::size_t first = hit.find(':');
        const std::size_t second = hit.find(':', first + 1);
        focus_.Focus("row:" + hit.substr(first + 1, second - first - 1));
    } else if (StartsWith(hit, "cmd:")) {
        // Igual que "opt:": el foco vive en la fila transitoria, no en su
        // segmento (Block 11B).
        focus_.Focus(hit == "cmd:resetpos" ? "row:position" : "row:visibility");
    }
    SettingsViewResult r = ActivateWidget(hit);
    r.dirty = true;
    dirty_ = true;
    return r;
}

SettingsViewResult SettingsView::OnWheel(float dyLines) {
    SettingsViewResult r;
    const float before = scrollY_;
    scrollY_ = ClampSettingsScroll(scrollY_ - dyLines * kWheelStep, lastContentHeight_, viewportH_);
    if (scrollY_ != before) {
        dirty_ = true;
        r.dirty = true;
    }
    return r;
}

SettingsViewResult SettingsView::OnKey(int sdlKeycode, bool shiftHeld) {
    SettingsViewResult r;
    const std::string focused = focus_.FocusedId();
    const bool onRow = StartsWith(focused, "row:");

    switch (sdlKeycode) {
        case SDLK_TAB:
            if (shiftHeld) {
                focus_.Prev();
            } else {
                focus_.Next();
            }
            keyboardFocus_ = true;
            dirty_ = true;
            r.dirty = true;
            return r;
        case SDLK_DOWN:
            focus_.Next();
            keyboardFocus_ = true;
            dirty_ = true;
            r.dirty = true;
            return r;
        case SDLK_UP:
            focus_.Prev();
            keyboardFocus_ = true;
            dirty_ = true;
            r.dirty = true;
            return r;
        case SDLK_RIGHT:
        case SDLK_LEFT: {
            keyboardFocus_ = true;
            const int dir = sdlKeycode == SDLK_RIGHT ? +1 : -1;
            if (focused == "row:visibility") {
                // Segmentos [Shown, Hidden]. ← va hacia "Shown", → hacia
                // "Hidden"; en el extremo no hace nada (clamp, igual que
                // las filas de preferencia).
                if (dir > 0 && petShown_) {
                    r.hasCommand = true;
                    r.command = SettingsCommand::kHidePet;
                } else if (dir < 0 && !petShown_) {
                    r.hasCommand = true;
                    r.command = SettingsCommand::kShowPet;
                }
                dirty_ = true;
                r.dirty = true;
                return r;
            }
            if (focused == "row:position") {
                // Un solo botón: las flechas no cambian ningún valor
                // (Enter/Espacio lo activan). Igual se marca dirty por si
                // el chrome de foco de teclado se acaba de encender.
                dirty_ = true;
                r.dirty = true;
                return r;
            }
            if (onRow) {
                PreferenceField field = PreferenceField::kSize;
                if (ParseField(focused.substr(4), field)) {
                    const SettingsChange change = ChangeForStep(field, dir, /*wrap=*/false);
                    // Una opción no elegible en este sistema ("Anywhere"
                    // sin capacidad) se comporta como el extremo del
                    // control: la flecha no hace nada.
                    if (ChangeIsAllowed(change)) {
                        r.hasChange = true;
                        r.change = change;
                    }
                }
            } else {
                // Sobre una pestaña de nav: ← → recorren el foco (igual
                // que Collection / Shop).
                if (dir > 0) {
                    focus_.Next();
                } else {
                    focus_.Prev();
                }
            }
            dirty_ = true;
            r.dirty = true;
            return r;
        }
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_SPACE:
            keyboardFocus_ = true;
            return ActivateWidget(focus_.FocusedId());
        case SDLK_ESCAPE:
            if (globalClickExplanationVisible_) {
                // La explicación es un paso modal chico: Esc la descarta
                // (equivale a "Not now") antes de poder cerrar la
                // ventana — mismo patrón que Esc saliendo del submodo
                // del Starter Shop antes de cerrar.
                r.hasGlobalClickAction = true;
                r.globalClickAction = GlobalClickAction::kNotNow;
                r.dirty = true;
                return r;
            }
            r.requestClose = true;
            return r;
        default:
            return r;
    }
}

SettingsViewResult SettingsView::OnViewportChanged() {
    SettingsViewResult r;
    dirty_ = true;
    r.dirty = true;
    return r;
}

void SettingsView::Render(
    UiPainter& painter, TextCache& text, float viewportW, float viewportH, std::uint64_t clickBalance) {
    viewportW_ = viewportW;
    viewportH_ = viewportH;
    // Balance CANÓNICO de ProductWindow (corrección de QA del owner,
    // Block 10): antes Settings pasaba `0` hard-codeado y su cabecera
    // mostraba "0 clicks" mientras Collection / Shop mostraban el real.
    clickBalance_ = clickBalance;

    const SettingsLayout layout = BuildLayout(viewportW, viewportH);
    lastContentHeight_ = layout.contentHeight;
    SyncFocusList(layout);
    const std::string focusedId = keyboardFocus_ ? focus_.FocusedId() : std::string();

    painter.Clear(theme::kBackground);
    DrawSectionHeader(painter, text, layout.header, hoverId_, focusedId);

    painter.PushClip(UiRect{0.0f, kHeaderClipTop, viewportW, std::max(0.0f, viewportH - kHeaderClipTop)});

    for (const SettingsGroup& g : layout.groups) {
        DrawText(painter, text, g.title, type::kSectionSub, TextWeight::kMedium, theme::kTextMuted,
                 g.titleAnchor.x, g.titleAnchor.y + 11.0f, HAlign::kLeft);
        painter.FillRect(g.rule, theme::kHairline);

        for (const SettingsRow& row : g.rows) {
            if (!row.label.empty()) {
                DrawText(painter, text, row.label, type::kHeroSpecies, TextWeight::kRegular,
                         theme::kText, row.labelAnchor.x, row.labelAnchor.CenterY() + 4.5f,
                         HAlign::kLeft);
            }

            if (focusedId == row.focusId) {
                painter.StrokeRoundRect(row.focusRect, 10.0f, 2.0f, theme::kText);
            }

            for (const SettingsSegment& seg : row.segments) {
                const bool hovered = seg.enabled && hoverId_ == seg.focusId;
                if (seg.selected) {
                    painter.FillRoundRect(seg.rect, 7.0f, theme::kText);
                } else {
                    if (hovered) {
                        painter.FillRoundRect(seg.rect, 7.0f, theme::kHoverWash);
                    }
                    painter.StrokeRoundRect(seg.rect, 7.0f, 1.25f, theme::kHairline);
                }
                // Un segmento apagado ("Anywhere" sin capacidad de
                // plataforma) se dibuja con la tinta más tenue del tema:
                // se ve que la opción EXISTE, y la línea de estado de
                // abajo explica por qué no se puede elegir acá.
                const UiColor segInk = seg.selected      ? theme::kBackground
                                       : seg.enabled     ? theme::kTextMuted
                                                         : theme::kHairline;
                DrawText(painter, text, seg.label, type::kButton,
                         seg.selected ? TextWeight::kSemibold : TextWeight::kMedium, segInk,
                         seg.rect.CenterX(), seg.rect.CenterY() + 4.5f, HAlign::kCenter,
                         static_cast<int>(seg.rect.w - 4.0f));
            }

            if (!row.hint.empty()) {
                DrawText(painter, text, row.hint, type::kGalleryStatus, TextWeight::kRegular,
                         theme::kTextMuted, row.hintAnchor.x, row.hintAnchor.y + 11.0f, HAlign::kLeft,
                         static_cast<int>(row.hintAnchor.w));
            }

            DrawNotice(painter, text, row.notice, focusedId);
        }
    }

    painter.PopClip();
}

}  // namespace nimvlets::productui
