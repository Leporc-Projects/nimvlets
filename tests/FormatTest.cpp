#include "FormatTest.h"

#include "core/Localization.h"
#include "productui/Format.h"

#include <string>

using nimvlets::core::Language;
using nimvlets::productui::FormatClickCount;
using nimvlets::productui::FormatGroupedNumber;

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

}  // namespace

void RegisterFormatTests(testing::TestRunner& runner) {
    runner.Add("Format/GroupedNumber", TestGroupedNumber);
    runner.Add("Format/ClickCountWordingEnglish", TestClickCountWordingEnglish);
    runner.Add("Format/ClickCountWordingSpanish", TestClickCountWordingSpanish);
}

}  // namespace nimvlets::tests
