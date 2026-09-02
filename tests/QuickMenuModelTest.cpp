#include "QuickMenuModelTest.h"

#include "core/Localization.h"
#include "platform/QuickMenuModel.h"

#include <string>
#include <vector>

using nimvlets::core::Language;
using nimvlets::platform::BuildQuickMenuModel;
using nimvlets::platform::MenuItem;
using nimvlets::platform::MenuItemKind;
using nimvlets::platform::QuickMenuModel;
using nimvlets::platform::ShellAction;
using nimvlets::platform::ShellState;

namespace nimvlets::tests {

namespace {

const MenuItem* Find(const QuickMenuModel& m, ShellAction action) {
    for (const MenuItem& it : m.items) {
        if ((it.kind == MenuItemKind::kAction || it.kind == MenuItemKind::kCheckable) && it.action == action) {
            return &it;
        }
        if (it.kind == MenuItemKind::kSubmenu) {
            for (const MenuItem& sub : it.submenu) {
                if (sub.action == action) {
                    return &sub;
                }
            }
        }
    }
    return nullptr;
}

const MenuItem* FindSubmenu(const QuickMenuModel& m, const std::string& label) {
    for (const MenuItem& it : m.items) {
        if (it.kind == MenuItemKind::kSubmenu && it.label == label) {
            return &it;
        }
    }
    return nullptr;
}

bool TestHeaderShowsPetNameOrFallback() {
    ShellState s;
    s.currentPetName = "Nidir";
    QuickMenuModel m = BuildQuickMenuModel(s);
    NIMVLETS_CHECK(!m.items.empty());
    NIMVLETS_CHECK(m.items[0].kind == MenuItemKind::kHeader);
    NIMVLETS_CHECK(m.items[0].label == "Nidir");
    NIMVLETS_CHECK(!m.items[0].enabled);

    s.currentPetName.clear();
    QuickMenuModel fallback = BuildQuickMenuModel(s);
    NIMVLETS_CHECK(fallback.items[0].label == "Nimvlets");
    return true;
}

// Block brief §14/§17: Show/Hide es sobre el pet, y su etiqueta refleja
// el estado.
bool TestShowHideLabelFollowsVisibility() {
    ShellState shown;
    shown.petHidden = false;
    NIMVLETS_CHECK(Find(BuildQuickMenuModel(shown), ShellAction::kTogglePetVisibility)->label == "Hide Nimvlet");

    ShellState hidden;
    hidden.petHidden = true;
    NIMVLETS_CHECK(Find(BuildQuickMenuModel(hidden), ShellAction::kTogglePetVisibility)->label == "Show Nimvlet");
    return true;
}

bool TestOpenProductUiAndQuitArePresent() {
    const QuickMenuModel m = BuildQuickMenuModel(ShellState{});
    // Block 11B: el item que abre el Product UI se llama "Open Nimvlets…"
    // (antes "Collection…"), y su acción es kOpenProductUi (antes
    // kOpenCollection). La ventana ya trae Collection · Shop · Settings.
    const MenuItem* openUi = Find(m, ShellAction::kOpenProductUi);
    NIMVLETS_CHECK(openUi != nullptr);
    NIMVLETS_CHECK(openUi->label == "Open Nimvlets…");

    // Quit es el último item accionable.
    NIMVLETS_CHECK(m.items.back().kind == MenuItemKind::kAction);
    NIMVLETS_CHECK(m.items.back().action == ShellAction::kQuit);
    NIMVLETS_CHECK(m.items.back().label == "Quit Nimvlets");
    return true;
}

bool TestLockPositionIsCheckable() {
    const QuickMenuModel off = BuildQuickMenuModel(ShellState{});
    const MenuItem* a = Find(off, ShellAction::kToggleLockPosition);
    NIMVLETS_CHECK(a != nullptr);
    NIMVLETS_CHECK(a->kind == MenuItemKind::kCheckable);
    NIMVLETS_CHECK(!a->checked);

    ShellState onState;
    onState.lockPosition = true;
    const QuickMenuModel on = BuildQuickMenuModel(onState);
    NIMVLETS_CHECK(Find(on, ShellAction::kToggleLockPosition)->checked);
    return true;
}

// El submenú de tamaño tiene exactamente un item marcado, el que
// corresponde a sizeChoiceId, y un id desconocido cae a "Medium".
bool TestSizeSubmenuChecksExactlyOne() {
    ShellState s;
    s.sizeChoiceId = "large";
    const QuickMenuModel large = BuildQuickMenuModel(s);
    const MenuItem* sizeMenu = FindSubmenu(large, "Size");
    NIMVLETS_CHECK(sizeMenu != nullptr);
    NIMVLETS_CHECK(sizeMenu->submenu.size() == 3);
    int checked = 0;
    for (const MenuItem& it : sizeMenu->submenu) {
        if (it.checked) {
            ++checked;
            NIMVLETS_CHECK(it.action == ShellAction::kSetSizeLarge);
        }
    }
    NIMVLETS_CHECK(checked == 1);

    s.sizeChoiceId = "bogus";
    const QuickMenuModel fallback = BuildQuickMenuModel(s);
    const MenuItem* fb = FindSubmenu(fallback, "Size");
    NIMVLETS_CHECK(fb->submenu[1].action == ShellAction::kSetSizeMedium);
    NIMVLETS_CHECK(fb->submenu[1].checked);
    return true;
}

bool TestOpacitySubmenuSnapsToNearestChoice() {
    ShellState s;
    s.opacityPercent = 78;  // más cerca de 85
    const QuickMenuModel m = BuildQuickMenuModel(s);
    const MenuItem* op = FindSubmenu(m, "Opacity");
    NIMVLETS_CHECK(op != nullptr);
    NIMVLETS_CHECK(op->submenu.size() == 4);
    int checked = 0;
    for (const MenuItem& it : op->submenu) {
        if (it.checked) {
            ++checked;
            NIMVLETS_CHECK(it.action == ShellAction::kSetOpacity85);
        }
    }
    NIMVLETS_CHECK(checked == 1);
    return true;
}

// Estructura: separadores entre grupos, y ningún item accionable con
// etiqueta vacía.
bool TestStructureHasSeparatorsAndNoEmptyLabels() {
    const QuickMenuModel m = BuildQuickMenuModel(ShellState{});
    int separators = 0;
    for (const MenuItem& it : m.items) {
        if (it.kind == MenuItemKind::kSeparator) {
            ++separators;
        } else {
            NIMVLETS_CHECK(!it.label.empty());
        }
    }
    NIMVLETS_CHECK(separators == 3);  // pet name | acciones | tamaño/opacidad/lock/idioma | quit
    return true;
}

// --- Block 06.1: localización + submenú Language ------------------

// Con state.language = kEs, TODAS las etiquetas traducibles cambian.
bool TestSpanishRelabelsEverything() {
    ShellState s;
    s.language = Language::kEs;
    const QuickMenuModel m = BuildQuickMenuModel(s);

    NIMVLETS_CHECK(Find(m, ShellAction::kTogglePetVisibility)->label == "Ocultar Nimvlet");
    NIMVLETS_CHECK(Find(m, ShellAction::kOpenProductUi)->label == "Abrir Nimvlets…");
    NIMVLETS_CHECK(Find(m, ShellAction::kToggleLockPosition)->label == "Bloquear posición");
    NIMVLETS_CHECK(FindSubmenu(m, "Tamaño") != nullptr);
    NIMVLETS_CHECK(FindSubmenu(m, "Opacidad") != nullptr);
    NIMVLETS_CHECK(FindSubmenu(m, "Idioma") != nullptr);
    NIMVLETS_CHECK(m.items.back().label == "Salir de Nimvlets");
    // El nombre del pet es un nombre propio: NO se traduce.
    s.currentPetName = "Nidir";
    NIMVLETS_CHECK(BuildQuickMenuModel(s).items[0].label == "Nidir");
    return true;
}

// Cambiar state.petHidden en español.
bool TestShowHideLabelSpanish() {
    ShellState s;
    s.language = Language::kEs;
    s.petHidden = true;
    NIMVLETS_CHECK(Find(BuildQuickMenuModel(s), ShellAction::kTogglePetVisibility)->label == "Mostrar Nimvlet");
    s.petHidden = false;
    NIMVLETS_CHECK(Find(BuildQuickMenuModel(s), ShellAction::kTogglePetVisibility)->label == "Ocultar Nimvlet");
    return true;
}

// El submenú Language: dos items (English / Español, endónimos), uno
// marcado según state.language, con las acciones correctas.
bool TestLanguageSubmenu() {
    ShellState en;
    en.language = Language::kEn;
    const QuickMenuModel me = BuildQuickMenuModel(en);
    const MenuItem* langMenu = FindSubmenu(me, "Language");
    NIMVLETS_CHECK(langMenu != nullptr);
    NIMVLETS_CHECK(langMenu->submenu.size() == 2);
    NIMVLETS_CHECK(langMenu->submenu[0].label == "English");
    NIMVLETS_CHECK(langMenu->submenu[0].action == ShellAction::kSetLanguageEn);
    NIMVLETS_CHECK(langMenu->submenu[0].checked);
    NIMVLETS_CHECK(langMenu->submenu[1].label == "Español");
    NIMVLETS_CHECK(langMenu->submenu[1].action == ShellAction::kSetLanguageEs);
    NIMVLETS_CHECK(!langMenu->submenu[1].checked);

    ShellState es;
    es.language = Language::kEs;
    const QuickMenuModel ms = BuildQuickMenuModel(es);
    const MenuItem* langMenuEs = FindSubmenu(ms, "Idioma");
    NIMVLETS_CHECK(langMenuEs != nullptr);
    // Endónimos: SIEMPRE en su propio idioma, no traducidos.
    NIMVLETS_CHECK(langMenuEs->submenu[0].label == "English");
    NIMVLETS_CHECK(langMenuEs->submenu[1].label == "Español");
    NIMVLETS_CHECK(!langMenuEs->submenu[0].checked);
    NIMVLETS_CHECK(langMenuEs->submenu[1].checked);
    return true;
}

// Block 11A, brief §10: el menú rápido NO gana "Click counting". Es una
// decisión de producto explícita —Settings se vuelve deliberadamente más
// capaz que el menú— y esta regresión existe para que agregar la
// preferencia a core::Preferences no la filtre al menú por inercia.
bool TestQuickMenuHasNoClickCountingOption() {
    for (const Language lang : {Language::kEn, Language::kEs}) {
        ShellState s;
        s.language = lang;
        s.currentPetName = "Nidir";
        const QuickMenuModel m = BuildQuickMenuModel(s);

        // Ninguna etiqueta, en ningún nivel, menciona el conteo de clics
        // ni el permiso que necesitaría.
        const char* forbidden[] = {"Click counting", "Conteo de clics", "Anywhere",
                                   "En cualquier lugar", "Input Monitoring", "Interaction",
                                   "Interacción"};
        for (const MenuItem& it : m.items) {
            for (const char* bad : forbidden) {
                NIMVLETS_CHECK(it.label.find(bad) == std::string::npos);
            }
            for (const MenuItem& sub : it.submenu) {
                for (const char* bad : forbidden) {
                    NIMVLETS_CHECK(sub.label.find(bad) == std::string::npos);
                }
            }
        }

        // Y el conjunto de submenús sigue siendo exactamente el de Block
        // 06.1: Size, Opacity, Language — ninguno nuevo.
        int submenus = 0;
        for (const MenuItem& it : m.items) {
            submenus += it.kind == MenuItemKind::kSubmenu ? 1 : 0;
        }
        NIMVLETS_CHECK(submenus == 3);
    }
    return true;
}

// Block 11B, brief §1/§14: Settings es la superficie de configuración
// COMPLETA; el menú rápido es un subconjunto deliberadamente chico. Los
// controles que 11B agrega SOLO a Settings —Visibility (Shown/Hidden) y
// Position (Reset position)— NUNCA aparecen en el menú. El menú conserva
// su Show/Hide de siempre (que es la MISMA visibilidad, por la misma
// ruta canónica), pero no gana "Reset position" ni un submenú nuevo.
bool TestQuickMenuStaysASmallSubsetOfSettings() {
    for (const Language lang : {Language::kEn, Language::kEs}) {
        ShellState s;
        s.language = lang;
        s.currentPetName = "Nidir";
        const QuickMenuModel m = BuildQuickMenuModel(s);

        // "Position" / "Posición" a secas NO se prohíben: "Lock Position"
        // / "Bloquear posición" son items legítimos del menú desde Block
        // 06. Se prohíben las etiquetas EXACTAS de los controles nuevos.
        const char* forbidden[] = {
            "Reset position", "Restablecer posición", "Visibility", "Visibilidad",
            "Shown",          "Hidden",               "Visible",
        };
        for (const MenuItem& it : m.items) {
            for (const char* bad : forbidden) {
                NIMVLETS_CHECK(it.label.find(bad) == std::string::npos);
            }
            for (const MenuItem& sub : it.submenu) {
                for (const char* bad : forbidden) {
                    NIMVLETS_CHECK(sub.label.find(bad) == std::string::npos);
                }
            }
        }

        // Sigue habiendo exactamente los tres submenús de Block 06.1
        // (Size / Opacity / Language) — 11B no agrega ninguno.
        int submenus = 0;
        for (const MenuItem& it : m.items) {
            submenus += it.kind == MenuItemKind::kSubmenu ? 1 : 0;
        }
        NIMVLETS_CHECK(submenus == 3);

        // Y el toggle de visibilidad de siempre sigue estando (es la
        // MISMA visibilidad transitoria que Settings — solo que el menú
        // la muestra como un toggle Show/Hide).
        NIMVLETS_CHECK(Find(m, ShellAction::kTogglePetVisibility) != nullptr);
    }
    return true;
}

}  // namespace

void RegisterQuickMenuModelTests(testing::TestRunner& runner) {
    runner.Add("QuickMenuModel/QuickMenuHasNoClickCountingOption", TestQuickMenuHasNoClickCountingOption);
    runner.Add("QuickMenuModel/QuickMenuStaysASmallSubsetOfSettings", TestQuickMenuStaysASmallSubsetOfSettings);
    runner.Add("QuickMenuModel/HeaderShowsPetNameOrFallback", TestHeaderShowsPetNameOrFallback);
    runner.Add("QuickMenuModel/ShowHideLabelFollowsVisibility", TestShowHideLabelFollowsVisibility);
    runner.Add("QuickMenuModel/OpenProductUiAndQuitArePresent", TestOpenProductUiAndQuitArePresent);
    runner.Add("QuickMenuModel/LockPositionIsCheckable", TestLockPositionIsCheckable);
    runner.Add("QuickMenuModel/SizeSubmenuChecksExactlyOne", TestSizeSubmenuChecksExactlyOne);
    runner.Add("QuickMenuModel/OpacitySubmenuSnapsToNearestChoice", TestOpacitySubmenuSnapsToNearestChoice);
    runner.Add("QuickMenuModel/StructureHasSeparatorsAndNoEmptyLabels", TestStructureHasSeparatorsAndNoEmptyLabels);
    runner.Add("QuickMenuModel/SpanishRelabelsEverything", TestSpanishRelabelsEverything);
    runner.Add("QuickMenuModel/ShowHideLabelSpanish", TestShowHideLabelSpanish);
    runner.Add("QuickMenuModel/LanguageSubmenu", TestLanguageSubmenu);
}

}  // namespace nimvlets::tests
