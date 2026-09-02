#include "platform/GlobalClickTypes.h"

namespace nimvlets::platform {

namespace {

bool CapabilityIsSupported(GlobalClickCapability capability) {
    return capability != GlobalClickCapability::kUnavailable;
}

}  // namespace

GlobalClickUiState ResolveGlobalClickUiState(
    core::ClickCountingMode requested, const GlobalClickStatus& status) {
    GlobalClickUiState out;
    out.permissionName = status.permissionName;

    if (!CapabilityIsSupported(status.capability)) {
        // Se dice la verdad y se termina: sin capacidad no hay permiso
        // que pedir ni reintento que ofrecer. El segmento se dibuja pero
        // no se puede elegir, así el owner entiende POR QUÉ.
        out.anywhereSelectable = false;
        out.statusLine = GlobalClickStatusLine::kUnavailable;
        return out;
    }

    out.anywhereSelectable = true;

    if (requested == core::ClickCountingMode::kNimvletOnly) {
        // Modo local: no se dice nada. Un estado de permiso que el owner
        // no pidió no es información útil, es ruido (brief §9).
        return out;
    }

    if (status.monitorActive) {
        out.statusLine = GlobalClickStatusLine::kActive;
        return out;
    }

    // Pedido pero NO activo: nunca se calla. El owner tiene que poder
    // ver que el conteo global no está funcionando (brief §5).
    if (status.capability == GlobalClickCapability::kSupportedNeedsPermission &&
        status.permission != GlobalClickPermission::kGranted) {
        out.statusLine = GlobalClickStatusLine::kPermissionRequired;
    } else {
        // Permiso en regla (o innecesario) y aun así no corre: arranque
        // fallido o monitor caído. `startFailed` puede venir en false si
        // nunca se llegó a intentar; igual se ofrece el reintento, que es
        // la acción útil en los dos casos.
        out.statusLine = GlobalClickStatusLine::kFailed;
    }
    out.showCheckAgain = true;
    return out;
}

GlobalClickRequestOutcome EvaluateGlobalClickRequest(const GlobalClickStatus& status) {
    if (!CapabilityIsSupported(status.capability)) {
        return GlobalClickRequestOutcome::kUnavailable;
    }
    if (status.capability == GlobalClickCapability::kSupportedNoPermission ||
        status.permission == GlobalClickPermission::kGranted) {
        // Ya está todo lo que hace falta: no se muestra una explicación
        // sobre un permiso que no se va a pedir.
        return GlobalClickRequestOutcome::kApplyDirectly;
    }
    // kNotGranted y kUnknown van los dos por la explicación: si no
    // podemos AFIRMAR que ya está concedido, el owner ve primero qué se
    // va a pedir y qué observa el monitor.
    return GlobalClickRequestOutcome::kNeedsExplanation;
}

}  // namespace nimvlets::platform
