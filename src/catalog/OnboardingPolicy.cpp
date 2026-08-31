#include "catalog/OnboardingPolicy.h"

#include <algorithm>
#include <set>
#include <string>

namespace nimvlets::catalog {

const char* ToString(OnboardingSelectionResult result) {
    switch (result) {
        case OnboardingSelectionResult::kOk:
            return "ok";
        case OnboardingSelectionResult::kUnknownStarter:
            return "unknown starter";
        case OnboardingSelectionResult::kSecretNotYetRevealed:
            return "secret not yet revealed";
        case OnboardingSelectionResult::kSecretNeedsVariant:
            return "secret needs a variant choice";
        case OnboardingSelectionResult::kAlreadyCompleted:
            return "onboarding already completed";
    }
    return "?";
}

OnboardingOffer BuildOnboardingOffer(const PetCatalog& catalog) {
    OnboardingOffer offer;
    OnboardingStarter secret;
    bool haveSecret = false;

    for (const CatalogEntry& entry : catalog.Entries()) {
        if (entry.starterRole == StarterRole::kNormal) {
            OnboardingStarter s;
            // La identidad de un candidato NORMAL es su identidad de
            // catálogo tal cual (un pet sin variantes -> {petId, ""}).
            s.identity = entry.identity;
            s.displayName = entry.displayName;
            s.role = StarterRole::kNormal;
            offer.normal.push_back(std::move(s));
        } else if (entry.starterRole == StarterRole::kSecret) {
            // El secreto es UN pet lógico: sus entradas de catálogo
            // (frin/male, frin/female) se colapsan bajo {petId, ""} + la
            // lista de variantes, en orden de catálogo.
            if (!haveSecret) {
                secret.identity = PetIdentity{entry.identity.petId, std::string()};
                secret.displayName = entry.displayName;
                secret.role = StarterRole::kSecret;
                haveSecret = true;
            }
            if (!entry.identity.variantId.empty()) {
                secret.variants.push_back(entry.identity);
            }
        }
    }

    if (haveSecret) {
        offer.secret = std::move(secret);
    }
    return offer;
}

namespace {

// El grant EXITOSO de una identidad concreta: el usuario nuevo arranca
// con 0 clics y exactamente esa autorización activa (brief §14/§16).
OnboardingGrant GrantOf(const PetIdentity& id) {
    OnboardingGrant g;
    g.result = OnboardingSelectionResult::kOk;
    g.entitlement = PetEntitlement{id.petId, id.variantId};
    g.activeIdentity = id;
    g.newBalance = 0;
    return g;
}

OnboardingGrant Reject(OnboardingSelectionResult r) {
    OnboardingGrant g;
    g.result = r;
    return g;  // grant en default -> CERO mutación aguas arriba
}

}  // namespace

OnboardingGrant EvaluateOnboardingSelection(
    const OnboardingOffer& offer, const PetIdentity& selected, bool alreadyCompleted) {
    // Idempotencia (brief §15): una vez completado, cualquier pedido de
    // selección se rechaza sin tocar nada.
    if (alreadyCompleted) {
        return Reject(OnboardingSelectionResult::kAlreadyCompleted);
    }

    // 1) ¿Es un candidato NORMAL ofrecido? (identidad exacta)
    for (const OnboardingStarter& s : offer.normal) {
        if (s.identity == selected) {
            return GrantOf(selected);
        }
    }

    // 2) ¿Tiene que ver con el candidato SECRETO?
    if (offer.secret.has_value()) {
        const OnboardingStarter& sec = *offer.secret;
        const bool isLogicalSecret =
            selected.petId == sec.identity.petId && selected.variantId.empty();
        const bool isSecretVariant =
            std::any_of(sec.variants.begin(), sec.variants.end(),
                        [&](const PetIdentity& v) { return v == selected; });

        if (isLogicalSecret || isSecretVariant) {
            if (!offer.secretRevealed) {
                // El secreto no existe para el usuario hasta los 44 s
                // (brief §10/§13/§28).
                return Reject(OnboardingSelectionResult::kSecretNotYetRevealed);
            }
            if (isSecretVariant) {
                // Otorga EXACTAMENTE la variante elegida — la otra queda
                // sin otorgar (brief §5/§13). No se reutiliza la regla de
                // migración histórica: esto es una política de grant
                // nueva.
                return GrantOf(selected);
            }
            // {frin, ""} revelado: el UI todavía debe pedir macho/hembra.
            return Reject(OnboardingSelectionResult::kSecretNeedsVariant);
        }
    }

    return Reject(OnboardingSelectionResult::kUnknownStarter);
}

// --- Gate de contenido listo ------------------------------------

std::size_t CountNormalStarters(const PetCatalog& catalog) {
    // Cuenta IDENTIDADES LÓGICAS distintas, no filas: filas duplicadas
    // o variantes de un mismo Nimvlet no inflan la tríada (DEC-133). Un
    // starter normal no lleva variante (lo garantizan el compilador y
    // el loader), así que su identidad lógica es su petId.
    std::set<std::string> distinct;
    for (const CatalogEntry& entry : catalog.Entries()) {
        if (entry.starterRole == StarterRole::kNormal) {
            distinct.insert(entry.identity.petId);
        }
    }
    return distinct.size();
}

OnboardingReadiness EvaluateOnboardingReadiness(
    bool manifestProductionReady, std::size_t normalStarterCount) {
    OnboardingReadiness r;
    if (!manifestProductionReady) {
        r.reason = "catalog does not mark production onboarding ready";
        return r;
    }
    if (normalStarterCount < kRequiredNormalStarterCount) {
        r.reason = "only " + std::to_string(normalStarterCount) + " normal starter(s); need " +
                   std::to_string(kRequiredNormalStarterCount);
        return r;
    }
    r.armed = true;
    return r;
}

OnboardingReadiness EvaluateCatalogOnboardingReadiness(const PetCatalog& catalog) {
    return EvaluateOnboardingReadiness(catalog.ProductionOnboardingReady(), CountNormalStarters(catalog));
}

}  // namespace nimvlets::catalog
