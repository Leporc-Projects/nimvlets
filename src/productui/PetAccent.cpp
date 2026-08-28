#include "productui/PetAccent.h"

namespace nimvlets::productui {

namespace {

// Tono neutro por defecto: la misma terracota cálida del tema base
// (theme::kAccent). Para un pet desconocido o sin identidad asignada.
constexpr PetAccent kNeutral{
    UiColor{0xB4, 0x6E, 0x3C, 0xFF},
    UiColor{0xEC, 0xDD, 0xCC, 0xFF},
    false,
};

}  // namespace

PetAccent PetAccentFor(const std::string& petId) {
    // Nimvlets con arte real (Block 05/06).
    if (petId == "bunny") {
        // Apricot / crema cálida.
        return PetAccent{UiColor{0xE0, 0x9B, 0x63, 0xFF}, UiColor{0xF3, 0xE4, 0xD2, 0xFF}, false};
    }
    if (petId == "nidir") {
        // Violeta apagado, forma un poco más angular (dragón).
        return PetAccent{UiColor{0x93, 0x86, 0xBE, 0xFF}, UiColor{0xE7, 0xE2, 0xEF, 0xFF}, true};
    }
    if (petId == "frin") {
        // Azul hielo / neutro frío pálido (lobo blanco).
        return PetAccent{UiColor{0x8F, 0xAF, 0xC4, 0xFF}, UiColor{0xE3, 0xEC, 0xF1, 0xFF}, false};
    }

    // Nimvlets sin arte todavía — ids TENTATIVOS, tonos del brief §9,
    // registrados para no perder la dirección de diseño.
    if (petId == "rato") {
        return PetAccent{UiColor{0xE0, 0x95, 0x4E, 0xFF}, UiColor{0xF3, 0xE2, 0xCE, 0xFF}, false};
    }
    if (petId == "rin_rin" || petId == "rinrin") {
        return PetAccent{UiColor{0x5E, 0x8C, 0x63, 0xFF}, UiColor{0xDD, 0xE8, 0xDD, 0xFF}, false};
    }
    if (petId == "artu") {
        return PetAccent{UiColor{0xB7, 0x8A, 0x4E, 0xFF}, UiColor{0xEE, 0xE4, 0xCF, 0xFF}, false};
    }
    if (petId == "kyubi") {
        return PetAccent{UiColor{0x7A, 0x5A, 0x9E, 0xFF}, UiColor{0xE2, 0xDA, 0xEC, 0xFF}, true};
    }
    if (petId == "sweetie") {
        return PetAccent{UiColor{0xD0, 0x6A, 0x45, 0xFF}, UiColor{0xF2, 0xDD, 0xD1, 0xFF}, false};
    }

    return kNeutral;
}

}  // namespace nimvlets::productui
