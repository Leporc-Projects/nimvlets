#include "productui/Format.h"

namespace nimvlets::productui {

std::string FormatGroupedNumber(std::uint64_t value) {
    const std::string digits = std::to_string(value);
    std::string out;
    const std::size_t n = digits.size();
    for (std::size_t i = 0; i < n; ++i) {
        if (i != 0 && (n - i) % 3 == 0) {
            out.push_back(' ');
        }
        out.push_back(digits[i]);
    }
    return out;
}

std::string FormatClickCount(std::uint64_t clicks, core::Language lang) {
    const core::StringKey word = clicks == 1 ? core::StringKey::kClickSingular : core::StringKey::kClickPlural;
    return FormatGroupedNumber(clicks) + " " + core::Localized(word, lang);
}

}  // namespace nimvlets::productui
