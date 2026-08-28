#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "catalog/PetCatalog.h"
#include "catalog/PetEntitlement.h"

namespace nimvlets::catalog {

// La política de compra del Shop (Block 07): PURA y testeable sin GUI
// (brief §14). No muta nada — evalúa una compra contra el catálogo, el
// balance y las autorizaciones actuales, y devuelve el estado que
// resultaría. src/app aplica ese resultado de forma ATÓMICA (balance +
// propiedad en el mismo AppState, un solo write) si es kSuccess — ver
// SpikeApp::HandlePurchaseRequest y docs/PERSISTENCE.md.
//
// Clicks son la única moneda (AGENTS.md §2): comprar consume balance,
// la propiedad es permanente, cambiar de contenido ya poseído es gratis
// (eso lo maneja el switch de runtime, no esta política).

enum class PurchaseResult {
    kSuccess,
    kAlreadyOwned,          // la autorización objetivo ya está cubierta
    kInsufficientBalance,   // balance < precio
    kNotPurchasable,        // el pet no es público, o su precio es 0
    kInvalidTarget,         // `petId` no está en el catálogo en absoluto
};

const char* ToString(PurchaseResult result);

struct PurchaseOutcome {
    PurchaseResult result = PurchaseResult::kInvalidTarget;

    // El precio que se cobró (kSuccess) o que se habría cobrado
    // (kInsufficientBalance / kAlreadyOwned): para que la UI pueda
    // mostrar "Necesitas N clics más" sin re-consultar el catálogo.
    std::uint64_t price = 0;

    // Qué autorización otorga (o habría otorgado) la compra.
    PetEntitlement grantedEntitlement;

    // SOLO válidos si result == kSuccess: el estado ya mutado, listo
    // para que src/app lo copie a AppState de una. En cualquier fallo,
    // `newBalance` == balance de entrada y `newEntitlements` ==
    // autorizaciones de entrada (canonicalizadas) — nunca una mutación
    // parcial (brief §26).
    std::uint64_t newBalance = 0;
    std::vector<PetEntitlement> newEntitlements;
};

// Evalúa comprar el pet lógico `petId` (el Shop compra por pet, no por
// variante en Block 07). Determinista. Nunca puede producir un balance
// negativo ni un underflow: la resta solo ocurre tras verificar
// balance >= precio (brief §26).
PurchaseOutcome EvaluatePurchase(
    const PetCatalog& catalog,
    const std::string& petId,
    std::uint64_t currentBalance,
    const std::vector<PetEntitlement>& currentEntitlements);

}  // namespace nimvlets::catalog
