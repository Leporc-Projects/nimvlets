#pragma once

#include <string>
#include <vector>

#include "catalog/PetIdentity.h"

namespace nimvlets::catalog {

// Una AUTORIZACIÓN de propiedad genérica (Block 07). Reemplaza al modelo
// de "poseer un petId entero" de Block 06 (`AppState::ownedPetIds`, ahora
// superseded — ver docs/CATALOG.md §11 y docs/PERSISTENCE.md §3).
//
//   petId no vacío + variantId vacío  -> "el Nimvlet completo": cubre
//                                        toda variante presente y futura.
//   petId no vacío + variantId no vacío -> SOLO esa variante.
//
// El segundo caso es lo que el producto final necesita y Block 06 no
// podía expresar: el onboarding futuro (Block 09) otorga UNA sola
// variante de Frin, y la otra se obtiene por separado. Block 07 NO
// implementa ese flujo — solo deja el modelo listo (brief §4).
//
// Deliberadamente la MISMA forma que PetIdentity (dos strings, sin
// enum): agregar un pet o una variante nunca toca este tipo. Se
// mantiene como tipo propio y no un alias de PetIdentity porque su
// semántica es distinta — "qué tengo derecho a usar" vs. "qué está
// seleccionado/activo" — y `variantId` vacío significa cosas distintas
// en cada uno ("cualquier variante" acá; "el pet no tiene variantes"
// en una identidad).
struct PetEntitlement {
    std::string petId;
    std::string variantId;  // "" = el pet entero (cualquier variante)

    friend bool operator==(const PetEntitlement& a, const PetEntitlement& b) {
        return a.petId == b.petId && a.variantId == b.variantId;
    }
    friend bool operator!=(const PetEntitlement& a, const PetEntitlement& b) { return !(a == b); }

    // Orden canónico total: por petId, luego por variantId, con el
    // "pet entero" (variantId == "") SIEMPRE antes que sus variantes
    // concretas del mismo petId — así CanonicalizePetEntitlements puede
    // aplicar la subsunción en una sola pasada lineal.
    friend bool operator<(const PetEntitlement& a, const PetEntitlement& b) {
        if (a.petId != b.petId) {
            return a.petId < b.petId;
        }
        return a.variantId < b.variantId;  // "" ordena primero
    }

    // ¿Esta autorización da derecho a usar `identity`? Un "pet entero"
    // cubre cualquier variante de su petId; una autorización de variante
    // concreta cubre solo esa. (Para un pet sin variantes, `identity`
    // llega con variantId == "" y ambos lados coinciden.)
    bool Covers(const PetIdentity& identity) const {
        return petId == identity.petId && (variantId.empty() || variantId == identity.variantId);
    }
};

// Deja `ents` en forma canónica, determinista y sin redundancia:
//   1. descarta cualquier entrada con petId vacío (nunca es un pet real);
//   2. ordena ascendente por (petId, variantId);
//   3. elimina duplicados exactos;
//   4. SUBSUNCIÓN: si existe (p, "") para un petId `p`, descarta toda
//      (p, <no vacío>) — la autorización de pet entero ya las incluye.
// Idempotente. Sin conocimiento del catálogo (es lógica de strings
// pura), así que sirve igual al serializer (que la aplica sobre una
// copia para que el formato en disco sea determinista) y a src/app (que
// la aplica antes de guardar cada mutación).
void CanonicalizePetEntitlements(std::vector<PetEntitlement>& ents);

// ¿`ents` autoriza a usar `identity` (por ejemplo, el pet/variante que
// se quiere poner en el escritorio)? Este es el "exact active variant
// must be owned before activation" del brief §4: para Frin, la variante
// exacta tiene que estar cubierta, no solo "algo de Frin".
bool OwnsIdentity(const std::vector<PetEntitlement>& ents, const PetIdentity& identity);

// ¿`ents` autoriza ALGUNA variante de `petId`? (El pet aparece en la
// colección como poseído, aunque no todas sus variantes lo estén.)
bool OwnsAnyVariantOfPet(const std::vector<PetEntitlement>& ents, const std::string& petId);

}  // namespace nimvlets::catalog
