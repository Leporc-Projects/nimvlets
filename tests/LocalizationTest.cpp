#include "LocalizationTest.h"

#include "core/Localization.h"

#include <cstring>
#include <string>

using nimvlets::core::Language;
using nimvlets::core::LanguageEndonym;
using nimvlets::core::LanguageId;
using nimvlets::core::Localized;
using nimvlets::core::ParseLanguage;
using nimvlets::core::StringKey;

namespace nimvlets::tests {

namespace {

bool Eq(const char* a, const char* b) { return a != nullptr && b != nullptr && std::strcmp(a, b) == 0; }

bool TestLanguageIdRoundTrips() {
    NIMVLETS_CHECK(Eq(LanguageId(Language::kEn), "en"));
    NIMVLETS_CHECK(Eq(LanguageId(Language::kEs), "es"));
    NIMVLETS_CHECK(ParseLanguage(LanguageId(Language::kEn)) == Language::kEn);
    NIMVLETS_CHECK(ParseLanguage(LanguageId(Language::kEs)) == Language::kEs);
    return true;
}

// Cualquier id desconocido / vacío -> inglés (brief §5: "Otherwise
// default to English").
bool TestUnknownLanguageFallsBackToEnglish() {
    NIMVLETS_CHECK(ParseLanguage("") == Language::kEn);
    NIMVLETS_CHECK(ParseLanguage("fr") == Language::kEn);
    NIMVLETS_CHECK(ParseLanguage("EN") == Language::kEn);  // case-sensitive por diseño
    NIMVLETS_CHECK(ParseLanguage("english") == Language::kEn);
    return true;
}

// Los endónimos se muestran SIEMPRE en su propio idioma, sin importar
// el idioma activo (convención de selectores de idioma).
bool TestLanguageEndonyms() {
    NIMVLETS_CHECK(Eq(LanguageEndonym(Language::kEn), "English"));
    NIMVLETS_CHECK(Eq(LanguageEndonym(Language::kEs), "Español"));
    return true;
}

bool TestEnglishMenuStrings() {
    NIMVLETS_CHECK(Eq(Localized(StringKey::kShowNimvlet, Language::kEn), "Show Nimvlet"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kHideNimvlet, Language::kEn), "Hide Nimvlet"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kCollectionMenuItem, Language::kEn), "Collection…"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kSize, Language::kEn), "Size"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kOpacity, Language::kEn), "Opacity"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kLockPosition, Language::kEn), "Lock Position"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kLanguage, Language::kEn), "Language"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kQuitNimvlets, Language::kEn), "Quit Nimvlets"));
    return true;
}

bool TestSpanishMenuStrings() {
    NIMVLETS_CHECK(Eq(Localized(StringKey::kShowNimvlet, Language::kEs), "Mostrar Nimvlet"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kHideNimvlet, Language::kEs), "Ocultar Nimvlet"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kCollectionMenuItem, Language::kEs), "Colección…"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kSize, Language::kEs), "Tamaño"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kOpacity, Language::kEs), "Opacidad"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kLockPosition, Language::kEs), "Bloquear posición"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kLanguage, Language::kEs), "Idioma"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kQuitNimvlets, Language::kEs), "Salir de Nimvlets"));
    return true;
}

// Estados de la Collection (brief §11): humanos, nunca badges.
bool TestCollectionStatusStrings() {
    NIMVLETS_CHECK(Eq(Localized(StringKey::kOnDesktop, Language::kEn), "On desktop"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kOnDesktop, Language::kEs), "En el escritorio"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kUse, Language::kEn), "Use"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kUse, Language::kEs), "Usar"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kNotInCollection, Language::kEn), "Not in your collection"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kNotInCollection, Language::kEs), "No está en tu colección"));
    return true;
}

// Frin: Male/Female vs Macho/Hembra (brief §13).
bool TestVariantLabels() {
    NIMVLETS_CHECK(Eq(Localized(StringKey::kMale, Language::kEn), "Male"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kFemale, Language::kEn), "Female"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kMale, Language::kEs), "Macho"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kFemale, Language::kEs), "Hembra"));
    return true;
}

// click/clicks (en) — clic/clics (es). NUNCA "coins"/"monedas"
// (brief §16).
bool TestClickWording() {
    NIMVLETS_CHECK(Eq(Localized(StringKey::kClickSingular, Language::kEn), "click"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kClickPlural, Language::kEn), "clicks"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kClickSingular, Language::kEs), "clic"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kClickPlural, Language::kEs), "clics"));
    return true;
}

// Shop + wallet (Block 07). "Shop" SÍ se traduce; los nombres propios
// (que se concatenan con kGetPetPrefix) no.
bool TestShopStrings() {
    NIMVLETS_CHECK(Eq(Localized(StringKey::kShop, Language::kEn), "Shop"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kShop, Language::kEs), "Tienda"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kGetPetPrefix, Language::kEn), "Get "));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kGetPetPrefix, Language::kEs), "Obtener "));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kInYourCollection, Language::kEn), "In your collection"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kInYourCollection, Language::kEs), "En tu colección"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kCancel, Language::kEn), "Cancel"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kCancel, Language::kEs), "Cancelar"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kConfirm, Language::kEn), "Confirm"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kConfirm, Language::kEs), "Confirmar"));
    // Plantillas con placeholder — Format las rellena; acá solo se
    // verifica que el placeholder está presente y la frase se tradujo.
    NIMVLETS_CHECK(std::string(Localized(StringKey::kNeedMoreClicksMany, Language::kEn)).find("{n}") !=
                   std::string::npos);
    NIMVLETS_CHECK(std::string(Localized(StringKey::kSpendPromptMany, Language::kEs)).find("{pet}") !=
                   std::string::npos);
    NIMVLETS_CHECK(std::string(Localized(StringKey::kSpendPromptMany, Language::kEs)).find("Gastar") !=
                   std::string::npos);
    return true;
}

// Settings (Block 08). "Settings" SÍ se traduce; los nombres de idioma
// del selector son endónimos (probados arriba), no una clave nueva.
bool TestSettingsStrings() {
    NIMVLETS_CHECK(Eq(Localized(StringKey::kSettings, Language::kEn), "Settings"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kSettings, Language::kEs), "Ajustes"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kSettingsCompanion, Language::kEn), "Companion"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kSettingsCompanion, Language::kEs), "Compañero"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kOn, Language::kEn), "On"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kOn, Language::kEs), "Activado"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kOff, Language::kEn), "Off"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kOff, Language::kEs), "Desactivado"));
    // El hint de Lock position: frase corta, traducida, sin placeholder.
    NIMVLETS_CHECK(std::string(Localized(StringKey::kLockPositionHint, Language::kEn)).size() > 10);
    NIMVLETS_CHECK(std::string(Localized(StringKey::kLockPositionHint, Language::kEs)).find("arrastr") !=
                   std::string::npos);
    // Settings reusa kSize..kLanguage del menú rápido — se prueban en
    // TestEnglishMenuStrings / TestSpanishMenuStrings.
    return true;
}

// Onboarding de primer arranque (Block 09A). Los nombres propios
// (Nimvlet, Artu, Rato, Rin Rin, Frin) NO se traducen; se concatenan
// con kOnboardingConfirmChoosePrefix o se sustituyen en {pet}.
bool TestOnboardingStrings() {
    NIMVLETS_CHECK(Eq(Localized(StringKey::kOnboardingChooseFirst, Language::kEn),
                      "Choose your first Nimvlet"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kOnboardingChooseFirst, Language::kEs),
                      "Elige tu primer Nimvlet"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kOnboardingConfirmChoosePrefix, Language::kEn), "Choose "));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kOnboardingConfirmChoosePrefix, Language::kEs), "Elegir a "));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kOnboardingWhichVariant, Language::kEn), "Which Frin?"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kOnboardingWhichVariant, Language::kEs), "¿Qué Frin?"));
    // La plantilla del confirm lleva {pet} en los dos idiomas.
    NIMVLETS_CHECK(std::string(Localized(StringKey::kOnboardingConfirmStarter, Language::kEn))
                       .find("{pet}") != std::string::npos);
    NIMVLETS_CHECK(std::string(Localized(StringKey::kOnboardingConfirmStarter, Language::kEs))
                       .find("{pet}") != std::string::npos);
    return true;
}

// Shop oculto de starters (Block 10). "Shop" SÍ se traduce ("Tienda");
// los nombres propios (Frin) no. El acceso desde el Shop público es un
// HOTSPOT INVISIBLE (corrección de QA del owner) — NO hay una clave de
// "afordancia visible": estas 3 son SOLO texto DENTRO del submodo.
bool TestStarterShopStrings() {
    NIMVLETS_CHECK(Eq(Localized(StringKey::kStarterChoicesHeading, Language::kEn), "Starter choices"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kStarterChoicesHeading, Language::kEs), "Opciones iniciales"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kStarterShopBack, Language::kEn), "\xE2\x86\x90 Shop"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kStarterShopBack, Language::kEs), "\xE2\x86\x90 Tienda"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kStarterShopEmpty, Language::kEn), "No more starter choices."));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kStarterShopEmpty, Language::kEs),
                      "No quedan opciones iniciales."));
    return true;
}

// El header de la Collection.
bool TestCollectionHeaderStrings() {
    NIMVLETS_CHECK(Eq(Localized(StringKey::kCollection, Language::kEn), "Collection"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kCollection, Language::kEs), "Colección"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kYourCompanions, Language::kEn), "Your companions"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kYourCompanions, Language::kEs), "Tus compañeros"));
    return true;
}

// Toda clave devuelve un string no vacío en los dos idiomas, y EN != ES
// para toda clave traducible salvo casos donde coinciden a propósito
// (ninguno hoy). También: ninguna traducción menciona "coin"/"moneda".
bool TestEveryKeyIsPopulatedAndDistinct() {
    for (int i = 0; i < static_cast<int>(StringKey::kCount); ++i) {
        const auto key = static_cast<StringKey>(i);
        const char* en = Localized(key, Language::kEn);
        const char* es = Localized(key, Language::kEs);
        NIMVLETS_CHECK(en != nullptr && en[0] != '\0');
        NIMVLETS_CHECK(es != nullptr && es[0] != '\0');
        NIMVLETS_CHECK(std::string(en).find("coin") == std::string::npos);
        NIMVLETS_CHECK(std::string(es).find("moneda") == std::string::npos);
        // Todas las claves de este bloque tienen traducción distinta;
        // "Use "/"Usar " y el resto difieren.
        NIMVLETS_CHECK(std::strcmp(en, es) != 0);
    }
    return true;
}

// --- Block 11A: Interaction / conteo de clics global --------------

bool TestInteractionStrings() {
    NIMVLETS_CHECK(Eq(Localized(StringKey::kSettingsInteraction, Language::kEn), "Interaction"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kSettingsInteraction, Language::kEs), "Interacción"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kClickCounting, Language::kEn), "Click counting"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kClickCounting, Language::kEs), "Conteo de clics"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kClickCountingNimvletOnly, Language::kEn), "Nimvlet only"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kClickCountingNimvletOnly, Language::kEs), "Solo el Nimvlet"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kClickCountingAnywhere, Language::kEn), "Anywhere"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kClickCountingAnywhere, Language::kEs), "En cualquier lugar"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kGlobalClickContinue, Language::kEn), "Continue"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kGlobalClickContinue, Language::kEs), "Continuar"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kGlobalClickNotNow, Language::kEn), "Not now"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kGlobalClickNotNow, Language::kEs), "Ahora no"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kGlobalClickCheckAgain, Language::kEn), "Check again"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kGlobalClickCheckAgain, Language::kEs), "Comprobar de nuevo"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kGlobalClickActive, Language::kEn), "Active"));
    NIMVLETS_CHECK(Eq(Localized(StringKey::kGlobalClickActive, Language::kEs), "Activo"));
    NIMVLETS_CHECK(
        Eq(Localized(StringKey::kGlobalClickUnavailable, Language::kEn), "Not available on this system"));
    NIMVLETS_CHECK(
        Eq(Localized(StringKey::kGlobalClickUnavailable, Language::kEs), "No disponible en este sistema"));

    // "Nimvlet" es marca: NO se traduce ni en la etiqueta del segmento.
    for (const Language lang : {Language::kEn, Language::kEs}) {
        NIMVLETS_CHECK(std::string(Localized(StringKey::kClickCountingNimvletOnly, lang))
                           .find("Nimvlet") != std::string::npos);
    }
    return true;
}

// Las claves que nombran el permiso del OS llevan el placeholder
// "{permission}" en LOS DOS idiomas — es lo que deja que el adapter
// aporte "Input Monitoring" sin que src/productui conozca la plataforma
// (brief §18). Si una traducción se olvidara del token, el usuario
// leería una frase incompleta.
bool TestPermissionPlaceholderPresentInBothLanguages() {
    const StringKey withToken[] = {
        StringKey::kGlobalClickExplain,
        StringKey::kGlobalClickPermissionNeeded,
        StringKey::kGlobalClickGrantHint,
    };
    for (const StringKey key : withToken) {
        for (const Language lang : {Language::kEn, Language::kEs}) {
            NIMVLETS_CHECK(std::string(Localized(key, lang)).find("{permission}") != std::string::npos);
        }
    }

    // Y la explicación de privacidad dice, en los dos idiomas, qué NO se
    // observa nunca — es el contenido del brief §8, no decoración.
    NIMVLETS_CHECK(std::string(Localized(StringKey::kGlobalClickExplain, Language::kEn))
                       .find("never keys") != std::string::npos);
    NIMVLETS_CHECK(
        std::string(Localized(StringKey::kGlobalClickExplain, Language::kEs)).find("nunca teclas") !=
        std::string::npos);
    return true;
}

}  // namespace

void RegisterLocalizationTests(testing::TestRunner& runner) {
    runner.Add("Localization/InteractionStrings", TestInteractionStrings);
    runner.Add("Localization/PermissionPlaceholderPresentInBothLanguages",
               TestPermissionPlaceholderPresentInBothLanguages);
    runner.Add("Localization/LanguageIdRoundTrips", TestLanguageIdRoundTrips);
    runner.Add("Localization/UnknownLanguageFallsBackToEnglish", TestUnknownLanguageFallsBackToEnglish);
    runner.Add("Localization/LanguageEndonyms", TestLanguageEndonyms);
    runner.Add("Localization/EnglishMenuStrings", TestEnglishMenuStrings);
    runner.Add("Localization/SpanishMenuStrings", TestSpanishMenuStrings);
    runner.Add("Localization/CollectionStatusStrings", TestCollectionStatusStrings);
    runner.Add("Localization/VariantLabels", TestVariantLabels);
    runner.Add("Localization/ClickWording", TestClickWording);
    runner.Add("Localization/CollectionHeaderStrings", TestCollectionHeaderStrings);
    runner.Add("Localization/ShopStrings", TestShopStrings);
    runner.Add("Localization/SettingsStrings", TestSettingsStrings);
    runner.Add("Localization/OnboardingStrings", TestOnboardingStrings);
    runner.Add("Localization/StarterShopStrings", TestStarterShopStrings);
    runner.Add("Localization/EveryKeyIsPopulatedAndDistinct", TestEveryKeyIsPopulatedAndDistinct);
}

}  // namespace nimvlets::tests
