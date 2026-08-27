#pragma once

#include <string>
#include <vector>

#include "catalog/PetCatalog.h"
#include "catalog/PetIdentity.h"

namespace nimvlets::catalog {

// La vista de "álbum de compañeros" que el Product UI de Block 06
// dibuja: qué Nimvlets existen, cuáles posee el owner, cuál está en el
// escritorio, y — para Frin — sus variantes como UN solo Nimvlet
// lógico. Puro: sin SDL, sin AppKit, sin I/O — construido a partir de
// un PetCatalog ya cargado + el conjunto de petIds poseídos + la
// identidad activa. Ver docs/PRODUCT_UI.md §4 y block brief §8-§11.
//
// Deliberadamente NO es un grid de ecommerce ni un modelo de Shop:
// nada acá conoce precios ni compras (Block 07). Los tres estados de
// propiedad ya existen ahora para que el modelo no necesite rediseño
// cuando llegue el Shop.

enum class OwnershipStatus {
    // Poseído y actualmente en el escritorio ("On desktop"). Exactamente
    // uno de estos existe en un modelo bien formado.
    kActive,
    // Poseído pero no activo ("Use").
    kOwnedInactive,
    // No está en la colección del owner ("Not in your collection").
    // Este bloque NO puede desbloquearlo (sin compra todavía).
    kLocked,
};

// Una variante seleccionable de un CollectionItem. Un pet sin variantes
// (Bunny, Nidir) tiene exactamente una, con variantId vacío.
struct CollectionVariant {
    std::string variantId;    // "" = el pet no tiene variantes
    std::string displayName;  // normalmente el mismo displayName del pet
};

// Una fila del álbum: UN Nimvlet lógico. Las dos entradas de catálogo
// de Frin ({"frin","male"} y {"frin","female"}) colapsan a un solo
// CollectionItem con dos variantes — nunca dos filas no relacionadas
// (block brief §11).
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
    // activo siempre es propio, ver EnsureActivePetOwned).
    const CollectionItem* Active() const;
};

// Construye el modelo. `ownedPetIds` puede venir en cualquier orden y
// con duplicados; `activeId` es la identidad que el runtime tiene
// realmente activa. Determinista y puro. NO impone el invariante "el
// activo es propio" — para eso ver EnsureActivePetOwned, que src/app
// llama antes, al resolver el arranque.
CollectionModel BuildCollectionModel(
    const PetCatalog& catalog,
    const std::vector<std::string>& ownedPetIds,
    const PetIdentity& activeId);

// true solo si `petId` está en `model` y su status permite activarlo:
// kActive (ya activo — activar de nuevo es un no-op inofensivo) o
// kOwnedInactive. Un pet kLocked NUNCA puede activarse en este bloque
// (block brief §9: "locked pet cannot become active").
bool CanActivate(const CollectionModel& model, const std::string& petId);

// Invariante "el pet activo siempre es propio" (block brief §9/§27).
// Agrega `activePetId` a `ownedPetIds` si falta; ordena y deduplica.
// Devuelve true si tuvo que cambiar algo. No-op si `activePetId` está
// vacío. src/app lo llama al resolver el pet de arranque, ANTES de
// construir el modelo.
bool EnsureActivePetOwned(std::vector<std::string>& ownedPetIds, const std::string& activePetId);

// La semilla de propiedad de desarrollo/default (block brief §12): los
// petIds de las entradas `initiallyOwned` del catálogo, deduplicados y
// ordenados. src/app la usa SOLO cuando AppState::ownershipSeeded
// todavía es false — nunca en cada arranque. Un bloque futuro de
// onboarding (Block 09) reemplaza esta llamada por la elección real de
// starter sin tocar el catálogo ni este archivo.
std::vector<std::string> SeedOwnershipFromCatalog(const PetCatalog& catalog);

}  // namespace nimvlets::catalog
