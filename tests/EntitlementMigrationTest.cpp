#include "EntitlementMigrationTest.h"

#include "catalog/CollectionModel.h"
#include "catalog/PetCatalog.h"
#include "catalog/PetEntitlement.h"
#include "persistence/AppState.h"
#include "persistence/AppStateSerializer.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

// Prueba END-TO-END de la migración de propiedad legacy (DEC-128,
// afinada por DEC-129): un `ownedPetIds` "frin" de un save v1/v2/v3 debe
// terminar, en el estado ACTUAL, como `{frin, "male"} + {frin, "female"}`
// — las variantes que Block 06 realmente exponía — y NUNCA como
// `{frin, ""}` ("todo Frin", incluidas variantes futuras) ni con una
// variante agregada al catálogo DESPUÉS del schema v3.
//
// El camino real (SpikeApp::Init) es:
//   DeserializeAppState  -> parseo provisional de `ownedPetIds` a
//                           `{petId, ""}` (sin catálogo) + la versión
//                           EN DISCO por out-param;
//   src/app              -> SOLO si esa versión < la actual, corre
//                           ExpandHistoricalWholePetEntitlements, que
//                           usa una tabla histórica CONGELADA (no mira
//                           el catálogo actual).
// MigrateThroughAppPath() de abajo reproduce exactamente esas dos
// etapas, gate incluido.

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
// variante), Frin male/female.
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

// El caso del brief §4 / DEC-129: un catálogo ACTUAL que YA contiene una
// TERCERA variante de Frin (`spirit`) — agregada por un bloque futuro,
// nunca parte del modelo de propiedad por-pet-lógico de v1..v3. Existe
// ANTES de que se migre el save legacy. Migrar "poseer frin" NO debe
// otorgar spirit.
PetCatalog MakeCatalogWithSpiritVariant() {
    std::vector<CatalogEntry> e;
    CatalogEntry bunny;
    bunny.identity = PetIdentity{"bunny", ""};
    bunny.displayName = "Bunny";
    bunny.packPath = "b.nvpack";
    bunny.isDefault = true;
    bunny.initiallyOwned = true;
    e.push_back(bunny);
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
    CatalogEntry fs;
    fs.identity = PetIdentity{"frin", "spirit"};
    fs.displayName = "Frin";
    fs.packPath = "fs.nvpack";
    fs.initiallyOwned = false;  // variante futura, no parte del modelo viejo
    e.push_back(fs);
    return PetCatalog(std::move(e));
}

// --- Constructores de buffers "NVSTATE1" (mismo layout que
//     docs/PERSISTENCE.md §3 y SerializeAppState) --------------------

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

// v4 (Block 07): cuerpo v1 + ownershipSeeded + count + pares
// (petId, variantId) + lock/size/opacity + language. Mismo layout que
// SerializeAppState. Sirve para fabricar un save v4 "actual" (o uno
// editado a mano) y probar que el camino de carga NO le corre la
// expansión histórica.
std::vector<std::uint8_t> BuildV4(
    const std::string& activePetId, const std::string& activeVariantId,
    const std::vector<std::pair<std::string, std::string>>& ownedPairs,
    const std::string& language) {
    std::vector<std::uint8_t> b;
    const char magic[8] = {'N', 'V', 'S', 'T', 'A', 'T', 'E', '1'};
    b.insert(b.end(), magic, magic + 8);
    PutU32(b, 4);
    PutU64(b, 100);
    PutStr(b, activePetId);
    PutStr(b, activeVariantId);
    PutU8(b, 0);   // hasPosition
    PutU32(b, 0);  // posX
    PutU32(b, 0);  // posY
    PutU8(b, 1);   // ownershipSeeded
    PutU32(b, static_cast<std::uint32_t>(ownedPairs.size()));
    for (const auto& p : ownedPairs) {
        PutStr(b, p.first);
        PutStr(b, p.second);
    }
    PutU8(b, 0);          // lockPosition
    PutStr(b, "medium");  // sizeChoice
    PutU32(b, 100);       // opacityPercent
    PutStr(b, language);
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

// Reproduce EXACTAMENTE lo que hace src/app (SpikeApp::Init) al cargar
// un save: deserializa, lee la versión EN DISCO por out-param, y SOLO
// corre la expansión histórica cuando ese save vino GENUINAMENTE de un
// schema legacy (< actual). Sobre un v4 no toca nada — igual que la app.
// El catálogo no participa: la expansión ya no lo recibe.
Ents MigrateThroughAppPath(const std::vector<std::uint8_t>& buf) {
    AppState st;
    std::string err;
    std::uint32_t onDiskVer = AppState::kCurrentSchemaVersion;
    if (!DeserializeAppState(buf.data(), buf.size(), st, err, &onDiskVer)) {
        return {};
    }
    Ents ents;
    for (const OwnedEntitlement& e : st.ownedEntitlements) {
        ents.push_back(PetEntitlement{e.petId, e.variantId});
    }
    if (onDiskVer < AppState::kCurrentSchemaVersion) {
        ExpandHistoricalWholePetEntitlements(ents);
    }
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
    const Ents migrated = MigrateThroughAppPath(BuildLegacyOwned(2, "nidir", {"bunny", "frin"}, ""));
    NIMVLETS_CHECK((migrated == Ents{NoVar("bunny"), Var("frin", "female"), Var("frin", "male")}));
    NIMVLETS_CHECK(!HasBareFrin(migrated));
    return true;
}

bool TestV3LegacyFrinExpandsToMaleAndFemale() {
    const Ents migrated =
        MigrateThroughAppPath(BuildLegacyOwned(3, "bunny", {"frin", "bunny"}, "es"));
    NIMVLETS_CHECK((migrated == Ents{NoVar("bunny"), Var("frin", "female"), Var("frin", "male")}));
    NIMVLETS_CHECK(!HasBareFrin(migrated));
    return true;
}

// Un petId legacy SIN variantes (bunny, nidir) mapea natural a
// `{petId, ""}` — la expansión histórica no lo toca.
bool TestNonVariantLegacyOwnershipMapsToBarePairs() {
    for (std::uint32_t v : {2u, 3u}) {
        const Ents migrated =
            MigrateThroughAppPath(BuildLegacyOwned(v, "bunny", {"nidir", "bunny"}, "en"));
        NIMVLETS_CHECK((migrated == Ents{NoVar("bunny"), NoVar("nidir")}));
        NIMVLETS_CHECK(!HasBareFrin(migrated));
    }
    return true;
}

// El resultado actual NUNCA contiene {frin, ""}, en ninguna versión
// legacy.
bool TestMigrationNeverProducesBareWholeFrin() {
    const PetCatalog catalog = MakeDevCatalog();
    NIMVLETS_CHECK(!HasBareFrin(MigrateThroughAppPath(BuildLegacyOwned(2, "frin", {"frin"}, ""))));
    NIMVLETS_CHECK(!HasBareFrin(MigrateThroughAppPath(BuildLegacyOwned(3, "frin", {"frin"}, "en"))));
    NIMVLETS_CHECK(!HasBareFrin(SeedEntitlementsFromCatalog(catalog)));
    return true;
}

// Un owner migrado activa Macho Y Hembra.
bool TestMigratedOwnerActivatesBothVariants() {
    const PetCatalog catalog = MakeDevCatalog();
    const Ents migrated = MigrateThroughAppPath(BuildLegacyOwned(3, "frin", {"bunny", "frin"}, "en"));

    const CollectionModel modelMale = BuildCollectionModel(catalog, migrated, PetIdentity{"frin", "male"});
    NIMVLETS_CHECK(CanActivate(modelMale, "frin", "male"));
    NIMVLETS_CHECK(CanActivate(modelMale, "frin", "female"));

    const CollectionModel modelFemale =
        BuildCollectionModel(catalog, migrated, PetIdentity{"frin", "female"});
    NIMVLETS_CHECK(CanActivate(modelFemale, "frin", "female"));
    NIMVLETS_CHECK(CanActivate(modelFemale, "frin", "male"));
    return true;
}

// Caso "catálogo al momento de migrar solo conocía male/female": una
// tercera variante que aparece DESPUÉS no queda cubierta.
bool TestMigratedFrinDoesNotOwnAThirdVariantAddedLater() {
    const Ents migrated = MigrateThroughAppPath(BuildLegacyOwned(2, "frin", {"frin"}, ""));
    NIMVLETS_CHECK((migrated == Ents{Var("frin", "female"), Var("frin", "male")}));
    NIMVLETS_CHECK(!OwnsIdentity(migrated, PetIdentity{"frin", "spirit"}));
    NIMVLETS_CHECK(!OwnsIdentity(migrated, PetIdentity{"frin", ""}));
    return true;
}

// *** El test clave del brief §4 ***
// La variante futura (frin/spirit) YA EXISTE en el catálogo ACTUAL ANTES
// de que se migre el estado legacy. Se migra un save v2/v3 real con
// `ownedPetIds = ["frin"]` por el MISMO camino que usa la app. Resultado
// exigido: posee male + female, NO posee spirit, NO posee {frin, ""}.
bool TestFutureVariantAlreadyInCatalogBeforeMigrationStaysUnowned() {
    const PetCatalog currentCatalog = MakeCatalogWithSpiritVariant();  // male, female, spirit

    for (std::uint32_t schema : {2u, 3u}) {
        const Ents migrated =
            MigrateThroughAppPath(BuildLegacyOwned(schema, "frin", {"frin"}, "es"));

        NIMVLETS_CHECK(OwnsIdentity(migrated, PetIdentity{"frin", "male"}));
        NIMVLETS_CHECK(OwnsIdentity(migrated, PetIdentity{"frin", "female"}));
        NIMVLETS_CHECK(!OwnsIdentity(migrated, PetIdentity{"frin", "spirit"}));
        NIMVLETS_CHECK(!OwnsIdentity(migrated, PetIdentity{"frin", ""}));
        NIMVLETS_CHECK((migrated == Ents{Var("frin", "female"), Var("frin", "male")}));

        // Y a través del modelo construido contra el catálogo que YA
        // contiene spirit: Macho y Hembra activables, Spirit NO.
        const CollectionModel model =
            BuildCollectionModel(currentCatalog, migrated, PetIdentity{"frin", "male"});
        NIMVLETS_CHECK(CanActivate(model, "frin", "male"));
        NIMVLETS_CHECK(CanActivate(model, "frin", "female"));
        NIMVLETS_CHECK(!CanActivate(model, "frin", "spirit"));
        const auto* frin = model.Find("frin");
        NIMVLETS_CHECK(frin != nullptr);
        NIMVLETS_CHECK(frin->VariantOwned("male"));
        NIMVLETS_CHECK(frin->VariantOwned("female"));
        NIMVLETS_CHECK(!frin->VariantOwned("spirit"));
        NIMVLETS_CHECK(!frin->AllVariantsOwned());
    }
    return true;
}

// --- Frontera de migración (brief §5) -----------------------------
// Un save v4 con un `{frin, ""}` suelto (editado a mano / corrupto) NO
// se expande al cargarlo: la versión EN DISCO es 4, así que el gate de
// src/app no corre la reconciliación histórica. `{frin, ""}` no cubre
// ninguna identidad real -> NO se fabrica propiedad.
bool TestCurrentV4BareWholeFrinIsNotExpanded() {
    const std::vector<std::uint8_t> buf =
        BuildV4("bunny", "", {{"bunny", ""}, {"frin", ""}}, "en");
    const Ents loaded = MigrateThroughAppPath(buf);

    // Queda TAL CUAL vino: {bunny, ""} + {frin, ""}. Nada de male/female.
    NIMVLETS_CHECK((loaded == Ents{NoVar("bunny"), NoVar("frin")}));
    NIMVLETS_CHECK(!OwnsIdentity(loaded, PetIdentity{"frin", "male"}));
    NIMVLETS_CHECK(!OwnsIdentity(loaded, PetIdentity{"frin", "female"}));

    // Y no manufactura una activación: contra el catálogo dev, Frin no
    // tiene NINGUNA variante poseída -> no activable.
    const PetCatalog catalog = MakeDevCatalog();
    const CollectionModel model = BuildCollectionModel(catalog, loaded, PetIdentity{"bunny", ""});
    NIMVLETS_CHECK(!CanActivate(model, "frin", "male"));
    NIMVLETS_CHECK(!CanActivate(model, "frin", "female"));
    const auto* frin = model.Find("frin");
    NIMVLETS_CHECK(frin != nullptr);
    NIMVLETS_CHECK(!frin->VariantOwned("male"));
    NIMVLETS_CHECK(!frin->VariantOwned("female"));
    return true;
}

// Un save v4 LIMPIO (ya con variantes explícitas) pasa por el camino de
// carga sin cambiar ni ensancharse.
bool TestCleanV4ThroughAppPathIsUnchanged() {
    const std::vector<std::uint8_t> buf = BuildV4(
        "frin", "male", {{"bunny", ""}, {"frin", "male"}, {"frin", "female"}}, "en");
    const Ents loaded = MigrateThroughAppPath(buf);
    NIMVLETS_CHECK((loaded == Ents{NoVar("bunny"), Var("frin", "female"), Var("frin", "male")}));
    NIMVLETS_CHECK(loaded.size() == 3);  // no se agregó nada

    const PetCatalog catalog = MakeDevCatalog();
    const CollectionModel model = BuildCollectionModel(catalog, loaded, PetIdentity{"frin", "male"});
    NIMVLETS_CHECK(CanActivate(model, "frin", "male"));
    NIMVLETS_CHECK(CanActivate(model, "frin", "female"));
    return true;
}

// La expansión directa sobre un conjunto ya explícito es idempotente
// (sin catálogo — DEC-129).
bool TestCleanV4IsIdempotentUnderExpansion() {
    Ents ents = {NoVar("bunny"), Var("frin", "female"), Var("frin", "male")};
    NIMVLETS_CHECK(!ExpandHistoricalWholePetEntitlements(ents));
    NIMVLETS_CHECK((ents == Ents{NoVar("bunny"), Var("frin", "female"), Var("frin", "male")}));
    return true;
}

}  // namespace

void RegisterEntitlementMigrationTests(testing::TestRunner& runner) {
    runner.Add("EntitlementMigration/V1LegacyFrinBecomesExplicitVariants", TestV1LegacyFrinBecomesExplicitVariants);
    runner.Add("EntitlementMigration/V2LegacyFrinExpandsToMaleAndFemale", TestV2LegacyFrinExpandsToMaleAndFemale);
    runner.Add("EntitlementMigration/V3LegacyFrinExpandsToMaleAndFemale", TestV3LegacyFrinExpandsToMaleAndFemale);
    runner.Add("EntitlementMigration/NonVariantLegacyOwnershipMapsToBarePairs",
               TestNonVariantLegacyOwnershipMapsToBarePairs);
    runner.Add("EntitlementMigration/MigrationNeverProducesBareWholeFrin", TestMigrationNeverProducesBareWholeFrin);
    runner.Add("EntitlementMigration/MigratedOwnerActivatesBothVariants", TestMigratedOwnerActivatesBothVariants);
    runner.Add("EntitlementMigration/MigratedFrinDoesNotOwnAThirdVariantAddedLater",
               TestMigratedFrinDoesNotOwnAThirdVariantAddedLater);
    runner.Add("EntitlementMigration/FutureVariantAlreadyInCatalogBeforeMigrationStaysUnowned",
               TestFutureVariantAlreadyInCatalogBeforeMigrationStaysUnowned);
    runner.Add("EntitlementMigration/CurrentV4BareWholeFrinIsNotExpanded",
               TestCurrentV4BareWholeFrinIsNotExpanded);
    runner.Add("EntitlementMigration/CleanV4ThroughAppPathIsUnchanged", TestCleanV4ThroughAppPathIsUnchanged);
    runner.Add("EntitlementMigration/CleanV4IsIdempotentUnderExpansion", TestCleanV4IsIdempotentUnderExpansion);
}

}  // namespace nimvlets::tests
