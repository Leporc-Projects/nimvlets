#include "SettingsLayoutTest.h"

#include "core/Localization.h"
#include "core/Preferences.h"
#include "platform/QuickMenuModel.h"
#include "productui/SettingsLayout.h"

#include <string>
#include <vector>

using nimvlets::core::Language;
using nimvlets::core::Localized;
using nimvlets::core::PetSizeChoice;
using nimvlets::core::Preferences;
using nimvlets::core::PreferenceField;
using nimvlets::core::StringKey;
using nimvlets::platform::BuildQuickMenuModel;
using nimvlets::platform::MenuItem;
using nimvlets::platform::MenuItemKind;
using nimvlets::platform::QuickMenuModel;
using nimvlets::platform::ShellState;
using nimvlets::productui::BuildSettingsLayout;
using nimvlets::productui::SettingsLayout;
using nimvlets::productui::SettingsLayoutInput;
using nimvlets::productui::SettingsRow;
using nimvlets::productui::SettingsSegment;

namespace nimvlets::tests {

namespace {

SettingsLayout Layout(const Preferences& prefs, float w = 800.0f, float h = 560.0f) {
    SettingsLayoutInput in;
    in.prefs = prefs;
    in.viewportW = w;
    in.viewportH = h;
    return BuildSettingsLayout(in);
}

Preferences Prefs(PetSizeChoice size, int opacity, bool lock, Language lang) {
    Preferences p;
    p.size = size;
    p.opacityPercent = opacity;
    p.lockPosition = lock;
    p.language = lang;
    return p;
}

const SettingsSegment* SelectedSegment(const SettingsRow& row) {
    const SettingsSegment* found = nullptr;
    int count = 0;
    for (const SettingsSegment& s : row.segments) {
        if (s.selected) {
            ++count;
            found = &s;
        }
    }
    return count == 1 ? found : nullptr;  // exactamente uno
}

// La etiqueta del item accionable CHEQUEADO en un submenú del menú
// rápido (Size / Opacity / Language) — para el test de sincronización.
std::string CheckedSubmenuLabel(const QuickMenuModel& m, const std::string& submenuLabel) {
    for (const MenuItem& it : m.items) {
        if (it.kind == MenuItemKind::kSubmenu && it.label == submenuLabel) {
            for (const MenuItem& sub : it.submenu) {
                if (sub.checked) {
                    return sub.label;
                }
            }
        }
    }
    return "";
}

bool LockCheckedInMenu(const QuickMenuModel& m) {
    for (const MenuItem& it : m.items) {
        if (it.kind == MenuItemKind::kCheckable &&
            it.action == nimvlets::platform::ShellAction::kToggleLockPosition) {
            return it.checked;
        }
    }
    return false;
}

// --- Cabecera + navegación ---------------------------------------

bool TestHeaderHasThreeTabsSettingsActive() {
    const SettingsLayout en = Layout(Preferences{});
    NIMVLETS_CHECK(en.header.tabs.size() == 3);
    NIMVLETS_CHECK(en.header.tabs[0].label == "Collection" && !en.header.tabs[0].active);
    NIMVLETS_CHECK(en.header.tabs[1].label == "Shop" && !en.header.tabs[1].active);
    NIMVLETS_CHECK(en.header.tabs[2].label == "Settings" && en.header.tabs[2].active);
    NIMVLETS_CHECK(en.header.tabs[2].focusId == "nav:settings");

    Preferences es;
    es.language = Language::kEs;
    const SettingsLayout esL = Layout(es);
    NIMVLETS_CHECK(esL.header.tabs[2].label == "Ajustes" && esL.header.tabs[2].active);

    // Hit-test de la pestaña activa.
    const auto& tab = en.header.tabs[2];
    NIMVLETS_CHECK(en.HitTest(tab.hitRect.CenterX(), tab.hitRect.CenterY()) == "nav:settings");
    return true;
}

// Orden de tabulación: nav (3) y luego las filas de arriba hacia abajo.
bool TestFocusOrderIsNavThenRows() {
    const SettingsLayout l = Layout(Preferences{});
    NIMVLETS_CHECK(l.focusOrder.size() == 7);
    NIMVLETS_CHECK(l.focusOrder[0] == "nav:collection");
    NIMVLETS_CHECK(l.focusOrder[1] == "nav:shop");
    NIMVLETS_CHECK(l.focusOrder[2] == "nav:settings");
    NIMVLETS_CHECK(l.focusOrder[3] == "row:size");
    NIMVLETS_CHECK(l.focusOrder[4] == "row:opacity");
    NIMVLETS_CHECK(l.focusOrder[5] == "row:lock");
    NIMVLETS_CHECK(l.focusOrder[6] == "row:language");
    return true;
}

// --- Estado seleccionado por preferencia --------------------------

bool TestSelectedSegmentReflectsPreferences() {
    const SettingsLayout l = Layout(Prefs(PetSizeChoice::kLarge, 70, true, Language::kEs));

    const SettingsRow* size = l.FindRow(PreferenceField::kSize);
    const SettingsRow* opacity = l.FindRow(PreferenceField::kOpacity);
    const SettingsRow* lock = l.FindRow(PreferenceField::kLockPosition);
    const SettingsRow* lang = l.FindRow(PreferenceField::kLanguage);
    NIMVLETS_CHECK(size && opacity && lock && lang);

    NIMVLETS_CHECK(size->segments.size() == 3);
    NIMVLETS_CHECK(opacity->segments.size() == 4);
    NIMVLETS_CHECK(lock->segments.size() == 2);
    NIMVLETS_CHECK(lang->segments.size() == 2);

    const SettingsSegment* sSel = SelectedSegment(*size);
    const SettingsSegment* oSel = SelectedSegment(*opacity);
    const SettingsSegment* lSel = SelectedSegment(*lock);
    const SettingsSegment* gSel = SelectedSegment(*lang);
    NIMVLETS_CHECK(sSel && oSel && lSel && gSel);  // exactamente uno por fila

    NIMVLETS_CHECK(sSel->focusId == "opt:size:large");
    NIMVLETS_CHECK(oSel->focusId == "opt:opacity:70");
    NIMVLETS_CHECK(lSel->focusId == "opt:lock:on");
    NIMVLETS_CHECK(gSel->focusId == "opt:language:es");

    // Y otra combinación: default (medium / 100 / unlock / en).
    const SettingsLayout d = Layout(Preferences{});
    NIMVLETS_CHECK(SelectedSegment(*d.FindRow(PreferenceField::kSize))->focusId == "opt:size:medium");
    NIMVLETS_CHECK(SelectedSegment(*d.FindRow(PreferenceField::kOpacity))->focusId == "opt:opacity:100");
    NIMVLETS_CHECK(SelectedSegment(*d.FindRow(PreferenceField::kLockPosition))->focusId == "opt:lock:off");
    NIMVLETS_CHECK(SelectedSegment(*d.FindRow(PreferenceField::kLanguage))->focusId == "opt:language:en");
    return true;
}

// --- Localización EN / ES ---------------------------------------

bool TestLabelsLocalized() {
    const SettingsLayout en = Layout(Preferences{});
    const SettingsRow* enSize = en.FindRow(PreferenceField::kSize);
    NIMVLETS_CHECK(en.groups.size() == 2);
    NIMVLETS_CHECK(en.groups[0].title == "Companion");
    NIMVLETS_CHECK(en.groups[1].title == "Language");
    NIMVLETS_CHECK(enSize->label == "Size");
    NIMVLETS_CHECK(enSize->segments[0].label == "Small");
    NIMVLETS_CHECK(enSize->segments[1].label == "Medium");
    NIMVLETS_CHECK(enSize->segments[2].label == "Large");
    NIMVLETS_CHECK(en.FindRow(PreferenceField::kOpacity)->label == "Opacity");
    NIMVLETS_CHECK(en.FindRow(PreferenceField::kLockPosition)->label == "Lock Position");
    NIMVLETS_CHECK(en.FindRow(PreferenceField::kLockPosition)->segments[0].label == "On");
    NIMVLETS_CHECK(en.FindRow(PreferenceField::kLockPosition)->segments[1].label == "Off");
    NIMVLETS_CHECK(!en.FindRow(PreferenceField::kLockPosition)->hint.empty());
    // La fila de idioma no repite un label: el encabezado del grupo
    // "Language" ya la nombra.
    NIMVLETS_CHECK(en.FindRow(PreferenceField::kLanguage)->label.empty());

    Preferences esPrefs;
    esPrefs.language = Language::kEs;
    const SettingsLayout es = Layout(esPrefs);
    NIMVLETS_CHECK(es.groups[0].title == "Compañero");
    NIMVLETS_CHECK(es.groups[1].title == "Idioma");
    NIMVLETS_CHECK(es.FindRow(PreferenceField::kSize)->label == "Tamaño");
    NIMVLETS_CHECK(es.FindRow(PreferenceField::kSize)->segments[0].label == "Pequeño");
    NIMVLETS_CHECK(es.FindRow(PreferenceField::kSize)->segments[1].label == "Mediano");
    NIMVLETS_CHECK(es.FindRow(PreferenceField::kSize)->segments[2].label == "Grande");
    NIMVLETS_CHECK(es.FindRow(PreferenceField::kOpacity)->label == "Opacidad");
    NIMVLETS_CHECK(es.FindRow(PreferenceField::kLockPosition)->label == "Bloquear posición");
    NIMVLETS_CHECK(es.FindRow(PreferenceField::kLockPosition)->segments[0].label == "Activado");
    NIMVLETS_CHECK(es.FindRow(PreferenceField::kLockPosition)->segments[1].label == "Desactivado");
    NIMVLETS_CHECK(es.FindRow(PreferenceField::kLockPosition)->hint ==
                   std::string(Localized(StringKey::kLockPositionHint, Language::kEs)));
    NIMVLETS_CHECK(es.FindRow(PreferenceField::kLanguage)->label.empty());

    // Los porcentajes de opacidad NO se traducen; los nombres de idioma
    // son endónimos, iguales en las dos UIs de idioma.
    for (const SettingsLayout* l : {&en, &es}) {
        const SettingsRow* op = l->FindRow(PreferenceField::kOpacity);
        NIMVLETS_CHECK(op->segments[0].label == "100%");
        NIMVLETS_CHECK(op->segments[1].label == "85%");
        NIMVLETS_CHECK(op->segments[2].label == "70%");
        NIMVLETS_CHECK(op->segments[3].label == "55%");
        const SettingsRow* lang = l->FindRow(PreferenceField::kLanguage);
        NIMVLETS_CHECK(lang->segments[0].label == "English");
        NIMVLETS_CHECK(lang->segments[1].label == "Español");
    }
    return true;
}

// Solo la fila de Lock position lleva un hint.
bool TestOnlyLockRowHasHint() {
    const SettingsLayout l = Layout(Preferences{});
    for (const auto& g : l.groups) {
        for (const SettingsRow& r : g.rows) {
            if (r.field == PreferenceField::kLockPosition) {
                NIMVLETS_CHECK(!r.hint.empty());
            } else {
                NIMVLETS_CHECK(r.hint.empty());
            }
        }
    }
    return true;
}

// --- Hit-test -------------------------------------------------

bool TestHitTestReturnsSegmentAndNavIds() {
    const SettingsLayout l = Layout(Prefs(PetSizeChoice::kSmall, 100, false, Language::kEn));

    const SettingsRow* size = l.FindRow(PreferenceField::kSize);
    for (const SettingsSegment& s : size->segments) {
        NIMVLETS_CHECK(l.HitTest(s.rect.CenterX(), s.rect.CenterY()) == s.focusId);
    }
    const SettingsRow* op = l.FindRow(PreferenceField::kOpacity);
    NIMVLETS_CHECK(l.HitTest(op->segments[2].rect.CenterX(), op->segments[2].rect.CenterY()) ==
                   "opt:opacity:70");

    NIMVLETS_CHECK(l.HitTest(l.header.tabs[0].hitRect.CenterX(), l.header.tabs[0].hitRect.CenterY()) ==
                   "nav:collection");
    NIMVLETS_CHECK(l.HitTest(l.header.tabs[1].hitRect.CenterX(), l.header.tabs[1].hitRect.CenterY()) ==
                   "nav:shop");

    // Área vacía -> nada.
    NIMVLETS_CHECK(l.HitTest(5.0f, 4.0f).empty());
    NIMVLETS_CHECK(l.HitTest(size->labelAnchor.x + 2.0f, size->labelAnchor.CenterY()).empty());
    return true;
}

// --- Layout: cabe sin scroll, sin solapes ----------------------

bool TestFitsWithoutScrollBothLanguages() {
    NIMVLETS_CHECK(Layout(Preferences{}).contentHeight <= 560.0f + 1.0f);
    Preferences es;
    es.language = Language::kEs;
    es.lockPosition = true;
    NIMVLETS_CHECK(Layout(es).contentHeight <= 560.0f + 1.0f);
    return true;
}

bool TestNoOverlapWithinRows() {
    for (const Language lang : {Language::kEn, Language::kEs}) {
        Preferences p;
        p.language = lang;
        const SettingsLayout l = Layout(p);
        float prevRowBottom = 0.0f;
        for (const auto& g : l.groups) {
            // El título y la regla del grupo van sobre la primera fila.
            NIMVLETS_CHECK(g.rule.y >= g.titleAnchor.Bottom() - 4.0f);
            for (const SettingsRow& r : g.rows) {
                // La columna de label no pisa el primer segmento.
                NIMVLETS_CHECK(r.segments.front().rect.x >= r.labelAnchor.Right());
                // Segmentos adyacentes no se solapan.
                for (std::size_t i = 1; i < r.segments.size(); ++i) {
                    NIMVLETS_CHECK(r.segments[i].rect.x >= r.segments[i - 1].rect.Right());
                }
                // Las filas no se solapan verticalmente (incluido el hint).
                const float top = r.labelAnchor.y;
                NIMVLETS_CHECK(top >= prevRowBottom - 0.5f);
                prevRowBottom = r.hint.empty() ? r.labelAnchor.Bottom() : r.hintAnchor.Bottom();
            }
        }
    }
    return true;
}

// --- Sincronización: Settings y el menú rápido coinciden ---------
//
// Para un MISMO estado de preferencias, el segmento seleccionado en
// Settings y el item chequeado en el submenú equivalente del menú rápido
// nombran el mismo valor. Es la prueba pura de "una sola fuente de
// verdad" (brief §25): las dos superficies derivan de los mismos campos.
bool TestSettingsAndQuickMenuAgreeOnSelection() {
    struct Case {
        PetSizeChoice size;
        int opacity;
        bool lock;
        Language lang;
    };
    const Case cases[] = {
        {PetSizeChoice::kSmall, 55, true, Language::kEs},
        {PetSizeChoice::kMedium, 100, false, Language::kEn},
        {PetSizeChoice::kLarge, 70, true, Language::kEn},
        {PetSizeChoice::kMedium, 85, false, Language::kEs},
    };
    for (const Case& c : cases) {
        const Preferences p = Prefs(c.size, c.opacity, c.lock, c.lang);
        const SettingsLayout l = Layout(p);

        ShellState s;
        s.sizeChoiceId = core::PetSizeChoiceId(c.size);
        s.opacityPercent = c.opacity;
        s.lockPosition = c.lock;
        s.language = c.lang;
        const QuickMenuModel m = BuildQuickMenuModel(s);

        // Size: misma etiqueta seleccionada / chequeada.
        NIMVLETS_CHECK(SelectedSegment(*l.FindRow(PreferenceField::kSize))->label ==
                       CheckedSubmenuLabel(m, Localized(StringKey::kSize, c.lang)));
        // Opacity: idem (los % no se traducen).
        NIMVLETS_CHECK(SelectedSegment(*l.FindRow(PreferenceField::kOpacity))->label ==
                       CheckedSubmenuLabel(m, Localized(StringKey::kOpacity, c.lang)));
        // Language: endónimo seleccionado / chequeado.
        NIMVLETS_CHECK(SelectedSegment(*l.FindRow(PreferenceField::kLanguage))->label ==
                       CheckedSubmenuLabel(m, Localized(StringKey::kLanguage, c.lang)));
        // Lock: el segmento "On" está seleccionado exactamente cuando el
        // item del menú está chequeado.
        const bool settingsLockOn =
            SelectedSegment(*l.FindRow(PreferenceField::kLockPosition))->focusId == "opt:lock:on";
        NIMVLETS_CHECK(settingsLockOn == LockCheckedInMenu(m));
        NIMVLETS_CHECK(settingsLockOn == c.lock);
    }
    return true;
}

}  // namespace

void RegisterSettingsLayoutTests(testing::TestRunner& runner) {
    runner.Add("SettingsLayout/HeaderHasThreeTabsSettingsActive", TestHeaderHasThreeTabsSettingsActive);
    runner.Add("SettingsLayout/FocusOrderIsNavThenRows", TestFocusOrderIsNavThenRows);
    runner.Add("SettingsLayout/SelectedSegmentReflectsPreferences", TestSelectedSegmentReflectsPreferences);
    runner.Add("SettingsLayout/LabelsLocalized", TestLabelsLocalized);
    runner.Add("SettingsLayout/OnlyLockRowHasHint", TestOnlyLockRowHasHint);
    runner.Add("SettingsLayout/HitTestReturnsSegmentAndNavIds", TestHitTestReturnsSegmentAndNavIds);
    runner.Add("SettingsLayout/FitsWithoutScrollBothLanguages", TestFitsWithoutScrollBothLanguages);
    runner.Add("SettingsLayout/NoOverlapWithinRows", TestNoOverlapWithinRows);
    runner.Add("SettingsLayout/SettingsAndQuickMenuAgreeOnSelection", TestSettingsAndQuickMenuAgreeOnSelection);
}

}  // namespace nimvlets::tests
