#include "PersistenceIntegrationTest.h"

#include "persistence/AppStateStore.h"
#include "persistence/PersistenceScheduler.h"

#include <filesystem>
#include <string>

// Integration test: exercises persistence::AppState, AppStateStore, and
// PersistenceScheduler wired together exactly the way src/app/SpikeApp
// does (click -> increment + mark dirty; drag end -> update position +
// mark dirty; the event loop's deadline check -> flush only if dirty;
// clean shutdown -> flush regardless of the deadline). All three pieces
// are already pure/file-scoped-only, so this needs no SDL and no
// mocking — only a real, isolated temp directory (never the user's
// real app-data location).

using nimvlets::persistence::AppState;
using nimvlets::persistence::AppStateStore;
using nimvlets::persistence::PersistenceScheduler;

namespace nimvlets::tests {

namespace {

// Same small RAII temp-directory helper as tests/AppStateStoreTest.cpp
// (kept file-local rather than shared, consistent with this
// repository's other single-use test helpers).
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

// Mirrors SpikeApp::FlushPersistedState(): a no-op unless the
// scheduler is actually dirty.
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

    // Mirrors SpikeApp's MOUSE_BUTTON_UP click branch.
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

    // Five rapid clicks, all well before the 2000ms debounce deadline.
    for (int i = 0; i < 5; ++i) {
        ++state.clickBalance;
        scheduler.MarkDirty(static_cast<double>(i) * 10.0);  // t=0,10,20,30,40
    }
    NIMVLETS_CHECK(state.clickBalance == 5);
    NIMVLETS_CHECK(*scheduler.NextFlushDeadlineMs() == 2000.0);  // armed by the *first* click only

    // Nothing on disk yet — the event loop hasn't reached the deadline.
    NIMVLETS_CHECK(!std::filesystem::exists(std::filesystem::path(dir.path()) / "state.nvstate"));

    // The event loop wakes at (or after) the deadline and flushes once.
    FlushIfDirty(store, state, scheduler, 2000.0);
    NIMVLETS_CHECK(!scheduler.IsDirty());

    std::string warning;
    const AppState loaded = store.Load(&warning);
    NIMVLETS_CHECK(loaded.clickBalance == 5);  // all five coalesced into this one write
    return true;
}

bool TestDragEndUpdatesWindowPositionAndMarksDirty() {
    TempTestDirectory dir;
    const AppStateStore store(dir.path());
    AppState state;
    PersistenceScheduler scheduler(2000.0);

    // Mirrors SpikeApp's MOUSE_BUTTON_UP drag branch.
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

    // "Clean shutdown" happens almost immediately, long before the
    // debounce deadline — it must still flush (see
    // src/app/SpikeApp.cpp's Shutdown()).
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

    // Force this attempt to fail (see AppStateStoreTest.cpp for why
    // this technique is portable across platforms).
    std::filesystem::create_directories(std::filesystem::path(dir.path()) / "state.nvstate.tmp");
    FlushIfDirty(store, state, scheduler, 2000.0);
    NIMVLETS_CHECK(scheduler.IsDirty());  // not lost — still pending
    NIMVLETS_CHECK(!std::filesystem::exists(std::filesystem::path(dir.path()) / "state.nvstate"));

    // Clear the obstruction and let the (rescheduled) retry succeed.
    std::filesystem::remove_all(std::filesystem::path(dir.path()) / "state.nvstate.tmp");
    const double retryDeadline = *scheduler.NextFlushDeadlineMs();
    FlushIfDirty(store, state, scheduler, retryDeadline);
    NIMVLETS_CHECK(!scheduler.IsDirty());

    const AppState loaded = store.Load();
    NIMVLETS_CHECK(loaded.clickBalance == 1);
    return true;
}

}  // namespace

void RegisterPersistenceIntegrationTests(testing::TestRunner& runner) {
    runner.Add("PersistenceIntegration/ClickMarksDirtyButDoesNotWriteImmediately", TestClickMarksDirtyButDoesNotWriteImmediately);
    runner.Add("PersistenceIntegration/RapidClicksCoalesceIntoOneWriteAtDeadline", TestRapidClicksCoalesceIntoOneWriteAtDeadline);
    runner.Add("PersistenceIntegration/DragEndUpdatesWindowPositionAndMarksDirty", TestDragEndUpdatesWindowPositionAndMarksDirty);
    runner.Add("PersistenceIntegration/CleanShutdownFlushesRegardlessOfDeadline", TestCleanShutdownFlushesRegardlessOfDeadline);
    runner.Add("PersistenceIntegration/FailedFlushKeepsPendingChangeForNextAttempt", TestFailedFlushKeepsPendingChangeForNextAttempt);
}

}  // namespace nimvlets::tests
