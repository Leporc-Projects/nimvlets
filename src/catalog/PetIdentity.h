#pragma once

#include <cstddef>
#include <functional>
#include <string>

namespace nimvlets::catalog {

// Identidad genérica de un Nimvlet seleccionable: un pet id estable
// más una variante opcional (ver el concepto de variante de Frin en
// docs/PET_CONTENT_SPEC.md — male/female comparten un mismo grupo pero
// son packs distintos). Deliberadamente solo dos strings, sin ningún
// enum de Nimvlets específicos: agregar un nuevo pet o variante nunca
// requiere tocar este tipo ni el código que lo consume — ver
// docs/CATALOG.md.
//
// `petId`/`variantId` son el esquema de identidad a nivel de catálogo
// (lo que se persiste y se usa para resolver/seleccionar), distinto de
// `content::PetDefinition::id`/`variantGroup` (el propio conocimiento
// interno de un pack ya cargado sobre sí mismo) — ver
// docs/CATALOG.md para por qué no se exige ninguna relación fija entre
// ambos en este bloque.
struct PetIdentity {
    std::string petId;
    std::string variantId;  // vacío = sin variante

    friend bool operator==(const PetIdentity& a, const PetIdentity& b) {
        return a.petId == b.petId && a.variantId == b.variantId;
    }
    friend bool operator!=(const PetIdentity& a, const PetIdentity& b) {
        return !(a == b);
    }

    // Orden estable y arbitrario (no tiene significado de producto) —
    // solo para poder usar PetIdentity como clave en contenedores
    // ordenados si algún día hace falta.
    friend bool operator<(const PetIdentity& a, const PetIdentity& b) {
        if (a.petId != b.petId) {
            return a.petId < b.petId;
        }
        return a.variantId < b.variantId;
    }
};

// Hash consistente con operator==: dos PetIdentity iguales siempre
// producen el mismo hash. Para uso con std::unordered_map/_set si hace
// falta más adelante — no hay ningún unordered_* de PetIdentity en
// este bloque todavía.
struct PetIdentityHash {
    std::size_t operator()(const PetIdentity& identity) const {
        const std::size_t h1 = std::hash<std::string>()(identity.petId);
        const std::size_t h2 = std::hash<std::string>()(identity.variantId);
        // Combinación simple (no criptográfica) — suficiente para un
        // hash de uso interno en un contenedor, no para nada de
        // seguridad.
        return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};

}  // namespace nimvlets::catalog
