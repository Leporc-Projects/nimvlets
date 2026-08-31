#include "PreferencesTest.h"

#include "core/DisplayControls.h"
#include "core/Preferences.h"

#include <cmath>

using nimvlets::core::Language;
using nimvlets::core::OtherLanguage;
using nimvlets::core::PetSizeChoice;
using nimvlets::core::PetSizeScaleFactor;
using nimvlets::core::Preferences;
using nimvlets::core::PreferencesFromStored;
using nimvlets::core::StepOpacityPercent;
using nimvlets::core::StepSize;

namespace nimvlets::tests {

namespace {

bool AboutEqual(double a, double b) { return std::fabs(a - b) < 1e-9; }

// --- PreferencesFromStored: normaliza los campos crudos de AppState ---

// SIZE: los tres ids conocidos, y el factor de escala aprobado.
bool TestSizeFromStored() {
    NIMVLETS_CHECK(PreferencesFromStored("small", 100, false, "en").size == PetSizeChoice::kSmall);
    NIMVLETS_CHECK(PreferencesFromStored("medium", 100, false, "en").size == PetSizeChoice::kMedium);
    NIMVLETS_CHECK(PreferencesFromStored("large", 100, false, "en").size == PetSizeChoice::kLarge);

    // Small = 0.80, Medium = 1.00, Large = 1.15 (DEC-114). Cada uno
    // independiente de los otros.
    NIMVLETS_CHECK(AboutEqual(PetSizeScaleFactor(PetSizeChoice::kSmall), 0.80));
    NIMVLETS_CHECK(AboutEqual(PetSizeScaleFactor(PetSizeChoice::kMedium), 1.00));
    NIMVLETS_CHECK(AboutEqual(PetSizeScaleFactor(PetSizeChoice::kLarge), 1.15));
    return true;
}

// SIZE: un id desconocido / vacío -> medium (no crashea, no un valor
// imposible).
bool TestUnknownSizeIsMedium() {
    NIMVLETS_CHECK(PreferencesFromStored("", 100, false, "en").size == PetSizeChoice::kMedium);
    NIMVLETS_CHECK(PreferencesFromStored("gigantic", 100, false, "en").size == PetSizeChoice::kMedium);
    return true;
}

// OPACITY: 0 = "sin preferencia" -> 100; cualquier valor se ajusta al
// conjunto {100,85,70,55}; nunca queda un valor imposible.
bool TestOpacityFromStored() {
    NIMVLETS_CHECK(PreferencesFromStored("medium", 0, false, "en").opacityPercent == 100);
    NIMVLETS_CHECK(PreferencesFromStored("medium", 100, false, "en").opacityPercent == 100);
    NIMVLETS_CHECK(PreferencesFromStored("medium", 85, false, "en").opacityPercent == 85);
    NIMVLETS_CHECK(PreferencesFromStored("medium", 70, false, "en").opacityPercent == 70);
    NIMVLETS_CHECK(PreferencesFromStored("medium", 55, false, "en").opacityPercent == 55);
    NIMVLETS_CHECK(PreferencesFromStored("medium", 78, false, "en").opacityPercent == 85);   // más cerca de 85
    NIMVLETS_CHECK(PreferencesFromStored("medium", 9999, false, "en").opacityPercent == 100);
    NIMVLETS_CHECK(PreferencesFromStored("medium", 1, false, "en").opacityPercent == 55);    // piso
    return true;
}

// LOCK: bool tal cual. LANGUAGE: "es" -> kEs, todo lo demás -> kEn.
bool TestLockAndLanguageFromStored() {
    NIMVLETS_CHECK(PreferencesFromStored("medium", 100, true, "en").lockPosition);
    NIMVLETS_CHECK(!PreferencesFromStored("medium", 100, false, "en").lockPosition);

    NIMVLETS_CHECK(PreferencesFromStored("medium", 100, false, "es").language == Language::kEs);
    NIMVLETS_CHECK(PreferencesFromStored("medium", 100, false, "en").language == Language::kEn);
    NIMVLETS_CHECK(PreferencesFromStored("medium", 100, false, "").language == Language::kEn);
    NIMVLETS_CHECK(PreferencesFromStored("medium", 100, false, "fr").language == Language::kEn);
    return true;
}

bool TestDefaultsAndEquality() {
    const Preferences a;
    NIMVLETS_CHECK(a.size == PetSizeChoice::kMedium);
    NIMVLETS_CHECK(a.opacityPercent == 100);
    NIMVLETS_CHECK(!a.lockPosition);
    NIMVLETS_CHECK(a.language == Language::kEn);
    NIMVLETS_CHECK(a == PreferencesFromStored("", 0, false, ""));
    Preferences b = a;
    b.opacityPercent = 70;
    NIMVLETS_CHECK(!(a == b));
    return true;
}

// --- Ciclado de opciones para el control segmentado ---------------

// SIZE: ← → recorren Small · Medium · Large. `clamp` se detiene en los
// extremos; sin clamp envuelve (Enter/Espacio).
bool TestStepSize() {
    NIMVLETS_CHECK(StepSize(PetSizeChoice::kSmall, +1, /*clamp=*/true) == PetSizeChoice::kMedium);
    NIMVLETS_CHECK(StepSize(PetSizeChoice::kMedium, +1, true) == PetSizeChoice::kLarge);
    NIMVLETS_CHECK(StepSize(PetSizeChoice::kLarge, +1, true) == PetSizeChoice::kLarge);   // clamp
    NIMVLETS_CHECK(StepSize(PetSizeChoice::kSmall, -1, true) == PetSizeChoice::kSmall);   // clamp
    NIMVLETS_CHECK(StepSize(PetSizeChoice::kLarge, +1, /*clamp=*/false) == PetSizeChoice::kSmall);  // wrap
    NIMVLETS_CHECK(StepSize(PetSizeChoice::kSmall, -1, false) == PetSizeChoice::kLarge);            // wrap
    return true;
}

// OPACITY: segmentos en el orden 100 · 85 · 70 · 55.
bool TestStepOpacity() {
    NIMVLETS_CHECK(StepOpacityPercent(100, +1, /*clamp=*/true) == 85);
    NIMVLETS_CHECK(StepOpacityPercent(85, +1, true) == 70);
    NIMVLETS_CHECK(StepOpacityPercent(70, +1, true) == 55);
    NIMVLETS_CHECK(StepOpacityPercent(55, +1, true) == 55);    // clamp
    NIMVLETS_CHECK(StepOpacityPercent(100, -1, true) == 100);  // clamp
    NIMVLETS_CHECK(StepOpacityPercent(55, +1, /*clamp=*/false) == 100);  // wrap
    NIMVLETS_CHECK(StepOpacityPercent(100, -1, false) == 55);            // wrap
    // Un valor de partida arbitrario se normaliza antes de dar el paso.
    NIMVLETS_CHECK(StepOpacityPercent(78, +1, true) == 70);  // 78 -> 85 -> +1 -> 70
    return true;
}

bool TestOtherLanguage() {
    NIMVLETS_CHECK(OtherLanguage(Language::kEn) == Language::kEs);
    NIMVLETS_CHECK(OtherLanguage(Language::kEs) == Language::kEn);
    return true;
}

}  // namespace

void RegisterPreferencesTests(testing::TestRunner& runner) {
    runner.Add("Preferences/SizeFromStored", TestSizeFromStored);
    runner.Add("Preferences/UnknownSizeIsMedium", TestUnknownSizeIsMedium);
    runner.Add("Preferences/OpacityFromStored", TestOpacityFromStored);
    runner.Add("Preferences/LockAndLanguageFromStored", TestLockAndLanguageFromStored);
    runner.Add("Preferences/DefaultsAndEquality", TestDefaultsAndEquality);
    runner.Add("Preferences/StepSize", TestStepSize);
    runner.Add("Preferences/StepOpacity", TestStepOpacity);
    runner.Add("Preferences/OtherLanguage", TestOtherLanguage);
}

}  // namespace nimvlets::tests
