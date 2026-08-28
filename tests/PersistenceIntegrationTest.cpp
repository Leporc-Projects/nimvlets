#include "PersistenceIntegrationTest.h"

#include "catalog/PetCatalog.h"
#include "catalog/PetEntitlement.h"
#include "catalog/PurchasePolicy.h"
#include "persistence/AppStateStore.h"
#include "persistence/PersistenceScheduler.h"

#include <filesystem>
#include <string>
#include <vector>

// Test de integración: ejercita persistence::AppState, AppStateStore y
// PersistenceScheduler conectados exactamente igual que
// src/app/SpikeApp (click -> incrementar + marcar dirty; fin de drag
// -> actualizar posición + marcar dirty; el chequeo de deadline del
// event loop -> flush solo si está dirty; shutdown limpio -> flush sin
// importar el deadline). Las tres piezas ya son puras/de alcance de
// archivo únicamente, así que esto no necesita SDL ni mocking — solo
// un directorio temporal real y aislado (nunca la ubicación real de
// app-data del usuario).

using nimvlets::persistence::AppState;
using nimvlets::persistence::AppStateStore;
using nimvlets::persistence::PersistenceScheduler;

namespace nimvlets::tests {

namespace {

// El mismo pequeño helper RAII de directorio temporal que
// tests/AppStateStoreTest.cpp (se mantiene local al archivo en vez de
// compartido, consistente con los demás helpers de test de uso único
// de este repositorio).
class TempTestDirectory {
 public:
    TempTestDirectory() {
        static int counter = 0;
        path_ = std::filesystem::temp_directory_path() /
                ("nimvlets_persistence_integration_test_" + std::to_string(counter++));
        std::filesystem::create_directories(path_);
    }

    ~TempTestDirectory() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    TempTestDirectory(const TempTestDirectory&) = delete;
    TempTestDirectory& operator=(const TempTestDirectory&) = delete;

    std::string path() const { return path_.string(); }

 private:
    std::filesystem::path path_;
};

// Refleja SpikeApp::FlushPersistedState(): no hace nada salvo que el
// scheduler esté realmente dirty.
void FlushIfDirty(const AppStateStore& store, AppState& state, PersistenceScheduler& scheduler, double nowMs) {
    if (!scheduler.IsDirty()) {
        return;
    }
    std::string error;
    if (store.Save(state, error)) {
        scheduler.OnFlushSucceeded();
    } else {
        scheduler.OnFlushFailed(nowMs);
    }
}

bool TestClickMarksDirtyButDoesNotWriteImmediately() {
    TempTestDirectory dir;
    const AppStateStore store(dir.path());
    AppState state;
    PersistenceScheduler scheduler(2000.0);

    // Refleja la rama de click de MOUSE_BUTTON_UP en SpikeApp.
    ++state.clickBalance;
    scheduler.MarkDirty(0.0);

    NIMVLETS_CHECK(scheduler.IsDirty());
    NIMVLETS_CHECK(!std::filesystem::exists(std::filesystem::path(dir.path()) / "state.nvstate"));
    return true;
}

bool TestRapidClicksCoalesceIntoOneWriteAtDeadline() {
    TempTestDirectory dir;
    const AppStateStore store(dir.path());
    AppState state;
    PersistenceScheduler scheduler(2000.0);

    // Cinco clicks rápidos, todos bastante antes del deadline de
    // debounce de 2000ms.
    for (int i = 0; i < 5; ++i) {
        ++state.clickBalance;
        scheduler.MarkDirty(static_cast<double>(i) * 10.0);  // t=0,10,20,30,40
    }
    NIMVLETS_CHECK(state.clickBalance == 5);
    NIMVLETS_CHECK(*scheduler.NextFlushDeadlineMs() == 2000.0);  // armado solo por el *primer* click

    // Nada en disco todavía — el event loop no llegó al deadline.
    NIMVLETS_CHECK(!std::filesystem::exists(std::filesystem::path(dir.path()) / "state.nvstate"));

    // El event loop despierta en (o después de) el deadline y flushea una vez.
    FlushIfDirty(store, state, scheduler, 2000.0);
    NIMVLETS_CHECK(!scheduler.IsDirty());

    std::string warning;
    const AppState loaded = store.Load(&warning);
    NIMVLETS_CHECK(loaded.clickBalance == 5);  // los cinco se coalescieron en esta única escritura
    return true;
}

bool TestDragEndUpdatesWindowPositionAndMarksDirty() {
    TempTestDirectory dir;
    const AppStateStore store(dir.path());
    AppState state;
    PersistenceScheduler scheduler(2000.0);

    // Refleja la rama de drag de MOUSE_BUTTON_UP en SpikeApp.
    state.lastWindowPosition = nimvlets::persistence::WindowPosition{321, 654};
    scheduler.MarkDirty(0.0);

    NIMVLETS_CHECK(scheduler.IsDirty());
    FlushIfDirty(store, state, scheduler, 2000.0);

    const AppState loaded = store.Load();
    NIMVLETS_CHECK(loaded.lastWindowPosition.has_value());
    NIMVLETS_CHECK(loaded.lastWindowPosition->x == 321);
    NIMVLETS_CHECK(loaded.lastWindowPosition->y == 654);
    return true;
}

bool TestCleanShutdownFlushesRegardlessOfDeadline() {
    TempTestDirectory dir;
    const AppStateStore store(dir.path());
    AppState state;
    PersistenceScheduler scheduler(2000.0);

    ++state.clickBalance;
    scheduler.MarkDirty(0.0);  // deadline = 2000.0

    // El "shutdown limpio" ocurre casi de inmediato, mucho antes del
    // deadline de debounce — igual debe flushear (ver Shutdown() en
    // src/app/SpikeApp.cpp).
    FlushIfDirty(store, state, scheduler, 5.0);
    NIMVLETS_CHECK(!scheduler.IsDirty());

    const AppState loaded = store.Load();
    NIMVLETS_CHECK(loaded.clickBalance == 1);
    return true;
}

bool TestFailedFlushKeepsPendingChangeForNextAttempt() {
    TempTestDirectory dir;
    const AppStateStore store(dir.path());
    AppState state;
    PersistenceScheduler scheduler(2000.0);

    ++state.clickBalance;
    scheduler.MarkDirty(0.0);

    // Fuerza a que este intento falle (ver AppStateStoreTest.cpp para
    // por qué esta técnica es portátil entre plataformas).
    std::filesystem::create_directories(std::filesystem::path(dir.path()) / "state.nvstate.tmp");
    FlushIfDirty(store, state, scheduler, 2000.0);
    NIMVLETS_CHECK(scheduler.IsDirty());  // no se perdió — sigue pendiente
    NIMVLETS_CHECK(!std::filesystem::exists(std::filesystem::path(dir.path()) / "state.nvstate"));

    // Quita el obstáculo y deja que el reintento (reprogramado) tenga éxito.
    std::filesystem::remove_all(std::filesystem::path(dir.path()) / "state.nvstate.tmp");
    const double retryDeadline = *scheduler.NextFlushDeadlineMs();
    FlushIfDirty(store, state, scheduler, retryDeadline);
    NIMVLETS_CHECK(!scheduler.IsDirty());

    const AppState loaded = store.Load();
    NIMVLETS_CHECK(loaded.clickBalance == 1);
    return true;
}

// Block 07: refleja SpikeApp::HandlePurchaseRequest — evalúa la compra,
// muta balance + propiedad en el MISMO AppState, y flushea de inmediato
// (una sola escritura atómica). Tras recargar, el balance reducido Y la
// propiedad nueva sobreviven JUNTOS (brief §13/§26).
bool TestPurchaseMutatesBalanceAndOwnershipTogetherAndSurvivesReload() {
    using nimvlets::catalog::CatalogEntry;
    using nimvlets::catalog::EvaluatePurchase;
    using nimvlets::catalog::PetCatalog;
    using nimvlets::catalog::PetEntitlement;
    using nimvlets::catalog::PetIdentity;
    using nimvlets::catalog::PurchaseResult;

    std::vector<CatalogEntry> entries;
    CatalogEntry nidir;
    nidir.identity = PetIdentity{"nidir", ""};
    nidir.displayName = "Nidir";
    nidir.packPath = "n.nvpack";
    nidir.isDefault = true;
    nidir.priceClicks = 300;
    nidir.publiclyPurchasable = true;
    entries.push_back(nidir);
    const PetCatalog catalog(std::move(entries));

    TempTestDirectory dir;
    const AppStateStore store(dir.path());
    PersistenceScheduler scheduler(2000.0);

    AppState state;
    state.ownershipSeeded = true;
    state.clickBalance = 512;
    state.ownedEntitlements = {nimvlets::persistence::OwnedEntitlement{"bunny", ""}};

    // --- "confirmar compra" ---
    const auto outcome = EvaluatePurchase(catalog, "nidir", state.clickBalance,
                                          {PetEntitlement{"bunny", ""}});
    NIMVLETS_CHECK(outcome.result == PurchaseResult::kSuccess);
    state.clickBalance = outcome.newBalance;  // 512 - 300 = 212
    state.ownedEntitlements.clear();
    for (const auto& e : outcome.newEntitlements) {
        state.ownedEntitlements.push_back(nimvlets::persistence::OwnedEntitlement{e.petId, e.variantId});
    }
    scheduler.MarkDirty(0.0);
    // Flush INMEDIATO (no se espera al debounce) — un solo Save().
    FlushIfDirty(store, state, scheduler, 1.0);
    NIMVLETS_CHECK(!scheduler.IsDirty());

    // --- "reiniciar Nimvlets" ---
    const AppState reloaded = store.Load();
    NIMVLETS_CHECK(reloaded.clickBalance == 212);  // balance reducido persistió
    NIMVLETS_CHECK((reloaded.ownedEntitlements ==
                    std::vector<nimvlets::persistence::OwnedEntitlement>{
                        nimvlets::persistence::OwnedEntitlement{"bunny", ""},
                        nimvlets::persistence::OwnedEntitlement{"nidir", ""}}));
    NIMVLETS_CHECK(reloaded.ownershipSeeded);
    return true;
}

}  // namespace

void RegisterPersistenceIntegrationTests(testing::TestRunner& runner) {
    runner.Add("PersistenceIntegration/ClickMarksDirtyButDoesNotWriteImmediately", TestClickMarksDirtyButDoesNotWriteImmediately);
    runner.Add("PersistenceIntegration/RapidClicksCoalesceIntoOneWriteAtDeadline", TestRapidClicksCoalesceIntoOneWriteAtDeadline);
    runner.Add("PersistenceIntegration/DragEndUpdatesWindowPositionAndMarksDirty", TestDragEndUpdatesWindowPositionAndMarksDirty);
    runner.Add("PersistenceIntegration/CleanShutdownFlushesRegardlessOfDeadline", TestCleanShutdownFlushesRegardlessOfDeadline);
    runner.Add("PersistenceIntegration/FailedFlushKeepsPendingChangeForNextAttempt", TestFailedFlushKeepsPendingChangeForNextAttempt);
    runner.Add("PersistenceIntegration/PurchaseMutatesBalanceAndOwnershipTogetherAndSurvivesReload",
               TestPurchaseMutatesBalanceAndOwnershipTogetherAndSurvivesReload);
}

}  // namespace nimvlets::tests
