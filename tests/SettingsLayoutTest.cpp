#include "SettingsLayoutTest.h"

#include "core/ClickCounting.h"
#include "core/Localization.h"
#include "core/Preferences.h"
#include "platform/GlobalClickTypes.h"
#include "platform/QuickMenuModel.h"
#include "productui/SettingsLayout.h"

#include <cstdint>
#include <string>
#include <vector>

using nimvlets::core::ClickCountingMode;
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
using nimvlets::platform::GlobalClickCapability;
using nimvlets::platform::GlobalClickPermission;
using nimvlets::platform::GlobalClickStatus;
using nimvlets::platform::GlobalClickUiState;
using nimvlets::platform::ResolveGlobalClickUiState;
using nimvlets::platform::ShellState;
using nimvlets::productui::BuildSettingsLayout;
using nimvlets::productui::ClampSettingsScroll;
using nimvlets::productui::SettingsLayout;
using nimvlets::productui::SettingsLayoutInput;
using nimvlets::productui::SettingsRow;
using nimvlets::productui::SettingsSegment;
using nimvlets::productui::GlobalClickAction;
using nimvlets::productui::ParseGlobalClickAction;
using nimvlets::productui::SettingsNoticeButton;

namespace nimvlets::tests {

namespace {

SettingsLayout Layout(const Preferences& prefs, float w = 800.0f, float h = 560.0f) {
    SettingsLayoutInput in;
    in.prefs = prefs;
    in.viewportW = w;
    in.viewportH = h;
    return BuildSettingsLayout(in);
}

SettingsLayout LayoutWithBalance(std::uint64_t balance, Language lang = Language::kEn) {
    Preferences p;
    p.language = lang;
    SettingsLayoutInput in;
    in.prefs = p;
    in.clickBalance = balance;
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

// --- Helpers de Block 11A -----------------------------------------

GlobalClickStatus Status(
    GlobalClickCapability capability, GlobalClickPermission permission, bool active = false) {
    GlobalClickStatus st;
    st.capability = capability;
    st.permission = permission;
    st.monitorActive = active;
    st.permissionName = "Input Monitoring";
    return st;
}

// Layout con un estado de conteo global explícito, ya derivado por la
// política pura de plataforma — el mismo camino que src/app usa.
SettingsLayout LayoutWithGlobalClick(
    ClickCountingMode mode, const GlobalClickStatus& status, bool explanationVisible = false,
    Language lang = Language::kEn) {
    Preferences p;
    p.language = lang;
    p.clickCounting = mode;
    SettingsLayoutInput in;
    in.prefs = p;
    in.globalClick = ResolveGlobalClickUiState(mode, status);
    in.globalClickExplanationVisible = explanationVisible;
    return BuildSettingsLayout(in);
}

const SettingsRow* ClickCountingRow(const SettingsLayout& l) {
    return l.FindRow(PreferenceField::kClickCounting);
}

bool HasButton(const SettingsRow& row, const std::string& focusId) {
    for (const SettingsNoticeButton& b : row.notice.buttons) {
        if (b.focusId == focusId) {
            return true;
        }
    }
    return false;
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

// REGRESIÓN de la falla exacta de QA del owner (Block 10): Settings
// mostraba "0 clicks" mientras Collection / Shop mostraban 500. Ahora el
// balance CANÓNICO llega a `BuildSettingsLayout` por `in.clickBalance` y
// se formatea en `header.clicksText` — la MISMA ruta que las otras
// secciones. `SettingsView::Render` ya no elige un valor propio.
bool TestSettingsHeaderShowsCanonicalWallet() {
    NIMVLETS_CHECK(LayoutWithBalance(0).header.clicksText == "0 clicks");
    NIMVLETS_CHECK(LayoutWithBalance(500).header.clicksText == "500 clicks");
    NIMVLETS_CHECK(LayoutWithBalance(350).header.clicksText == "350 clicks");
    NIMVLETS_CHECK(LayoutWithBalance(1).header.clicksText == "1 click");
    NIMVLETS_CHECK(LayoutWithBalance(1248).header.clicksText == "1 248 clicks");
    NIMVLETS_CHECK(LayoutWithBalance(500, Language::kEs).header.clicksText == "500 clics");
    // El default de `SettingsLayoutInput::clickBalance` es 0 — un caller
    // que se OLVIDE de pasar el balance muestra "0 clicks" (el bug); por
    // eso ProductWindow es la autoridad única y DrawFrame() SIEMPRE lo
    // pasa a Render (ver SectionNavTest / ProductWindow).
    NIMVLETS_CHECK(Layout(Preferences{}).header.clicksText == "0 clicks");
    return true;
}

// Orden de tabulación: nav (3) y luego las filas de arriba hacia abajo.
// Block 11A agrega "row:clickcounting" entre lock y language; con el
// estado por defecto (plataforma sin capacidad, modo local) ese grupo no
// aporta ningún botón de aviso al anillo de foco.
bool TestFocusOrderIsNavThenRows() {
    const SettingsLayout l = Layout(Preferences{});
    NIMVLETS_CHECK(l.focusOrder.size() == 8);
    NIMVLETS_CHECK(l.focusOrder[0] == "nav:collection");
    NIMVLETS_CHECK(l.focusOrder[1] == "nav:shop");
    NIMVLETS_CHECK(l.focusOrder[2] == "nav:settings");
    NIMVLETS_CHECK(l.focusOrder[3] == "row:size");
    NIMVLETS_CHECK(l.focusOrder[4] == "row:opacity");
    NIMVLETS_CHECK(l.focusOrder[5] == "row:lock");
    NIMVLETS_CHECK(l.focusOrder[6] == "row:clickcounting");
    NIMVLETS_CHECK(l.focusOrder[7] == "row:language");
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
    NIMVLETS_CHECK(en.groups.size() == 3);
    NIMVLETS_CHECK(en.groups[0].title == "Companion");
    NIMVLETS_CHECK(en.groups[1].title == "Interaction");
    NIMVLETS_CHECK(en.groups[2].title == "Language");
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
    NIMVLETS_CHECK(es.groups[1].title == "Interacción");
    NIMVLETS_CHECK(es.groups[2].title == "Idioma");
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

// Solo las filas de Lock position y Click counting llevan un hint —
// Size / Opacity / Language se explican solas.
bool TestOnlyLockAndClickCountingRowsHaveHint() {
    const SettingsLayout l = Layout(Preferences{});
    for (const auto& g : l.groups) {
        for (const SettingsRow& r : g.rows) {
            if (r.field == PreferenceField::kLockPosition ||
                r.field == PreferenceField::kClickCounting) {
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

// --- Block 11A: el grupo "Interaction" ----------------------------

// La fila existe, con sus dos segmentos, y el seleccionado sigue a la
// preferencia. Va entre "Companion" y "Language": las filas de Companion
// NO se movieron.
bool TestInteractionGroupHasClickCountingRow() {
    const GlobalClickStatus supported =
        Status(GlobalClickCapability::kSupportedNeedsPermission, GlobalClickPermission::kGranted);
    const SettingsLayout l = LayoutWithGlobalClick(ClickCountingMode::kNimvletOnly, supported);

    NIMVLETS_CHECK(l.groups.size() == 3);
    NIMVLETS_CHECK(l.groups[1].title == "Interaction");
    NIMVLETS_CHECK(l.groups[1].rows.size() == 1);

    const SettingsRow* row = ClickCountingRow(l);
    NIMVLETS_CHECK(row != nullptr);
    NIMVLETS_CHECK(row->label == "Click counting");
    NIMVLETS_CHECK(row->focusId == "row:clickcounting");
    NIMVLETS_CHECK(row->segments.size() == 2);
    NIMVLETS_CHECK(row->segments[0].label == "Nimvlet only");
    NIMVLETS_CHECK(row->segments[0].focusId == "opt:clickcounting:nimvlet_only");
    NIMVLETS_CHECK(row->segments[1].label == "Anywhere");
    NIMVLETS_CHECK(row->segments[1].focusId == "opt:clickcounting:anywhere");
    NIMVLETS_CHECK(SelectedSegment(*row)->focusId == "opt:clickcounting:nimvlet_only");

    // "Companion" intacto, y el grupo de idioma sigue último.
    NIMVLETS_CHECK(l.groups[0].title == "Companion");
    NIMVLETS_CHECK(l.groups[0].rows.size() == 3);
    NIMVLETS_CHECK(l.groups[2].title == "Language");

    // Con la preferencia en "anywhere", el seleccionado cambia.
    const SettingsLayout anywhere = LayoutWithGlobalClick(
        ClickCountingMode::kAnywhere,
        Status(GlobalClickCapability::kSupportedNeedsPermission, GlobalClickPermission::kGranted,
               /*active=*/true));
    NIMVLETS_CHECK(SelectedSegment(*ClickCountingRow(anywhere))->focusId ==
                   "opt:clickcounting:anywhere");
    return true;
}

// Modo local sobre una plataforma capaz: sin aviso ninguno. Settings no
// se llena de estado de permiso que el owner no pidió (brief §9).
bool TestLocalModeShowsNoNotice() {
    const SettingsLayout l = LayoutWithGlobalClick(
        ClickCountingMode::kNimvletOnly,
        Status(GlobalClickCapability::kSupportedNeedsPermission, GlobalClickPermission::kNotGranted));
    const SettingsRow* row = ClickCountingRow(l);
    NIMVLETS_CHECK(!row->notice.present);
    NIMVLETS_CHECK(row->notice.buttons.empty());
    NIMVLETS_CHECK(row->segments[1].enabled);  // "Anywhere" sí se puede elegir
    // Y la fila sigue teniendo su hint corto.
    NIMVLETS_CHECK(!row->hint.empty());
    return true;
}

// Plataforma sin capacidad: "Anywhere" se DIBUJA pero no es accionable,
// y la línea de estado explica por qué.
bool TestUnavailablePlatformDisablesAnywhereAndSaysSo() {
    const SettingsLayout l = LayoutWithGlobalClick(
        ClickCountingMode::kNimvletOnly,
        Status(GlobalClickCapability::kUnavailable, GlobalClickPermission::kNotRequired));
    const SettingsRow* row = ClickCountingRow(l);

    NIMVLETS_CHECK(row->segments.size() == 2);
    NIMVLETS_CHECK(row->segments[0].enabled);
    NIMVLETS_CHECK(!row->segments[1].enabled);
    NIMVLETS_CHECK(row->notice.present);
    NIMVLETS_CHECK(row->notice.statusLabel == "Not available on this system");
    NIMVLETS_CHECK(row->notice.statusIsAlert);
    NIMVLETS_CHECK(row->notice.buttons.empty());  // nada que reintentar

    // El hit-test NUNCA devuelve un segmento apagado, aunque el punto
    // caiga exactamente encima.
    const SettingsSegment& off = row->segments[1];
    NIMVLETS_CHECK(l.HitTest(off.rect.CenterX(), off.rect.CenterY()).empty());
    // ...mientras que el habilitado sí responde.
    const SettingsSegment& on = row->segments[0];
    NIMVLETS_CHECK(l.HitTest(on.rect.CenterX(), on.rect.CenterY()) == "opt:clickcounting:nimvlet_only");
    // Y tampoco está en el orden de foco como widget propio (el foco
    // vive en la fila).
    for (const std::string& id : l.focusOrder) {
        NIMVLETS_CHECK(id != "opt:clickcounting:anywhere");
    }
    return true;
}

// La explicación de primera parte: texto + [Not now] [Continue], y NADA
// de estado de permiso (todavía no se pidió nada).
bool TestExplanationShowsBothButtonsAndNamesThePermission() {
    const SettingsLayout l = LayoutWithGlobalClick(
        ClickCountingMode::kNimvletOnly,
        Status(GlobalClickCapability::kSupportedNeedsPermission, GlobalClickPermission::kNotGranted),
        /*explanationVisible=*/true);
    const SettingsRow* row = ClickCountingRow(l);

    NIMVLETS_CHECK(row->notice.present);
    NIMVLETS_CHECK(row->notice.statusLabel.empty());
    NIMVLETS_CHECK(!row->notice.body.empty());
    NIMVLETS_CHECK(row->notice.bodyLines >= 1);
    // El nombre del permiso llega como DATO del adapter y se sustituye.
    NIMVLETS_CHECK(row->notice.body.find("Input Monitoring") != std::string::npos);
    NIMVLETS_CHECK(row->notice.body.find("{permission}") == std::string::npos);
    // Y dice explícitamente qué NO se observa.
    NIMVLETS_CHECK(row->notice.body.find("never keys") != std::string::npos);
    // Corrección de QA del owner (Block 11A): el diálogo real de macOS
    // habla de "keystrokes from any application" y Ajustes del Sistema
    // describe Input Monitoring en términos de teclado. Se anticipa esa
    // redacción AMPLIA acá, sin nombrar la plataforma (la copy sigue sin
    // ramas por OS) y sin prometer que podamos cambiarla.
    NIMVLETS_CHECK(row->notice.body.find("describe that permission broadly") != std::string::npos);
    NIMVLETS_CHECK(row->notice.body.find("keyboard or keystroke access") != std::string::npos);
    NIMVLETS_CHECK(row->notice.body.find("not what Nimvlets does") != std::string::npos);

    NIMVLETS_CHECK(row->notice.buttons.size() == 2);
    NIMVLETS_CHECK(HasButton(*row, "gc:notnow"));
    NIMVLETS_CHECK(HasButton(*row, "gc:continue"));
    // "Not now" va primero: la opción segura es la de menos fricción.
    NIMVLETS_CHECK(row->notice.buttons[0].focusId == "gc:notnow");

    // Los botones son alcanzables con el mouse y con Tab.
    for (const SettingsNoticeButton& b : row->notice.buttons) {
        NIMVLETS_CHECK(l.HitTest(b.rect.CenterX(), b.rect.CenterY()) == b.focusId);
        bool inOrder = false;
        for (const std::string& id : l.focusOrder) {
            inOrder = inOrder || id == b.focusId;
        }
        NIMVLETS_CHECK(inOrder);
    }
    return true;
}

// Pedido pero sin permiso: estado + cómo concederlo + "Check again".
// Nunca se muestra "Active" sobre un monitor que no corre (brief §5).
bool TestPermissionNeededStateOffersCheckAgain() {
    const SettingsLayout l = LayoutWithGlobalClick(
        ClickCountingMode::kAnywhere,
        Status(GlobalClickCapability::kSupportedNeedsPermission, GlobalClickPermission::kNotGranted));
    const SettingsRow* row = ClickCountingRow(l);

    NIMVLETS_CHECK(row->notice.present);
    NIMVLETS_CHECK(row->notice.statusLabel == "Input Monitoring permission needed");
    NIMVLETS_CHECK(row->notice.statusIsAlert);
    NIMVLETS_CHECK(row->notice.body.find("Input Monitoring") != std::string::npos);
    NIMVLETS_CHECK(row->notice.body.find("System Settings") != std::string::npos);
    // Es el momento exacto en que el owner va a leer la redacción amplia
    // del sistema, así que el recordatorio de alcance viaja con el hint.
    NIMVLETS_CHECK(row->notice.body.find("only for primary mouse presses") != std::string::npos);
    NIMVLETS_CHECK(row->notice.buttons.size() == 1);
    NIMVLETS_CHECK(HasButton(*row, "gc:recheck"));
    // La preferencia SIGUE en "anywhere" — no se auto-degrada.
    NIMVLETS_CHECK(SelectedSegment(*row)->focusId == "opt:clickcounting:anywhere");
    return true;
}

// Activo: se dice, y se explica la semántica de drag — que solo importa
// cuando el conteo global está realmente contando (brief §21).
bool TestActiveStateSaysActiveAndExplainsDragSemantics() {
    const SettingsLayout l = LayoutWithGlobalClick(
        ClickCountingMode::kAnywhere,
        Status(GlobalClickCapability::kSupportedNeedsPermission, GlobalClickPermission::kGranted,
               /*active=*/true));
    const SettingsRow* row = ClickCountingRow(l);

    NIMVLETS_CHECK(row->notice.present);
    NIMVLETS_CHECK(row->notice.statusLabel == "Active");
    NIMVLETS_CHECK(!row->notice.statusIsAlert);
    NIMVLETS_CHECK(row->notice.body.find("counts once") != std::string::npos);
    // Mientras cuenta de verdad, la entrada del permiso está viva en
    // Ajustes del Sistema con la redacción amplia del OS: el alcance
    // real se repite acá, primero.
    NIMVLETS_CHECK(row->notice.body.find("only for primary mouse presses") != std::string::npos);
    NIMVLETS_CHECK(
        row->notice.body.find("only for primary mouse presses") < row->notice.body.find("counts once"));
    NIMVLETS_CHECK(row->notice.buttons.empty());  // anda: nada que reintentar
    return true;
}

// El aviso más largo (la explicación previa al permiso, ya con la frase
// sobre la redacción amplia del OS) sigue ENTERO dentro del ancho de
// contenido y no se corta: kNoticeMaxLines tiene que alcanzarle.
bool TestBroadWordingDisclosureFitsWithoutTruncation() {
    for (const Language lang : {Language::kEn, Language::kEs}) {
        const SettingsLayout l = LayoutWithGlobalClick(
            ClickCountingMode::kNimvletOnly,
            Status(GlobalClickCapability::kSupportedNeedsPermission,
                   GlobalClickPermission::kNotGranted),
            /*explanationVisible=*/true, lang);
        const SettingsRow* row = ClickCountingRow(l);

        NIMVLETS_CHECK(row->notice.present);
        // El párrafo entra en las líneas que el layout reserva (si no, la
        // vista lo cortaría con "…" justo donde dice qué NO miramos).
        // 8.0f = el mismo kApproxCharW con el que SettingsLayout estima.
        const float perLine = row->notice.bodyAnchor.w / 8.0f;
        NIMVLETS_CHECK(static_cast<float>(row->notice.body.size()) <=
                       perLine * static_cast<float>(row->notice.bodyLines));
        // Y el bloque entero sigue dentro de la ventana, con los botones
        // por debajo del texto.
        NIMVLETS_CHECK(row->notice.bodyAnchor.Right() <= l.viewport.w);
        NIMVLETS_CHECK(row->notice.buttons.front().rect.y >= row->notice.bodyAnchor.Bottom());
        NIMVLETS_CHECK(l.contentHeight > row->notice.buttons.back().rect.Bottom());
    }
    return true;
}

// Los botones del aviso se tabulan JUSTO DESPUÉS de su fila, no al final.
bool TestNoticeButtonsFollowTheirRowInFocusOrder() {
    const SettingsLayout l = LayoutWithGlobalClick(
        ClickCountingMode::kNimvletOnly,
        Status(GlobalClickCapability::kSupportedNeedsPermission, GlobalClickPermission::kNotGranted),
        /*explanationVisible=*/true);

    const std::vector<std::string> expected = {
        "nav:collection", "nav:shop", "nav:settings", "row:size",     "row:opacity",
        "row:lock",       "row:clickcounting", "gc:notnow", "gc:continue", "row:language"};
    NIMVLETS_CHECK(l.focusOrder == expected);
    return true;
}

bool TestGlobalClickActionTokensParse() {
    NIMVLETS_CHECK(ParseGlobalClickAction("gc:continue") == GlobalClickAction::kContinue);
    NIMVLETS_CHECK(ParseGlobalClickAction("gc:notnow") == GlobalClickAction::kNotNow);
    NIMVLETS_CHECK(ParseGlobalClickAction("gc:recheck") == GlobalClickAction::kCheckAgain);
    NIMVLETS_CHECK(ParseGlobalClickAction("row:clickcounting") == GlobalClickAction::kNone);
    NIMVLETS_CHECK(ParseGlobalClickAction("opt:clickcounting:anywhere") == GlobalClickAction::kNone);
    NIMVLETS_CHECK(ParseGlobalClickAction("nav:settings") == GlobalClickAction::kNone);
    NIMVLETS_CHECK(ParseGlobalClickAction("") == GlobalClickAction::kNone);
    return true;
}

// EN/ES completo del grupo nuevo, incluidos los avisos.
bool TestInteractionGroupLocalized() {
    const GlobalClickStatus needsPerm =
        Status(GlobalClickCapability::kSupportedNeedsPermission, GlobalClickPermission::kNotGranted);

    const SettingsLayout es =
        LayoutWithGlobalClick(ClickCountingMode::kAnywhere, needsPerm, false, Language::kEs);
    const SettingsRow* esRow = ClickCountingRow(es);
    NIMVLETS_CHECK(es.groups[1].title == "Interacción");
    NIMVLETS_CHECK(esRow->label == "Conteo de clics");
    NIMVLETS_CHECK(esRow->segments[0].label == "Solo el Nimvlet");
    NIMVLETS_CHECK(esRow->segments[1].label == "En cualquier lugar");
    NIMVLETS_CHECK(esRow->notice.statusLabel == "Falta el permiso Input Monitoring");
    NIMVLETS_CHECK(esRow->notice.body.find("Ajustes del Sistema") != std::string::npos);
    NIMVLETS_CHECK(esRow->notice.body.find("botón principal") != std::string::npos);
    NIMVLETS_CHECK(esRow->hint == std::string(Localized(StringKey::kClickCountingHint, Language::kEs)));

    // Botones y estados en los dos idiomas, siempre distintos entre sí.
    const SettingsLayout enExplain =
        LayoutWithGlobalClick(ClickCountingMode::kNimvletOnly, needsPerm, true, Language::kEn);
    const SettingsLayout esExplain =
        LayoutWithGlobalClick(ClickCountingMode::kNimvletOnly, needsPerm, true, Language::kEs);
    NIMVLETS_CHECK(enExplain.FindRow(PreferenceField::kClickCounting)->notice.buttons[1].label ==
                   "Continue");
    NIMVLETS_CHECK(esExplain.FindRow(PreferenceField::kClickCounting)->notice.buttons[1].label ==
                   "Continuar");
    NIMVLETS_CHECK(enExplain.FindRow(PreferenceField::kClickCounting)->notice.buttons[0].label ==
                   "Not now");
    NIMVLETS_CHECK(esExplain.FindRow(PreferenceField::kClickCounting)->notice.buttons[0].label ==
                   "Ahora no");
    // "Nimvlet" (nombre de marca) NUNCA se traduce.
    NIMVLETS_CHECK(esRow->segments[0].label.find("Nimvlet") != std::string::npos);

    const SettingsLayout esUnavail = LayoutWithGlobalClick(
        ClickCountingMode::kNimvletOnly,
        Status(GlobalClickCapability::kUnavailable, GlobalClickPermission::kNotRequired), false,
        Language::kEs);
    NIMVLETS_CHECK(ClickCountingRow(esUnavail)->notice.statusLabel == "No disponible en este sistema");
    return true;
}

// El grupo nuevo no rompe el layout al cambiar el tamaño de la ventana:
// la fila y su aviso siguen dentro del ancho de contenido y el
// contentHeight crece de forma coherente (hay scroll si hace falta).
bool TestInteractionGroupSurvivesResize() {
    const GlobalClickStatus needsPerm =
        Status(GlobalClickCapability::kSupportedNeedsPermission, GlobalClickPermission::kNotGranted);
    for (const float w : {640.0f, 800.0f, 1200.0f}) {
        Preferences p;
        p.clickCounting = ClickCountingMode::kAnywhere;
        SettingsLayoutInput in;
        in.prefs = p;
        in.viewportW = w;
        in.viewportH = 400.0f;
        in.globalClick = ResolveGlobalClickUiState(ClickCountingMode::kAnywhere, needsPerm);
        const SettingsLayout l = BuildSettingsLayout(in);

        const SettingsRow* row = ClickCountingRow(l);
        NIMVLETS_CHECK(row != nullptr);
        NIMVLETS_CHECK(row->notice.present);
        // El párrafo se envuelve dentro del ancho disponible, nunca lo excede.
        NIMVLETS_CHECK(row->notice.bodyAnchor.w > 0.0f);
        NIMVLETS_CHECK(row->notice.bodyAnchor.Right() <= w);
        // El botón de reintento también entra.
        NIMVLETS_CHECK(row->notice.buttons.size() == 1);
        NIMVLETS_CHECK(row->notice.buttons[0].rect.Right() <= w);
        // Todo el contenido queda por encima del contentHeight declarado.
        NIMVLETS_CHECK(l.contentHeight > row->notice.buttons[0].rect.Bottom());
        // Un viewport chico produce contenido más alto que la ventana ->
        // hay scroll, no recorte silencioso.
        NIMVLETS_CHECK(l.contentHeight > in.viewportH);
    }
    return true;
}

// Altura de contenido a tamaño por defecto (800x560). Block 08 fijó que
// Settings entraba sin scroll; Block 11A agrega un grupo, así que este
// test documenta el resultado REAL en vez de dar por hecho el anterior:
//
//   - estado por DEFECTO del producto (modo local, sin aviso): sigue
//     entrando sin scroll, en EN y ES;
//   - con un aviso desplegado (explicación / estado del permiso) el
//     contenido pasa de largo y la vista scrollea — que es exactamente
//     para lo que existe ClampSettingsScroll, y por qué el aviso es
//     condicional en vez de permanente.
bool TestDefaultStateStillFitsWithoutScroll() {
    const GlobalClickStatus supported =
        Status(GlobalClickCapability::kSupportedNeedsPermission, GlobalClickPermission::kNotGranted);

    for (const Language lang : {Language::kEn, Language::kEs}) {
        Preferences p;
        p.language = lang;
        SettingsLayoutInput in;
        in.prefs = p;
        in.globalClick = ResolveGlobalClickUiState(ClickCountingMode::kNimvletOnly, supported);
        const SettingsLayout l = BuildSettingsLayout(in);
        NIMVLETS_CHECK(!ClickCountingRow(l)->notice.present);
        NIMVLETS_CHECK(l.contentHeight <= 560.0f);
    }

    // Con la explicación abierta, hay más contenido que ventana -> scroll.
    Preferences p;
    SettingsLayoutInput in;
    in.prefs = p;
    in.globalClick = ResolveGlobalClickUiState(ClickCountingMode::kNimvletOnly, supported);
    in.globalClickExplanationVisible = true;
    const SettingsLayout scrolled = BuildSettingsLayout(in);
    NIMVLETS_CHECK(scrolled.contentHeight > 560.0f);
    // Y el scroll está acotado a ese excedente exacto (nada se pierde).
    const float maxScroll = scrolled.contentHeight - 560.0f;
    NIMVLETS_CHECK(ClampSettingsScroll(9999.0f, scrolled.contentHeight, 560.0f) == maxScroll);
    NIMVLETS_CHECK(ClampSettingsScroll(-50.0f, scrolled.contentHeight, 560.0f) == 0.0f);
    return true;
}

}  // namespace

void RegisterSettingsLayoutTests(testing::TestRunner& runner) {
    runner.Add("SettingsLayout/DefaultStateStillFitsWithoutScroll", TestDefaultStateStillFitsWithoutScroll);
    runner.Add("SettingsLayout/InteractionGroupHasClickCountingRow", TestInteractionGroupHasClickCountingRow);
    runner.Add("SettingsLayout/LocalModeShowsNoNotice", TestLocalModeShowsNoNotice);
    runner.Add("SettingsLayout/UnavailablePlatformDisablesAnywhereAndSaysSo",
               TestUnavailablePlatformDisablesAnywhereAndSaysSo);
    runner.Add("SettingsLayout/ExplanationShowsBothButtonsAndNamesThePermission",
               TestExplanationShowsBothButtonsAndNamesThePermission);
    runner.Add("SettingsLayout/PermissionNeededStateOffersCheckAgain",
               TestPermissionNeededStateOffersCheckAgain);
    runner.Add("SettingsLayout/ActiveStateSaysActiveAndExplainsDragSemantics",
               TestActiveStateSaysActiveAndExplainsDragSemantics);
    runner.Add("SettingsLayout/BroadWordingDisclosureFitsWithoutTruncation",
               TestBroadWordingDisclosureFitsWithoutTruncation);
    runner.Add("SettingsLayout/NoticeButtonsFollowTheirRowInFocusOrder",
               TestNoticeButtonsFollowTheirRowInFocusOrder);
    runner.Add("SettingsLayout/GlobalClickActionTokensParse", TestGlobalClickActionTokensParse);
    runner.Add("SettingsLayout/InteractionGroupLocalized", TestInteractionGroupLocalized);
    runner.Add("SettingsLayout/InteractionGroupSurvivesResize", TestInteractionGroupSurvivesResize);
    runner.Add("SettingsLayout/HeaderHasThreeTabsSettingsActive", TestHeaderHasThreeTabsSettingsActive);
    runner.Add("SettingsLayout/SettingsHeaderShowsCanonicalWallet", TestSettingsHeaderShowsCanonicalWallet);
    runner.Add("SettingsLayout/FocusOrderIsNavThenRows", TestFocusOrderIsNavThenRows);
    runner.Add("SettingsLayout/SelectedSegmentReflectsPreferences", TestSelectedSegmentReflectsPreferences);
    runner.Add("SettingsLayout/LabelsLocalized", TestLabelsLocalized);
    runner.Add("SettingsLayout/OnlyLockAndClickCountingRowsHaveHint", TestOnlyLockAndClickCountingRowsHaveHint);
    runner.Add("SettingsLayout/HitTestReturnsSegmentAndNavIds", TestHitTestReturnsSegmentAndNavIds);
    runner.Add("SettingsLayout/FitsWithoutScrollBothLanguages", TestFitsWithoutScrollBothLanguages);
    runner.Add("SettingsLayout/NoOverlapWithinRows", TestNoOverlapWithinRows);
    runner.Add("SettingsLayout/SettingsAndQuickMenuAgreeOnSelection", TestSettingsAndQuickMenuAgreeOnSelection);
}

}  // namespace nimvlets::tests
