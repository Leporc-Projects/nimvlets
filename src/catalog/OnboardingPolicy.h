#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "catalog/PetCatalog.h"
#include "catalog/PetEntitlement.h"
#include "catalog/PetIdentity.h"

namespace nimvlets::catalog {

// La POLÍTICA / máquina de estados PURA del onboarding de primer
// arranque (Block 09A). Sin SDL, sin AppKit, sin I/O: `src/app` la
// consulta y aplica su resultado como una transacción atómica sobre
// AppState; la vista de onboarding (`src/productui`) la usa para saber
// qué dibujar. NUNCA hay una rama `if (pet == "artu")` — todo sale de
// StarterRole (dato de catálogo) o de descriptores sintéticos que el
// harness DEV / los tests construyen. Ver docs/ONBOARDING.md y DEC-132.
//
// Lo que NO está acá: la persistencia del lifecycle (eso es
// persistence::OnboardingLifecycle / schema v5, DEC-131), el dibujo
// (productui::OnboardingLayout / OnboardingView), y el gate de contenido
// de producción a nivel de archivo (tools/compile_pet_catalog.py).

// --- Secreto de los 44 segundos (brief §10/§11/§12/§29) -----------
//
// Los 44 s son DWELL DE SESIÓN sobre la pantalla de selección activa —
// reloj MONOTÓNICO, nunca wall-clock, nunca progreso acumulado en
// AppState. Si la app sale antes del reveal con el onboarding aún
// incompleto, la próxima sesión arranca un dwell FRESCO.
inline constexpr double kSecretRevealDwellMs = 44.0 * 1000.0;

// true si, con `dwellMs` de dwell continuo sobre la pantalla, el
// candidato secreto debe estar visible. Frontera EXACTA:
// dwellMs == 44000 -> revelado.
inline bool SecretRevealedAfterDwell(double dwellMs) { return dwellMs >= kSecretRevealDwellMs; }

// El instante monotónico del reveal, dado el instante en que la
// pantalla se volvió activa y capaz de recibir input. `src/app` arma el
// deadline de su event loop con esto (SDL_WaitEventTimeout /
// next-deadline — brief §12), sin ningún timer thread ni polling.
inline double SecretRevealDeadlineMs(double screenActivatedAtMs) {
    return screenActivatedAtMs + kSecretRevealDwellMs;
}

// --- Candidatos de starter (data-driven — brief §7) -------------

struct OnboardingStarter {
    // Identidad LÓGICA del candidato. Para un starter normal es
    // {petId, ""}. Para el secreto (Frin) es {petId, ""} y `variants`
    // lista las variantes reales elegibles.
    PetIdentity identity;
    std::string displayName;   // nombre propio — nunca traducido
    StarterRole role = StarterRole::kNormal;
    // Solo el secreto: las variantes elegibles ({frin,"male"},
    // {frin,"female"}) en orden de catálogo. Vacío para un normal.
    std::vector<PetIdentity> variants;
};

// Lo que la pantalla de selección ofrece.
struct OnboardingOffer {
    std::vector<OnboardingStarter> normal;    // Artu, Rato, Rin Rin (orden de catálogo)
    std::optional<OnboardingStarter> secret;  // Frin, o nullopt si el catálogo no declara secreto
    // Lo pone el runtime según el dwell de sesión (SecretRevealedAfterDwell).
    bool secretRevealed = false;
};

// Agrupa las entradas del catálogo por StarterRole en un
// OnboardingOffer. Las variantes del secreto se colapsan bajo su petId
// lógico ({frin,""} + variants). `secretRevealed` queda en false. Puro
// y determinista.
OnboardingOffer BuildOnboardingOffer(const PetCatalog& catalog);

// --- Evaluación de una selección (brief §6/§13/§15/§28) ----------

enum class OnboardingSelectionResult : std::uint8_t {
    kOk = 0,
    kUnknownStarter,        // no está entre los candidatos ofrecidos
    kSecretNotYetRevealed,  // es el secreto pero aún no pasaron los 44 s
    kSecretNeedsVariant,    // es el secreto (identidad lógica) SIN variante concreta elegida
    kAlreadyCompleted,      // el onboarding ya terminó -> se ignora (idempotencia)
};

const char* ToString(OnboardingSelectionResult result);

// El resultado de evaluar una selección. NUNCA muta nada: el caller
// aplica el grant como UNA transacción atómica.
struct OnboardingGrant {
    OnboardingSelectionResult result = OnboardingSelectionResult::kUnknownStarter;

    // Solo con result == kOk:
    PetEntitlement entitlement;    // EXACTAMENTE la identidad elegida (para Frin, la variante concreta)
    PetIdentity activeIdentity;    // == {entitlement.petId, entitlement.variantId}
    std::uint64_t newBalance = 0;  // SIEMPRE 0: un usuario nuevo arranca sin clics (brief §16)
};

// Evalúa `selected` contra `offer` para un onboarding cuyo
// `alreadyCompleted` = (persistence::OnboardingConsideredComplete de su
// lifecycle). Reglas (brief §28):
//   - alreadyCompleted            -> kAlreadyCompleted, CERO mutación.
//   - identidad normal ofrecida   -> kOk, grant {esa identidad}.
//   - {secretPetId, ""} sin reveal -> kSecretNotYetRevealed.
//   - {secretPetId, ""} con reveal -> kSecretNeedsVariant.
//   - variante ofrecida del secreto, con reveal -> kOk, grant esa
//     variante EXACTA (la otra queda SIN otorgar — brief §5/§13).
//   - variante del secreto sin reveal -> kSecretNotYetRevealed.
//   - cualquier otra cosa -> kUnknownStarter.
// Un target inválido produce CERO mutación (result != kOk, grant en
// default). Puro y determinista.
OnboardingGrant EvaluateOnboardingSelection(
    const OnboardingOffer& offer, const PetIdentity& selected, bool alreadyCompleted);

// --- Gate de contenido listo para producción (brief §8/§30) -----

struct OnboardingReadiness {
    bool armed = false;   // el producto PUEDE mostrar onboarding de producción
    std::string reason;   // por qué NO (para logging); "" si armed
};

// armed sii `manifestProductionReady` (PetCatalog::ProductionOnboardingReady)
// && `normalStarterCount` >= kRequiredNormalStarterCount. El chequeo de
// que el CONTENIDO real (pack + .nvprev) de cada starter existe lo hace
// tools/compile_pet_catalog.py al compilar; el runtime confía en el
// datum ya validado y re-verifica el conteo (defensa en profundidad).
// La metadata del SECRETO sola nunca arma nada (brief §30).
OnboardingReadiness EvaluateOnboardingReadiness(bool manifestProductionReady, std::size_t normalStarterCount);

// # de IDENTIDADES LÓGICAS distintas con StarterRole::kNormal en un
// catálogo (petId distinto; filas duplicadas o variantes de un mismo
// Nimvlet no inflan el conteo — DEC-133).
std::size_t CountNormalStarters(const PetCatalog& catalog);

// Atajo: EvaluateOnboardingReadiness a partir de un catálogo entero.
OnboardingReadiness EvaluateCatalogOnboardingReadiness(const PetCatalog& catalog);

}  // namespace nimvlets::catalog
