#include "ContrastTest.h"

#include "productui/Contrast.h"
#include "productui/VisualTokens.h"

using nimvlets::productui::BestForeground;
using nimvlets::productui::ContrastRatio;
using nimvlets::productui::Darken;
using nimvlets::productui::EnsureContrastOn;
using nimvlets::productui::Lighten;
using nimvlets::productui::Mix;
using nimvlets::productui::RelativeLuminance;
using nimvlets::productui::UiColor;
namespace tokens = nimvlets::productui::tokens;

namespace nimvlets::tests {

namespace {

constexpr UiColor kBlack{0, 0, 0, 255};
constexpr UiColor kWhite{255, 255, 255, 255};

bool AboutEqual(double a, double b, double eps = 1e-6) { return (a > b ? a - b : b - a) <= eps; }

bool TestLuminanceEndpointsAndOrder() {
    NIMVLETS_CHECK(AboutEqual(RelativeLuminance(kBlack), 0.0));
    NIMVLETS_CHECK(AboutEqual(RelativeLuminance(kWhite), 1.0));
    // Monótona: un gris más claro tiene más luminancia.
    NIMVLETS_CHECK(RelativeLuminance(UiColor{0x40, 0x40, 0x40, 255}) <
                   RelativeLuminance(UiColor{0xC0, 0xC0, 0xC0, 255}));
    return true;
}

bool TestContrastRatioRangeAndSymmetry() {
    NIMVLETS_CHECK(AboutEqual(ContrastRatio(kBlack, kWhite), 21.0, 1e-3));
    NIMVLETS_CHECK(AboutEqual(ContrastRatio(kWhite, kWhite), 1.0));
    // Simétrico en sus argumentos.
    const UiColor a{0x26, 0x22, 0x1E, 255};
    const UiColor b{0xF6, 0xF3, 0xEE, 255};
    NIMVLETS_CHECK(AboutEqual(ContrastRatio(a, b), ContrastRatio(b, a)));
    NIMVLETS_CHECK(ContrastRatio(a, b) >= 1.0 && ContrastRatio(a, b) <= 21.0);
    return true;
}

// Los tokens de texto contra el canvas: primario alto, secundario AA
// para texto normal, muted un escalón por debajo (jerarquía intacta).
bool TestSemanticTextTokensMeetTheirTargets() {
    const double primary = ContrastRatio(tokens::kTextPrimary, tokens::kCanvas);
    const double secondary = ContrastRatio(tokens::kTextSecondary, tokens::kCanvas);
    const double muted = ContrastRatio(tokens::kTextMuted, tokens::kCanvas);
    NIMVLETS_CHECK(primary >= 12.0);
    NIMVLETS_CHECK(secondary >= 4.5);  // WCAG AA texto normal
    NIMVLETS_CHECK(muted >= 3.0);      // legible, un escalón bajo secundario
    NIMVLETS_CHECK(primary > secondary && secondary > muted);
    return true;
}

bool TestSurfacesAndBorderRelationships() {
    // SurfaceRaised "levanta" (más claro) sobre el canvas; SurfaceSoft es
    // más profundo; el borde es más oscuro que el canvas.
    NIMVLETS_CHECK(RelativeLuminance(tokens::kSurfaceRaised) > RelativeLuminance(tokens::kCanvas));
    NIMVLETS_CHECK(RelativeLuminance(tokens::kSurfaceSoft) < RelativeLuminance(tokens::kCanvas));
    NIMVLETS_CHECK(RelativeLuminance(tokens::kBorder) < RelativeLuminance(tokens::kCanvas));
    return true;
}

bool TestMixAndDarken() {
    NIMVLETS_CHECK(Mix(kBlack, kWhite, 0.0) == kBlack);
    NIMVLETS_CHECK(Mix(kBlack, kWhite, 1.0) == kWhite);
    const UiColor mid = Mix(kBlack, kWhite, 0.5);
    NIMVLETS_CHECK(mid.r >= 126 && mid.r <= 129);
    // Darken conserva el alpha y nunca aclara.
    const UiColor c{0xC0, 0x80, 0x40, 200};
    const UiColor d = Darken(c, 0.25);
    NIMVLETS_CHECK(d.a == 200);
    NIMVLETS_CHECK(d.r < c.r && d.g < c.g && d.b < c.b);
    return true;
}

bool TestEnsureContrastReachesTargetAndNeverLightens() {
    // Ya cumple -> se devuelve tal cual.
    const UiColor darkOnLight{0x30, 0x2A, 0x22, 255};
    const UiColor lightBg{0xF3, 0xE4, 0xD2, 255};
    NIMVLETS_CHECK(EnsureContrastOn(darkOnLight, lightBg, 4.5) == darkOnLight);

    // Un acento demasiado claro sobre su relleno: se oscurece hasta
    // alcanzar el objetivo, y el resultado NO es más claro que la
    // entrada.
    const UiColor paleInk{0x9A, 0xB0, 0xC4, 255};
    const UiColor paleFill{0xDA, 0xE7, 0xEF, 255};
    const UiColor fixed = EnsureContrastOn(paleInk, paleFill, 4.5);
    NIMVLETS_CHECK(ContrastRatio(fixed, paleFill) >= 4.5);
    NIMVLETS_CHECK(RelativeLuminance(fixed) <= RelativeLuminance(paleInk) + 1e-9);
    return true;
}

// Convergencia DEC-147: BestForeground elige entre una tinta oscura y
// una clara la de mejor contraste sobre `bg`, y si hace falta la empuja
// al extremo para alcanzar el objetivo.
bool TestBestForegroundPicksAndReaches() {
    const UiColor darkInk{0x30, 0x2A, 0x22, 255};
    const UiColor lightInk{0xF4, 0xEE, 0xE3, 255};

    // Relleno CLARO -> gana la tinta oscura, ya legible.
    const UiColor lightFill{0xE9, 0xC8, 0xA2, 255};
    const UiColor onLight = BestForeground(lightFill, darkInk, lightInk, 4.5);
    NIMVLETS_CHECK(onLight == darkInk);
    NIMVLETS_CHECK(ContrastRatio(onLight, lightFill) >= 4.5);

    // Relleno OSCURO -> gana la tinta clara.
    const UiColor darkFill{0x3A, 0x2E, 0x55, 255};
    const UiColor onDark = BestForeground(darkFill, darkInk, lightInk, 4.5);
    NIMVLETS_CHECK(ContrastRatio(onDark, darkFill) >= 4.5);
    NIMVLETS_CHECK(RelativeLuminance(onDark) > RelativeLuminance(darkFill));

    // Ninguna candidata llega -> se empuja al extremo y AUN ASÍ cumple
    // (o hace su mejor esfuerzo) — nunca un par ilegible por elección.
    const UiColor midFill{0x8C, 0x8C, 0x8C, 255};
    const UiColor onMid = BestForeground(midFill, UiColor{0x70, 0x70, 0x70, 255},
                                         UiColor{0xA0, 0xA0, 0xA0, 255}, 4.5);
    NIMVLETS_CHECK(ContrastRatio(onMid, midFill) >= 4.5);
    return true;
}

bool TestLightenEndpointsAndAlpha() {
    NIMVLETS_CHECK(Lighten(kBlack, 0.0) == kBlack);
    NIMVLETS_CHECK(Lighten(kBlack, 1.0) == kWhite);
    const UiColor c{0x33, 0x2B, 0x22, 200};
    const UiColor l = Lighten(c, 0.25);
    NIMVLETS_CHECK(l.a == 200);
    NIMVLETS_CHECK(l.r > c.r && l.g > c.g && l.b > c.b);
    return true;
}

}  // namespace

void RegisterContrastTests(testing::TestRunner& runner) {
    runner.Add("Contrast/BestForegroundPicksAndReaches", TestBestForegroundPicksAndReaches);
    runner.Add("Contrast/LightenEndpointsAndAlpha", TestLightenEndpointsAndAlpha);
    runner.Add("Contrast/LuminanceEndpointsAndOrder", TestLuminanceEndpointsAndOrder);
    runner.Add("Contrast/ContrastRatioRangeAndSymmetry", TestContrastRatioRangeAndSymmetry);
    runner.Add("Contrast/SemanticTextTokensMeetTheirTargets", TestSemanticTextTokensMeetTheirTargets);
    runner.Add("Contrast/SurfacesAndBorderRelationships", TestSurfacesAndBorderRelationships);
    runner.Add("Contrast/MixAndDarken", TestMixAndDarken);
    runner.Add("Contrast/EnsureContrastReachesTargetAndNeverLightens",
               TestEnsureContrastReachesTargetAndNeverLightens);
}

}  // namespace nimvlets::tests
