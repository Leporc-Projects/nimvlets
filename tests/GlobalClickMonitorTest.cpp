#include "GlobalClickMonitorTest.h"

#include <cstdint>
#include <string>

#include "FakeGlobalClickMonitor.h"
#include "core/ClickCounting.h"
#include "platform/GlobalClickTypes.h"

// La costura de plataforma del conteo global (Block 11A): la política
// pura que decide qué muestra Settings, la que decide si hace falta
// explicar antes de pedir un permiso, y el ciclo de vida del monitor
// ejercitado contra el doble puro tests/FakeGlobalClickMonitor.h.
//
// Todo corre en cualquier host, sin display, sin permiso del OS, sin
// AppKit — que es exactamente por qué la interfaz de
// platform::GlobalClickMonitor no incluye SDL ni cabeceras nativas.

using nimvlets::core::ClickCountingMode;
using nimvlets::core::ClickSource;
using nimvlets::core::CountedClickShouldIncrement;
using nimvlets::core::ResolveEffectiveClickCounting;
using nimvlets::platform::EvaluateGlobalClickRequest;
using nimvlets::platform::GlobalClickCapability;
using nimvlets::platform::GlobalClickPermission;
using nimvlets::platform::GlobalClickRequestOutcome;
using nimvlets::platform::GlobalClickStatus;
using nimvlets::platform::GlobalClickStatusLine;
using nimvlets::platform::GlobalClickUiState;
using nimvlets::platform::ResolveGlobalClickUiState;

namespace nimvlets::tests {

namespace {

GlobalClickStatus Status(
    GlobalClickCapability capability, GlobalClickPermission permission, bool active = false,
    bool startFailed = false) {
    GlobalClickStatus s;
    s.capability = capability;
    s.permission = permission;
    s.monitorActive = active;
    s.startFailed = startFailed;
    s.permissionName = capability == GlobalClickCapability::kSupportedNeedsPermission
                           ? "Input Monitoring"
                           : std::string();
    return s;
}

// --- Estado de UI derivado de la capacidad ------------------------

// Una plataforma sin capacidad lo dice, y "Anywhere" no se puede elegir
// — pase lo que pase con la preferencia persistida.
bool TestUnavailablePlatformSaysSoAndBlocksTheOption() {
    for (const ClickCountingMode requested :
         {ClickCountingMode::kNimvletOnly, ClickCountingMode::kAnywhere}) {
        const GlobalClickUiState ui = ResolveGlobalClickUiState(
            requested, Status(GlobalClickCapability::kUnavailable, GlobalClickPermission::kNotRequired));
        NIMVLETS_CHECK(!ui.anywhereSelectable);
        NIMVLETS_CHECK(ui.statusLine == GlobalClickStatusLine::kUnavailable);
        // Sin capacidad no hay nada que reintentar: no se ofrece
        // "Comprobar de nuevo" (sería una promesa vacía).
        NIMVLETS_CHECK(!ui.showCheckAgain);
    }
    return true;
}

// En modo local, con una plataforma capaz, Settings NO dice nada del
// permiso: sería ruido sobre algo que el owner no pidió (brief §9).
bool TestLocalModeShowsNoPermissionNoise() {
    for (const GlobalClickPermission perm :
         {GlobalClickPermission::kGranted, GlobalClickPermission::kNotGranted,
          GlobalClickPermission::kUnknown}) {
        const GlobalClickUiState ui = ResolveGlobalClickUiState(
            ClickCountingMode::kNimvletOnly,
            Status(GlobalClickCapability::kSupportedNeedsPermission, perm));
        NIMVLETS_CHECK(ui.anywhereSelectable);
        NIMVLETS_CHECK(ui.statusLine == GlobalClickStatusLine::kNone);
        NIMVLETS_CHECK(!ui.showCheckAgain);
    }
    return true;
}

bool TestAnywhereActiveReportsActive() {
    const GlobalClickUiState ui = ResolveGlobalClickUiState(
        ClickCountingMode::kAnywhere,
        Status(GlobalClickCapability::kSupportedNeedsPermission, GlobalClickPermission::kGranted,
               /*active=*/true));
    NIMVLETS_CHECK(ui.anywhereSelectable);
    NIMVLETS_CHECK(ui.statusLine == GlobalClickStatusLine::kActive);
    // Anda: no hay nada que reintentar.
    NIMVLETS_CHECK(!ui.showCheckAgain);
    return true;
}

// El caso que el brief §5 exige que NUNCA sea silencioso: pedido pero
// sin permiso -> se dice, y se ofrece un reintento manual.
bool TestAnywhereWithoutPermissionIsNeverSilent() {
    for (const GlobalClickPermission perm :
         {GlobalClickPermission::kNotGranted, GlobalClickPermission::kUnknown}) {
        const GlobalClickUiState ui = ResolveGlobalClickUiState(
            ClickCountingMode::kAnywhere,
            Status(GlobalClickCapability::kSupportedNeedsPermission, perm));
        NIMVLETS_CHECK(ui.anywhereSelectable);
        NIMVLETS_CHECK(ui.statusLine == GlobalClickStatusLine::kPermissionRequired);
        NIMVLETS_CHECK(ui.showCheckAgain);
        // El nombre del permiso viaja como DATO, para que la copy pueda
        // nombrarlo sin ninguna rama por plataforma.
        NIMVLETS_CHECK(ui.permissionName == "Input Monitoring");
    }
    return true;
}

// Permiso en regla (o innecesario) y aun así sin correr: "no se pudo
// iniciar" + reintento. Nunca se muestra "Active" sobre un monitor
// muerto (brief §5: no fingir que el modo global está activo).
bool TestAnywhereWithPermissionButNotRunningReportsFailure() {
    const GlobalClickUiState granted = ResolveGlobalClickUiState(
        ClickCountingMode::kAnywhere,
        Status(GlobalClickCapability::kSupportedNeedsPermission, GlobalClickPermission::kGranted,
               /*active=*/false, /*startFailed=*/true));
    NIMVLETS_CHECK(granted.statusLine == GlobalClickStatusLine::kFailed);
    NIMVLETS_CHECK(granted.showCheckAgain);

    // Una plataforma que no necesita permiso, con el monitor caído:
    // mismo tratamiento.
    const GlobalClickUiState noPerm = ResolveGlobalClickUiState(
        ClickCountingMode::kAnywhere,
        Status(GlobalClickCapability::kSupportedNoPermission, GlobalClickPermission::kNotRequired));
    NIMVLETS_CHECK(noPerm.statusLine == GlobalClickStatusLine::kFailed);
    NIMVLETS_CHECK(noPerm.showCheckAgain);

    // Y con ese mismo backend corriendo: "Active".
    const GlobalClickUiState noPermActive = ResolveGlobalClickUiState(
        ClickCountingMode::kAnywhere,
        Status(GlobalClickCapability::kSupportedNoPermission, GlobalClickPermission::kNotRequired,
               /*active=*/true));
    NIMVLETS_CHECK(noPermActive.statusLine == GlobalClickStatusLine::kActive);
    return true;
}

// --- ¿Hace falta explicar antes de pedir? -------------------------

bool TestRequestOutcomeGatesThePermissionPrompt() {
    // Sin capacidad: el pedido se ignora, no se explica ni se pide nada.
    NIMVLETS_CHECK(EvaluateGlobalClickRequest(Status(GlobalClickCapability::kUnavailable,
                                                     GlobalClickPermission::kNotRequired)) ==
                   GlobalClickRequestOutcome::kUnavailable);

    // Sin permiso de por medio: se aplica directo (no hay diálogo que
    // explicar).
    NIMVLETS_CHECK(EvaluateGlobalClickRequest(Status(GlobalClickCapability::kSupportedNoPermission,
                                                     GlobalClickPermission::kNotRequired)) ==
                   GlobalClickRequestOutcome::kApplyDirectly);

    // Ya concedido: tampoco se explica — no se va a pedir nada.
    NIMVLETS_CHECK(EvaluateGlobalClickRequest(
                       Status(GlobalClickCapability::kSupportedNeedsPermission,
                              GlobalClickPermission::kGranted)) == GlobalClickRequestOutcome::kApplyDirectly);

    // No concedido, o no se pudo consultar: SIEMPRE se explica primero.
    // Solo "Continue" puede llamar al pedido nativo (brief §8).
    for (const GlobalClickPermission perm :
         {GlobalClickPermission::kNotGranted, GlobalClickPermission::kUnknown}) {
        NIMVLETS_CHECK(EvaluateGlobalClickRequest(Status(
                           GlobalClickCapability::kSupportedNeedsPermission, perm)) ==
                       GlobalClickRequestOutcome::kNeedsExplanation);
    }
    return true;
}

// --- Ciclo de vida del monitor (contra el doble puro) -------------

bool TestMonitorDoesNotStartWithoutPermission() {
    FakeGlobalClickMonitor monitor;
    monitor.permission = GlobalClickPermission::kNotGranted;

    int forwarded = 0;
    NIMVLETS_CHECK(!monitor.Start(
        [](void* u) { ++*static_cast<int*>(u); }, &forwarded));
    NIMVLETS_CHECK(!monitor.IsActive());
    NIMVLETS_CHECK(monitor.QueryStatus().startFailed);
    // Y un "clic" no llega a ningún lado.
    monitor.EmitPrimaryClick();
    NIMVLETS_CHECK(forwarded == 0);

    // Consultar el estado NUNCA pide permiso.
    NIMVLETS_CHECK(monitor.requestPermissionCalls == 0);
    return true;
}

bool TestMonitorStartsStopsAndForwardsExactlyOnePerClick() {
    FakeGlobalClickMonitor monitor;
    monitor.permission = GlobalClickPermission::kGranted;

    int forwarded = 0;
    NIMVLETS_CHECK(monitor.Start([](void* u) { ++*static_cast<int*>(u); }, &forwarded));
    NIMVLETS_CHECK(monitor.IsActive());
    NIMVLETS_CHECK(monitor.QueryStatus().monitorActive);
    NIMVLETS_CHECK(!monitor.QueryStatus().startFailed);

    monitor.EmitPrimaryClick();
    NIMVLETS_CHECK(forwarded == 1);
    monitor.EmitPrimaryClicks(4);
    NIMVLETS_CHECK(forwarded == 5);  // uno por presión, sin coalescer

    // Start() de nuevo con el monitor vivo es un no-op exitoso.
    NIMVLETS_CHECK(monitor.Start([](void* u) { ++*static_cast<int*>(u); }, &forwarded));
    NIMVLETS_CHECK(monitor.IsActive());

    // Tras Stop() el callback ya no puede invocarse — la garantía en la
    // que se apoya el orden de shutdown de SpikeApp (brief §19).
    monitor.Stop();
    NIMVLETS_CHECK(!monitor.IsActive());
    monitor.EmitPrimaryClicks(3);
    NIMVLETS_CHECK(forwarded == 5);

    // Stop() es idempotente.
    monitor.Stop();
    NIMVLETS_CHECK(!monitor.IsActive());
    return true;
}

bool TestStartFailureAndRuntimeFailureAreReported() {
    FakeGlobalClickMonitor monitor;
    monitor.permission = GlobalClickPermission::kGranted;
    monitor.failStart = true;

    int forwarded = 0;
    NIMVLETS_CHECK(!monitor.Start([](void* u) { ++*static_cast<int*>(u); }, &forwarded));
    NIMVLETS_CHECK(!monitor.IsActive());
    NIMVLETS_CHECK(monitor.QueryStatus().startFailed);
    // Settings lo traduce a "no se pudo iniciar" + reintento.
    NIMVLETS_CHECK(ResolveGlobalClickUiState(ClickCountingMode::kAnywhere, monitor.QueryStatus())
                       .statusLine == GlobalClickStatusLine::kFailed);

    // Ahora sí arranca, y después se cae sola en runtime.
    monitor.failStart = false;
    NIMVLETS_CHECK(monitor.Start([](void* u) { ++*static_cast<int*>(u); }, &forwarded));
    monitor.EmitPrimaryClick();
    NIMVLETS_CHECK(forwarded == 1);

    monitor.SimulateRuntimeFailure();
    NIMVLETS_CHECK(!monitor.IsActive());
    monitor.EmitPrimaryClick();
    NIMVLETS_CHECK(forwarded == 1);  // ya no llega nada
    // Y el modo EFECTIVO cae solo a local: los clics del pet vuelven a
    // contar sin que nadie tenga que tocar la preferencia.
    NIMVLETS_CHECK(ResolveEffectiveClickCounting(ClickCountingMode::kAnywhere, monitor.IsActive()) ==
                   core::EffectiveClickCounting::kLocal);
    return true;
}

// El flujo de permiso de macOS tal como REALMENTE se comporta: pedir no
// concede — el usuario tiene que ir a Ajustes del Sistema. La app queda
// en "pedido pero no activo", nunca mintiendo.
bool TestRequestPermissionUsuallyLeavesItPendingAndThatIsNotAFailure() {
    FakeGlobalClickMonitor monitor;
    monitor.permission = GlobalClickPermission::kNotGranted;
    monitor.grantOnRequest = false;  // el caso normal en macOS

    NIMVLETS_CHECK(!monitor.RequestPermission());
    NIMVLETS_CHECK(monitor.requestPermissionCalls == 1);
    NIMVLETS_CHECK(monitor.QueryStatus().permission == GlobalClickPermission::kNotGranted);

    // Con la preferencia ya en "anywhere", Settings dice "falta permiso"
    // y ofrece "Comprobar de nuevo" — no "Active", no silencio.
    const GlobalClickUiState pending =
        ResolveGlobalClickUiState(ClickCountingMode::kAnywhere, monitor.QueryStatus());
    NIMVLETS_CHECK(pending.statusLine == GlobalClickStatusLine::kPermissionRequired);
    NIMVLETS_CHECK(pending.showCheckAgain);
    // Y el conteo sigue funcionando localmente mientras tanto.
    NIMVLETS_CHECK(CountedClickShouldIncrement(
        ResolveEffectiveClickCounting(ClickCountingMode::kAnywhere, monitor.IsActive()),
        ClickSource::kLocalPet));

    // El owner lo activa en Ajustes del Sistema y vuelve: "Check again"
    // re-consulta y arranca, sin pedir permiso de nuevo.
    monitor.permission = GlobalClickPermission::kGranted;
    int forwarded = 0;
    NIMVLETS_CHECK(monitor.Start([](void* u) { ++*static_cast<int*>(u); }, &forwarded));
    NIMVLETS_CHECK(monitor.requestPermissionCalls == 1);  // no volvió a pedir
    NIMVLETS_CHECK(
        ResolveGlobalClickUiState(ClickCountingMode::kAnywhere, monitor.QueryStatus()).statusLine ==
        GlobalClickStatusLine::kActive);
    return true;
}

// --- Integración: monitor -> reenvío -> wallet canónico -----------

// Espeja el cableado real de src/app: el callback del monitor reenvía un
// evento sin payload, y el "hilo principal" corre la MISMA política que
// SpikeApp::HandleCountedClick. Prueba de punta a punta que un clic
// global reenviado suma exactamente 1, y que un clic sobre el pet en ese
// mismo modo no suma nada.
struct AppMirror {
    std::uint64_t clickBalance = 0;
    ClickCountingMode requested = ClickCountingMode::kNimvletOnly;
    FakeGlobalClickMonitor* monitor = nullptr;
    int dirtyMarks = 0;
    int walletRefreshes = 0;

    void CountedClick(ClickSource source) {
        if (!CountedClickShouldIncrement(
                ResolveEffectiveClickCounting(requested, monitor != nullptr && monitor->IsActive()),
                source)) {
            return;
        }
        ++clickBalance;
        ++dirtyMarks;       // persistenceScheduler_.MarkDirty (mismo debounce de siempre)
        ++walletRefreshes;  // PushModelsToProductWindow (wallet canónico en vivo)
    }

    static void OnGlobalClick(void* userData) {
        // El equivalente de OnGlobalPrimaryClick + el despacho del evento
        // en el hilo principal: sin coordenadas, sin código, sin datos.
        static_cast<AppMirror*>(userData)->CountedClick(ClickSource::kGlobalMonitor);
    }
};

bool TestForwardedGlobalClickIncrementsTheCanonicalWalletOnce() {
    FakeGlobalClickMonitor monitor;
    monitor.permission = GlobalClickPermission::kGranted;

    AppMirror app;
    app.monitor = &monitor;
    app.requested = ClickCountingMode::kAnywhere;
    NIMVLETS_CHECK(monitor.Start(&AppMirror::OnGlobalClick, &app));

    monitor.EmitPrimaryClick();
    NIMVLETS_CHECK(app.clickBalance == 1);
    NIMVLETS_CHECK(app.dirtyMarks == 1);       // se persiste por el camino de siempre
    NIMVLETS_CHECK(app.walletRefreshes == 1);  // el header del Product UI se refresca

    // El MISMO clic físico también llega por la ruta local del pet: +0.
    app.CountedClick(ClickSource::kLocalPet);
    NIMVLETS_CHECK(app.clickBalance == 1);
    NIMVLETS_CHECK(app.walletRefreshes == 1);  // nada cambió -> no se redibuja

    monitor.EmitPrimaryClicks(7);
    NIMVLETS_CHECK(app.clickBalance == 8);

    // El owner vuelve a "Nimvlet only": src/app detiene el monitor y la
    // ruta local vuelve a ser la fuente.
    app.requested = ClickCountingMode::kNimvletOnly;
    monitor.Stop();
    monitor.EmitPrimaryClicks(5);
    NIMVLETS_CHECK(app.clickBalance == 8);  // nada global cuenta ya
    app.CountedClick(ClickSource::kLocalPet);
    NIMVLETS_CHECK(app.clickBalance == 9);
    return true;
}

}  // namespace

void RegisterGlobalClickMonitorTests(testing::TestRunner& runner) {
    runner.Add("GlobalClick/UnavailablePlatformSaysSoAndBlocksTheOption",
               TestUnavailablePlatformSaysSoAndBlocksTheOption);
    runner.Add("GlobalClick/LocalModeShowsNoPermissionNoise", TestLocalModeShowsNoPermissionNoise);
    runner.Add("GlobalClick/AnywhereActiveReportsActive", TestAnywhereActiveReportsActive);
    runner.Add("GlobalClick/AnywhereWithoutPermissionIsNeverSilent",
               TestAnywhereWithoutPermissionIsNeverSilent);
    runner.Add("GlobalClick/AnywhereWithPermissionButNotRunningReportsFailure",
               TestAnywhereWithPermissionButNotRunningReportsFailure);
    runner.Add("GlobalClick/RequestOutcomeGatesThePermissionPrompt",
               TestRequestOutcomeGatesThePermissionPrompt);
    runner.Add("GlobalClick/MonitorDoesNotStartWithoutPermission",
               TestMonitorDoesNotStartWithoutPermission);
    runner.Add("GlobalClick/MonitorStartsStopsAndForwardsExactlyOnePerClick",
               TestMonitorStartsStopsAndForwardsExactlyOnePerClick);
    runner.Add("GlobalClick/StartFailureAndRuntimeFailureAreReported",
               TestStartFailureAndRuntimeFailureAreReported);
    runner.Add("GlobalClick/RequestPermissionUsuallyLeavesItPendingAndThatIsNotAFailure",
               TestRequestPermissionUsuallyLeavesItPendingAndThatIsNotAFailure);
    runner.Add("GlobalClick/ForwardedGlobalClickIncrementsTheCanonicalWalletOnce",
               TestForwardedGlobalClickIncrementsTheCanonicalWalletOnce);
}

}  // namespace nimvlets::tests
