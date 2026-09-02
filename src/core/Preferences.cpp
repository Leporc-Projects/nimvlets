#include "core/Preferences.h"

#include <algorithm>
#include <array>

namespace nimvlets::core {

Preferences PreferencesFromStored(
    std::string_view sizeChoiceId, std::uint32_t opacityPercent, bool lockPosition,
    std::string_view languageId, std::string_view clickCountingModeId) {
    Preferences p;
    p.size = ParsePetSizeChoice(sizeChoiceId);
    // 0 = "sin preferencia guardada" -> 100 (totalmente opaco), igual que
    // SpikeApp al aplicar la opacidad al arrancar. Cualquier otro valor
    // se ajusta a la opción válida más cercana.
    p.opacityPercent = opacityPercent == 0
                           ? 100
                           : NormalizeOpacityPercent(static_cast<int>(opacityPercent));
    p.lockPosition = lockPosition;
    p.language = ParseLanguage(languageId);
    // Cualquier cosa que no sea el id EXACTO "anywhere" cae al default
    // local — incluido el "" de todo save v1..v5 (brief §12).
    p.clickCounting = ParseClickCountingMode(clickCountingModeId);
    return p;
}

namespace {

// Índice de `size` en el orden de la UI (Small · Medium · Large).
int SizeIndex(PetSizeChoice s) {
    switch (s) {
        case PetSizeChoice::kSmall:
            return 0;
        case PetSizeChoice::kMedium:
            return 1;
        case PetSizeChoice::kLarge:
            return 2;
    }
    return 1;
}

PetSizeChoice SizeAt(int index) {
    switch (index) {
        case 0:
            return PetSizeChoice::kSmall;
        case 2:
            return PetSizeChoice::kLarge;
        default:
            return PetSizeChoice::kMedium;
    }
}

int StepIndex(int current, int count, int dir, bool clamp) {
    int next = current + (dir >= 0 ? 1 : -1);
    if (clamp) {
        return std::clamp(next, 0, count - 1);
    }
    // Envuelve: (-1 -> count-1), (count -> 0).
    return (next % count + count) % count;
}

}  // namespace

PetSizeChoice StepSize(PetSizeChoice current, int dir, bool clamp) {
    return SizeAt(StepIndex(SizeIndex(current), 3, dir, clamp));
}

int StepOpacityPercent(int currentPercent, int dir, bool clamp) {
    // Se normaliza primero: el `currentPercent` puede venir de un estado
    // viejo con un valor arbitrario.
    const int normalized = NormalizeOpacityPercent(currentPercent);
    constexpr int kCount = static_cast<int>(std::size(kOpacityChoicesPercent));
    int currentIndex = 0;
    for (int i = 0; i < kCount; ++i) {
        if (kOpacityChoicesPercent[i] == normalized) {
            currentIndex = i;
            break;
        }
    }
    return kOpacityChoicesPercent[StepIndex(currentIndex, kCount, dir, clamp)];
}

Language OtherLanguage(Language current) {
    return current == Language::kEn ? Language::kEs : Language::kEn;
}

}  // namespace nimvlets::core
