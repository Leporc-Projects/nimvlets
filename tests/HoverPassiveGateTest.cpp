#include "HoverPassiveGateTest.h"

#include "core/HoverPassiveGate.h"

using nimvlets::core::HoverPassiveGate;

namespace nimvlets::tests {

namespace {

bool StartsNotHovering() {
    HoverPassiveGate gate;
    NIMVLETS_CHECK(!gate.IsHovering());
    return true;
}

bool FirstSampleHoveringIsARisingEdge() {
    HoverPassiveGate gate;
    NIMVLETS_CHECK(gate.Update(true));
    NIMVLETS_CHECK(gate.IsHovering());
    return true;
}

bool SustainedHoverReportsEdgeOnlyOnce() {
    HoverPassiveGate gate;
    NIMVLETS_CHECK(gate.Update(true));   // el flanco
    NIMVLETS_CHECK(!gate.Update(true));  // sigue en hover -- no es un flanco nuevo
    NIMVLETS_CHECK(!gate.Update(true));  // ídem, una tercera muestra seguida
    NIMVLETS_CHECK(gate.IsHovering());
    return true;
}

bool NotHoveringNeverReportsAnEdge() {
    HoverPassiveGate gate;
    NIMVLETS_CHECK(!gate.Update(false));
    NIMVLETS_CHECK(!gate.Update(false));
    NIMVLETS_CHECK(!gate.IsHovering());
    return true;
}

bool LeavingAndReenteringReportsANewEdge() {
    HoverPassiveGate gate;
    NIMVLETS_CHECK(gate.Update(true));    // entra -- flanco
    NIMVLETS_CHECK(!gate.Update(true));   // se queda
    NIMVLETS_CHECK(!gate.Update(false));  // sale -- no es un flanco de ENTRADA
    NIMVLETS_CHECK(!gate.IsHovering());
    NIMVLETS_CHECK(gate.Update(true));  // vuelve a entrar -- flanco nuevo
    return true;
}

bool RepeatedEnterExitCyclesEachReportExactlyOneEdge() {
    HoverPassiveGate gate;
    int edgeCount = 0;
    // 5 ciclos de entrar/salir, con una muestra "sostenida" extra en
    // cada uno mientras está adentro -- simula el jitter real de
    // SDL_EVENT_MOUSE_MOTION mientras el mouse tiembla ligeramente sin
    // salir del área (ver el comentario de HoverPassiveGate::Update()).
    for (int i = 0; i < 5; ++i) {
        if (gate.Update(true)) {
            ++edgeCount;
        }
        gate.Update(true);   // muestra sostenida -- no debe contar
        gate.Update(false);  // sale
    }
    NIMVLETS_CHECK(edgeCount == 5);
    return true;
}

}  // namespace

void RegisterHoverPassiveGateTests(testing::TestRunner& runner) {
    runner.Add("HoverPassiveGate/StartsNotHovering", StartsNotHovering);
    runner.Add("HoverPassiveGate/FirstSampleHoveringIsARisingEdge", FirstSampleHoveringIsARisingEdge);
    runner.Add("HoverPassiveGate/SustainedHoverReportsEdgeOnlyOnce", SustainedHoverReportsEdgeOnlyOnce);
    runner.Add("HoverPassiveGate/NotHoveringNeverReportsAnEdge", NotHoveringNeverReportsAnEdge);
    runner.Add("HoverPassiveGate/LeavingAndReenteringReportsANewEdge", LeavingAndReenteringReportsANewEdge);
    runner.Add(
        "HoverPassiveGate/RepeatedEnterExitCyclesEachReportExactlyOneEdge",
        RepeatedEnterExitCyclesEachReportExactlyOneEdge);
}

}  // namespace nimvlets::tests
