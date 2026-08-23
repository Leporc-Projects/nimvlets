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

bool ProductDwellThresholdIsPointTwoSeconds() {
    // Valor de producto explícito (DEC-090, pasada de pulido final --
    // baja de 0.5s a 0.2s, ver DEC-084 para el valor anterior) -- se
    // fija acá porque src/app no es testeable en aislamiento y este
    // umbral es exactamente el tipo de constante que una pasada futura
    // podría mover sin querer.
    NIMVLETS_CHECK(core::kDefaultHoverDwellSeconds == 0.2);

    // Y que el umbral realmente gobierne el disparo, no solo exista:
    // a 199ms todavía no; a 200ms sí.
    core::HoverDwellTracker tracker{core::kDefaultHoverDwellSeconds};
    NIMVLETS_CHECK(!tracker.Update(true, 1000.0));
    NIMVLETS_CHECK(!tracker.Update(true, 1000.0 + 199.0));
    NIMVLETS_CHECK(tracker.Update(true, 1000.0 + 200.0));
    return true;
}

bool DwellDoesNotFireBeforeThreshold() {
    // Cobertura explícita del brief: "no trigger before 0.2s" -- un
    // muestreo denso que nunca alcanza el umbral no debe disparar en
    // ningún punto intermedio, ni una sola vez.
    core::HoverDwellTracker tracker{core::kDefaultHoverDwellSeconds};
    NIMVLETS_CHECK(!tracker.Update(true, 0.0));
    for (double t = 10.0; t < 200.0; t += 10.0) {
        NIMVLETS_CHECK(!tracker.Update(true, t));
    }
    return true;
}

bool DwellFiresAtOrAfterThreshold() {
    // Cobertura explícita del brief: "trigger at/after 0.2s" -- ambos
    // bordes del umbral, no solo el exacto.
    {
        core::HoverDwellTracker tracker{core::kDefaultHoverDwellSeconds};
        tracker.Update(true, 0.0);
        NIMVLETS_CHECK(tracker.Update(true, 200.0));  // exactamente en el umbral
    }
    {
        core::HoverDwellTracker tracker{core::kDefaultHoverDwellSeconds};
        tracker.Update(true, 0.0);
        NIMVLETS_CHECK(tracker.Update(true, 350.0));  // bien después del umbral
    }
    return true;
}

bool DwellDoesNotSpamAfterFiring() {
    // Cobertura explícita del brief: "no hover spam" -- una vez que
    // disparó, seguir muestreando "encima" en el mismo episodio no
    // debe volver a disparar nunca, sin importar cuánto tiempo pase.
    core::HoverDwellTracker tracker{core::kDefaultHoverDwellSeconds};
    tracker.Update(true, 0.0);
    NIMVLETS_CHECK(tracker.Update(true, 200.0));
    for (double t = 210.0; t <= 5000.0; t += 100.0) {
        NIMVLETS_CHECK(!tracker.Update(true, t));
    }
    return true;
}

}  // namespace

void RegisterHoverDwellTrackerTests(testing::TestRunner& runner) {
    runner.Add("HoverDwellTracker.ProductDwellThresholdIsPointTwoSeconds", ProductDwellThresholdIsPointTwoSeconds);
    runner.Add("HoverDwellTracker.DwellDoesNotFireBeforeThreshold", DwellDoesNotFireBeforeThreshold);
    runner.Add("HoverDwellTracker.DwellFiresAtOrAfterThreshold", DwellFiresAtOrAfterThreshold);
    runner.Add("HoverDwellTracker.DwellDoesNotSpamAfterFiring", DwellDoesNotSpamAfterFiring);
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
