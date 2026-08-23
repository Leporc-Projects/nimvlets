#include "HoverDwellTrackerTest.h"

#include "core/HoverDwellTracker.h"

using nimvlets::core::HoverDwellTracker;

namespace nimvlets::tests {

namespace {

constexpr double kDwellSeconds = 5.0;
constexpr double kDwellMs = kDwellSeconds * 1000.0;

bool StartsNotDwelling() {
    HoverDwellTracker tracker(kDwellSeconds);
    NIMVLETS_CHECK(!tracker.IsDwelling());
    return true;
}

bool DoesNotFireImmediatelyOnEntry() {
    // "No quiero que se dispare una animación apenas el mouse entra."
    HoverDwellTracker tracker(kDwellSeconds);
    NIMVLETS_CHECK(!tracker.Update(true, 0.0));
    NIMVLETS_CHECK(tracker.IsDwelling());
    return true;
}

bool FiresExactlyWhenThresholdIsCrossed() {
    HoverDwellTracker tracker(kDwellSeconds);
    tracker.Update(true, 0.0);
    NIMVLETS_CHECK(!tracker.Update(true, kDwellMs - 1.0));  // todavía no
    NIMVLETS_CHECK(tracker.Update(true, kDwellMs));         // exactamente en el umbral -- dispara
    return true;
}

bool DoesNotRefireOnSubsequentSamplesWhileStillOver() {
    HoverDwellTracker tracker(kDwellSeconds);
    tracker.Update(true, 0.0);
    NIMVLETS_CHECK(tracker.Update(true, kDwellMs));
    NIMVLETS_CHECK(!tracker.Update(true, kDwellMs + 1000.0));
    NIMVLETS_CHECK(!tracker.Update(true, kDwellMs + 50000.0));  // ni mucho después
    return true;
}

bool LeavingBeforeThresholdResetsTheCounter() {
    // "Si el puntero sale antes, el contador se reinicia."
    HoverDwellTracker tracker(kDwellSeconds);
    tracker.Update(true, 0.0);
    NIMVLETS_CHECK(!tracker.Update(true, kDwellMs - 100.0));  // casi, pero no llega
    NIMVLETS_CHECK(!tracker.Update(false, kDwellMs - 50.0));  // sale justo antes -- reinicia
    NIMVLETS_CHECK(!tracker.IsDwelling());

    // Reentra: el reloj vuelve a empezar desde CERO, no desde donde quedó.
    NIMVLETS_CHECK(!tracker.Update(true, kDwellMs - 50.0));
    NIMVLETS_CHECK(!tracker.Update(true, (kDwellMs - 50.0) + kDwellMs - 1.0));  // todavía no (nuevo umbral completo)
    NIMVLETS_CHECK(tracker.Update(true, (kDwellMs - 50.0) + kDwellMs));        // ahora sí
    return true;
}

bool LeavingAfterFiringAllowsANewEpisodeToFireAgain() {
    HoverDwellTracker tracker(kDwellSeconds);
    tracker.Update(true, 0.0);
    NIMVLETS_CHECK(tracker.Update(true, kDwellMs));  // primer episodio dispara

    NIMVLETS_CHECK(!tracker.Update(false, kDwellMs + 10.0));  // sale
    NIMVLETS_CHECK(!tracker.Update(true, kDwellMs + 20.0));   // reentra -- nuevo episodio
    NIMVLETS_CHECK(!tracker.Update(true, kDwellMs + 20.0 + kDwellMs - 1.0));
    NIMVLETS_CHECK(tracker.Update(true, kDwellMs + 20.0 + kDwellMs));  // dispara de nuevo
    return true;
}

bool ExplicitResetClearsAnInProgressDwellEvenWhileStillOver() {
    // "Si hay click, drag o cambio de estado, el dwell-hover debe
    // resetearse" -- incluso si el cursor sigue físicamente encima.
    HoverDwellTracker tracker(kDwellSeconds);
    tracker.Update(true, 0.0);
    NIMVLETS_CHECK(!tracker.Update(true, kDwellMs - 1.0));  // casi por cruzar el umbral

    tracker.Reset();
    NIMVLETS_CHECK(!tracker.IsDwelling());

    // El cursor NUNCA salió (isOverOpaque sigue true en cada muestra),
    // pero el reset debe exigir un umbral completo NUEVO de todos modos.
    NIMVLETS_CHECK(!tracker.Update(true, kDwellMs - 1.0 + 1.0));
    NIMVLETS_CHECK(!tracker.Update(true, (kDwellMs - 1.0 + 1.0) + kDwellMs - 1.0));
    NIMVLETS_CHECK(tracker.Update(true, (kDwellMs - 1.0 + 1.0) + kDwellMs));
    return true;
}

bool ExplicitResetAfterFiringRequiresAFullNewDwellToFireAgain() {
    HoverDwellTracker tracker(kDwellSeconds);
    tracker.Update(true, 0.0);
    NIMVLETS_CHECK(tracker.Update(true, kDwellMs));  // dispara

    tracker.Reset();  // p.ej. un click, sin que el cursor se haya movido
    NIMVLETS_CHECK(!tracker.Update(true, kDwellMs + 1.0));
    NIMVLETS_CHECK(!tracker.Update(true, kDwellMs + 1.0 + kDwellMs - 1.0));
    NIMVLETS_CHECK(tracker.Update(true, kDwellMs + 1.0 + kDwellMs));
    return true;
}

bool NeverOverNeverFires() {
    HoverDwellTracker tracker(kDwellSeconds);
    for (double t = 0.0; t <= kDwellMs * 3; t += 500.0) {
        NIMVLETS_CHECK(!tracker.Update(false, t));
    }
    NIMVLETS_CHECK(!tracker.IsDwelling());
    return true;
}

bool ProductDwellThresholdIsHalfASecond() {
    // Valor de producto explícito (DEC-084), sin cambios en la pasada
    // de estabilización -- se fija acá porque src/app no es testeable
    // en aislamiento y este umbral es exactamente el tipo de constante
    // que una pasada futura podría mover sin querer.
    NIMVLETS_CHECK(core::kDefaultHoverDwellSeconds == 0.5);

    // Y que el umbral realmente gobierne el disparo, no solo exista:
    // a 499ms todavía no; a 500ms sí.
    core::HoverDwellTracker tracker{core::kDefaultHoverDwellSeconds};
    NIMVLETS_CHECK(!tracker.Update(true, 1000.0));
    NIMVLETS_CHECK(!tracker.Update(true, 1000.0 + 499.0));
    NIMVLETS_CHECK(tracker.Update(true, 1000.0 + 500.0));
    return true;
}

}  // namespace

void RegisterHoverDwellTrackerTests(testing::TestRunner& runner) {
    runner.Add("HoverDwellTracker.ProductDwellThresholdIsHalfASecond", ProductDwellThresholdIsHalfASecond);
    runner.Add("HoverDwellTracker/StartsNotDwelling", StartsNotDwelling);
    runner.Add("HoverDwellTracker/DoesNotFireImmediatelyOnEntry", DoesNotFireImmediatelyOnEntry);
    runner.Add("HoverDwellTracker/FiresExactlyWhenThresholdIsCrossed", FiresExactlyWhenThresholdIsCrossed);
    runner.Add("HoverDwellTracker/DoesNotRefireOnSubsequentSamplesWhileStillOver", DoesNotRefireOnSubsequentSamplesWhileStillOver);
    runner.Add("HoverDwellTracker/LeavingBeforeThresholdResetsTheCounter", LeavingBeforeThresholdResetsTheCounter);
    runner.Add("HoverDwellTracker/LeavingAfterFiringAllowsANewEpisodeToFireAgain", LeavingAfterFiringAllowsANewEpisodeToFireAgain);
    runner.Add(
        "HoverDwellTracker/ExplicitResetClearsAnInProgressDwellEvenWhileStillOver",
        ExplicitResetClearsAnInProgressDwellEvenWhileStillOver);
    runner.Add(
        "HoverDwellTracker/ExplicitResetAfterFiringRequiresAFullNewDwellToFireAgain",
        ExplicitResetAfterFiringRequiresAFullNewDwellToFireAgain);
    runner.Add("HoverDwellTracker/NeverOverNeverFires", NeverOverNeverFires);
}

}  // namespace nimvlets::tests
