#include "QuickMenuModelTest.h"

#include "platform/QuickMenuModel.h"

#include <string>
#include <vector>

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

bool TestCollectionAndQuitArePresent() {
    const QuickMenuModel m = BuildQuickMenuModel(ShellState{});
    const MenuItem* collection = Find(m, ShellAction::kOpenCollection);
    NIMVLETS_CHECK(collection != nullptr);
    NIMVLETS_CHECK(collection->label == "Collection…");

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
    NIMVLETS_CHECK(separators == 3);  // pet name | acciones | tamaño/opacidad/lock | quit
    return true;
}

}  // namespace

void RegisterQuickMenuModelTests(testing::TestRunner& runner) {
    runner.Add("QuickMenuModel/HeaderShowsPetNameOrFallback", TestHeaderShowsPetNameOrFallback);
    runner.Add("QuickMenuModel/ShowHideLabelFollowsVisibility", TestShowHideLabelFollowsVisibility);
    runner.Add("QuickMenuModel/CollectionAndQuitArePresent", TestCollectionAndQuitArePresent);
    runner.Add("QuickMenuModel/LockPositionIsCheckable", TestLockPositionIsCheckable);
    runner.Add("QuickMenuModel/SizeSubmenuChecksExactlyOne", TestSizeSubmenuChecksExactlyOne);
    runner.Add("QuickMenuModel/OpacitySubmenuSnapsToNearestChoice", TestOpacitySubmenuSnapsToNearestChoice);
    runner.Add("QuickMenuModel/StructureHasSeparatorsAndNoEmptyLabels", TestStructureHasSeparatorsAndNoEmptyLabels);
}

}  // namespace nimvlets::tests
