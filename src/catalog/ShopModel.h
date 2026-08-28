#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "catalog/PetCatalog.h"
#include "catalog/PetEntitlement.h"

namespace nimvlets::catalog {

// El modelo del SHOP (Block 07): los Nimvlets que el owner puede comprar
// públicamente, con su precio en clics y su estado frente al balance y
// la propiedad actuales. Puro: sin SDL, sin I/O — construido a partir de
// un PetCatalog ya cargado + el balance + las autorizaciones. Ver
// docs/PRODUCT_UI.md §7-§10.
//
// El Shop es una sección SEPARADA de la Collection (brief §6/§16): esto
// no reemplaza a CollectionModel ni lo conoce. Una fila por pet lógico,
// igual que la Collection, pero SOLO los que tienen al menos una entrada
// de catálogo con `publiclyPurchasable`. Frin no lo tiene en ninguna
// variante -> NUNCA aparece en el Shop, aunque su entrada de catálogo
// exista (brief §11).

enum class ShopItemStatus {
    // No poseído, el balance alcanza -> acción de compra habilitada.
    kAffordable,
    // No poseído, faltan clics -> acción no disponible; `clicksShort`
    // dice cuántos faltan.
    kInsufficientBalance,
    // Ya en la colección -> sin acción, sin precio accionable ("In your
    // collection").
    kOwned,
};

struct ShopItem {
    std::string petId;
    std::string displayName;       // nombre propio — nunca traducido
    std::uint64_t priceClicks = 0;
    ShopItemStatus status = ShopItemStatus::kAffordable;

    // > 0 solo si status == kInsufficientBalance: price - balance.
    std::uint64_t clicksShort = 0;

    // Qué autorización otorga comprar este ítem. Para los pets sin
    // variantes de Block 07 (Bunny, Nidir) es {petId, ""} = el pet
    // entero. El campo existe para que una compra por variante (shop
    // oculto, futuro) encaje sin rediseño.
    PetEntitlement entitlementTarget;
};

struct ShopModel {
    // En orden de catálogo, SOLO pets públicamente comprables.
    std::vector<ShopItem> items;

    // nullptr si `petId` no está en el Shop (no público, o desconocido).
    const ShopItem* Find(const std::string& petId) const;
};

// Construye el modelo del Shop. Determinista y puro. Agrupa por petId
// (una fila por pet lógico); para un pet con varias entradas públicas
// —caso futuro, hoy ninguno— toma la primera en orden de catálogo.
ShopModel BuildShopModel(
    const PetCatalog& catalog,
    std::uint64_t clickBalance,
    const std::vector<PetEntitlement>& ownedEntitlements);

}  // namespace nimvlets::catalog
