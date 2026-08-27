#include "DisplayControlsTest.h"

#include "core/DisplayControls.h"

#include <cmath>
#include <string>

using nimvlets::core::NormalizeOpacityPercent;
using nimvlets::core::OpacityFraction;
using nimvlets::core::ParsePetSizeChoice;
using nimvlets::core::PetDragAllowed;
using nimvlets::core::PetSizeChoice;
using nimvlets::core::PetSizeChoiceId;
using nimvlets::core::PetSizeScaleFactor;

namespace nimvlets::tests {

namespace {

// Tolerancia laxa a propósito: OpacityFraction devuelve float, así que
// 0.55f != 0.55 a nivel de bits — 1e-6 alcanza para lo que estos casos
// verifican.
bool AboutEqual(double a, double b) { return std::fabs(a - b) < 1e-6; }

bool TestSizeChoiceRoundTripsThroughString() {
    NIMVLETS_CHECK(ParsePetSizeChoice(PetSizeChoiceId(PetSizeChoice::kSmall)) == PetSizeChoice::kSmall);
    NIMVLETS_CHECK(ParsePetSizeChoice(PetSizeChoiceId(PetSizeChoice::kMedium)) == PetSizeChoice::kMedium);
    NIMVLETS_CHECK(ParsePetSizeChoice(PetSizeChoiceId(PetSizeChoice::kLarge)) == PetSizeChoice::kLarge);
    return true;
}

// Un id desconocido o vacío -> el tamaño neutro (misma disciplina de
// "defaults seguros" que el resto de la persistencia).
bool TestUnknownSizeChoiceIsMedium() {
    NIMVLETS_CHECK(ParsePetSizeChoice("") == PetSizeChoice::kMedium);
    NIMVLETS_CHECK(ParsePetSizeChoice("gigantic") == PetSizeChoice::kMedium);
    NIMVLETS_CHECK(ParsePetSizeChoice("MEDIUM") == PetSizeChoice::kMedium);  // case-sensitive por diseño
    return true;
}

// Medium DEBE ser exactamente 1.0: un owner que nunca toca el control
// ve el pet al tamaño que declara su contenido, sin cambio de
// comportamiento respecto de antes de Block 06.
bool TestMediumScaleFactorIsExactlyOne() {
    NIMVLETS_CHECK(AboutEqual(PetSizeScaleFactor(PetSizeChoice::kMedium), 1.0));
    NIMVLETS_CHECK(PetSizeScaleFactor(PetSizeChoice::kSmall) < 1.0);
    NIMVLETS_CHECK(PetSizeScaleFactor(PetSizeChoice::kLarge) > 1.0);
    return true;
}

// Block 06.1 (DEC-114): Small 0.80, Medium 1.00, Large 1.15 exactos.
// Cambiar Large NO afecta a los otros dos ni al contenido del pet.
bool TestSizeFactorsAreTheBlock061Presets() {
    NIMVLETS_CHECK(AboutEqual(PetSizeScaleFactor(PetSizeChoice::kSmall), 0.80));
    NIMVLETS_CHECK(AboutEqual(PetSizeScaleFactor(PetSizeChoice::kMedium), 1.00));
    NIMVLETS_CHECK(AboutEqual(PetSizeScaleFactor(PetSizeChoice::kLarge), 1.15));
    return true;
}

// Una preferencia persistida "large" (de Block 06, cuando valía 1.30)
// se re-interpreta sola al nuevo factor: sigue parseando a kLarge, y
// kLarge ahora es 1.15. Sin migración.
bool TestPersistedLargeResolvesToNewFactor() {
    NIMVLETS_CHECK(ParsePetSizeChoice("large") == PetSizeChoice::kLarge);
    NIMVLETS_CHECK(AboutEqual(PetSizeScaleFactor(ParsePetSizeChoice("large")), 1.15));
    return true;
}

bool TestOpacityNormalizesToNearestChoice() {
    NIMVLETS_CHECK(NormalizeOpacityPercent(100) == 100);
    NIMVLETS_CHECK(NormalizeOpacityPercent(0) == 55);     // el piso
    NIMVLETS_CHECK(NormalizeOpacityPercent(90) == 85);
    NIMVLETS_CHECK(NormalizeOpacityPercent(78) == 85);    // 78 está más cerca de 85 (7) que de 70 (8)
    NIMVLETS_CHECK(NormalizeOpacityPercent(60) == 55);
    NIMVLETS_CHECK(NormalizeOpacityPercent(1000) == 100);
    NIMVLETS_CHECK(NormalizeOpacityPercent(-5) == 55);
    return true;
}

bool TestOpacityFractionClamps() {
    NIMVLETS_CHECK(AboutEqual(OpacityFraction(100), 1.0));
    NIMVLETS_CHECK(AboutEqual(OpacityFraction(55), 0.55));
    NIMVLETS_CHECK(AboutEqual(OpacityFraction(200), 1.0));   // clamp alto
    NIMVLETS_CHECK(AboutEqual(OpacityFraction(-10), 0.0));   // clamp bajo
    return true;
}

bool TestDragAllowedTracksLock() {
    NIMVLETS_CHECK(PetDragAllowed(false));
    NIMVLETS_CHECK(!PetDragAllowed(true));
    return true;
}

}  // namespace

void RegisterDisplayControlsTests(testing::TestRunner& runner) {
    runner.Add("DisplayControls/SizeChoiceRoundTripsThroughString", TestSizeChoiceRoundTripsThroughString);
    runner.Add("DisplayControls/UnknownSizeChoiceIsMedium", TestUnknownSizeChoiceIsMedium);
    runner.Add("DisplayControls/MediumScaleFactorIsExactlyOne", TestMediumScaleFactorIsExactlyOne);
    runner.Add("DisplayControls/SizeFactorsAreTheBlock061Presets", TestSizeFactorsAreTheBlock061Presets);
    runner.Add("DisplayControls/PersistedLargeResolvesToNewFactor", TestPersistedLargeResolvesToNewFactor);
    runner.Add("DisplayControls/OpacityNormalizesToNearestChoice", TestOpacityNormalizesToNearestChoice);
    runner.Add("DisplayControls/OpacityFractionClamps", TestOpacityFractionClamps);
    runner.Add("DisplayControls/DragAllowedTracksLock", TestDragAllowedTracksLock);
}

}  // namespace nimvlets::tests
