#include "productui/PetEditorial.h"

namespace nimvlets::productui {

namespace {

struct Entry {
    const char* petId;
    const char* speciesEn;
    const char* speciesEs;
    const char* descEn;
    const char* descEs;
};

// Copy aprobada por el owner. Block 07 (brief §19): las descripciones
// pasan de una frase a un par de frases — un poco más de carácter, sin
// volverse un volcado de lore. La vista las envuelve en la columna del
// hero (DrawTextWrapped). Ampliar acá cuando un bloque futuro autorice
// copy para el resto del roster — idealmente moviendo esta tabla a
// datos del catálogo/pack.
constexpr Entry kEditorial[] = {
    {"bunny", "Rabbit", "Conejo",
     "Small, curious, and never in a hurry. Bunny prefers quiet corners, tiny adventures, and staying close while you work.",
     "Pequeño, curioso y sin ninguna prisa. Bunny prefiere los rincones tranquilos, las pequeñas aventuras y quedarse cerca mientras trabajas."},
    {"nidir", "Black dragon", "Dragón negro",
     "Quiet wings, bright eyes, and fire when it matters. Nidir watches the desktop like a tiny guardian, calm until something catches his attention.",
     "Alas quietas, ojos brillantes y fuego cuando hace falta. Nidir vigila el escritorio como un pequeño guardián, tranquilo hasta que algo llama su atención."},
    {"frin", "White wolf", "Lobo blanco",
     "Watchful, calm, and happiest close by. Frin carries the patience of a quiet wolf, always alert without needing to make a fuss.",
     "Atento, tranquilo y más feliz cerca. Frin tiene la paciencia de un lobo sereno, siempre alerta sin necesidad de hacer ruido."},
};

const Entry* Find(const std::string& petId) {
    for (const Entry& e : kEditorial) {
        if (petId == e.petId) {
            return &e;
        }
    }
    return nullptr;
}

}  // namespace

const char* Species(const std::string& petId, core::Language lang) {
    const Entry* e = Find(petId);
    if (e == nullptr) {
        return "";
    }
    return lang == core::Language::kEs ? e->speciesEs : e->speciesEn;
}

const char* ShortDescription(const std::string& petId, core::Language lang) {
    const Entry* e = Find(petId);
    if (e == nullptr) {
        return "";
    }
    return lang == core::Language::kEs ? e->descEs : e->descEn;
}

}  // namespace nimvlets::productui
