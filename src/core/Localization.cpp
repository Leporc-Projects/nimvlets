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
    /* kOpenNimvletsMenuItem */ {"Open Nimvlets…", "Abrir Nimvlets…"},
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

    /* kVisibility */          {"Visibility", "Visibilidad"},
    /* kVisibilityShown */     {"Shown", "Visible"},
    /* kVisibilityHidden */    {"Hidden", "Oculto"},
    /* kPosition */            {"Position", "Posición"},
    /* kResetPosition */       {"Reset position", "Restablecer posición"},
    /* kPositionUnavailable */ {"Position can't be reset on this system.",
                               "La posición no se puede restablecer en este sistema."},

    /* kOnboardingChooseFirst */        {"Choose your first Nimvlet", "Elige tu primer Nimvlet"},
    /* kOnboardingConfirmStarter */     {"Make {pet} your first Nimvlet?",
                                        "¿Quieres que {pet} sea tu primer Nimvlet?"},
    /* kOnboardingConfirmChoosePrefix */ {"Choose ", "Elegir a "},
    /* kOnboardingWhichVariant */       {"Which Frin?", "¿Qué Frin?"},

    /* kStarterChoicesHeading */        {"Starter choices", "Opciones iniciales"},
    /* kStarterShopBack */              {"\xE2\x86\x90 Shop", "\xE2\x86\x90 Tienda"},
    /* kStarterShopEmpty */             {"No more starter choices.", "No quedan opciones iniciales."},

    /* kSettingsInteraction */          {"Interaction", "Interacción"},
    /* kClickCounting */                {"Click counting", "Conteo de clics"},
    /* kClickCountingNimvletOnly */     {"Nimvlet only", "Solo el Nimvlet"},
    /* kClickCountingAnywhere */        {"Anywhere", "En cualquier lugar"},
    /* kClickCountingHint */            {"Where a click has to happen for it to count.",
                                        "Dónde tiene que ocurrir un clic para que cuente."},
    /* kGlobalClickExplain */           {"To count clicks outside Nimvlets, your system needs {permission}. "
                                        "Your system may describe that permission broadly \xE2\x80\x94 even as "
                                        "keyboard or keystroke access; that wording covers the whole permission, "
                                        "not what Nimvlets does. "
                                        "Nimvlets only counts primary mouse presses \xE2\x80\x94 never keys, "
                                        "pointer positions, apps, or screen content.",
                                        "Para contar clics fuera de Nimvlets, tu sistema necesita {permission}. "
                                        "Puede que tu sistema describa ese permiso de forma muy amplia, incluso "
                                        "como acceso al teclado: esa redacción abarca todo el permiso, no lo que "
                                        "hace Nimvlets. "
                                        "Nimvlets solo cuenta pulsaciones del botón principal: nunca teclas, "
                                        "posiciones del puntero, apps ni el contenido de la pantalla."},
    /* kGlobalClickContinue */          {"Continue", "Continuar"},
    /* kGlobalClickNotNow */            {"Not now", "Ahora no"},
    /* kGlobalClickCheckAgain */        {"Check again", "Comprobar de nuevo"},
    /* kGlobalClickActive */            {"Active", "Activo"},
    /* kGlobalClickPermissionNeeded */  {"{permission} permission needed", "Falta el permiso {permission}"},
    /* kGlobalClickUnavailable */       {"Not available on this system", "No disponible en este sistema"},
    /* kGlobalClickFailed */            {"Could not start", "No se pudo iniciar"},
    /* kGlobalClickGrantHint */         {"Turn Nimvlets on under {permission} in System Settings, "
                                        "then check again.",
                                        "Activa Nimvlets en {permission}, dentro de Ajustes del Sistema, "
                                        "y vuelve a comprobar."},
    /* kGlobalClickDragNote */          {"A press counts once, even if it becomes a drag.",
                                        "Una pulsación cuenta una vez, aunque se convierta en un arrastre."},
    /* kGlobalClickMouseOnly */         {"Nimvlets listens only for primary mouse presses, whatever the "
                                        "system permission is called.",
                                        "Nimvlets solo escucha pulsaciones del botón principal, se llame como "
                                        "se llame el permiso del sistema."},
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
