#include "FormatTest.h"

#include "productui/Format.h"

#include <string>

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

// Block brief §6: "1 248 clicks", nunca "1,248 COINS".
bool TestClickCountWording() {
    NIMVLETS_CHECK(FormatClickCount(0) == "0 clicks");
    NIMVLETS_CHECK(FormatClickCount(1) == "1 click");   // singular
    NIMVLETS_CHECK(FormatClickCount(2) == "2 clicks");
    NIMVLETS_CHECK(FormatClickCount(1248) == "1 248 clicks");
    return true;
}

}  // namespace

void RegisterFormatTests(testing::TestRunner& runner) {
    runner.Add("Format/GroupedNumber", TestGroupedNumber);
    runner.Add("Format/ClickCountWording", TestClickCountWording);
}

}  // namespace nimvlets::tests
