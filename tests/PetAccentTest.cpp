#include "PetAccentTest.h"

#include "productui/PetAccent.h"
#include "productui/PetEditorial.h"

#include <cstring>
#include <string>

using nimvlets::core::Language;
using nimvlets::productui::PetAccent;
using nimvlets::productui::PetAccentFor;
using nimvlets::productui::ProvisionalSpecies;
using nimvlets::productui::ShortDescription;
using nimvlets::productui::UiColor;

namespace nimvlets::tests {

namespace {

bool TestKnownPetsHaveDistinctAccents() {
    const PetAccent bunny = PetAccentFor("bunny");
    const PetAccent nidir = PetAccentFor("nidir");
    const PetAccent frin = PetAccentFor("frin");

    // Los tres tonos de identidad son distintos entre sí (brief §9).
    NIMVLETS_CHECK(!(bunny.line == nidir.line));
    NIMVLETS_CHECK(!(bunny.line == frin.line));
    NIMVLETS_CHECK(!(nidir.line == frin.line));

    // Todos opacos (a == 255) — la vista aplica el alpha bajo, no el dato.
    NIMVLETS_CHECK(bunny.line.a == 255 && bunny.shapeTint.a == 255);
    NIMVLETS_CHECK(nidir.shapeTint.a == 255);
    return true;
}

// Direcciones de diseño del brief §9: Bunny cálido (apricot), Nidir
// violeta (azul > rojo, tono frío-violeta), Frin azul frío (azul > rojo).
bool TestAccentHuesMatchDesignDirection() {
    const PetAccent bunny = PetAccentFor("bunny");
    NIMVLETS_CHECK(bunny.line.r > bunny.line.b);  // cálido

    const PetAccent frin = PetAccentFor("frin");
    NIMVLETS_CHECK(frin.line.b > frin.line.r);  // azul hielo / frío

    const PetAccent nidir = PetAccentFor("nidir");
    NIMVLETS_CHECK(nidir.line.b >= nidir.line.r);  // violeta apagado
    return true;
}

// Nidir usa una forma un poco más angular; Bunny/Frin, orgánica
// (brief §10).
bool TestHeroShapeAngularityPerPet() {
    NIMVLETS_CHECK(!PetAccentFor("bunny").angularShape);
    NIMVLETS_CHECK(!PetAccentFor("frin").angularShape);
    NIMVLETS_CHECK(PetAccentFor("nidir").angularShape);
    return true;
}

// Un petId desconocido nunca falla: cae al acento neutro (terracota).
bool TestUnknownPetFallsBackToNeutral() {
    const PetAccent unknown = PetAccentFor("does_not_exist");
    const PetAccent alsoUnknown = PetAccentFor("");
    NIMVLETS_CHECK(unknown.line == alsoUnknown.line);
    NIMVLETS_CHECK(unknown.line.a == 255);
    // El neutro es cálido (terracota), distinto de todo tono frío.
    NIMVLETS_CHECK(unknown.line.r > unknown.line.b);
    return true;
}

// El shapeTint es más claro que la línea (versión pálida del mismo
// tono) — para que la forma "apoye" el arte, no compita (brief §10).
bool TestShapeTintIsPalerThanLine() {
    for (const char* id : {"bunny", "nidir", "frin"}) {
        const PetAccent a = PetAccentFor(id);
        const int lineLum = a.line.r + a.line.g + a.line.b;
        const int tintLum = a.shapeTint.r + a.shapeTint.g + a.shapeTint.b;
        NIMVLETS_CHECK(tintLum > lineLum);
    }
    return true;
}

// --- Editorial provisional -------------------------------------------

bool TestProvisionalSpeciesFactualLabels() {
    NIMVLETS_CHECK(std::strcmp(ProvisionalSpecies("bunny", Language::kEn), "Rabbit") == 0);
    NIMVLETS_CHECK(std::strcmp(ProvisionalSpecies("bunny", Language::kEs), "Conejo") == 0);
    NIMVLETS_CHECK(std::strcmp(ProvisionalSpecies("nidir", Language::kEn), "Black dragon") == 0);
    NIMVLETS_CHECK(std::strcmp(ProvisionalSpecies("nidir", Language::kEs), "Dragón negro") == 0);
    NIMVLETS_CHECK(std::strcmp(ProvisionalSpecies("frin", Language::kEn), "Wolf") == 0);
    NIMVLETS_CHECK(std::strcmp(ProvisionalSpecies("frin", Language::kEs), "Lobo") == 0);
    return true;
}

// Un pet sin especie asignada -> "". Y la línea de personalidad NO se
// escribe en este bloque (brief §14): siempre "".
bool TestUnknownSpeciesAndNoShortDescription() {
    NIMVLETS_CHECK(ProvisionalSpecies("kyubi", Language::kEn)[0] == '\0');
    NIMVLETS_CHECK(ProvisionalSpecies("does_not_exist", Language::kEs)[0] == '\0');
    NIMVLETS_CHECK(ShortDescription("bunny", Language::kEn)[0] == '\0');
    NIMVLETS_CHECK(ShortDescription("nidir", Language::kEs)[0] == '\0');
    return true;
}

}  // namespace

void RegisterPetAccentTests(testing::TestRunner& runner) {
    runner.Add("PetAccent/KnownPetsHaveDistinctAccents", TestKnownPetsHaveDistinctAccents);
    runner.Add("PetAccent/AccentHuesMatchDesignDirection", TestAccentHuesMatchDesignDirection);
    runner.Add("PetAccent/HeroShapeAngularityPerPet", TestHeroShapeAngularityPerPet);
    runner.Add("PetAccent/UnknownPetFallsBackToNeutral", TestUnknownPetFallsBackToNeutral);
    runner.Add("PetAccent/ShapeTintIsPalerThanLine", TestShapeTintIsPalerThanLine);
    runner.Add("PetAccent/ProvisionalSpeciesFactualLabels", TestProvisionalSpeciesFactualLabels);
    runner.Add("PetAccent/UnknownSpeciesAndNoShortDescription", TestUnknownSpeciesAndNoShortDescription);
}

}  // namespace nimvlets::tests
