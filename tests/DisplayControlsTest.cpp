#include "DisplayControlsTest.h"

#include "core/DisplayControls.h"

#include <cmath>
#include <string>

using nimvlets::core::DisplayBounds;
using nimvlets::core::NormalizeOpacityPercent;
using nimvlets::core::OpacityFraction;
using nimvlets::core::ParsePetSizeChoice;
using nimvlets::core::PetDragAllowed;
using nimvlets::core::PetSizeChoice;
using nimvlets::core::PetSizeChoiceId;
using nimvlets::core::PetSizeScaleFactor;
using nimvlets::core::SafePetPlacement;
using nimvlets::core::WindowTopLeft;

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

// --- Block 11B: SafePetPlacement (la geometría de "Reset position") ---

// Caso normal: el pet cabe en el display -> queda CENTRADO, exactamente
// como SDL_WINDOWPOS_CENTERED. El origen del display puede ser (0,0) o no.
bool TestSafePlacementCentersWhenItFits() {
    // Display primario en (0,0), 1920x1080; pet 160x160.
    const WindowTopLeft a = SafePetPlacement(DisplayBounds{0, 0, 1920, 1080}, 160, 160);
    NIMVLETS_CHECK(a.x == (1920 - 160) / 2);   // 880
    NIMVLETS_CHECK(a.y == (1080 - 160) / 2);   // 460

    // Segundo monitor a la derecha, origen (1920, 0), 1280x720.
    const WindowTopLeft b = SafePetPlacement(DisplayBounds{1920, 0, 1280, 720}, 200, 200);
    NIMVLETS_CHECK(b.x == 1920 + (1280 - 200) / 2);  // 2460
    NIMVLETS_CHECK(b.y == (720 - 200) / 2);          // 260

    // Monitor "por encima" con y negativa, origen (0, -1080).
    const WindowTopLeft c = SafePetPlacement(DisplayBounds{0, -1080, 1920, 1080}, 300, 240);
    NIMVLETS_CHECK(c.x == (1920 - 300) / 2);
    NIMVLETS_CHECK(c.y == -1080 + (1080 - 240) / 2);
    return true;
}

// El destino SIEMPRE queda dentro de los límites del display objetivo:
// nunca deja la esquina superior-izquierda fuera, ni el rectángulo entero
// si cabe.
bool TestSafePlacementStaysInsideTargetDisplay() {
    const DisplayBounds displays[] = {
        {0, 0, 1440, 900},
        {-2560, 120, 2560, 1440},
        {1440, -300, 3840, 2160},
    };
    for (const DisplayBounds& d : displays) {
        for (const int size : {64, 320, 800}) {
            const WindowTopLeft p = SafePetPlacement(d, size, size);
            NIMVLETS_CHECK(p.x >= d.x);
            NIMVLETS_CHECK(p.y >= d.y);
            if (size <= d.w) {
                NIMVLETS_CHECK(p.x + size <= d.x + d.w);
            }
            if (size <= d.h) {
                NIMVLETS_CHECK(p.y + size <= d.y + d.h);
            }
        }
    }
    return true;
}

// Pet MÁS grande que el display en un eje: la esquina superior/izquierda
// se ancla al borde del display en vez de centrar (que la dejaría fuera
// de pantalla por arriba/izquierda).
bool TestSafePlacementAnchorsWhenPetOversizedForDisplay() {
    // Pet más ancho que el display: x se ancla al borde izquierdo; y
    // sigue centrado porque en alto sí cabe.
    const WindowTopLeft wide = SafePetPlacement(DisplayBounds{100, 50, 400, 900}, 600, 200);
    NIMVLETS_CHECK(wide.x == 100);                 // anclado, no 100 + (400-600)/2 = 0
    NIMVLETS_CHECK(wide.y == 50 + (900 - 200) / 2);

    // Pet más alto que el display: y se ancla al borde superior.
    const WindowTopLeft tall = SafePetPlacement(DisplayBounds{0, 0, 1000, 300}, 200, 500);
    NIMVLETS_CHECK(tall.x == (1000 - 200) / 2);
    NIMVLETS_CHECK(tall.y == 0);

    // Pet más grande en los dos ejes: ambas esquinas ancladas.
    const WindowTopLeft both = SafePetPlacement(DisplayBounds{-10, -20, 200, 200}, 400, 400);
    NIMVLETS_CHECK(both.x == -10 && both.y == -20);
    return true;
}

// Determinismo: mismas entradas -> mismo resultado. La geometría no
// conoce "pet oculto" ni "Lock Position" (eso lo decide src/app).
bool TestSafePlacementIsDeterministic() {
    const DisplayBounds d{37, 91, 1710, 1112};
    const WindowTopLeft first = SafePetPlacement(d, 173, 211);
    for (int i = 0; i < 5; ++i) {
        const WindowTopLeft again = SafePetPlacement(d, 173, 211);
        NIMVLETS_CHECK(again.x == first.x && again.y == first.y);
    }
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
    runner.Add("DisplayControls/SafePlacementCentersWhenItFits", TestSafePlacementCentersWhenItFits);
    runner.Add("DisplayControls/SafePlacementStaysInsideTargetDisplay",
               TestSafePlacementStaysInsideTargetDisplay);
    runner.Add("DisplayControls/SafePlacementAnchorsWhenPetOversizedForDisplay",
               TestSafePlacementAnchorsWhenPetOversizedForDisplay);
    runner.Add("DisplayControls/SafePlacementIsDeterministic", TestSafePlacementIsDeterministic);
}

}  // namespace nimvlets::tests
