#include "platform/QuickMenuModel.h"

#include "core/DisplayControls.h"

namespace nimvlets::platform {

namespace {

MenuItem Header(const std::string& label) {
    MenuItem it;
    it.kind = MenuItemKind::kHeader;
    it.label = label;
    it.enabled = false;
    return it;
}

MenuItem Separator() {
    MenuItem it;
    it.kind = MenuItemKind::kSeparator;
    return it;
}

MenuItem Action(const std::string& label, ShellAction action) {
    MenuItem it;
    it.kind = MenuItemKind::kAction;
    it.label = label;
    it.action = action;
    return it;
}

MenuItem Check(const std::string& label, ShellAction action, bool checked) {
    MenuItem it;
    it.kind = MenuItemKind::kCheckable;
    it.label = label;
    it.action = action;
    it.checked = checked;
    return it;
}

MenuItem Submenu(const std::string& label, std::vector<MenuItem> children) {
    MenuItem it;
    it.kind = MenuItemKind::kSubmenu;
    it.label = label;
    it.submenu = std::move(children);
    return it;
}

}  // namespace

QuickMenuModel BuildQuickMenuModel(const ShellState& state) {
    const core::PetSizeChoice size = core::ParsePetSizeChoice(state.sizeChoiceId);
    const int opacity = core::NormalizeOpacityPercent(state.opacityPercent);

    QuickMenuModel model;
    model.items.push_back(Header(state.currentPetName.empty() ? "Nimvlets" : state.currentPetName));
    model.items.push_back(Separator());

    model.items.push_back(Action(state.petHidden ? "Show Nimvlet" : "Hide Nimvlet",
                                 ShellAction::kTogglePetVisibility));
    model.items.push_back(Action("Collection…", ShellAction::kOpenCollection));
    model.items.push_back(Separator());

    model.items.push_back(Submenu("Size", {
        Check("Small", ShellAction::kSetSizeSmall, size == core::PetSizeChoice::kSmall),
        Check("Medium", ShellAction::kSetSizeMedium, size == core::PetSizeChoice::kMedium),
        Check("Large", ShellAction::kSetSizeLarge, size == core::PetSizeChoice::kLarge),
    }));
    model.items.push_back(Submenu("Opacity", {
        Check("100%", ShellAction::kSetOpacity100, opacity == 100),
        Check("85%", ShellAction::kSetOpacity85, opacity == 85),
        Check("70%", ShellAction::kSetOpacity70, opacity == 70),
        Check("55%", ShellAction::kSetOpacity55, opacity == 55),
    }));
    model.items.push_back(Check("Lock Position", ShellAction::kToggleLockPosition, state.lockPosition));
    model.items.push_back(Separator());

    model.items.push_back(Action("Quit Nimvlets", ShellAction::kQuit));
    return model;
}

}  // namespace nimvlets::platform
