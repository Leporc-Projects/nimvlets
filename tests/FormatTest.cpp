#include "FormatTest.h"

#include "core/Localization.h"
#include "productui/Format.h"

#include <string>

using nimvlets::core::Language;
using nimvlets::productui::FormatClickCount;
using nimvlets::productui::FormatGroupedNumber;
using nimvlets::productui::FormatNeedMoreClicks;
using nimvlets::productui::FormatSpendPrompt;
using nimvlets::productui::FormatWithPermission;
namespace core = nimvlets::core;

namespace nimvlets::tests {

namespace {

bool TestGroupedNumber() {
    NIMVLETS_CHECK(FormatGroupedNumber(0) == "0");
    NIMVLETS_CHECK(FormatGroupedNumber(7) == "7");
    NIMVLETS_CHECK(FormatGroupedNumber(42) == "42");
    NIMVLETS_CHECK(FormatGroupedNumber(999) == "999");
    NIMVLETS_CHECK(FormatGroupedNumber(1000) == "1 000");
    NIMVLETS_CHECK(FormatGroupedNumber(1248) == "1 248");
    NIMVLETS_CHECK(FormatGroupedNumber(12345) == "12 345");
    NIMVLETS_CHECK(FormatGroupedNumber(1000000) == "1 000 000");
    return true;
}

// Block brief 06 §6 / 06.1 §16: "1 248 clicks" (en), "1 248 clics"
// (es), nunca "coins"/"monedas".
bool TestClickCountWordingEnglish() {
    NIMVLETS_CHECK(FormatClickCount(0, Language::kEn) == "0 clicks");
    NIMVLETS_CHECK(FormatClickCount(1, Language::kEn) == "1 click");   // singular
    NIMVLETS_CHECK(FormatClickCount(2, Language::kEn) == "2 clicks");
    NIMVLETS_CHECK(FormatClickCount(1248, Language::kEn) == "1 248 clicks");
    return true;
}

bool TestClickCountWordingSpanish() {
    NIMVLETS_CHECK(FormatClickCount(0, Language::kEs) == "0 clics");
    NIMVLETS_CHECK(FormatClickCount(1, Language::kEs) == "1 clic");   // singular
    NIMVLETS_CHECK(FormatClickCount(2, Language::kEs) == "2 clics");
    NIMVLETS_CHECK(FormatClickCount(1248, Language::kEs) == "1 248 clics");
    return true;
}

// Block 07: "Need N more clicks" / "Te faltan N clics", con singular
// correcto ("Need 1 more click" / "Te falta 1 clic") — brief §9/§18.
bool TestNeedMoreClicks() {
    NIMVLETS_CHECK(FormatNeedMoreClicks(42, Language::kEn) == "Need 42 more clicks");
    NIMVLETS_CHECK(FormatNeedMoreClicks(1, Language::kEn) == "Need 1 more click");
    NIMVLETS_CHECK(FormatNeedMoreClicks(1248, Language::kEn) == "Need 1 248 more clicks");
    NIMVLETS_CHECK(FormatNeedMoreClicks(42, Language::kEs) == "Te faltan 42 clics");
    NIMVLETS_CHECK(FormatNeedMoreClicks(1, Language::kEs) == "Te falta 1 clic");
    return true;
}

// "Spend N clicks to add <pet> to your collection?" — el nombre propio
// va sin traducir, el número agrupado, y singular para price==1.
bool TestSpendPrompt() {
    NIMVLETS_CHECK(FormatSpendPrompt(300, "Nidir", Language::kEn) ==
                   "Spend 300 clicks to add Nidir to your collection?");
    NIMVLETS_CHECK(FormatSpendPrompt(300, "Nidir", Language::kEs) ==
                   "¿Gastar 300 clics para añadir Nidir a tu colección?");
    NIMVLETS_CHECK(FormatSpendPrompt(1, "Bunny", Language::kEn) ==
                   "Spend 1 click to add Bunny to your collection?");
    NIMVLETS_CHECK(FormatSpendPrompt(1, "Bunny", Language::kEs) ==
                   "¿Gastar 1 clic para añadir Bunny a tu colección?");
    NIMVLETS_CHECK(FormatSpendPrompt(1200, "Nidir", Language::kEn) ==
                   "Spend 1 200 clicks to add Nidir to your collection?");
    return true;
}

// Block 11A: el nombre del permiso del OS entra como DATO en la copy
// localizada. Es lo que evita un `#ifdef __APPLE__` dentro de
// src/productui (brief §18).
bool TestFormatWithPermission() {
    const std::string en = FormatWithPermission(
        core::StringKey::kGlobalClickPermissionNeeded, "Input Monitoring", Language::kEn);
    NIMVLETS_CHECK(en == "Input Monitoring permission needed");

    const std::string es = FormatWithPermission(
        core::StringKey::kGlobalClickPermissionNeeded, "Input Monitoring", Language::kEs);
    NIMVLETS_CHECK(es == "Falta el permiso Input Monitoring");

    // El nombre del permiso NO se traduce: es como lo llama el OS.
    NIMVLETS_CHECK(es.find("Input Monitoring") != std::string::npos);

    // La explicación sustituye TODAS las apariciones y no deja tokens.
    const std::string explain = FormatWithPermission(
        core::StringKey::kGlobalClickExplain, "Input Monitoring", Language::kEn);
    NIMVLETS_CHECK(explain.find("{permission}") == std::string::npos);
    NIMVLETS_CHECK(explain.find("Input Monitoring") != std::string::npos);

    // Con un nombre vacío el token desaparece y la frase sigue siendo
    // legible (una plataforma sin permiso nombrado nunca muestra "{}").
    const std::string blank =
        FormatWithPermission(core::StringKey::kGlobalClickGrantHint, "", Language::kEn);
    NIMVLETS_CHECK(blank.find("{permission}") == std::string::npos);
    NIMVLETS_CHECK(!blank.empty());
    return true;
}

}  // namespace

void RegisterFormatTests(testing::TestRunner& runner) {
    runner.Add("Format/FormatWithPermission", TestFormatWithPermission);
    runner.Add("Format/GroupedNumber", TestGroupedNumber);
    runner.Add("Format/ClickCountWordingEnglish", TestClickCountWordingEnglish);
    runner.Add("Format/ClickCountWordingSpanish", TestClickCountWordingSpanish);
    runner.Add("Format/NeedMoreClicks", TestNeedMoreClicks);
    runner.Add("Format/SpendPrompt", TestSpendPrompt);
}

}  // namespace nimvlets::tests
