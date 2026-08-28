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

}  // namespace

void RegisterLocalizationTests(testing::TestRunner& runner) {
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
    runner.Add("Localization/EveryKeyIsPopulatedAndDistinct", TestEveryKeyIsPopulatedAndDistinct);
}

}  // namespace nimvlets::tests
