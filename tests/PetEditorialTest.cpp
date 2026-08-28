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

// Copy APROBADA por el owner (Block 07 brief §19: descripciones un poco
// más largas, un par de frases) — inglés. Verbatim.
bool TestApprovedEditorialEnglish() {
    NIMVLETS_CHECK(Eq(Species("bunny", Language::kEn), "Rabbit"));
    NIMVLETS_CHECK(Eq(ShortDescription("bunny", Language::kEn),
                      "Small, curious, and never in a hurry. Bunny prefers quiet corners, tiny "
                      "adventures, and staying close while you work."));
    NIMVLETS_CHECK(Eq(Species("nidir", Language::kEn), "Black dragon"));
    NIMVLETS_CHECK(Eq(ShortDescription("nidir", Language::kEn),
                      "Quiet wings, bright eyes, and fire when it matters. Nidir watches the desktop "
                      "like a tiny guardian, calm until something catches his attention."));
    NIMVLETS_CHECK(Eq(Species("frin", Language::kEn), "White wolf"));
    NIMVLETS_CHECK(Eq(ShortDescription("frin", Language::kEn),
                      "Watchful, calm, and happiest close by. Frin carries the patience of a quiet "
                      "wolf, always alert without needing to make a fuss."));
    return true;
}

// Copy APROBADA — español. Verbatim.
bool TestApprovedEditorialSpanish() {
    NIMVLETS_CHECK(Eq(Species("bunny", Language::kEs), "Conejo"));
    NIMVLETS_CHECK(Eq(ShortDescription("bunny", Language::kEs),
                      "Pequeño, curioso y sin ninguna prisa. Bunny prefiere los rincones "
                      "tranquilos, las pequeñas aventuras y quedarse cerca mientras trabajas."));
    NIMVLETS_CHECK(Eq(Species("nidir", Language::kEs), "Dragón negro"));
    NIMVLETS_CHECK(Eq(ShortDescription("nidir", Language::kEs),
                      "Alas quietas, ojos brillantes y fuego cuando hace falta. Nidir vigila el "
                      "escritorio como un pequeño guardián, tranquilo hasta que algo llama su "
                      "atención."));
    NIMVLETS_CHECK(Eq(Species("frin", Language::kEs), "Lobo blanco"));
    NIMVLETS_CHECK(Eq(ShortDescription("frin", Language::kEs),
                      "Atento, tranquilo y más feliz cerca. Frin tiene la paciencia de un lobo "
                      "sereno, siempre alerta sin necesidad de hacer ruido."));
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

// Block 07: la copy ahora es de un PAR de frases (brief §19) — más
// larga que la de Block 06.2, con al menos un punto y seguido, y sin
// mencionar a OTROS Nimvlets ni "Nimvlets". (Que un pet se nombre a sí
// mismo en su propia descripción es la copy que el owner escribió —
// DEC-125.)
bool TestCopyIsTwoSentences() {
    struct { const char* id; const char* other1; const char* other2; } pets[] = {
        {"bunny", "Nidir", "Frin"}, {"nidir", "Bunny", "Frin"}, {"frin", "Bunny", "Nidir"}};
    for (const auto& p : pets) {
        for (Language lang : {Language::kEn, Language::kEs}) {
            const std::string d = ShortDescription(p.id, lang);
            NIMVLETS_CHECK(d.size() > 80);
            const std::size_t dot = d.find(". ");
            NIMVLETS_CHECK(dot != std::string::npos && dot > 0 && dot < d.size() - 2);
            NIMVLETS_CHECK(d.find(p.other1) == std::string::npos);
            NIMVLETS_CHECK(d.find(p.other2) == std::string::npos);
            NIMVLETS_CHECK(d.find("Nimvlet") == std::string::npos);
        }
    }
    return true;
}

}  // namespace

void RegisterPetEditorialTests(testing::TestRunner& runner) {
    runner.Add("PetEditorial/ApprovedEditorialEnglish", TestApprovedEditorialEnglish);
    runner.Add("PetEditorial/ApprovedEditorialSpanish", TestApprovedEditorialSpanish);
    runner.Add("PetEditorial/SpeciesIsLocalized", TestSpeciesIsLocalized);
    runner.Add("PetEditorial/UnauthoredPetsAreEmpty", TestUnauthoredPetsAreEmpty);
    runner.Add("PetEditorial/CopyIsTwoSentences", TestCopyIsTwoSentences);
}

}  // namespace nimvlets::tests
