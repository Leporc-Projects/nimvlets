#include "core/Localization.h"

#include <cstddef>

namespace nimvlets::core {

namespace {

// Tabla [clave][idioma]. Índice de idioma: 0 = kEn, 1 = kEs. El orden
// de filas DEBE seguir el de StringKey. `kCount` filas.
//
// Reglas del brief §4/§16: los nombres propios de pet y "Nimvlets"/
// "Nimvlet" nunca aparecen acá. "clicks" nunca se vuelve "monedas".
constexpr const char* kStrings[static_cast<std::size_t>(StringKey::kCount)][2] = {
    // {en, es}
    /* kCollection */         {"Collection", "Colección"},
    /* kYourCompanions */      {"Your companions", "Tus compañeros"},
    /* kCollectionOnlyActive */ {"Meet more Nimvlets in the Shop.", "Conoce más Nimvlets en la Tienda."},
    /* kOnDesktop */           {"On desktop", "En el escritorio"},
    /* kUse */                 {"Use", "Usar"},
    /* kUsePetPrefix */        {"Use ", "Usar "},
    /* kNotInCollection */     {"Not in your collection", "No está en tu colección"},
    /* kMale */                {"Male", "Macho"},
    /* kFemale */              {"Female", "Hembra"},
    /* kClickSingular */       {"click", "clic"},
    /* kClickPlural */         {"clicks", "clics"},

    /* kShop */                {"Shop", "Tienda"},
    /* kGetPetPrefix */        {"Get ", "Obtener "},
    /* kInYourCollection */    {"In your collection", "En tu colección"},
    /* kNeedMoreClicksOne */   {"Need 1 more click", "Te falta 1 clic"},
    /* kNeedMoreClicksMany */  {"Need {n} more clicks", "Te faltan {n} clics"},
    /* kCancel */              {"Cancel", "Cancelar"},
    /* kConfirm */             {"Confirm", "Confirmar"},
    /* kSpendPromptOne */      {"Spend 1 click to add {pet} to your collection?",
                               "¿Gastar 1 clic para añadir {pet} a tu colección?"},
    /* kSpendPromptMany */     {"Spend {n} clicks to add {pet} to your collection?",
                               "¿Gastar {n} clics para añadir {pet} a tu colección?"},
    /* kShopBrowseHeading */   {"Nimvlets you can meet", "Nimvlets que puedes conocer"},
    /* kShopEmpty */           {"No Nimvlets to show yet.", "Todavía no hay Nimvlets para mostrar."},

    /* kShowNimvlet */         {"Show Nimvlet", "Mostrar Nimvlet"},
    /* kHideNimvlet */         {"Hide Nimvlet", "Ocultar Nimvlet"},
    /* kCollectionMenuItem */  {"Collection…", "Colección…"},
    /* kSize */                {"Size", "Tamaño"},
    /* kSizeSmall */           {"Small", "Pequeño"},
    /* kSizeMedium */          {"Medium", "Mediano"},
    /* kSizeLarge */           {"Large", "Grande"},
    /* kOpacity */             {"Opacity", "Opacidad"},
    /* kLockPosition */        {"Lock Position", "Bloquear posición"},
    /* kLanguage */            {"Language", "Idioma"},
    /* kQuitNimvlets */        {"Quit Nimvlets", "Salir de Nimvlets"},

    /* kSettings */            {"Settings", "Ajustes"},
    /* kSettingsCompanion */   {"Companion", "Compañero"},
    /* kLockPositionHint */    {"Keeps your Nimvlet from being moved by dragging.",
                               "Evita que puedas mover tu Nimvlet al arrastrarlo."},
    /* kOn */                  {"On", "Activado"},
    /* kOff */                 {"Off", "Desactivado"},

    /* kOnboardingChooseFirst */        {"Choose your first Nimvlet", "Elige tu primer Nimvlet"},
    /* kOnboardingConfirmStarter */     {"Make {pet} your first Nimvlet?",
                                        "¿Quieres que {pet} sea tu primer Nimvlet?"},
    /* kOnboardingConfirmChoosePrefix */ {"Choose ", "Elegir a "},
    /* kOnboardingWhichVariant */       {"Which Frin?", "¿Qué Frin?"},
};

}  // namespace

const char* LanguageId(Language lang) {
    return lang == Language::kEs ? "es" : "en";
}

Language ParseLanguage(std::string_view id) {
    return id == "es" ? Language::kEs : Language::kEn;  // desconocido/vacío -> en
}

const char* LanguageEndonym(Language lang) {
    return lang == Language::kEs ? "Español" : "English";
}

const char* Localized(StringKey key, Language lang) {
    const auto row = static_cast<std::size_t>(key);
    if (row >= static_cast<std::size_t>(StringKey::kCount)) {
        return "";
    }
    return kStrings[row][lang == Language::kEs ? 1 : 0];
}

}  // namespace nimvlets::core
