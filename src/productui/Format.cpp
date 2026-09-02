#include "productui/Format.h"

#include <string>

namespace nimvlets::productui {

namespace {

// Sustituye TODAS las apariciones de `token` en `s` por `value`. Los
// tokens ("{n}", "{pet}", "{permission}") no se solapan con `value`, así
// que un solo paso alcanza.
std::string Substitute(std::string s, const std::string& token, const std::string& value) {
    std::string::size_type pos = 0;
    while ((pos = s.find(token, pos)) != std::string::npos) {
        s.replace(pos, token.size(), value);
        pos += value.size();
    }
    return s;
}

}  // namespace

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

std::string FormatNeedMoreClicks(std::uint64_t shortBy, core::Language lang) {
    if (shortBy == 1) {
        return core::Localized(core::StringKey::kNeedMoreClicksOne, lang);
    }
    return Substitute(core::Localized(core::StringKey::kNeedMoreClicksMany, lang), "{n}",
                      FormatGroupedNumber(shortBy));
}

std::string FormatSpendPrompt(std::uint64_t price, const std::string& petName, core::Language lang) {
    const core::StringKey key =
        price == 1 ? core::StringKey::kSpendPromptOne : core::StringKey::kSpendPromptMany;
    std::string out = core::Localized(key, lang);
    if (price != 1) {
        out = Substitute(std::move(out), "{n}", FormatGroupedNumber(price));
    }
    return Substitute(std::move(out), "{pet}", petName);
}

std::string FormatOnboardingConfirmPrompt(const std::string& petName, core::Language lang) {
    return Substitute(core::Localized(core::StringKey::kOnboardingConfirmStarter, lang), "{pet}", petName);
}

std::string FormatWithPermission(
    core::StringKey key, const std::string& permissionName, core::Language lang) {
    return Substitute(core::Localized(key, lang), "{permission}", permissionName);
}

}  // namespace nimvlets::productui
