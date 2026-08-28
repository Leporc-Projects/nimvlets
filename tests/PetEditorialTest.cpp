#include "PetEditorialTest.h"

#include <cstring>
#include <string>

#include "productui/PetEditorial.h"

using nimvlets::core::Language;
using nimvlets::productui::ShortDescription;
using nimvlets::productui::Species;

namespace nimvlets::tests {

namespace {

bool Eq(const char* a, const char* b) {
    return std::strcmp(a, b) == 0;
}

// Copy APROBADA por el owner (Block 06.2 §14/§15) — inglés.
bool TestApprovedEditorialEnglish() {
    NIMVLETS_CHECK(Eq(Species("bunny", Language::kEn), "Rabbit"));
    NIMVLETS_CHECK(Eq(ShortDescription("bunny", Language::kEn), "Small, curious, and never in a hurry."));
    NIMVLETS_CHECK(Eq(Species("nidir", Language::kEn), "Black dragon"));
    NIMVLETS_CHECK(Eq(ShortDescription("nidir", Language::kEn), "Quiet wings. Bright eyes. Fire when it matters."));
    NIMVLETS_CHECK(Eq(Species("frin", Language::kEn), "White wolf"));
    NIMVLETS_CHECK(Eq(ShortDescription("frin", Language::kEn), "Watchful, calm, and happiest close by."));
    return true;
}

// Copy APROBADA — español.
bool TestApprovedEditorialSpanish() {
    NIMVLETS_CHECK(Eq(Species("bunny", Language::kEs), "Conejo"));
    NIMVLETS_CHECK(Eq(ShortDescription("bunny", Language::kEs), "Pequeño, curioso y sin ninguna prisa."));
    NIMVLETS_CHECK(Eq(Species("nidir", Language::kEs), "Dragón negro"));
    NIMVLETS_CHECK(Eq(ShortDescription("nidir", Language::kEs), "Alas quietas. Ojos brillantes. Fuego cuando hace falta."));
    NIMVLETS_CHECK(Eq(Species("frin", Language::kEs), "Lobo blanco"));
    NIMVLETS_CHECK(Eq(ShortDescription("frin", Language::kEs), "Atento, tranquilo y más feliz cerca."));
    return true;
}

// La especie SÍ se traduce (es texto de interfaz, no un nombre propio).
bool TestSpeciesIsLocalized() {
    NIMVLETS_CHECK(!Eq(Species("bunny", Language::kEn), Species("bunny", Language::kEs)));
    NIMVLETS_CHECK(!Eq(Species("frin", Language::kEn), Species("frin", Language::kEs)));
    return true;
}

// Un pet sin copy aprobada -> "" en ambos campos y ambos idiomas (el
// hero omite la línea). El roster futuro queda sin copy hasta que se
// escriba (brief §14/§16).
bool TestUnauthoredPetsAreEmpty() {
    for (const char* id : {"kyubi", "rato", "artu", "sweetie", "rin_rin", "does_not_exist", ""}) {
        NIMVLETS_CHECK(Species(id, Language::kEn)[0] == '\0');
        NIMVLETS_CHECK(Species(id, Language::kEs)[0] == '\0');
        NIMVLETS_CHECK(ShortDescription(id, Language::kEn)[0] == '\0');
        NIMVLETS_CHECK(ShortDescription(id, Language::kEs)[0] == '\0');
    }
    return true;
}

// El nombre propio "Frin" nunca aparece dentro del copy editorial (no se
// traduce ni se repite ahí — el hero ya lo muestra aparte).
bool TestNoProperNameInsideCopy() {
    for (Language lang : {Language::kEn, Language::kEs}) {
        NIMVLETS_CHECK(std::string(ShortDescription("frin", lang)).find("Frin") == std::string::npos);
        NIMVLETS_CHECK(std::string(ShortDescription("bunny", lang)).find("Bunny") == std::string::npos);
        NIMVLETS_CHECK(std::string(ShortDescription("nidir", lang)).find("Nidir") == std::string::npos);
    }
    return true;
}

}  // namespace

void RegisterPetEditorialTests(testing::TestRunner& runner) {
    runner.Add("PetEditorial/ApprovedEditorialEnglish", TestApprovedEditorialEnglish);
    runner.Add("PetEditorial/ApprovedEditorialSpanish", TestApprovedEditorialSpanish);
    runner.Add("PetEditorial/SpeciesIsLocalized", TestSpeciesIsLocalized);
    runner.Add("PetEditorial/UnauthoredPetsAreEmpty", TestUnauthoredPetsAreEmpty);
    runner.Add("PetEditorial/NoProperNameInsideCopy", TestNoProperNameInsideCopy);
}

}  // namespace nimvlets::tests
