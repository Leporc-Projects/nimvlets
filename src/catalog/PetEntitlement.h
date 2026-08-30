#pragma once

#include <string>
#include <vector>

#include "catalog/PetIdentity.h"

namespace nimvlets::catalog {

// Una AUTORIZACIÓN de propiedad (Block 07). Reemplaza al modelo de
// "poseer un petId entero" de Block 06 (`AppState::ownedPetIds`, ahora
// superseded — ver docs/CATALOG.md §11 y docs/PERSISTENCE.md §3).
//
// Misma FORMA que PetIdentity (dos strings, sin enum) y misma semántica
// de `variantId`:
//
//   petId no vacío + variantId vacío   -> el Nimvlet NO tiene variantes
//                                         (Bunny, Nidir). Autoriza esa
//                                         única forma.
//   petId no vacío + variantId no vacío -> SOLO esa variante.
//
// NO existe una autorización de "todas las variantes de un Nimvlet
// capaz de variantes". La propiedad de Frin es SIEMPRE por variante,
// explícita — poseer Frin Macho no implica poseer Frin Hembra ni una
// tercera variante futura. Un `{frin, ""}` que llegue por un archivo
// hecho a mano o por una migración legacy se EXPANDE a las variantes
// que el catálogo define (ver ExpandHistoricalWholePetEntitlements en
// CollectionModel.h), nunca se interpreta como "todo Frin". Block 06
// exponía Macho + Hembra; no estableció propiedad de variantes futuras
// (DEC-128).
//
// El onboarding futuro (Block 09) otorga UNA variante de Frin, y la
// otra se obtiene por separado. Block 07 NO implementa ese flujo — solo
// deja el modelo listo (brief §4).
//
// Se mantiene como tipo propio y no un alias de PetIdentity porque su
// rol es distinto ("qué tengo derecho a usar" vs. "qué está
// seleccionado/activo").
struct PetEntitlement {
    std::string petId;
    std::string variantId;  // "" = el Nimvlet no tiene variantes

    friend bool operator==(const PetEntitlement& a, const PetEntitlement& b) {
        return a.petId == b.petId && a.variantId == b.variantId;
    }
    friend bool operator!=(const PetEntitlement& a, const PetEntitlement& b) { return !(a == b); }

    // Orden canónico total: por petId, luego por variantId. Solo es un
    // orden estable para canonicalizar (sin significado de producto).
    friend bool operator<(const PetEntitlement& a, const PetEntitlement& b) {
        if (a.petId != b.petId) {
            return a.petId < b.petId;
        }
        return a.variantId < b.variantId;
    }

    // ¿Esta autorización da derecho a usar EXACTAMENTE `identity`?
    // Coincidencia exacta en los dos campos — `{frin, ""}` NO cubre
    // `{frin, "male"}`, y `{frin, "male"}` NO cubre `{frin, "female"}`.
    // (Para un Nimvlet sin variantes ambos lados traen variantId "".)
    bool Covers(const PetIdentity& identity) const {
        return petId == identity.petId && variantId == identity.variantId;
    }
};

// Deja `ents` en forma canónica y determinista:
//   1. descarta cualquier entrada con petId vacío (nunca es un pet real);
//   2. ordena ascendente por (petId, variantId);
//   3. elimina duplicados exactos.
// Idempotente. Sin conocimiento del catálogo (lógica de strings pura),
// así que sirve igual al serializer (que aplica un orden equivalente
// sobre una copia para que el formato en disco sea determinista) y a
// src/app (que la aplica antes de guardar cada mutación).
//
// NO hay subsunción: `{frin, ""}` y `{frin, "male"}` conviven sin que
// uno absorba al otro — no existe la noción de "pet entero" que hacía
// falta para subsumir. Un `{frin, ""}` legacy se resuelve ANTES, en
// src/app, expandiéndolo contra el catálogo (DEC-128).
void CanonicalizePetEntitlements(std::vector<PetEntitlement>& ents);

// ¿`ents` autoriza a usar `identity` (por ejemplo, el pet/variante que
// se quiere poner en el escritorio)? Coincidencia EXACTA — el "exact
// active variant must be owned before activation" del brief §4: para
// Frin, la variante exacta tiene que estar autorizada, no solo "algo de
// Frin".
bool OwnsIdentity(const std::vector<PetEntitlement>& ents, const PetIdentity& identity);

// ¿`ents` autoriza ALGUNA variante de `petId`? (El pet aparece en la
// colección como poseído, aunque no todas sus variantes lo estén.)
bool OwnsAnyVariantOfPet(const std::vector<PetEntitlement>& ents, const std::string& petId);

}  // namespace nimvlets::catalog
