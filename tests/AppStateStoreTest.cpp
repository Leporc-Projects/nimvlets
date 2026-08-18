#include "AppStateStoreTest.h"

#include "persistence/AppStateStore.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using nimvlets::persistence::AppState;
using nimvlets::persistence::AppStateStore;
using nimvlets::persistence::WindowPosition;

namespace nimvlets::tests {

namespace {

// A fresh, isolated temporary directory per test, cleaned up on
// destruction — never the user's real per-user app-data directory (see
// docs/PERSISTENCE.md and the block brief's testability requirements).
// Each instance gets a distinct subdirectory of the system temp path;
// tests in this file run sequentially in one process, so a simple
// incrementing counter is sufficient for uniqueness.
class TempTestDirectory {
 public:
    TempTestDirectory() {
        static int counter = 0;
        path_ = std::filesystem::temp_directory_path() /
                ("nimvlets_appstate_test_" + std::to_string(counter++));
        std::filesystem::create_directories(path_);
    }

    ~TempTestDirectory() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);  // best-effort cleanup
    }

    TempTestDirectory(const TempTestDirectory&) = delete;
    TempTestDirectory& operator=(const TempTestDirectory&) = delete;

    std::string path() const { return path_.string(); }

 private:
    std::filesystem::path path_;
};

bool TestLoadReturnsDefaultsWhenNoSaveExists() {
    TempTestDirectory dir;
    const AppStateStore store(dir.path());

    std::string warning;
    const AppState state = store.Load(&warning);

    NIMVLETS_CHECK(state == AppState{});
    NIMVLETS_CHECK(!warning.empty());  // explains *why* it's defaults
    return true;
}

bool TestSaveThenLoadRoundTrips() {
    TempTestDirectory dir;
    const AppStateStore store(dir.path());

    AppState original;
    original.clickBalance = 777;
    original.activePetId = "bunny_dev";
    original.lastWindowPosition = WindowPosition{100, 200};

    std::string saveError;
    NIMVLETS_CHECK(store.Save(original, saveError));
    NIMVLETS_CHECK(saveError.empty());

    std::string loadWarning;
    const AppState loaded = store.Load(&loadWarning);
    NIMVLETS_CHECK(loaded == original);
    NIMVLETS_CHECK(loadWarning.empty());  // a valid, current-schema save was found
    return true;
}

bool TestSaveIsAtomicNoTempFileLeftBehind() {
    TempTestDirectory dir;
    const AppStateStore store(dir.path());

    AppState state;
    state.clickBalance = 1;
    std::string error;
    NIMVLETS_CHECK(store.Save(state, error));

    NIMVLETS_CHECK(std::filesystem::exists(std::filesystem::path(dir.path()) / "state.nvstate"));
    NIMVLETS_CHECK(!std::filesystem::exists(std::filesystem::path(dir.path()) / "state.nvstate.tmp"));
    return true;
}

bool TestLoadRecoversFromCorruptFile() {
    TempTestDirectory dir;
    const AppStateStore store(dir.path());

    // Write garbage directly, bypassing Save() entirely, to simulate
    // real on-disk corruption (partial write, bit rot, a stray editor
    // save, ...).
    {
        std::ofstream garbage(std::filesystem::path(dir.path()) / "state.nvstate", std::ios::binary);
        const std::vector<std::uint8_t> junk = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
        garbage.write(reinterpret_cast<const char*>(junk.data()), static_cast<std::streamsize>(junk.size()));
    }

    std::string warning;
    const AppState state = store.Load(&warning);
    NIMVLETS_CHECK(state == AppState{});  // safe defaults, not a crash
    NIMVLETS_CHECK(!warning.empty());
    return true;
}

bool TestFailedWriteDoesNotCrashAndReportsError() {
    TempTestDirectory dir;
    // Pre-create a *directory* at exactly the path AppStateStore would
    // use for its temp file — opening a directory for writing as a
    // regular file fails uniformly on every platform this project
    // targets, giving a portable, deterministic write failure without
    // relying on filesystem-permission tricks (chmod semantics differ
    // enough between POSIX and Windows to make that fragile in CI).
    std::filesystem::create_directories(std::filesystem::path(dir.path()) / "state.nvstate.tmp");

    const AppStateStore store(dir.path());
    AppState state;
    state.clickBalance = 99;

    std::string error;
    NIMVLETS_CHECK(!store.Save(state, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

bool TestFailedWritePreservesPriorValidSave() {
    TempTestDirectory dir;
    const AppStateStore store(dir.path());

    AppState good;
    good.clickBalance = 555;
    good.activePetId = "bunny_dev";
    std::string firstSaveError;
    NIMVLETS_CHECK(store.Save(good, firstSaveError));

    // Now force the *next* save to fail via the same directory-
    // collision technique, after a valid save already landed on disk.
    std::filesystem::create_directories(std::filesystem::path(dir.path()) / "state.nvstate.tmp");

    AppState bad;
    bad.clickBalance = 1;
    bad.activePetId = "should-not-be-saved";
    std::string secondSaveError;
    NIMVLETS_CHECK(!store.Save(bad, secondSaveError));
    NIMVLETS_CHECK(!secondSaveError.empty());

    // The previously good save must be completely untouched.
    std::string loadWarning;
    const AppState loaded = store.Load(&loadWarning);
    NIMVLETS_CHECK(loaded == good);
    NIMVLETS_CHECK(loadWarning.empty());
    return true;
}

}  // namespace

void RegisterAppStateStoreTests(testing::TestRunner& runner) {
    runner.Add("AppStateStore/LoadReturnsDefaultsWhenNoSaveExists", TestLoadReturnsDefaultsWhenNoSaveExists);
    runner.Add("AppStateStore/SaveThenLoadRoundTrips", TestSaveThenLoadRoundTrips);
    runner.Add("AppStateStore/SaveIsAtomicNoTempFileLeftBehind", TestSaveIsAtomicNoTempFileLeftBehind);
    runner.Add("AppStateStore/LoadRecoversFromCorruptFile", TestLoadRecoversFromCorruptFile);
    runner.Add("AppStateStore/FailedWriteDoesNotCrashAndReportsError", TestFailedWriteDoesNotCrashAndReportsError);
    runner.Add("AppStateStore/FailedWritePreservesPriorValidSave", TestFailedWritePreservesPriorValidSave);
}

}  // namespace nimvlets::tests
