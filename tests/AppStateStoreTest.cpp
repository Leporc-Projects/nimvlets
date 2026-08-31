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

// Un directorio temporal fresco y aislado por test, limpiado al
// destruirse — nunca el directorio real de app-data del usuario (ver
// docs/PERSISTENCE.md y los requisitos de testeabilidad del brief del
// bloque). Cada instancia recibe un subdirectorio distinto de la ruta
// temporal del sistema; los tests de este archivo corren
// secuencialmente en un mismo proceso, así que un contador incremental
// simple alcanza para garantizar unicidad.
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
        std::filesystem::remove_all(path_, ec);  // limpieza best-effort
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
    NIMVLETS_CHECK(!warning.empty());  // explica *por qué* son defaults
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
    NIMVLETS_CHECK(loadWarning.empty());  // se encontró un save válido con el schema actual
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

    // Escribe basura directamente, evitando Save() por completo, para
    // simular corrupción real en disco (escritura parcial, bit rot,
    // un guardado accidental de un editor, ...).
    {
        std::ofstream garbage(std::filesystem::path(dir.path()) / "state.nvstate", std::ios::binary);
        const std::vector<std::uint8_t> junk = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
        garbage.write(reinterpret_cast<const char*>(junk.data()), static_cast<std::streamsize>(junk.size()));
    }

    std::string warning;
    const AppState state = store.Load(&warning);
    NIMVLETS_CHECK(state == AppState{});  // defaults seguros, no un crash
    NIMVLETS_CHECK(!warning.empty());
    return true;
}

// Block 09A: `outSaveFileExisted` distingue "usuario genuinamente nuevo"
// (sin archivo) de "recuperación de un archivo corrupto" (existía) —
// src/app NUNCA manda a onboarding a este último (brief §4/§27).
bool TestSaveFileExistedFlag() {
    TempTestDirectory dir;
    const AppStateStore store(dir.path());

    // Sin archivo: existed == false, lifecycle default kPending.
    {
        bool existed = true;
        std::uint32_t onDisk = 999;
        const AppState state = store.Load(nullptr, &onDisk, &existed);
        NIMVLETS_CHECK(!existed);
        NIMVLETS_CHECK(state.onboardingLifecycle == nimvlets::persistence::OnboardingLifecycle::kPending);
    }

    // Tras un Save(): existed == true.
    {
        AppState s;
        s.clickBalance = 5;
        std::string err;
        NIMVLETS_CHECK(store.Save(s, err));
        bool existed = false;
        const AppState loaded = store.Load(nullptr, nullptr, &existed);
        NIMVLETS_CHECK(existed);
        NIMVLETS_CHECK(loaded.clickBalance == 5);
    }

    // Archivo corrupto: TAMBIÉN existed == true (el archivo está ahí).
    {
        std::ofstream garbage(std::filesystem::path(dir.path()) / "state.nvstate", std::ios::binary);
        const std::vector<std::uint8_t> junk = {0x01, 0x02, 0x03};
        garbage.write(reinterpret_cast<const char*>(junk.data()), static_cast<std::streamsize>(junk.size()));
        garbage.close();
        bool existed = false;
        const AppState state = store.Load(nullptr, nullptr, &existed);
        NIMVLETS_CHECK(existed);
        NIMVLETS_CHECK(state == AppState{});
    }
    return true;
}

bool TestFailedWriteDoesNotCrashAndReportsError() {
    TempTestDirectory dir;
    // Pre-crea un *directorio* exactamente en la ruta que
    // AppStateStore usaría para su archivo temporal — abrir un
    // directorio para escritura como si fuera un archivo regular falla
    // de forma uniforme en cada plataforma que este proyecto soporta,
    // dando un fallo de escritura portátil y determinista sin depender
    // de trucos de permisos de filesystem (la semántica de chmod
    // difiere bastante entre POSIX y Windows como para volverlo frágil
    // en CI).
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

    // Ahora fuerza a que el *siguiente* save falle, vía la misma
    // técnica de colisión de directorio, después de que un save válido
    // ya quedó en disco.
    std::filesystem::create_directories(std::filesystem::path(dir.path()) / "state.nvstate.tmp");

    AppState bad;
    bad.clickBalance = 1;
    bad.activePetId = "should-not-be-saved";
    std::string secondSaveError;
    NIMVLETS_CHECK(!store.Save(bad, secondSaveError));
    NIMVLETS_CHECK(!secondSaveError.empty());

    // El save previo bueno debe quedar completamente intacto.
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
    runner.Add("AppStateStore/SaveFileExistedFlag", TestSaveFileExistedFlag);
}

}  // namespace nimvlets::tests
