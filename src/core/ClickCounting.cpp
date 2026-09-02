#include "core/ClickCounting.h"

namespace nimvlets::core {

const char* ClickCountingModeId(ClickCountingMode mode) {
    return mode == ClickCountingMode::kAnywhere ? "anywhere" : "nimvlet_only";
}

ClickCountingMode ParseClickCountingMode(std::string_view id) {
    // Solo el id EXACTO de "anywhere" habilita el modo global. Todo lo
    // demás — vacío (un save v1..v5 que no traía el campo), un id
    // desconocido, un archivo tocado a mano — cae al default privado.
    return id == "anywhere" ? ClickCountingMode::kAnywhere : ClickCountingMode::kNimvletOnly;
}

EffectiveClickCounting ResolveEffectiveClickCounting(ClickCountingMode requested, bool monitorActive) {
    // El modo efectivo exige LAS DOS cosas: que el owner lo haya pedido
    // y que el monitor esté corriendo de verdad. `monitorActive` es un
    // hecho consultado al adapter nativo (IsActive()), no una suposición
    // derivada de la preferencia.
    if (requested == ClickCountingMode::kAnywhere && monitorActive) {
        return EffectiveClickCounting::kGlobal;
    }
    return EffectiveClickCounting::kLocal;
}

bool CountedClickShouldIncrement(EffectiveClickCounting effective, ClickSource source) {
    if (effective == EffectiveClickCounting::kGlobal) {
        return source == ClickSource::kGlobalMonitor;
    }
    return source == ClickSource::kLocalPet;
}

}  // namespace nimvlets::core
