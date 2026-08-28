#include "platform/QuickMenuModel.h"

#include "core/DisplayControls.h"
#include "core/Localization.h"

namespace nimvlets::platform {

namespace {

using core::Language;
using core::Localized;
using core::StringKey;

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
    const Language lang = state.language;
    const core::PetSizeChoice size = core::ParsePetSizeChoice(state.sizeChoiceId);
    const int opacity = core::NormalizeOpacityPercent(state.opacityPercent);

    QuickMenuModel model;
    // El nombre del pet es un nombre propio: no se traduce (brief §4).
    model.items.push_back(Header(state.currentPetName.empty() ? "Nimvlets" : state.currentPetName));
    model.items.push_back(Separator());

    model.items.push_back(Action(
        Localized(state.petHidden ? StringKey::kShowNimvlet : StringKey::kHideNimvlet, lang),
        ShellAction::kTogglePetVisibility));
    model.items.push_back(Action(Localized(StringKey::kCollectionMenuItem, lang), ShellAction::kOpenCollection));
    model.items.push_back(Separator());

    model.items.push_back(Submenu(Localized(StringKey::kSize, lang), {
        Check(Localized(StringKey::kSizeSmall, lang), ShellAction::kSetSizeSmall, size == core::PetSizeChoice::kSmall),
        Check(Localized(StringKey::kSizeMedium, lang), ShellAction::kSetSizeMedium, size == core::PetSizeChoice::kMedium),
        Check(Localized(StringKey::kSizeLarge, lang), ShellAction::kSetSizeLarge, size == core::PetSizeChoice::kLarge),
    }));
    // Los porcentajes de opacidad son numéricos — no se traducen.
    model.items.push_back(Submenu(Localized(StringKey::kOpacity, lang), {
        Check("100%", ShellAction::kSetOpacity100, opacity == 100),
        Check("85%", ShellAction::kSetOpacity85, opacity == 85),
        Check("70%", ShellAction::kSetOpacity70, opacity == 70),
        Check("55%", ShellAction::kSetOpacity55, opacity == 55),
    }));
    model.items.push_back(
        Check(Localized(StringKey::kLockPosition, lang), ShellAction::kToggleLockPosition, state.lockPosition));

    // Language ▸ — los nombres de idioma van SIEMPRE en su propio idioma
    // (endónimos), independientes del idioma activo.
    model.items.push_back(Submenu(Localized(StringKey::kLanguage, lang), {
        Check(core::LanguageEndonym(Language::kEn), ShellAction::kSetLanguageEn, lang == Language::kEn),
        Check(core::LanguageEndonym(Language::kEs), ShellAction::kSetLanguageEs, lang == Language::kEs),
    }));
    model.items.push_back(Separator());

    model.items.push_back(Action(Localized(StringKey::kQuitNimvlets, lang), ShellAction::kQuit));
    return model;
}

}  // namespace nimvlets::platform
