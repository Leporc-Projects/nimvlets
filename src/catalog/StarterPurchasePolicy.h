#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "catalog/PetCatalog.h"
#include "catalog/PetEntitlement.h"
#include "catalog/PetIdentity.h"

namespace nimvlets::catalog {

// La política de compra del SHOP OCULTO DE STARTERS (Block 10): PURA y
// testeable sin GUI. NO muta nada — evalúa una compra de starter contra
// el catálogo, el lifecycle, el balance y las autorizaciones actuales, y
// devuelve el estado que resultaría. src/app aplica ese resultado de
// forma ATÓMICA (balance + propiedad en el mismo AppState, un solo
// write) si es kSuccess — la MISMA ruta de aplicación que una compra del
// Shop público (ver SpikeApp / docs/PERSISTENCE.md §6).
//
// **NO se reutiliza `catalog::EvaluatePurchase` ignorando
// `publiclyPurchasable`** (brief §15): eso volvería comprable cualquier
// identidad privada del catálogo. Esta política es un canal DISTINTO con
// su propio gate de elegibilidad, que re-verifica TODO lo que el modelo
// verifica (`catalog::IsStarterShopEligible`) — el modelo/UI NO es un
// límite de seguridad.
//
// El OBJETIVO es una `PetIdentity` completa ({petId, variantId}), NUNCA
// un petId suelto: comprar una oferta de Frin otorga EXACTAMENTE {frin,
// "male"} o {frin, "female"} — nunca {frin, ""} ni ambas (brief §13). No
// se reutiliza NADA de la migración histórica de "Frin entero".

enum class StarterPurchaseResult : std::uint8_t {
    kSuccess,
    kAlreadyOwned,           // la identidad EXACTA ya está autorizada
    kInsufficientBalance,    // balance < precio
    kLifecycleNotCompleted,  // el lifecycle de onboarding no es kCompleted (brief §5)
    kInvalidTarget,          // no hay ninguna entrada de catálogo con esa identidad EXACTA
    kNotEligible,            // la entrada existe pero: no es starter / precio 0 /
                             // es secreta y el owner no posee ninguna variante
                             // hermana del mismo petId lógico (no-divulgación)
};

const char* ToString(StarterPurchaseResult result);

struct StarterPurchaseOutcome {
    StarterPurchaseResult result = StarterPurchaseResult::kNotEligible;

    // El precio cobrado (kSuccess) o que se habría cobrado
    // (kInsufficientBalance / kAlreadyOwned).
    std::uint64_t price = 0;

    // Qué autorización otorga (o habría otorgado) la compra — la
    // identidad EXACTA.
    PetEntitlement grantedEntitlement;

    // SOLO válidos si result == kSuccess. En CUALQUIER fallo,
    // `newBalance` == balance de entrada y `newEntitlements` ==
    // autorizaciones de entrada (canonicalizadas) — NUNCA una mutación
    // parcial (brief §28). La resta solo corre tras `balance >= precio`
    // -> sin underflow posible.
    std::uint64_t newBalance = 0;
    std::vector<PetEntitlement> newEntitlements;
};

// Evalúa comprar la identidad `target` en el Starter Shop oculto.
// `lifecycleCompleted` = (persistence::OnboardingLifecycle::kCompleted
// EXACTO — el caller compara). Re-verifica INDEPENDIENTEMENTE (brief
// §15): lifecycle == kCompleted, la entrada existe, es starter, precio >
// 0, regla de posesión de variante hermana del secreto, no poseída ya,
// balance suficiente. Determinista. Nunca produce un balance negativo.
StarterPurchaseOutcome EvaluateStarterPurchase(
    const PetCatalog& catalog,
    bool lifecycleCompleted,
    const PetIdentity& target,
    std::uint64_t currentBalance,
    const std::vector<PetEntitlement>& currentEntitlements);

}  // namespace nimvlets::catalog
