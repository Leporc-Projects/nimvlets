#include "productui/SettingsLayout.h"

#include <algorithm>
#include <array>
#include <string>

#include "core/DisplayControls.h"
#include "core/Localization.h"

namespace nimvlets::productui {

using core::Language;
using core::Localized;
using core::PetSizeChoice;
using core::PreferenceField;
using core::StringKey;

namespace {

// Métricas en PUNTOS lógicos. Composición compacta: label a la
// izquierda, control segmentado a la derecha; grupos separados por aire
// y una regla sutil (brief §10).
constexpr float kMargin = 40.0f;

constexpr float kGroupTitleH = 15.0f;
constexpr float kGroupTitleToRule = 8.0f;
constexpr float kRuleToRows = 16.0f;
constexpr float kGroupGap = 30.0f;   // fin de un grupo -> título del siguiente

constexpr float kRowH = 30.0f;
constexpr float kRowGap = 14.0f;
constexpr float kLabelColW = 132.0f;   // columna del texto de la fila
constexpr float kLabelToControl = 20.0f;

constexpr float kSegH = 26.0f;
constexpr float kSegGap = 8.0f;
constexpr float kSegPadX = 14.0f;

constexpr float kHintGap = 8.0f;
constexpr float kHintH = 15.0f;

// Ancho aproximado por carácter — la vista mide fino y centra el texto;
// alcanza para dimensionar el pill y el hit-test (mismo patrón que
// CollectionLayout / ShopLayout).
constexpr float kApproxCharW = 8.0f;

float SegWidth(const std::string& label) {
    return kSegPadX * 2.0f + static_cast<float>(label.size()) * kApproxCharW;
}

const char* FieldToken(PreferenceField field) {
    switch (field) {
        case PreferenceField::kSize:
            return "size";
        case PreferenceField::kOpacity:
            return "opacity";
        case PreferenceField::kLockPosition:
            return "lock";
        case PreferenceField::kLanguage:
            return "language";
    }
    return "";
}

struct SegSpec {
    std::string label;
    std::string value;   // token para el focusId ("small", "70", "on", "es")
    bool selected = false;
};

std::vector<SegSpec> SizeSegs(PetSizeChoice current, Language lang) {
    return {
        {Localized(StringKey::kSizeSmall, lang), "small", current == PetSizeChoice::kSmall},
        {Localized(StringKey::kSizeMedium, lang), "medium", current == PetSizeChoice::kMedium},
        {Localized(StringKey::kSizeLarge, lang), "large", current == PetSizeChoice::kLarge},
    };
}

std::vector<SegSpec> OpacitySegs(int currentPercent) {
    std::vector<SegSpec> out;
    for (const int pct : core::kOpacityChoicesPercent) {
        // Los porcentajes son numéricos: NO se traducen (misma
        // convención que el submenú Opacity del menú rápido).
        out.push_back({std::to_string(pct) + "%", std::to_string(pct), pct == currentPercent});
    }
    return out;
}

std::vector<SegSpec> LockSegs(bool locked, Language lang) {
    return {
        {Localized(StringKey::kOn, lang), "on", locked},
        {Localized(StringKey::kOff, lang), "off", !locked},
    };
}

std::vector<SegSpec> LanguageSegs(Language current) {
    // Endónimos: SIEMPRE en su propio idioma (igual que el submenú
    // Language del menú rápido).
    return {
        {core::LanguageEndonym(Language::kEn), "en", current == Language::kEn},
        {core::LanguageEndonym(Language::kEs), "es", current == Language::kEs},
    };
}

// Coloca una fila (label + segmentos [+ hint]) a partir de `y`. Devuelve
// la y tras la fila (hint incluido).
float LayoutRow(
    SettingsRow& row, float y, float contentX, float contentW, const std::vector<SegSpec>& segs) {
    row.labelAnchor = UiRect{contentX, y, kLabelColW, kRowH};

    float segX = contentX + kLabelColW + kLabelToControl;
    const float segY = y + (kRowH - kSegH) * 0.5f;
    float focusLeft = segX;
    for (const SegSpec& s : segs) {
        SettingsSegment seg;
        seg.label = s.label;
        seg.focusId = std::string("opt:") + FieldToken(row.field) + ":" + s.value;
        seg.selected = s.selected;
        const float w = SegWidth(s.label);
        seg.rect = UiRect{segX, segY, w, kSegH};
        row.segments.push_back(seg);
        segX += w + kSegGap;
    }
    const float focusRight = row.segments.empty() ? focusLeft : row.segments.back().rect.Right();
    row.focusRect = UiRect{focusLeft - 4.0f, segY - 4.0f, (focusRight - focusLeft) + 8.0f, kSegH + 8.0f};

    float afterY = y + kRowH;
    if (!row.hint.empty()) {
        row.hintAnchor =
            UiRect{contentX + kLabelColW + kLabelToControl, afterY + kHintGap,
                   contentW - kLabelColW - kLabelToControl, kHintH};
        afterY += kHintGap + kHintH;
    }
    return afterY;
}

}  // namespace

const SettingsRow* SettingsLayout::FindRow(PreferenceField field) const {
    for (const SettingsGroup& g : groups) {
        for (const SettingsRow& r : g.rows) {
            if (r.field == field) {
                return &r;
            }
        }
    }
    return nullptr;
}

std::string SettingsLayout::HitTest(float x, float y) const {
    for (const SectionTab& tab : header.tabs) {
        if (tab.hitRect.Contains(x, y)) {
            return tab.focusId;
        }
    }
    for (const SettingsGroup& g : groups) {
        for (const SettingsRow& r : g.rows) {
            for (const SettingsSegment& s : r.segments) {
                if (s.rect.Contains(x, y)) {
                    return s.focusId;
                }
            }
        }
    }
    return "";
}

float ClampSettingsScroll(float scrollY, float contentHeight, float viewportH) {
    return std::clamp(scrollY, 0.0f, std::max(0.0f, contentHeight - viewportH));
}

SettingsLayout BuildSettingsLayout(const SettingsLayoutInput& in) {
    const Language lang = in.prefs.language;
    const float sy = in.scrollY;

    SettingsLayout out;
    out.viewport = UiRect{0.0f, 0.0f, in.viewportW, in.viewportH};
    const float contentW = std::max(240.0f, in.viewportW - 2.0f * kMargin);
    const float contentX = kMargin;

    out.header = BuildSectionHeaderLayout(
        in.viewportW, kMargin, sy, ProductSection::kSettings, lang, in.clickBalance);
    for (const SectionTab& tab : out.header.tabs) {
        out.focusOrder.push_back(tab.focusId);
    }

    float y = out.header.bodyTop;

    // --- Grupo "Companion": tamaño, opacidad, lock ---
    {
        SettingsGroup g;
        g.title = Localized(StringKey::kSettingsCompanion, lang);
        g.titleAnchor = UiRect{contentX, y, contentW, kGroupTitleH};
        y += kGroupTitleH + kGroupTitleToRule;
        g.rule = UiRect{contentX, y, contentW, 1.0f};
        y += 1.0f + kRuleToRows;

        SettingsRow size;
        size.field = PreferenceField::kSize;
        size.label = Localized(StringKey::kSize, lang);
        size.focusId = "row:size";
        y = LayoutRow(size, y, contentX, contentW, SizeSegs(in.prefs.size, lang));
        y += kRowGap;
        g.rows.push_back(std::move(size));

        SettingsRow opacity;
        opacity.field = PreferenceField::kOpacity;
        opacity.label = Localized(StringKey::kOpacity, lang);
        opacity.focusId = "row:opacity";
        y = LayoutRow(opacity, y, contentX, contentW, OpacitySegs(in.prefs.opacityPercent));
        y += kRowGap;
        g.rows.push_back(std::move(opacity));

        SettingsRow lock;
        lock.field = PreferenceField::kLockPosition;
        lock.label = Localized(StringKey::kLockPosition, lang);
        lock.focusId = "row:lock";
        lock.hint = Localized(StringKey::kLockPositionHint, lang);
        y = LayoutRow(lock, y, contentX, contentW, LockSegs(in.prefs.lockPosition, lang));
        g.rows.push_back(std::move(lock));

        out.groups.push_back(std::move(g));
    }

    y += kGroupGap;

    // --- Grupo "Language": el selector de idioma ---
    {
        SettingsGroup g;
        g.title = Localized(StringKey::kLanguage, lang);
        g.titleAnchor = UiRect{contentX, y, contentW, kGroupTitleH};
        y += kGroupTitleH + kGroupTitleToRule;
        g.rule = UiRect{contentX, y, contentW, 1.0f};
        y += 1.0f + kRuleToRows;

        SettingsRow language;
        language.field = PreferenceField::kLanguage;
        // El encabezado del grupo ("Language" / "Idioma") ya nombra esta
        // fila — no se repite como label de fila. El control arranca en
        // la MISMA columna que los de "Companion" para que las dos
        // secciones se alineen.
        language.label = "";
        language.focusId = "row:language";
        y = LayoutRow(language, y, contentX, contentW, LanguageSegs(in.prefs.language));
        g.rows.push_back(std::move(language));

        out.groups.push_back(std::move(g));
    }

    for (const SettingsGroup& g : out.groups) {
        for (const SettingsRow& r : g.rows) {
            out.focusOrder.push_back(r.focusId);
        }
    }

    out.contentHeight = y + kMargin + sy;
    return out;
}

}  // namespace nimvlets::productui
