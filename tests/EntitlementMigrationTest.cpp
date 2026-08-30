#include "EntitlementMigrationTest.h"

#include "catalog/CollectionModel.h"
#include "catalog/PetCatalog.h"
#include "catalog/PetEntitlement.h"
#include "persistence/AppState.h"
#include "persistence/AppStateSerializer.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// Prueba END-TO-END de la corrección de migración de propiedad legacy
// (DEC-128): un `ownedPetIds` "frin" de un save v1/v2/v3 debe terminar,
// en el estado ACTUAL, como `{frin, "male"} + {frin, "female"}` — las
// variantes que Block 06 realmente exponía — y NUNCA como `{frin, ""}`
// ("todo Frin", incluidas variantes futuras).
//
// El camino real es: DeserializeAppState (parseo provisional a
// `{frin, ""}`, sin catálogo) -> src/app corre
// ExpandHistoricalWholePetEntitlements contra el catálogo. Este test
// ejercita las dos etapas.

using nimvlets::catalog::BuildCollectionModel;
using nimvlets::catalog::CanActivate;
using nimvlets::catalog::CatalogEntry;
using nimvlets::catalog::CollectionModel;
using nimvlets::catalog::ExpandHistoricalWholePetEntitlements;
using nimvlets::catalog::OwnsIdentity;
using nimvlets::catalog::PetCatalog;
using nimvlets::catalog::PetEntitlement;
using nimvlets::catalog::PetIdentity;
using nimvlets::catalog::SeedEntitlementsFromCatalog;
using nimvlets::persistence::AppState;
using nimvlets::persistence::DeserializeAppState;
using nimvlets::persistence::OwnedEntitlement;

namespace nimvlets::tests {

namespace {

using Ents = std::vector<PetEntitlement>;

PetEntitlement NoVar(const std::string& p) { return PetEntitlement{p, ""}; }
PetEntitlement Var(const std::string& p, const std::string& v) { return PetEntitlement{p, v}; }

// El catálogo de dev: Bunny (sin variante, default), Nidir (sin
// variante), Frin male/female. Es el catálogo "al momento de la
// migración" — solo conoce macho y hembra.
PetCatalog MakeDevCatalog() {
    std::vector<CatalogEntry> e;
    CatalogEntry bunny;
    bunny.identity = PetIdentity{"bunny", ""};
    bunny.displayName = "Bunny";
    bunny.packPath = "b.nvpack";
    bunny.isDefault = true;
    bunny.initiallyOwned = true;
    e.push_back(bunny);
    CatalogEntry nidir;
    nidir.identity = PetIdentity{"nidir", ""};
    nidir.displayName = "Nidir";
    nidir.packPath = "n.nvpack";
    e.push_back(nidir);
    CatalogEntry fm;
    fm.identity = PetIdentity{"frin", "male"};
    fm.displayName = "Frin";
    fm.packPath = "fm.nvpack";
    fm.initiallyOwned = true;
    e.push_back(fm);
    CatalogEntry ff;
    ff.identity = PetIdentity{"frin", "female"};
    ff.displayName = "Frin";
    ff.packPath = "ff.nvpack";
    ff.initiallyOwned = true;
    e.push_back(ff);
    return PetCatalog(std::move(e));
}

// --- Constructores de buffers "NVSTATE1" legacy (mismo layout que
//     docs/PERSISTENCE.md §3) -----------------------------------------

void PutU8(std::vector<std::uint8_t>& b, std::uint8_t v) { b.push_back(v); }
void PutU32(std::vector<std::uint8_t>& b, std::uint32_t v) {
    std::uint8_t x[4];
    std::memcpy(x, &v, 4);
    b.insert(b.end(), x, x + 4);
}
void PutU64(std::vector<std::uint8_t>& b, std::uint64_t v) {
    std::uint8_t x[8];
    std::memcpy(x, &v, 8);
    b.insert(b.end(), x, x + 8);
}
void PutStr(std::vector<std::uint8_t>& b, const std::string& s) {
    PutU32(b, static_cast<std::uint32_t>(s.size()));
    b.insert(b.end(), s.begin(), s.end());
}

// v1: magic + version(1) + balance + activePetId + activeVariantId +
//     hasPos + posX + posY. Sin bloque de propiedad.
std::vector<std::uint8_t> BuildV1(const std::string& activePetId, const std::string& activeVariantId) {
    std::vector<std::uint8_t> b;
    const char magic[8] = {'N', 'V', 'S', 'T', 'A', 'T', 'E', '1'};
    b.insert(b.end(), magic, magic + 8);
    PutU32(b, 1);
    PutU64(b, 100);
    PutStr(b, activePetId);
    PutStr(b, activeVariantId);
    PutU8(b, 0);
    PutU32(b, 0);
    PutU32(b, 0);
    return b;
}

// v2/v3: v1 + ownershipSeeded + ownedPetIdCount + ownedPetIds[] +
//        lockPosition + sizeChoice + opacityPercent (+ language en v3).
std::vector<std::uint8_t> BuildLegacyOwned(
    std::uint32_t schemaVersion, const std::string& activePetId,
    const std::vector<std::string>& ownedPetIds, const std::string& language) {
    std::vector<std::uint8_t> b = BuildV1(activePetId, "");
    // reemplazar la versión (bytes 8..11) por schemaVersion
    std::memcpy(b.data() + 8, &schemaVersion, 4);
    PutU8(b, 1);  // ownershipSeeded
    PutU32(b, static_cast<std::uint32_t>(ownedPetIds.size()));
    for (const std::string& id : ownedPetIds) {
        PutStr(b, id);
    }
    PutU8(b, 0);          // lockPosition
    PutStr(b, "medium");  // sizeChoice
    PutU32(b, 100);       // opacityPercent
    if (schemaVersion >= 3) {
        PutStr(b, language);
    }
    return b;
}

Ents DecodeToEntitlements(const std::vector<std::uint8_t>& buf) {
    AppState st;
    std::string err;
    if (!DeserializeAppState(buf.data(), buf.size(), st, err)) {
        return {};
    }
    Ents out;
    for (const OwnedEntitlement& e : st.ownedEntitlements) {
        out.push_back(PetEntitlement{e.petId, e.variantId});
    }
    return out;
}

// El resultado ACTUAL de migrar un save legacy: parseo + expansión
// contra el catálogo.
Ents MigrateLegacy(const std::vector<std::uint8_t>& buf, const PetCatalog& catalog) {
    Ents ents = DecodeToEntitlements(buf);
    ExpandHistoricalWholePetEntitlements(ents, catalog);
    return ents;
}

bool HasBareFrin(const Ents& ents) {
    for (const auto& e : ents) {
        if (e.petId == "frin" && e.variantId.empty()) {
            return true;
        }
    }
    return false;
}

// --- v1: Frin legacy llega por la SIEMBRA (v1 no tiene propiedad) ---

bool TestV1LegacyFrinBecomesExplicitVariants() {
    const PetCatalog catalog = MakeDevCatalog();
    const Ents parsed = DecodeToEntitlements(BuildV1("frin", "male"));
    NIMVLETS_CHECK(parsed.empty());  // v1 no persiste propiedad

    // src/app siembra en ese caso (ownershipSeeded llega false para v1).
    const Ents seed = SeedEntitlementsFromCatalog(catalog);
    NIMVLETS_CHECK((seed == Ents{NoVar("bunny"), Var("frin", "female"), Var("frin", "male")}));
    NIMVLETS_CHECK(!HasBareFrin(seed));
    return true;
}

// --- v2 / v3: Frin legacy se EXPANDE a male + female ---------------

bool TestV2LegacyFrinExpandsToMaleAndFemale() {
    const PetCatalog catalog = MakeDevCatalog();
    const Ents migrated = MigrateLegacy(BuildLegacyOwned(2, "nidir", {"bunny", "frin"}, ""), catalog);
    NIMVLETS_CHECK((migrated == Ents{NoVar("bunny"), Var("frin", "female"), Var("frin", "male")}));
    NIMVLETS_CHECK(!HasBareFrin(migrated));
    return true;
}

bool TestV3LegacyFrinExpandsToMaleAndFemale() {
    const PetCatalog catalog = MakeDevCatalog();
    const Ents migrated =
        MigrateLegacy(BuildLegacyOwned(3, "bunny", {"frin", "bunny"}, "es"), catalog);
    NIMVLETS_CHECK((migrated == Ents{NoVar("bunny"), Var("frin", "female"), Var("frin", "male")}));
    NIMVLETS_CHECK(!HasBareFrin(migrated));
    return true;
}

// El resultado actual NUNCA contiene {frin, ""}, en ninguna versión
// legacy.
bool TestMigrationNeverProducesBareWholeFrin() {
    const PetCatalog catalog = MakeDevCatalog();
    NIMVLETS_CHECK(!HasBareFrin(MigrateLegacy(BuildLegacyOwned(2, "frin", {"frin"}, ""), catalog)));
    NIMVLETS_CHECK(!HasBareFrin(MigrateLegacy(BuildLegacyOwned(3, "frin", {"frin"}, "en"), catalog)));
    NIMVLETS_CHECK(!HasBareFrin(SeedEntitlementsFromCatalog(catalog)));
    return true;
}

// Un owner migrado activa Macho Y Hembra.
bool TestMigratedOwnerActivatesBothVariants() {
    const PetCatalog catalog = MakeDevCatalog();
    const Ents migrated = MigrateLegacy(BuildLegacyOwned(3, "frin", {"bunny", "frin"}, "en"), catalog);

    const CollectionModel modelMale = BuildCollectionModel(catalog, migrated, PetIdentity{"frin", "male"});
    NIMVLETS_CHECK(CanActivate(modelMale, "frin", "male"));
    NIMVLETS_CHECK(CanActivate(modelMale, "frin", "female"));

    const CollectionModel modelFemale =
        BuildCollectionModel(catalog, migrated, PetIdentity{"frin", "female"});
    NIMVLETS_CHECK(CanActivate(modelFemale, "frin", "female"));
    NIMVLETS_CHECK(CanActivate(modelFemale, "frin", "male"));
    return true;
}

// Una TERCERA variante de Frin hipotética (agregada al catálogo después
// de la migración) NO queda cubierta por el conjunto migrado.
bool TestMigratedFrinDoesNotOwnAFutureThirdVariant() {
    const PetCatalog catalogAtMigration = MakeDevCatalog();  // male/female
    const Ents migrated =
        MigrateLegacy(BuildLegacyOwned(2, "frin", {"frin"}, ""), catalogAtMigration);
    NIMVLETS_CHECK((migrated == Ents{Var("frin", "female"), Var("frin", "male")}));

    // Bloque futuro agrega frin/spirit. El conjunto migrado NO lo cubre.
    NIMVLETS_CHECK(!OwnsIdentity(migrated, PetIdentity{"frin", "spirit"}));
    // (Y tampoco un {frin, ""} — que ni existe en el conjunto.)
    NIMVLETS_CHECK(!OwnsIdentity(migrated, PetIdentity{"frin", ""}));
    return true;
}

// Un save v4 LIMPIO (ya con variantes explícitas) no se toca al re-abrir.
bool TestCleanV4IsIdempotentUnderExpansion() {
    const PetCatalog catalog = MakeDevCatalog();
    Ents ents = {NoVar("bunny"), Var("frin", "female"), Var("frin", "male")};
    NIMVLETS_CHECK(!ExpandHistoricalWholePetEntitlements(ents, catalog));
    NIMVLETS_CHECK((ents == Ents{NoVar("bunny"), Var("frin", "female"), Var("frin", "male")}));
    return true;
}

}  // namespace

void RegisterEntitlementMigrationTests(testing::TestRunner& runner) {
    runner.Add("EntitlementMigration/V1LegacyFrinBecomesExplicitVariants", TestV1LegacyFrinBecomesExplicitVariants);
    runner.Add("EntitlementMigration/V2LegacyFrinExpandsToMaleAndFemale", TestV2LegacyFrinExpandsToMaleAndFemale);
    runner.Add("EntitlementMigration/V3LegacyFrinExpandsToMaleAndFemale", TestV3LegacyFrinExpandsToMaleAndFemale);
    runner.Add("EntitlementMigration/MigrationNeverProducesBareWholeFrin", TestMigrationNeverProducesBareWholeFrin);
    runner.Add("EntitlementMigration/MigratedOwnerActivatesBothVariants", TestMigratedOwnerActivatesBothVariants);
    runner.Add("EntitlementMigration/MigratedFrinDoesNotOwnAFutureThirdVariant",
               TestMigratedFrinDoesNotOwnAFutureThirdVariant);
    runner.Add("EntitlementMigration/CleanV4IsIdempotentUnderExpansion", TestCleanV4IsIdempotentUnderExpansion);
}

}  // namespace nimvlets::tests
