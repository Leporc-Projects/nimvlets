#include "productui/SettingsView.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdlib>
#include <utility>

#include "productui/SectionHeaderView.h"
#include "productui/UiTheme.h"

namespace nimvlets::productui {

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
    } else {
        return false;
    }
    return true;
}

}  // namespace

void SettingsView::SetPreferences(core::Preferences prefs) {
    prefs_ = prefs;
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
    }
    return c;
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
    }
    return c;
}

SettingsViewResult SettingsView::ActivateWidget(const std::string& focusId) {
    SettingsViewResult r;
    if (focusId.empty()) {
        return r;
    }
    if (StartsWith(focusId, "nav:")) {
        r.switchSection = true;
        r.targetSection = focusId == "nav:shop"      ? ProductSection::kShop
                          : focusId == "nav:settings" ? ProductSection::kSettings
                                                      : ProductSection::kCollection;
        r.dirty = true;
        return r;
    }
    if (StartsWith(focusId, "opt:")) {
        r.hasChange = true;
        r.change = ChangeForSegment(focusId);
        r.dirty = true;
        return r;
    }
    if (StartsWith(focusId, "row:")) {
        // Enter / Espacio sobre una fila: avanza al siguiente valor,
        // cíclico (un teclado-solo alcanza cualquier opción con una
        // tecla).
        PreferenceField field = PreferenceField::kSize;
        if (ParseField(focusId.substr(4), field)) {
            r.hasChange = true;
            r.change = ChangeForStep(field, +1, /*wrap=*/true);
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
    if (StartsWith(hit, "nav:")) {
        focus_.Focus(hit);
    } else if (StartsWith(hit, "opt:")) {
        // El foco de teclado vive en la FILA, no en el segmento.
        const std::size_t first = hit.find(':');
        const std::size_t second = hit.find(':', first + 1);
        focus_.Focus("row:" + hit.substr(first + 1, second - first - 1));
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
            if (onRow) {
                PreferenceField field = PreferenceField::kSize;
                if (ParseField(focused.substr(4), field)) {
                    r.hasChange = true;
                    r.change = ChangeForStep(field, dir, /*wrap=*/false);
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

void SettingsView::Render(UiPainter& painter, TextCache& text, float viewportW, float viewportH) {
    viewportW_ = viewportW;
    viewportH_ = viewportH;

    const SettingsLayout layout = BuildLayout(viewportW, viewportH);
    lastContentHeight_ = layout.contentHeight;
    SyncFocusList(layout);
    const std::string focusedId = keyboardFocus_ ? focus_.FocusedId() : std::string();

    painter.Clear(theme::kBackground);
    DrawSectionHeader(painter, text, layout.header, /*clickBalance=*/0, prefs_.language, hoverId_,
                      focusedId);

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
                const bool hovered = hoverId_ == seg.focusId;
                if (seg.selected) {
                    painter.FillRoundRect(seg.rect, 7.0f, theme::kText);
                } else {
                    if (hovered) {
                        painter.FillRoundRect(seg.rect, 7.0f, theme::kHoverWash);
                    }
                    painter.StrokeRoundRect(seg.rect, 7.0f, 1.25f, theme::kHairline);
                }
                DrawText(painter, text, seg.label, type::kButton,
                         seg.selected ? TextWeight::kSemibold : TextWeight::kMedium,
                         seg.selected ? theme::kBackground : theme::kTextMuted, seg.rect.CenterX(),
                         seg.rect.CenterY() + 4.5f, HAlign::kCenter,
                         static_cast<int>(seg.rect.w - 4.0f));
            }

            if (!row.hint.empty()) {
                DrawText(painter, text, row.hint, type::kGalleryStatus, TextWeight::kRegular,
                         theme::kTextMuted, row.hintAnchor.x, row.hintAnchor.y + 11.0f, HAlign::kLeft,
                         static_cast<int>(row.hintAnchor.w));
            }
        }
    }

    painter.PopClip();
}

}  // namespace nimvlets::productui
