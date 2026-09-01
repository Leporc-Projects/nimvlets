#pragma once

#include <string>
#include <vector>

#include "catalog/PetCatalog.h"
#include "catalog/PetEntitlement.h"
#include "catalog/PetIdentity.h"

namespace nimvlets::catalog {

// La vista de "álbum de compañeros" que el Product UI dibuja: qué
// Nimvlets POSEE el owner (y — para Frin — qué variantes concretas),
// cuál está en el escritorio. Puro: sin SDL, sin AppKit, sin I/O —
// construido a partir de un PetCatalog ya cargado + las AUTORIZACIONES
// de propiedad (PetEntitlement, Block 07) + la identidad activa. Ver
// docs/PRODUCT_UI.md §4/§5/§6.
//
// **Block 09C (DEC-136) — la Collection es SOLO lo poseído.** Un Nimvlet
// sin ninguna variante en la colección del owner NO aparece en el modelo
// (antes se listaba `kLocked` con "Not in your collection"). Lo
// públicamente comprable pero no poseído vive en el Shop, no acá:
// `BuildCollectionModel` descarta los ítems que quedarían `kLocked`.
//
// La Collection sigue siendo UN ítem lógico por Nimvlet. Block 07 agrega
// propiedad a nivel de VARIANTE: Frin puede tener Macho poseído y Hembra
// no (o al revés, o ambas). Un Frin con al menos una variante poseída SÍ
// aparece (`kOwnedInactive`), con la otra variante marcada no poseída. El
// Shop es una sección aparte — ver ShopModel.h; nada acá conoce precios
// ni compras.

enum class OwnershipStatus {
    // Poseído y actualmente en el escritorio ("On desktop"). Exactamente
    // uno de estos existe en un modelo bien formado.
    kActive,
    // Poseído (al menos una variante) pero no activo ("Use").
    kOwnedInactive,
    // Ninguna variante en la colección del owner. **Un ítem que deriva a
    // este estado se DESCARTA del modelo** (Block 09C / DEC-136) — el
    // valor sobrevive solo como paso intermedio del cálculo y para
    // `CanActivate`, que trata "no está en el modelo" == "no activable".
    kLocked,
};

// Una variante seleccionable de un CollectionItem. Un pet sin variantes
// (Bunny, Nidir) tiene exactamente una, con variantId vacío.
struct CollectionVariant {
    std::string variantId;    // "" = el pet no tiene variantes
    std::string displayName;  // normalmente el mismo displayName del pet
    // ¿El owner tiene derecho a poner ESTA variante en el escritorio?
    // Para un pet sin variantes = ¿posee el pet? Para Frin, por
    // variante. Una variante no poseída se muestra en el selector pero
    // NO es activable, y sin ninguna ruta de compra visible (brief §6).
    bool owned = false;
};

// Una fila del álbum: UN Nimvlet lógico. Las dos entradas de catálogo
// de Frin ({"frin","male"} y {"frin","female"}) colapsan a un solo
// CollectionItem con dos variantes.
struct CollectionItem {
    std::string petId;
    std::string displayName;
    OwnershipStatus status = OwnershipStatus::kLocked;

    // En el orden en que aparecen en el catálogo. size() >= 1 siempre.
    std::vector<CollectionVariant> variants;

    // Qué variante mostrar como seleccionada al abrir la colección. Para
    // el pet activo es la variante activa persistida (validada contra
    // `variants`); si no, la primera del catálogo. Vacío para un pet sin
    // variantes.
    std::string selectedVariantId;

    bool HasVariants() const { return variants.size() > 1; }

    // true si TODA variante está poseída (el caso normal tras la
    // migración de un Frin de Block 06). Un pet locked -> false.
    bool AllVariantsOwned() const;
    // true si `variantId` existe y su `owned` es true.
    bool VariantOwned(const std::string& variantId) const;
};

struct CollectionModel {
    // En orden de catálogo, una fila por petId lógico POSEÍDO (los
    // `kLocked` se descartan — Block 09C / DEC-136). size() >= 1 en un
    // modelo bien formado (siempre está al menos el pet activo).
    std::vector<CollectionItem> items;

    std::string activePetId;
    std::string activeVariantId;

    // nullptr si `petId` no está en el modelo.
    const CollectionItem* Find(const std::string& petId) const;

    // La fila con status kActive, o nullptr si el modelo está mal
    // formado (nunca debería pasar con un catálogo válido — el pet
    // activo/variante siempre es propio, ver ResolveOwnedActiveIdentity).
    const CollectionItem* Active() const;
};

// Construye el modelo. `owned` puede venir en cualquier orden y con
// duplicados. `activeId` es la identidad que el runtime tiene realmente
// activa. Determinista y puro. Descarta los Nimvlets no poseídos (Block
// 09C / DEC-136); el pet activo se conserva SIEMPRE. NO impone el
// invariante "el activo es propio" — para eso ver
// ResolveOwnedActiveIdentity, que src/app usa al resolver el arranque.
CollectionModel BuildCollectionModel(
    const PetCatalog& catalog,
    const std::vector<PetEntitlement>& owned,
    const PetIdentity& activeId);

// true solo si `petId` está en `model`, su status permite activarlo
// (kActive o kOwnedInactive) Y la variante pedida es poseída. Con
// `variantId` vacío en un pet CON variantes, exige que la variante por
// defecto (selectedVariantId) sea poseída. Un pet no poseído no está en
// el modelo (Block 09C) -> `Find` devuelve nullptr -> no activable; una
// variante no poseída de un pet por lo demás poseído TAMPOCO se puede
// activar (brief §6: "it must not be activatable").
bool CanActivate(const CollectionModel& model, const std::string& petId, const std::string& variantId = "");

// Devuelve una identidad que el owner REALMENTE tiene autorizada, para
// poner en el escritorio al arrancar — SIN otorgar nada. Ya que la
// propiedad es un entitlement pagado, cargar el estado no puede
// fabricar propiedad solo porque una identidad no autorizada aparezca
// como activa (p. ej. un archivo de estado corrupto/editado a mano con
// `active = nidir, owned = {bunny}` NO debe volverse `owned = bunny +
// nidir`). En cambio:
//   - si `wanted` está autorizado -> `wanted` sin cambios;
//   - si no -> el default del catálogo si está autorizado; si no, la
//     primera entrada del catálogo (en orden) cuya identidad esté
//     autorizada; si NADA está autorizado (estado degenerado, solo
//     alcanzable antes de la siembra) -> `wanted` sin cambios.
// `outFellBack` queda en true si el resultado != `wanted`. Determinista
// y puro. src/app repara `AppState::activePetId/activeVariantId` y
// marca dirty cuando `outFellBack` (salvo en una selección solo-DEV
// transitoria). Ver DEC-128.
PetIdentity ResolveOwnedActiveIdentity(
    const std::vector<PetEntitlement>& owned,
    const PetCatalog& catalog,
    const PetIdentity& wanted,
    bool& outFellBack);

// Reconcilia autorizaciones "históricas de pet entero" ({p, ""}) que
// vienen del modelo de propiedad "por pet lógico" de los schemas
// v1..v3 (el `ownedPetIds` "frin" de Block 06 se parsea PROVISIONALMENTE
// a `{frin, ""}` — el serializer no tiene catálogo, ver
// docs/PERSISTENCE.md §3). Reemplaza cada `{p, ""}` por las
// autorizaciones EXPLÍCITAS que poseer `p` significaba HISTÓRICAMENTE:
//
//   "frin"  ->  {frin, "male"}  {frin, "female"}
//   (cualquier otro petId)  ->  se deja como {p, ""} (era sin variantes)
//
// **El mapeo está CONGELADO y NO consulta el catálogo actual.** Una
// variante agregada al catálogo DESPUÉS del schema v3 (p. ej. un
// hipotético `frin/spirit`) nunca formó parte de lo que "poseer frin"
// significaba bajo el modelo viejo, así que migrar un estado legacy
// NUNCA la otorga — aunque ya exista en el catálogo cuando ocurre la
// migración (DEC-129).
//
// Es dato de compatibilidad histórica, NO lógica de producto de
// runtime: no hay una rama de Frin en la política de compra, el
// ShopModel, el switch de runtime, ni el matching de autorizaciones.
//
// src/app SOLO la corre cuando el estado vino GENUINAMENTE de un schema
// v1/v2/v3 en disco (ver DeserializeAppState / AppStateStore::Load y su
// out-param de versión); NUNCA sobre un v4. Canonicaliza el resultado.
// Idempotente, determinista. Devuelve true si cambió algo (src/app
// marca dirty para reescribir el save como v4 limpio).
bool ExpandHistoricalWholePetEntitlements(std::vector<PetEntitlement>& ents);

// La semilla de propiedad de desarrollo/default: por cada entrada
// `initiallyOwned` del catálogo, la autorización EXPLÍCITA de esa
// entrada ({petId, variantId}). El manifest de dev marca las DOS
// entradas de Frin, así que la semilla otorga `{frin, "male"}` y
// `{frin, "female"}` — NO un `{frin, ""}` de "todo Frin" (DEC-128).
// Canonicalizada. src/app la usa SOLO cuando AppState::ownershipSeeded
// todavía es false (o el conjunto quedó vacío por corrupción). Un
// bloque futuro de onboarding (Block 09) la reemplaza sin tocar el
// catálogo.
std::vector<PetEntitlement> SeedEntitlementsFromCatalog(const PetCatalog& catalog);

}  // namespace nimvlets::catalog
