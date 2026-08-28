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

// Copy aprobada por el owner (Block 06.2 §14/§15). Ampliar acá cuando un
// bloque futuro autorice copy para el resto del roster — idealmente
// moviendo esta tabla a datos del catálogo/pack.
constexpr Entry kEditorial[] = {
    {"bunny", "Rabbit", "Conejo",
     "Small, curious, and never in a hurry.", "Pequeño, curioso y sin ninguna prisa."},
    {"nidir", "Black dragon", "Dragón negro",
     "Quiet wings. Bright eyes. Fire when it matters.", "Alas quietas. Ojos brillantes. Fuego cuando hace falta."},
    {"frin", "White wolf", "Lobo blanco",
     "Watchful, calm, and happiest close by.", "Atento, tranquilo y más feliz cerca."},
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
