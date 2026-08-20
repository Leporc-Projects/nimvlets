#include "PetSwitchingTest.h"

#include "NvPack2TestBuilder.h"
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
// packs "NVPACK2" sintéticos reales en disco (no solo en memoria),
// para probar el camino completo catálogo -> content::LoadPetPackFromFile
// que src/app/SpikeApp usa de verdad al hacer switching. La parte de
// persistencia (marcar dirty solo tras un cambio exitoso) se ejercita
// conectando persistence::AppState + PersistenceScheduler de la misma
// forma que tests/PersistenceIntegrationTest.cpp hizo para click/drag.

using nimvlets::catalog::CatalogEntry;
using nimvlets::catalog::LoadPetForIdentity;
using nimvlets::catalog::PetCatalog;
using nimvlets::catalog::PetIdentity;
using nimvlets::content::PetDefinition;
using nimvlets::persistence::AppState;
using nimvlets::persistence::PersistenceScheduler;

namespace nimvlets::tests {

namespace {

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

std::string WritePackFile(const std::string& dir, const std::string& fileName, const std::vector<std::uint8_t>& bytes) {
    const std::filesystem::path path = std::filesystem::path(dir) / fileName;
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return path.string();
}

// Catálogo de dos entradas reales en disco: "pet_a" (default) y
// "pet_b", cada una con un pack sintético distinto en `dir`.
PetCatalog BuildTwoPetCatalog(const std::string& dir) {
    WritePackFile(dir, "pet_a.nvpack", nvpack2::BuildMinimalPackBytes("pet_a", 0x0A));
    WritePackFile(dir, "pet_b.nvpack", nvpack2::BuildMinimalPackBytes("pet_b", 0x0B));

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
    NIMVLETS_CHECK(pet.states[0].baseAnimation.frames[0].pixels[0] == 0x0A);

    NIMVLETS_CHECK(LoadPetForIdentity(catalog, PetIdentity{"pet_b", ""}, pet, error));
    NIMVLETS_CHECK(pet.id == "pet_b");
    NIMVLETS_CHECK(pet.states[0].baseAnimation.frames[0].pixels[0] == 0x0B);
    return true;
}

bool TestFailedSwitchToUnknownIdentityPreservesPriorPet() {
    TempTestDirectory dir;
    const PetCatalog catalog = BuildTwoPetCatalog(dir.path());

    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(LoadPetForIdentity(catalog, PetIdentity{"pet_a", ""}, pet, error));
    NIMVLETS_CHECK(pet.id == "pet_a");

    const bool switched = LoadPetForIdentity(catalog, PetIdentity{"pet_c", ""}, pet, error);
    NIMVLETS_CHECK(!switched);
    NIMVLETS_CHECK(!error.empty());
    NIMVLETS_CHECK(pet.id == "pet_a");  // sin cambios -- el pet activo anterior sigue usable
    return true;
}

bool TestFailedSwitchWithMissingPackFilePreservesPriorPet() {
    TempTestDirectory dir;

    WritePackFile(dir.path(), "pet_a.nvpack", nvpack2::BuildMinimalPackBytes("pet_a", 0x0A));
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
        NIMVLETS_CHECK(pet.states[0].baseAnimation.frames.size() == 1);
        NIMVLETS_CHECK(pet.states[0].baseAnimation.frames[0].pixels[0] == 0x0A);

        NIMVLETS_CHECK(LoadPetForIdentity(catalog, identityB, pet, error));
        NIMVLETS_CHECK(pet.id == "pet_b");
        NIMVLETS_CHECK(pet.states[0].baseAnimation.frames.size() == 1);
        NIMVLETS_CHECK(pet.states[0].baseAnimation.frames[0].pixels[0] == 0x0B);
    }
    return true;
}

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

    const bool failedSwitch = LoadPetForIdentity(catalog, PetIdentity{"pet_c", ""}, pet, error);
    NIMVLETS_CHECK(!failedSwitch);
    NIMVLETS_CHECK(appState.activePetId == "pet_a");
    NIMVLETS_CHECK(!scheduler.IsDirty());

    const bool successfulSwitch = LoadPetForIdentity(catalog, PetIdentity{"pet_b", ""}, pet, error);
    NIMVLETS_CHECK(successfulSwitch);
    appState.activePetId = "pet_b";
    appState.activeVariantId = "";
    scheduler.MarkDirty(0.0);
    NIMVLETS_CHECK(appState.activePetId == "pet_b");
    NIMVLETS_CHECK(scheduler.IsDirty());
    return true;
}

// Un pack no direccional (forma Bunny) y uno direccional (forma Nidir)
// pueden alternarse libremente en cualquier orden -- ninguno deja
// baseAnimationDirectionOverrides del otro filtrándose, y cada carga
// nueva resuelve kRight/kLeft correctamente contra SU PROPIO contenido.
bool TestSwitchingBetweenNonDirectionalAndDirectionalPetsRoundTrips() {
    TempTestDirectory dir;
    WritePackFile(dir.path(), "bunny_like.nvpack", nvpack2::BuildMinimalPackBytes("bunny_like", 0xB0));
    WritePackFile(dir.path(), "nidir_like.nvpack", nvpack2::BuildDirectionalPackBytes("nidir_like", 0xD0, 0xD1));

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
    NIMVLETS_CHECK(pet.states[0].baseAnimationDirectionOverrides.empty());  // forma Bunny: nunca tuvo overrides
    NIMVLETS_CHECK(LoadPetForIdentity(catalog, PetIdentity{"nidir_like", ""}, pet, error));
    NIMVLETS_CHECK(pet.states[0].baseAnimationDirectionOverrides.size() == 1);
    NIMVLETS_CHECK(pet.states[0].baseAnimation.frames[0].pixels[0] == 0xD0);  // kRight canónico
    NIMVLETS_CHECK(pet.states[0].baseAnimationDirectionOverrides[0].animation.frames[0].pixels[0] == 0xD1);  // kLeft

    // Nidir -> Bunny: los overrides de Nidir no deben sobrevivir al switch.
    NIMVLETS_CHECK(LoadPetForIdentity(catalog, PetIdentity{"bunny_like", ""}, pet, error));
    NIMVLETS_CHECK(pet.states[0].baseAnimationDirectionOverrides.empty());
    NIMVLETS_CHECK(pet.states[0].baseAnimation.frames[0].pixels[0] == 0xB0);

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
