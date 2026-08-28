#pragma once

#include <string>
#include <vector>

#include "catalog/PetCatalog.h"
#include "catalog/PetEntitlement.h"
#include "catalog/PetIdentity.h"

namespace nimvlets::catalog {

// La vista de "álbum de compañeros" que el Product UI dibuja: qué
// Nimvlets existen, cuáles posee el owner (y — para Frin — qué variantes
// concretas), cuál está en el escritorio. Puro: sin SDL, sin AppKit, sin
// I/O — construido a partir de un PetCatalog ya cargado + las
// AUTORIZACIONES de propiedad (PetEntitlement, Block 07 — antes un
// simple conjunto de petIds) + la identidad activa. Ver
// docs/PRODUCT_UI.md §4 y §6.
//
// La Collection sigue siendo UN ítem lógico por Nimvlet. Lo que Block 07
// agrega es propiedad a nivel de VARIANTE: Frin puede tener Macho
// poseído y Hembra no (o al revés, o ambas, o ninguna). El Shop es una
// sección aparte — ver ShopModel.h; nada acá conoce precios ni compras.

enum class OwnershipStatus {
    // Poseído y actualmente en el escritorio ("On desktop"). Exactamente
    // uno de estos existe en un modelo bien formado.
    kActive,
    // Poseído (al menos una variante) pero no activo ("Use").
    kOwnedInactive,
    // Ninguna variante en la colección del owner ("Not in your
    // collection"). El Shop puede cambiar esto (Block 07); un pet locked
    // sigue sin poder activarse.
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
    // En orden de catálogo, una fila por petId lógico.
    std::vector<CollectionItem> items;

    std::string activePetId;
    std::string activeVariantId;

    // nullptr si `petId` no está en el modelo.
    const CollectionItem* Find(const std::string& petId) const;

    // La fila con status kActive, o nullptr si el modelo está mal
    // formado (nunca debería pasar con un catálogo válido — el pet
    // activo/variante siempre es propio, ver EnsureActiveEntitlementOwned).
    const CollectionItem* Active() const;
};

// Construye el modelo. `owned` puede venir en cualquier orden y con
// duplicados. `activeId` es la identidad que el runtime tiene realmente
// activa. Determinista y puro. NO impone el invariante "el activo es
// propio" — para eso ver EnsureActiveEntitlementOwned, que src/app llama
// antes, al resolver el arranque.
CollectionModel BuildCollectionModel(
    const PetCatalog& catalog,
    const std::vector<PetEntitlement>& owned,
    const PetIdentity& activeId);

// true solo si `petId` está en `model`, su status permite activarlo
// (kActive o kOwnedInactive) Y la variante pedida es poseída. Con
// `variantId` vacío en un pet CON variantes, exige que la variante por
// defecto (selectedVariantId) sea poseída. Un pet kLocked NUNCA puede
// activarse, y una variante no poseída de un pet por lo demás poseído
// TAMPOCO (brief §6: "it must not be activatable").
bool CanActivate(const CollectionModel& model, const std::string& petId, const std::string& variantId = "");

// Invariante "el pet/variante activo siempre es propio" (brief §5).
// Si `active` no está cubierto por `ents`, agrega la autorización
// EXACTA de esa identidad ({petId, variantId}); canonicaliza. Devuelve
// true si tuvo que cambiar algo. No-op si `active.petId` está vacío.
// src/app lo llama al resolver el pet de arranque y tras cada switch,
// ANTES de construir el modelo.
bool EnsureActiveEntitlementOwned(std::vector<PetEntitlement>& ents, const PetIdentity& active);

// La semilla de propiedad de desarrollo/default: por cada entrada
// `initiallyOwned` del catálogo, una autorización de PET ENTERO
// ({petId, ""}) — así un Frin sembrado da acceso a macho y hembra,
// exactamente como el `ownedPetIds` de Block 06 (brief §5).
// Canonicalizada. src/app la usa SOLO cuando
// AppState::ownershipSeeded todavía es false. Un bloque futuro de
// onboarding (Block 09) la reemplaza sin tocar el catálogo.
std::vector<PetEntitlement> SeedEntitlementsFromCatalog(const PetCatalog& catalog);

}  // namespace nimvlets::catalog
