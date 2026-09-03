#include "PetAccentTest.h"

#include "productui/Contrast.h"
#include "productui/PetAccent.h"

using nimvlets::productui::AccentEmphasis;
using nimvlets::productui::ContrastRatio;
using nimvlets::productui::HeroStageStyle;
using nimvlets::productui::PetAccent;
using nimvlets::productui::PetAccentFor;
using nimvlets::productui::UiColor;

namespace nimvlets::tests {

namespace {

int Lum(UiColor c) {
    return static_cast<int>(c.r) + static_cast<int>(c.g) + static_cast<int>(c.b);
}

bool TestKnownPetsHaveDistinctAccents() {
    const PetAccent bunny = PetAccentFor("bunny");
    const PetAccent nidir = PetAccentFor("nidir");
    const PetAccent frin = PetAccentFor("frin");

    NIMVLETS_CHECK(!(bunny.line == nidir.line));
    NIMVLETS_CHECK(!(bunny.line == frin.line));
    NIMVLETS_CHECK(!(nidir.line == frin.line));

    // Todos opacos (a == 255) — la vista aplica el alpha bajo, no el dato.
    NIMVLETS_CHECK(bunny.line.a == 255 && bunny.shapeTint.a == 255);
    NIMVLETS_CHECK(bunny.softFill.a == 255 && bunny.deepInk.a == 255);
    return true;
}

// Direcciones de diseño del brief §9: Bunny cálido (apricot), Nidir
// violeta (azul >= rojo), Frin azul frío (azul > rojo).
bool TestAccentHuesMatchDesignDirection() {
    NIMVLETS_CHECK(PetAccentFor("bunny").line.r > PetAccentFor("bunny").line.b);   // cálido
    NIMVLETS_CHECK(PetAccentFor("frin").line.b > PetAccentFor("frin").line.r);     // azul hielo
    NIMVLETS_CHECK(PetAccentFor("nidir").line.b >= PetAccentFor("nidir").line.r);  // violeta apagado
    return true;
}

bool TestHeroShapeAngularityPerPet() {
    NIMVLETS_CHECK(!PetAccentFor("bunny").angularShape);
    NIMVLETS_CHECK(!PetAccentFor("frin").angularShape);
    NIMVLETS_CHECK(PetAccentFor("nidir").angularShape);
    return true;
}

// Block 12A: la costura `heroStage` es consistente con el bool
// `angularShape` de compatibilidad, para cada pet y el fallback.
bool TestHeroStageStyleMatchesAngularShape() {
    for (const char* id : {"bunny", "nidir", "frin", "kyubi", "sweetie", "does_not_exist"}) {
        const PetAccent a = PetAccentFor(id);
        const bool faceted = a.heroStage == HeroStageStyle::kFacetedRound;
        NIMVLETS_CHECK(faceted == a.angularShape);
    }
    NIMVLETS_CHECK(PetAccentFor("nidir").heroStage == HeroStageStyle::kFacetedRound);
    NIMVLETS_CHECK(PetAccentFor("bunny").heroStage == HeroStageStyle::kSoftOval);
    return true;
}

// Block 12A: cada identidad trae un acento SECUNDARIO opaco y poblado
// (nunca cero) — incluido el fallback neutro (brief §10: nunca deja un
// campo sin poblar).
bool TestSecondaryAccentPresentForEveryIdentity() {
    for (const char* id : {"bunny", "nidir", "frin", "rato", "rin_rin", "artu", "kyubi", "sweetie",
                           "", "does_not_exist"}) {
        const PetAccent a = PetAccentFor(id);
        NIMVLETS_CHECK(a.secondary.a == 255);
        NIMVLETS_CHECK(Lum(a.secondary) > 0);
        NIMVLETS_CHECK(!(a.secondary == a.line));  // un tono complementario, no una copia
    }
    return true;
}

// Block 12A / DEC-143: el botón Primary usa deepInk sobre softFill; el
// par tiene que ser legible (>= 4.5:1, WCAG AA texto normal) para cada
// pet real y el fallback neutro, medido con productui::ContrastRatio —
// no "a ojo".
bool TestButtonInkMeetsWcagAaOnSoftFill() {
    for (const char* id : {"bunny", "nidir", "frin", "rato", "rin_rin", "artu", "kyubi", "sweetie",
                           "", "does_not_exist"}) {
        const PetAccent a = PetAccentFor(id);
        NIMVLETS_CHECK(ContrastRatio(a.deepInk, a.softFill) >= 4.5);
    }
    return true;
}

bool TestUnknownPetFallsBackToNeutral() {
    const PetAccent unknown = PetAccentFor("does_not_exist");
    const PetAccent alsoUnknown = PetAccentFor("");
    NIMVLETS_CHECK(unknown.line == alsoUnknown.line);
    NIMVLETS_CHECK(unknown.softFill == alsoUnknown.softFill);
    NIMVLETS_CHECK(unknown.line.a == 255);
    NIMVLETS_CHECK(unknown.line.r > unknown.line.b);  // el neutro es cálido (terracota)
    return true;
}

bool TestShapeTintIsPalerThanLine() {
    for (const char* id : {"bunny", "nidir", "frin", "does_not_exist"}) {
        const PetAccent a = PetAccentFor(id);
        NIMVLETS_CHECK(Lum(a.shapeTint) > Lum(a.line));
    }
    return true;
}

// Block 06.2 §17: el botón primario usa un relleno TENUE (claro) y un
// ink OSCURO del mismo tono — nunca casi-negro arbitrario.
bool TestButtonSoftFillIsLightAndDeepInkIsDark() {
    for (const char* id : {"bunny", "nidir", "frin", "sweetie", "does_not_exist"}) {
        const PetAccent a = PetAccentFor(id);
        NIMVLETS_CHECK(Lum(a.softFill) > 3 * 200);  // relleno claro (media > ~200/canal)
        NIMVLETS_CHECK(Lum(a.deepInk) < 3 * 120);   // ink oscuro (media < ~120/canal)
        // El ink es marcadamente más oscuro que el relleno -> contraste.
        NIMVLETS_CHECK(Lum(a.softFill) - Lum(a.deepInk) > 250);
        // Pero deepInk NO es negro puro (el brief prohíbe "arbitrary black").
        NIMVLETS_CHECK(Lum(a.deepInk) > 30);
    }
    return true;
}

// DEC-148: cada identidad trae un `emphasis` válido; el fallback neutro
// y cualquier id desconocido caen al valor SEGURO kSoft (nunca kDeep sin
// un perfil que lo justifique — brief §34).
bool TestAccentEmphasisPopulatedAndSafeFallback() {
    for (const char* id : {"bunny", "nidir", "frin", "rato", "rin_rin", "artu", "kyubi", "sweetie",
                           "", "does_not_exist"}) {
        const PetAccent a = PetAccentFor(id);
        NIMVLETS_CHECK(a.emphasis == AccentEmphasis::kSoft || a.emphasis == AccentEmphasis::kDeep);
    }
    NIMVLETS_CHECK(PetAccentFor("").emphasis == AccentEmphasis::kSoft);
    NIMVLETS_CHECK(PetAccentFor("does_not_exist").emphasis == AccentEmphasis::kSoft);
    return true;
}

// El owner quiere el CTA / la card violeta HONDA para Nidir (concept),
// mientras Bunny / Frin quedan en el tratamiento suave.
bool TestDramaticPetsUseDeepEmphasis() {
    NIMVLETS_CHECK(PetAccentFor("nidir").emphasis == AccentEmphasis::kDeep);
    NIMVLETS_CHECK(PetAccentFor("kyubi").emphasis == AccentEmphasis::kDeep);
    NIMVLETS_CHECK(PetAccentFor("bunny").emphasis == AccentEmphasis::kSoft);
    NIMVLETS_CHECK(PetAccentFor("frin").emphasis == AccentEmphasis::kSoft);
    return true;
}

// El tono del softFill sigue la familia de color del pet (Bunny cálido,
// Frin frío) — no es un gris genérico.
bool TestButtonFillKeepsPetHue() {
    NIMVLETS_CHECK(PetAccentFor("bunny").softFill.r > PetAccentFor("bunny").softFill.b);  // cálido
    NIMVLETS_CHECK(PetAccentFor("frin").softFill.b >= PetAccentFor("frin").softFill.r);   // frío
    NIMVLETS_CHECK(PetAccentFor("bunny").deepInk.r > PetAccentFor("bunny").deepInk.b);
    NIMVLETS_CHECK(PetAccentFor("frin").deepInk.b > PetAccentFor("frin").deepInk.r);
    return true;
}

}  // namespace

void RegisterPetAccentTests(testing::TestRunner& runner) {
    runner.Add("PetAccent/KnownPetsHaveDistinctAccents", TestKnownPetsHaveDistinctAccents);
    runner.Add("PetAccent/AccentHuesMatchDesignDirection", TestAccentHuesMatchDesignDirection);
    runner.Add("PetAccent/HeroShapeAngularityPerPet", TestHeroShapeAngularityPerPet);
    runner.Add("PetAccent/HeroStageStyleMatchesAngularShape", TestHeroStageStyleMatchesAngularShape);
    runner.Add("PetAccent/SecondaryAccentPresentForEveryIdentity",
               TestSecondaryAccentPresentForEveryIdentity);
    runner.Add("PetAccent/ButtonInkMeetsWcagAaOnSoftFill", TestButtonInkMeetsWcagAaOnSoftFill);
    runner.Add("PetAccent/UnknownPetFallsBackToNeutral", TestUnknownPetFallsBackToNeutral);
    runner.Add("PetAccent/ShapeTintIsPalerThanLine", TestShapeTintIsPalerThanLine);
    runner.Add("PetAccent/ButtonSoftFillIsLightAndDeepInkIsDark", TestButtonSoftFillIsLightAndDeepInkIsDark);
    runner.Add("PetAccent/ButtonFillKeepsPetHue", TestButtonFillKeepsPetHue);
    runner.Add("PetAccent/AccentEmphasisPopulatedAndSafeFallback",
               TestAccentEmphasisPopulatedAndSafeFallback);
    runner.Add("PetAccent/DramaticPetsUseDeepEmphasis", TestDramaticPetsUseDeepEmphasis);
}

}  // namespace nimvlets::tests
