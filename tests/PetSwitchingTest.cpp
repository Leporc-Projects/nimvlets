#include "PetSwitchingTest.h"

#include "catalog/ActivePetResolution.h"
#include "catalog/PetCatalog.h"
#include "content/AnimationDefinition.h"
#include "persistence/AppState.h"
#include "persistence/PersistenceScheduler.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// Tests de integración: ejercitan catalog::LoadPetForIdentity contra
// packs "NVPACK1" sintéticos reales en disco (no solo en memoria),
// para probar el camino completo catálogo -> content::LoadPetPackFromFile
// que src/app/SpikeApp usará de verdad al hacer switching -- el mismo
// nivel de realismo que tests/AppStateStoreTest.cpp usa para
// persistence (directorios temporales aislados, nunca datos reales).
// La parte de persistencia (marcar dirty solo tras un cambio exitoso)
// se ejercita conectando persistence::AppState + PersistenceScheduler
// de la misma forma que tests/PersistenceIntegrationTest.cpp hizo para
// click/drag.

using nimvlets::catalog::CatalogEntry;
using nimvlets::catalog::LoadPetForIdentity;
using nimvlets::catalog::PetCatalog;
using nimvlets::catalog::PetIdentity;
using nimvlets::content::PetDefinition;
using nimvlets::persistence::AppState;
using nimvlets::persistence::PersistenceScheduler;

namespace nimvlets::tests {

namespace {

// Mismo helper RAII de directorio temporal que tests/AppStateStoreTest.cpp
// y tests/PersistenceIntegrationTest.cpp (se mantiene local al archivo,
// consistente con los demás helpers de test de uso único de este
// repositorio).
class TempTestDirectory {
 public:
    TempTestDirectory() {
        static int counter = 0;
        path_ = std::filesystem::temp_directory_path() /
                ("nimvlets_pet_switching_test_" + std::to_string(counter++));
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

void AppendBytes(std::vector<std::uint8_t>& buf, const void* data, std::size_t n) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    buf.insert(buf.end(), bytes, bytes + n);
}

void AppendUint8(std::vector<std::uint8_t>& buf, std::uint8_t v) {
    buf.push_back(v);
}

void AppendUint32(std::vector<std::uint8_t>& buf, std::uint32_t v) {
    AppendBytes(buf, &v, sizeof(v));
}

void AppendFloat64(std::vector<std::uint8_t>& buf, double v) {
    AppendBytes(buf, &v, sizeof(v));
}

void AppendString(std::vector<std::uint8_t>& buf, const std::string& s) {
    AppendUint32(buf, static_cast<std::uint32_t>(s.size()));
    AppendBytes(buf, s.data(), s.size());
}

// Un pack "NVPACK1" mínimo pero válido -- mismo formato que
// tests/PetPackLoaderTest.cpp construye a mano -- con `id` y un
// `fillByte` distintivo en el pixel de su frame de idle, para poder
// confirmar exactamente qué pack terminó cargado sin ambigüedad.
std::vector<std::uint8_t> BuildMinimalPackBytes(const std::string& id, std::uint8_t fillByte) {
    std::vector<std::uint8_t> buf;
    const char magic[8] = {'N', 'V', 'P', 'A', 'C', 'K', '1', '\0'};
    AppendBytes(buf, magic, sizeof(magic));
    AppendString(buf, id);
    AppendString(buf, id);  // displayName, no importa para estos tests
    AppendString(buf, "");  // variantGroup
    AppendUint32(buf, 1);   // canvasWidth
    AppendUint32(buf, 1);   // canvasHeight
    AppendUint8(buf, 128);  // alphaHitThreshold
    AppendFloat64(buf, 300.0);
    AppendString(buf, "");  // contentVersion

    // idle: estático, 1 frame, 1x1, con el fillByte distintivo.
    AppendString(buf, "idle");
    AppendUint8(buf, 0);  // kStatic
    AppendFloat64(buf, 0.0);
    AppendUint8(buf, 1);
    AppendUint32(buf, 1);  // frameCount
    AppendUint32(buf, 1);
    AppendUint32(buf, 1);
    AppendFloat64(buf, 0.0);
    AppendFloat64(buf, 0.0);
    AppendFloat64(buf, 0.0);
    const std::vector<std::uint8_t> px = {fillByte, fillByte, fillByte, 255};
    AppendBytes(buf, px.data(), px.size());

    // click_reaction: mínimo, 1 frame estático.
    AppendString(buf, "click_reaction");
    AppendUint8(buf, 0);
    AppendFloat64(buf, 0.0);
    AppendUint8(buf, 1);
    AppendUint32(buf, 1);
    AppendUint32(buf, 1);
    AppendUint32(buf, 1);
    AppendFloat64(buf, 0.0);
    AppendFloat64(buf, 0.0);
    AppendFloat64(buf, 0.0);
    AppendBytes(buf, px.data(), px.size());

    AppendUint32(buf, 0);  // cero passive actions
    return buf;
}

// Variante "forma Nidir" de BuildMinimalPackBytes(): agrega la sección
// final opcional de idleDirectionOverrides (Block 04.2 -- ver
// docs/NIDIR_CONTENT.md) con una única entrada kLeft, distinguible por
// su propio fillByte. Usado para probar que switchear entre un pack NO
// direccional (forma Bunny) y uno SÍ direccional (forma Nidir) -- en
// cualquier orden -- nunca deja campos/estado del pet anterior
// filtrándose al nuevo.
std::vector<std::uint8_t> BuildDirectionalPackBytes(const std::string& id, std::uint8_t rightFillByte, std::uint8_t leftFillByte) {
    std::vector<std::uint8_t> buf = BuildMinimalPackBytes(id, rightFillByte);

    AppendUint32(buf, 1);  // directionalIdleOverrideCount = 1
    AppendUint8(buf, 1);   // direction = kLeft
    AppendString(buf, "idle_left");
    AppendUint8(buf, 1);  // kLoop
    AppendFloat64(buf, 0.0);
    AppendUint8(buf, 1);
    AppendUint32(buf, 1);  // frameCount
    AppendUint32(buf, 1);
    AppendUint32(buf, 1);
    AppendFloat64(buf, 0.0);
    AppendFloat64(buf, 0.0);
    AppendFloat64(buf, 0.0);
    const std::vector<std::uint8_t> px = {leftFillByte, leftFillByte, leftFillByte, 255};
    AppendBytes(buf, px.data(), px.size());

    return buf;
}

std::string WritePackFile(const std::string& dir, const std::string& fileName, const std::vector<std::uint8_t>& bytes) {
    const std::filesystem::path path = std::filesystem::path(dir) / fileName;
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return path.string();
}

// Catálogo de dos entradas reales en disco: "pet_a" (default) y
// "pet_b", cada una con un pack sintético distinto en `dir`.
PetCatalog BuildTwoPetCatalog(const std::string& dir) {
    WritePackFile(dir, "pet_a.nvpack", BuildMinimalPackBytes("pet_a", 0x0A));
    WritePackFile(dir, "pet_b.nvpack", BuildMinimalPackBytes("pet_b", 0x0B));

    std::vector<CatalogEntry> entries;
    CatalogEntry a;
    a.identity = PetIdentity{"pet_a", ""};
    a.displayName = "Pet A";
    a.packPath = (std::filesystem::path(dir) / "pet_a.nvpack").string();
    a.isDefault = true;
    entries.push_back(a);

    CatalogEntry b;
    b.identity = PetIdentity{"pet_b", ""};
    b.displayName = "Pet B";
    b.packPath = (std::filesystem::path(dir) / "pet_b.nvpack").string();
    b.isDefault = false;
    entries.push_back(b);

    return PetCatalog(std::move(entries));
}

bool TestSuccessfulSwitchUpdatesActiveIdentity() {
    TempTestDirectory dir;
    const PetCatalog catalog = BuildTwoPetCatalog(dir.path());

    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(LoadPetForIdentity(catalog, PetIdentity{"pet_a", ""}, pet, error));
    NIMVLETS_CHECK(pet.id == "pet_a");
    NIMVLETS_CHECK(pet.idle.frames[0].pixels[0] == 0x0A);

    NIMVLETS_CHECK(LoadPetForIdentity(catalog, PetIdentity{"pet_b", ""}, pet, error));
    NIMVLETS_CHECK(pet.id == "pet_b");
    NIMVLETS_CHECK(pet.idle.frames[0].pixels[0] == 0x0B);
    return true;
}

bool TestFailedSwitchToUnknownIdentityPreservesPriorPet() {
    TempTestDirectory dir;
    const PetCatalog catalog = BuildTwoPetCatalog(dir.path());

    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(LoadPetForIdentity(catalog, PetIdentity{"pet_a", ""}, pet, error));
    NIMVLETS_CHECK(pet.id == "pet_a");

    // "pet_c" no existe en el catálogo -- debe fallar sin tocar `pet`.
    const bool switched = LoadPetForIdentity(catalog, PetIdentity{"pet_c", ""}, pet, error);
    NIMVLETS_CHECK(!switched);
    NIMVLETS_CHECK(!error.empty());
    NIMVLETS_CHECK(pet.id == "pet_a");  // sin cambios -- el pet activo anterior sigue usable
    return true;
}

bool TestFailedSwitchWithMissingPackFilePreservesPriorPet() {
    TempTestDirectory dir;

    // Solo escribe el pack de "pet_a" en disco -- la entrada de
    // "pet_b" en el catálogo apunta a un archivo que nunca se crea.
    WritePackFile(dir.path(), "pet_a.nvpack", BuildMinimalPackBytes("pet_a", 0x0A));
    std::vector<CatalogEntry> entries;
    CatalogEntry a;
    a.identity = PetIdentity{"pet_a", ""};
    a.displayName = "Pet A";
    a.packPath = (std::filesystem::path(dir.path()) / "pet_a.nvpack").string();
    a.isDefault = true;
    entries.push_back(a);
    CatalogEntry b;
    b.identity = PetIdentity{"pet_b", ""};
    b.displayName = "Pet B";
    b.packPath = (std::filesystem::path(dir.path()) / "does_not_exist.nvpack").string();
    b.isDefault = false;
    entries.push_back(b);
    const PetCatalog catalog(std::move(entries));

    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(LoadPetForIdentity(catalog, PetIdentity{"pet_a", ""}, pet, error));

    const bool switched = LoadPetForIdentity(catalog, PetIdentity{"pet_b", ""}, pet, error);
    NIMVLETS_CHECK(!switched);
    NIMVLETS_CHECK(!error.empty());
    NIMVLETS_CHECK(pet.id == "pet_a");
    return true;
}

// Alterna A->B->A->B->A varias veces y confirma que, en todo momento,
// `pet` refleja *exactamente* el último target -- sin frames ni
// campos remanentes del pet anterior filtrándose entre cambios.
bool TestRepeatedSwitchingDoesNotAccumulateLoadedPetState() {
    TempTestDirectory dir;
    const PetCatalog catalog = BuildTwoPetCatalog(dir.path());

    PetDefinition pet;
    std::string error;
    const PetIdentity identityA{"pet_a", ""};
    const PetIdentity identityB{"pet_b", ""};

    for (int i = 0; i < 5; ++i) {
        NIMVLETS_CHECK(LoadPetForIdentity(catalog, identityA, pet, error));
        NIMVLETS_CHECK(pet.id == "pet_a");
        NIMVLETS_CHECK(pet.idle.frames.size() == 1);
        NIMVLETS_CHECK(pet.idle.frames[0].pixels[0] == 0x0A);

        NIMVLETS_CHECK(LoadPetForIdentity(catalog, identityB, pet, error));
        NIMVLETS_CHECK(pet.id == "pet_b");
        NIMVLETS_CHECK(pet.idle.frames.size() == 1);
        NIMVLETS_CHECK(pet.idle.frames[0].pixels[0] == 0x0B);
    }
    return true;
}

// Refleja exactamente lo que SpikeApp::TrySwitchActivePet hará: solo
// tras una carga exitosa se actualizan appState_.activePetId/
// activeVariantId y se marca el scheduler como dirty; un intento
// fallido no debe tocar ninguno de los dos.
bool TestPersistenceMarkedDirtyOnlyAfterSuccessfulSwitch() {
    TempTestDirectory dir;
    const PetCatalog catalog = BuildTwoPetCatalog(dir.path());

    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(LoadPetForIdentity(catalog, PetIdentity{"pet_a", ""}, pet, error));

    AppState appState;
    appState.activePetId = "pet_a";
    PersistenceScheduler scheduler(2000.0);
    NIMVLETS_CHECK(!scheduler.IsDirty());

    // Intento fallido: identidad inexistente.
    const bool failedSwitch = LoadPetForIdentity(catalog, PetIdentity{"pet_c", ""}, pet, error);
    NIMVLETS_CHECK(!failedSwitch);
    // Mirroring SpikeApp: en un fallo, ni appState ni el scheduler se tocan.
    NIMVLETS_CHECK(appState.activePetId == "pet_a");
    NIMVLETS_CHECK(!scheduler.IsDirty());

    // Intento exitoso: switch real a "pet_b".
    const bool successfulSwitch = LoadPetForIdentity(catalog, PetIdentity{"pet_b", ""}, pet, error);
    NIMVLETS_CHECK(successfulSwitch);
    // Mirroring SpikeApp: recién ahora se actualiza appState y se marca dirty.
    appState.activePetId = "pet_b";
    appState.activeVariantId = "";
    scheduler.MarkDirty(0.0);
    NIMVLETS_CHECK(appState.activePetId == "pet_b");
    NIMVLETS_CHECK(scheduler.IsDirty());
    return true;
}

// "switching Bunny -> Nidir -> Bunny" (block brief 04.2 §9): un pack
// no direccional (forma Bunny) y uno direccional (forma Nidir) pueden
// alternarse libremente en cualquier orden -- ninguno deja
// idleDirectionOverrides del otro filtrándose, y cada carga nueva
// resuelve kRight/kLeft correctamente contra SU PROPIO contenido.
bool TestSwitchingBetweenNonDirectionalAndDirectionalPetsRoundTrips() {
    TempTestDirectory dir;
    WritePackFile(dir.path(), "bunny_like.nvpack", BuildMinimalPackBytes("bunny_like", 0xB0));
    WritePackFile(dir.path(), "nidir_like.nvpack", BuildDirectionalPackBytes("nidir_like", 0xD0, 0xD1));

    std::vector<CatalogEntry> entries;
    CatalogEntry bunnyLike;
    bunnyLike.identity = PetIdentity{"bunny_like", ""};
    bunnyLike.packPath = (std::filesystem::path(dir.path()) / "bunny_like.nvpack").string();
    bunnyLike.isDefault = true;
    entries.push_back(bunnyLike);
    CatalogEntry nidirLike;
    nidirLike.identity = PetIdentity{"nidir_like", ""};
    nidirLike.packPath = (std::filesystem::path(dir.path()) / "nidir_like.nvpack").string();
    nidirLike.isDefault = false;
    entries.push_back(nidirLike);
    const PetCatalog catalog(std::move(entries));

    PetDefinition pet;
    std::string error;

    // Bunny -> Nidir
    NIMVLETS_CHECK(LoadPetForIdentity(catalog, PetIdentity{"bunny_like", ""}, pet, error));
    NIMVLETS_CHECK(pet.idleDirectionOverrides.empty());  // forma Bunny: nunca tuvo overrides
    NIMVLETS_CHECK(LoadPetForIdentity(catalog, PetIdentity{"nidir_like", ""}, pet, error));
    NIMVLETS_CHECK(pet.idleDirectionOverrides.size() == 1);
    NIMVLETS_CHECK(pet.idle.frames[0].pixels[0] == 0xD0);  // kRight canónico
    NIMVLETS_CHECK(pet.idleDirectionOverrides[0].animation.frames[0].pixels[0] == 0xD1);  // kLeft

    // Nidir -> Bunny: los overrides de Nidir no deben sobrevivir al switch.
    NIMVLETS_CHECK(LoadPetForIdentity(catalog, PetIdentity{"bunny_like", ""}, pet, error));
    NIMVLETS_CHECK(pet.idleDirectionOverrides.empty());
    NIMVLETS_CHECK(pet.idle.frames[0].pixels[0] == 0xB0);

    return true;
}

}  // namespace

void RegisterPetSwitchingTests(testing::TestRunner& runner) {
    runner.Add("PetSwitching/SuccessfulSwitchUpdatesActiveIdentity", TestSuccessfulSwitchUpdatesActiveIdentity);
    runner.Add("PetSwitching/FailedSwitchToUnknownIdentityPreservesPriorPet", TestFailedSwitchToUnknownIdentityPreservesPriorPet);
    runner.Add("PetSwitching/FailedSwitchWithMissingPackFilePreservesPriorPet", TestFailedSwitchWithMissingPackFilePreservesPriorPet);
    runner.Add("PetSwitching/RepeatedSwitchingDoesNotAccumulateLoadedPetState", TestRepeatedSwitchingDoesNotAccumulateLoadedPetState);
    runner.Add("PetSwitching/PersistenceMarkedDirtyOnlyAfterSuccessfulSwitch", TestPersistenceMarkedDirtyOnlyAfterSuccessfulSwitch);
    runner.Add("PetSwitching/SwitchingBetweenNonDirectionalAndDirectionalPetsRoundTrips", TestSwitchingBetweenNonDirectionalAndDirectionalPetsRoundTrips);
}

}  // namespace nimvlets::tests
