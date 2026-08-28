#include "productui/PetEditorial.h"

namespace nimvlets::productui {

const char* ProvisionalSpecies(const std::string& petId, core::Language lang) {
    const bool es = lang == core::Language::kEs;
    if (petId == "bunny") {
        return es ? "Conejo" : "Rabbit";
    }
    if (petId == "nidir") {
        return es ? "Dragón negro" : "Black dragon";
    }
    if (petId == "frin") {
        return es ? "Lobo" : "Wolf";
    }
    return "";
}

const char* ShortDescription(const std::string& /*petId*/, core::Language /*lang*/) {
    // Reservado: sin contenido en Block 06.1 (brief §14).
    return "";
}

}  // namespace nimvlets::productui
