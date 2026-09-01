#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "catalog/PetCatalog.h"
#include "catalog/PetEntitlement.h"
#include "catalog/PetIdentity.h"

namespace nimvlets::catalog {

// El modelo del SHOP OCULTO DE STARTERS (Block 10): las compras de
// starter CONTEXTUALES que un usuario que YA completó el onboarding
// puede hacer con clics — su primer propósito real es comprar la
// variante de Frin que NO eligió en el onboarding, y la arquitectura
// soporta además starters NORMALES no poseídos.
//
// Puro: sin SDL, sin AppKit, sin I/O — construido a partir de un
// PetCatalog ya cargado + el lifecycle de onboarding + las
// autorizaciones de propiedad + el balance. Ver docs/ONBOARDING.md §15 y
// DEC-137.
//
// **El Starter Shop NO es el Shop público.** No reemplaza a ShopModel ni
// lo conoce: opera en IDENTIDADES EXACTAS ({petId, variantId}) — comprar
// una oferta de Frin otorga EXACTAMENTE {frin, "male"} o {frin,
// "female"}, nunca {frin, ""} ni ambas (brief §13). `BuildShopModel`
// sigue usando SOLO `publiclyPurchasable`; una entrada de starter con
// precio pero `publiclyPurchasable == false` es INVISIBLE para el Shop
// público y SOLO elegible acá.
//
// **Regla de no-divulgación del secreto (brief §3, crítica).** Una
// oferta de un starter SECRETO (StarterRole::kSecret) es elegible SOLO
// si el owner ya posee al menos una autorización EXACTA del MISMO petId
// lógico. Así un usuario que eligió {frin, "female"} puede comprar
// {frin, "male"}, pero un usuario que eligió un starter normal NUNCA
// descubre Frin por el Starter Shop. No existe ni se persiste ningún
// flag de "Frin fue revelado".

enum class StarterShopOfferStatus : std::uint8_t {
    // No poseída, el balance alcanza -> "Get" habilitado.
    kAffordable,
    // No poseída, faltan clics -> sin acción; `clicksShort` dice cuántos.
    kInsufficientBalance,
};

const char* ToString(StarterShopOfferStatus status);

struct StarterShopOffer {
    // La identidad EXACTA que se compra / se otorga (brief §13). Para una
    // variante secreta es {petId, "male"} / {petId, "female"}; para un
    // starter normal es {petId, ""}. NUNCA {petId, ""} para un pet con
    // variantes.
    PetIdentity target;
    std::string displayName;  // nombre propio — NUNCA se traduce
    StarterRole role = StarterRole::kNormal;
    std::uint64_t priceClicks = 0;
    StarterShopOfferStatus status = StarterShopOfferStatus::kAffordable;

    // > 0 solo si status == kInsufficientBalance: price - balance.
    std::uint64_t clicksShort = 0;

    // Lo que otorga la compra: == {target.petId, target.variantId}. Para
    // la UI y la política de compra — nunca una suposición sobre petId.
    PetEntitlement entitlementTarget;

    bool IsSecret() const { return role == StarterRole::kSecret; }
    const std::string& PetId() const { return target.petId; }
    const std::string& VariantId() const { return target.variantId; }
};

struct StarterShopModel {
    // En orden de catálogo (determinista). Solo ofertas legítimas.
    std::vector<StarterShopOffer> offers;

    bool Empty() const { return offers.empty(); }

    // nullptr si `target` no es una oferta del modelo.
    const StarterShopOffer* Find(const PetIdentity& target) const;
};

// ¿La identidad EXACTA `identity` es una oferta legítima del Starter
// Shop oculto? TODAS las condiciones del brief §8 tienen que valer:
//   1. `lifecycleCompleted` — el lifecycle persistido es EXACTAMENTE
//      persistence::OnboardingLifecycle::kCompleted (NO kLegacyComplete,
//      NO kPending — brief §5). El caller (src/app) hace la comparación;
//      esta capa recibe el bool ya resuelto para no depender de
//      src/persistence.
//   2. hay una entrada de catálogo con esa identidad EXACTA y su
//      `starterRole != kNone`.
//   3. `priceClicks > 0` en esa entrada (brief §6: el precio de la
//      identidad de starter; sin campo de schema nuevo).
//   4. la identidad EXACTA NO está ya poseída.
//   5. starter SECRETO: además, el owner ya posee al menos una
//      autorización EXACTA del MISMO petId lógico (no-divulgación —
//      brief §3). Genérico por rol + petId: NUNCA `if (petId == "frin")`.
//   7. la identidad es estructuralmente válida (la resuelve `Find`).
// Un starter NORMAL no poseído solo necesita 1..4 + 7 (brief §4).
bool IsStarterShopEligible(
    const PetCatalog& catalog,
    bool lifecycleCompleted,
    const std::vector<PetEntitlement>& owned,
    const PetIdentity& identity);

// Construye el modelo. `owned` puede venir en cualquier orden / con
// duplicados. Determinista y puro. NO muta nada. Con `lifecycleCompleted
// == false` devuelve SIEMPRE un modelo vacío (brief §5).
StarterShopModel BuildStarterShopModel(
    const PetCatalog& catalog,
    bool lifecycleCompleted,
    const std::vector<PetEntitlement>& owned,
    std::uint64_t clickBalance);

}  // namespace nimvlets::catalog
