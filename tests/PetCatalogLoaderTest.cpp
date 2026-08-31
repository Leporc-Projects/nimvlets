#include "PetCatalogLoaderTest.h"

#include "catalog/PetCatalogLoader.h"

#include <cstdint>
#include <string>
#include <vector>

using nimvlets::catalog::LoadCatalogFromMemory;
using nimvlets::catalog::PetCatalog;

namespace nimvlets::tests {

namespace {

// Construye a mano buffers de bytes "NVCATLG1" que calzan con el
// formato exacto (ver src/catalog/PetCatalogLoader.cpp y
// docs/CATALOG.md), la misma técnica que tests/PetPackLoaderTest.cpp
// usa para "NVPACK1" -- permite ejercitar el parser directamente
// contra datos sintéticos válidos *y* malformados, sin filesystem, sin
// el compilador Python, sin dependencia del directorio de trabajo.

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

void AppendUint64(std::vector<std::uint8_t>& buf, std::uint64_t v) {
    AppendBytes(buf, &v, sizeof(v));
}

void AppendString(std::vector<std::uint8_t>& buf, const std::string& s) {
    AppendUint32(buf, static_cast<std::uint32_t>(s.size()));
    AppendBytes(buf, s.data(), s.size());
}

void AppendMagic(std::vector<std::uint8_t>& buf) {
    const char magic[8] = {'N', 'V', 'C', 'A', 'T', 'L', 'G', '1'};
    AppendBytes(buf, magic, sizeof(magic));
}

// Schema actual del formato "NVCATLG1" — la pasada de endurecimiento de
// Block 09A lo subió a 5 (agrega `devSyntheticOnboarding` u8 a nivel de
// catálogo, tras `productionOnboardingReady`); Block 09A lo había subido
// a 4 (`productionOnboardingReady` u8 + `starterRole` u8 por entrada);
// Block 07 a 3 (`priceClicks` u64 + `publiclyPurchasable` u8), Block 06
// a 2 (`initiallyOwned` u8).
constexpr std::uint32_t kSchema = 5;

void AppendHeader(
    std::vector<std::uint8_t>& buf, std::uint32_t schemaVersion, std::uint32_t entryCount,
    bool productionOnboardingReady = false, bool devSyntheticOnboarding = false) {
    AppendMagic(buf);
    AppendUint32(buf, schemaVersion);
    AppendUint32(buf, entryCount);
    AppendUint8(buf, productionOnboardingReady ? 1 : 0);
    AppendUint8(buf, devSyntheticOnboarding ? 1 : 0);
}

void AppendEntry(
    std::vector<std::uint8_t>& buf,
    const std::string& petId,
    const std::string& variantId,
    const std::string& displayName,
    const std::string& packPath,
    bool isDefault,
    bool initiallyOwned = false,
    std::uint64_t priceClicks = 0,
    bool publiclyPurchasable = false,
    std::uint8_t starterRole = 0) {
    AppendString(buf, petId);
    AppendString(buf, variantId);
    AppendString(buf, displayName);
    AppendString(buf, packPath);
    AppendUint8(buf, isDefault ? 1 : 0);
    AppendUint8(buf, initiallyOwned ? 1 : 0);
    AppendUint64(buf, priceClicks);
    AppendUint8(buf, publiclyPurchasable ? 1 : 0);
    AppendUint8(buf, starterRole);
}

std::vector<std::uint8_t> BuildSingleEntryCatalog() {
    std::vector<std::uint8_t> buf;
    AppendHeader(buf, kSchema, 1);
    AppendEntry(buf, "bunny_dev", "", "Bunny (dev fixture)", "assets/dev/bunny_pack.nvpack", true, true);
    return buf;
}

bool TestValidSingleEntryCatalogLoadsSuccessfully() {
    const std::vector<std::uint8_t> bytes = BuildSingleEntryCatalog();
    PetCatalog catalog;
    std::string error;
    NIMVLETS_CHECK(LoadCatalogFromMemory(bytes.data(), bytes.size(), catalog, error));
    NIMVLETS_CHECK(error.empty());
    NIMVLETS_CHECK(catalog.Entries().size() == 1);
    NIMVLETS_CHECK(catalog.Entries()[0].identity.petId == "bunny_dev");
    NIMVLETS_CHECK(catalog.Entries()[0].isDefault);
    NIMVLETS_CHECK(&catalog.Default() == &catalog.Entries()[0]);
    return true;
}

// Dos entradas con el mismo petId pero variantId distinto (el caso
// Frin male/female) deben convivir sin conflicto -- no es un
// duplicado.
bool TestSamePetIdDifferentVariantsBothLoad() {
    std::vector<std::uint8_t> buf;
    AppendHeader(buf, kSchema, 2);
    AppendEntry(buf, "frin", "male", "Frin (male)", "assets/dev/frin_male.nvpack", true);
    AppendEntry(buf, "frin", "female", "Frin (female)", "assets/dev/frin_female.nvpack", false);

    PetCatalog catalog;
    std::string error;
    NIMVLETS_CHECK(LoadCatalogFromMemory(buf.data(), buf.size(), catalog, error));
    NIMVLETS_CHECK(catalog.Entries().size() == 2);
    NIMVLETS_CHECK(catalog.Entries()[0].identity.variantId == "male");
    NIMVLETS_CHECK(catalog.Entries()[1].identity.variantId == "female");
    return true;
}

// El byte `initiallyOwned` (schema v2, Block 06) hace round-trip por
// entrada, independiente de `isDefault`.
bool TestInitiallyOwnedFlagRoundTrips() {
    std::vector<std::uint8_t> buf;
    AppendHeader(buf, kSchema, 3);
    AppendEntry(buf, "owned_default", "", "A", "a.nvpack", /*isDefault=*/true, /*initiallyOwned=*/true);
    AppendEntry(buf, "locked", "", "B", "b.nvpack", /*isDefault=*/false, /*initiallyOwned=*/false);
    AppendEntry(buf, "owned_not_default", "", "C", "c.nvpack", /*isDefault=*/false, /*initiallyOwned=*/true);

    PetCatalog catalog;
    std::string error;
    NIMVLETS_CHECK(LoadCatalogFromMemory(buf.data(), buf.size(), catalog, error));
    NIMVLETS_CHECK(catalog.Entries().size() == 3);
    NIMVLETS_CHECK(catalog.Entries()[0].initiallyOwned);
    NIMVLETS_CHECK(!catalog.Entries()[1].initiallyOwned);
    NIMVLETS_CHECK(catalog.Entries()[2].initiallyOwned);
    NIMVLETS_CHECK(!catalog.Entries()[2].isDefault);
    return true;
}

// `priceClicks` + `publiclyPurchasable` (schema v3, Block 07) hacen
// round-trip por entrada.
bool TestShopMetadataRoundTrips() {
    std::vector<std::uint8_t> buf;
    AppendHeader(buf, kSchema, 3);
    AppendEntry(buf, "bunny", "", "Bunny", "b.nvpack", /*isDefault=*/true, /*initiallyOwned=*/true,
                /*priceClicks=*/120, /*publiclyPurchasable=*/true);
    AppendEntry(buf, "nidir", "", "Nidir", "n.nvpack", false, false, /*priceClicks=*/300,
                /*publiclyPurchasable=*/true);
    AppendEntry(buf, "frin", "male", "Frin", "f.nvpack", false, true, /*priceClicks=*/0,
                /*publiclyPurchasable=*/false);

    PetCatalog catalog;
    std::string error;
    NIMVLETS_CHECK(LoadCatalogFromMemory(buf.data(), buf.size(), catalog, error));
    NIMVLETS_CHECK(error.empty());
    NIMVLETS_CHECK(catalog.Entries()[0].priceClicks == 120);
    NIMVLETS_CHECK(catalog.Entries()[0].publiclyPurchasable);
    NIMVLETS_CHECK(catalog.Entries()[1].priceClicks == 300);
    NIMVLETS_CHECK(catalog.Entries()[1].publiclyPurchasable);
    NIMVLETS_CHECK(catalog.Entries()[2].priceClicks == 0);
    NIMVLETS_CHECK(!catalog.Entries()[2].publiclyPurchasable);
    return true;
}

// Una entrada pública con precio 0 nunca es válida (el loader la
// rechaza — brief §26): un ítem de Shop así sería inoperable.
bool TestPublicWithZeroPriceIsRejected() {
    std::vector<std::uint8_t> buf;
    AppendHeader(buf, kSchema, 1);
    AppendEntry(buf, "p", "", "P", "p.nvpack", /*isDefault=*/true, /*initiallyOwned=*/false,
                /*priceClicks=*/0, /*publiclyPurchasable=*/true);
    PetCatalog catalog;
    std::string error;
    NIMVLETS_CHECK(!LoadCatalogFromMemory(buf.data(), buf.size(), catalog, error));
    NIMVLETS_CHECK(error.find("price") != std::string::npos);
    return true;
}

// Block 09A: `starterRole` (0/1/2) por entrada hace round-trip; un byte
// de rol desconocido se rechaza.
bool TestStarterRoleRoundTrips() {
    std::vector<std::uint8_t> buf;
    AppendHeader(buf, kSchema, 3);
    AppendEntry(buf, "artu", "", "Artu", "a.nvpack", true, false, 0, false, /*starterRole=*/1);
    AppendEntry(buf, "frin", "male", "Frin", "f.nvpack", false, false, 0, false, /*starterRole=*/2);
    AppendEntry(buf, "bunny", "", "Bunny", "b.nvpack", false, false, 0, false, /*starterRole=*/0);
    PetCatalog catalog;
    std::string error;
    NIMVLETS_CHECK(LoadCatalogFromMemory(buf.data(), buf.size(), catalog, error));
    NIMVLETS_CHECK(catalog.Entries()[0].starterRole == nimvlets::catalog::StarterRole::kNormal);
    NIMVLETS_CHECK(catalog.Entries()[1].starterRole == nimvlets::catalog::StarterRole::kSecret);
    NIMVLETS_CHECK(catalog.Entries()[2].starterRole == nimvlets::catalog::StarterRole::kNone);
    NIMVLETS_CHECK(!catalog.ProductionOnboardingReady());

    std::vector<std::uint8_t> bad;
    AppendHeader(bad, kSchema, 1);
    AppendEntry(bad, "p", "", "P", "p.nvpack", true, false, 0, false, /*starterRole=*/7);
    NIMVLETS_CHECK(!LoadCatalogFromMemory(bad.data(), bad.size(), catalog, error));
    NIMVLETS_CHECK(error.find("starter role") != std::string::npos);
    return true;
}

// Block 09A: `productionOnboardingReady` solo se acepta con >= 3
// IDENTIDADES LÓGICAS distintas de starter normal (defensa en
// profundidad — brief §8/§30, endurecido por DEC-133).
bool TestProductionOnboardingReadyRequiresThreeNormalStarters() {
    // Ready + solo 1 normal -> rechazado.
    {
        std::vector<std::uint8_t> buf;
        AppendHeader(buf, kSchema, 2, /*productionOnboardingReady=*/true);
        AppendEntry(buf, "artu", "", "Artu", "a.nvpack", true, false, 0, false, 1);
        AppendEntry(buf, "frin", "male", "Frin", "f.nvpack", false, false, 0, false, 2);
        PetCatalog catalog;
        std::string error;
        NIMVLETS_CHECK(!LoadCatalogFromMemory(buf.data(), buf.size(), catalog, error));
        NIMVLETS_CHECK(error.find("normal starter") != std::string::npos);
    }
    // Ready + 3 normales -> carga, ProductionOnboardingReady() == true.
    {
        std::vector<std::uint8_t> buf;
        AppendHeader(buf, kSchema, 3, /*productionOnboardingReady=*/true);
        AppendEntry(buf, "artu", "", "Artu", "a.nvpack", true, false, 0, false, 1);
        AppendEntry(buf, "rato", "", "Rato", "r.nvpack", false, false, 0, false, 1);
        AppendEntry(buf, "rinrin", "", "Rin Rin", "rr.nvpack", false, false, 0, false, 1);
        PetCatalog catalog;
        std::string error;
        NIMVLETS_CHECK(LoadCatalogFromMemory(buf.data(), buf.size(), catalog, error));
        NIMVLETS_CHECK(catalog.ProductionOnboardingReady());
        NIMVLETS_CHECK(!catalog.DevSyntheticOnboarding());
    }
    return true;
}

// DEC-133: `devSyntheticOnboarding` (schema v5) hace round-trip y es
// MUTUAMENTE EXCLUYENTE con `productionOnboardingReady`.
bool TestDevSyntheticOnboardingByte() {
    // Round-trip del byte, con la tríada de identidades lógicas.
    {
        std::vector<std::uint8_t> buf;
        AppendHeader(buf, kSchema, 3, /*productionOnboardingReady=*/false,
                     /*devSyntheticOnboarding=*/true);
        AppendEntry(buf, "artu_dev", "", "Artu (dev)", "b.nvpack", true, false, 0, false, 1);
        AppendEntry(buf, "rato_dev", "", "Rato (dev)", "n.nvpack", false, false, 0, false, 1);
        AppendEntry(buf, "rinrin_dev", "", "Rin Rin (dev)", "b.nvpack", false, false, 0, false, 1);
        PetCatalog catalog;
        std::string error;
        NIMVLETS_CHECK(LoadCatalogFromMemory(buf.data(), buf.size(), catalog, error));
        NIMVLETS_CHECK(catalog.DevSyntheticOnboarding());
        NIMVLETS_CHECK(!catalog.ProductionOnboardingReady());
    }
    // Ambos flags en true -> rechazado.
    {
        std::vector<std::uint8_t> buf;
        AppendHeader(buf, kSchema, 3, /*productionOnboardingReady=*/true,
                     /*devSyntheticOnboarding=*/true);
        AppendEntry(buf, "artu", "", "Artu", "a.nvpack", true, false, 0, false, 1);
        AppendEntry(buf, "rato", "", "Rato", "r.nvpack", false, false, 0, false, 1);
        AppendEntry(buf, "rinrin", "", "Rin Rin", "rr.nvpack", false, false, 0, false, 1);
        PetCatalog catalog;
        std::string error;
        NIMVLETS_CHECK(!LoadCatalogFromMemory(buf.data(), buf.size(), catalog, error));
        NIMVLETS_CHECK(error.find("mutually exclusive") != std::string::npos);
    }
    // devSynthetic + solo 2 identidades lógicas distintas -> rechazado.
    {
        std::vector<std::uint8_t> buf;
        AppendHeader(buf, kSchema, 2, /*productionOnboardingReady=*/false,
                     /*devSyntheticOnboarding=*/true);
        AppendEntry(buf, "artu_dev", "", "Artu (dev)", "b.nvpack", true, false, 0, false, 1);
        AppendEntry(buf, "rato_dev", "", "Rato (dev)", "n.nvpack", false, false, 0, false, 1);
        PetCatalog catalog;
        std::string error;
        NIMVLETS_CHECK(!LoadCatalogFromMemory(buf.data(), buf.size(), catalog, error));
        NIMVLETS_CHECK(error.find("distinct logical normal") != std::string::npos);
    }
    return true;
}

// DEC-133: un starter `normal` con `variantId` no vacío se rechaza — un
// starter normal es un Nimvlet lógico entero, así dos variantes del
// mismo pet no inflan la tríada.
bool TestNormalStarterWithVariantIsRejected() {
    std::vector<std::uint8_t> buf;
    AppendHeader(buf, kSchema, 1);
    AppendEntry(buf, "artu", "gold", "Artu Gold", "a.nvpack", true, false, 0, false, /*starterRole=*/1);
    PetCatalog catalog;
    std::string error;
    NIMVLETS_CHECK(!LoadCatalogFromMemory(buf.data(), buf.size(), catalog, error));
    NIMVLETS_CHECK(error.find("variant") != std::string::npos);
    return true;
}

// DEC-133: el secreto (Frin) no cuenta para la tríada de normales, así
// que 2 normales + Frin male/female con el flag de producción -> rechazo.
bool TestSecretRoleDoesNotCountTowardTheTriad() {
    std::vector<std::uint8_t> buf;
    AppendHeader(buf, kSchema, 4, /*productionOnboardingReady=*/true);
    AppendEntry(buf, "artu", "", "Artu", "a.nvpack", true, false, 0, false, 1);
    AppendEntry(buf, "rato", "", "Rato", "r.nvpack", false, false, 0, false, 1);
    AppendEntry(buf, "frin", "male", "Frin", "fm.nvpack", false, false, 0, false, 2);
    AppendEntry(buf, "frin", "female", "Frin", "ff.nvpack", false, false, 0, false, 2);
    PetCatalog catalog;
    std::string error;
    NIMVLETS_CHECK(!LoadCatalogFromMemory(buf.data(), buf.size(), catalog, error));
    NIMVLETS_CHECK(error.find("distinct logical normal") != std::string::npos);
    return true;
}

// El schema v2 (Block 06) ya NO se acepta: el .nvcat es un artefacto de
// build que se recompila en el mismo commit, no datos del usuario — sin
// ruta de migración (a diferencia de AppState). Ver docs/CATALOG.md §10.
bool TestSchemaV2IsRejected() {
    std::vector<std::uint8_t> buf;
    AppendHeader(buf, 2, 1);
    // cuerpo estilo v2 (sin los campos de economía) — igual se rechaza
    // por la versión antes de intentar parsearlo.
    AppendString(buf, "p");
    AppendString(buf, "");
    AppendString(buf, "P");
    AppendString(buf, "p.nvpack");
    AppendUint8(buf, 1);
    AppendUint8(buf, 0);
    PetCatalog catalog;
    std::string error;
    NIMVLETS_CHECK(!LoadCatalogFromMemory(buf.data(), buf.size(), catalog, error));
    NIMVLETS_CHECK(error.find("schema") != std::string::npos);
    return true;
}

bool TestEntriesLoadInDeterministicOrder() {
    std::vector<std::uint8_t> buf;
    AppendHeader(buf, kSchema, 3);
    AppendEntry(buf, "a", "", "A", "a.nvpack", false);
    AppendEntry(buf, "b", "", "B", "b.nvpack", true);
    AppendEntry(buf, "c", "", "C", "c.nvpack", false);

    PetCatalog catalog;
    std::string error;
    NIMVLETS_CHECK(LoadCatalogFromMemory(buf.data(), buf.size(), catalog, error));
    NIMVLETS_CHECK(catalog.Entries().size() == 3);
    NIMVLETS_CHECK(catalog.Entries()[0].identity.petId == "a");
    NIMVLETS_CHECK(catalog.Entries()[1].identity.petId == "b");
    NIMVLETS_CHECK(catalog.Entries()[2].identity.petId == "c");
    return true;
}

bool TestBadMagicIsRejected() {
    std::vector<std::uint8_t> bytes = BuildSingleEntryCatalog();
    bytes[0] = 'X';
    PetCatalog catalog;
    std::string error;
    NIMVLETS_CHECK(!LoadCatalogFromMemory(bytes.data(), bytes.size(), catalog, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

bool TestEmptyBufferIsRejected() {
    PetCatalog catalog;
    std::string error;
    NIMVLETS_CHECK(!LoadCatalogFromMemory(nullptr, 0, catalog, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

bool TestTruncatedDataIsRejected() {
    std::vector<std::uint8_t> bytes = BuildSingleEntryCatalog();
    bytes.resize(bytes.size() / 2);
    PetCatalog catalog;
    std::string error;
    NIMVLETS_CHECK(!LoadCatalogFromMemory(bytes.data(), bytes.size(), catalog, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

bool TestUnsupportedSchemaVersionIsRejected() {
    std::vector<std::uint8_t> buf;
    AppendHeader(buf, 999, 1);
    AppendEntry(buf, "p", "", "P", "p.nvpack", true);
    PetCatalog catalog;
    std::string error;
    NIMVLETS_CHECK(!LoadCatalogFromMemory(buf.data(), buf.size(), catalog, error));
    NIMVLETS_CHECK(error.find("schema") != std::string::npos);
    return true;
}

bool TestZeroEntriesIsRejected() {
    std::vector<std::uint8_t> buf;
    AppendHeader(buf, kSchema, 0);
    PetCatalog catalog;
    std::string error;
    NIMVLETS_CHECK(!LoadCatalogFromMemory(buf.data(), buf.size(), catalog, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

bool TestEmptyPetIdIsRejected() {
    std::vector<std::uint8_t> buf;
    AppendHeader(buf, kSchema, 1);
    AppendEntry(buf, "", "", "Nameless", "p.nvpack", true);
    PetCatalog catalog;
    std::string error;
    NIMVLETS_CHECK(!LoadCatalogFromMemory(buf.data(), buf.size(), catalog, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

bool TestEmptyPackPathIsRejected() {
    std::vector<std::uint8_t> buf;
    AppendHeader(buf, kSchema, 1);
    AppendEntry(buf, "p", "", "P", "", true);
    PetCatalog catalog;
    std::string error;
    NIMVLETS_CHECK(!LoadCatalogFromMemory(buf.data(), buf.size(), catalog, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

bool TestDuplicateIdentityIsRejected() {
    std::vector<std::uint8_t> buf;
    AppendHeader(buf, kSchema, 2);
    AppendEntry(buf, "bunny_dev", "", "Bunny", "a.nvpack", true);
    AppendEntry(buf, "bunny_dev", "", "Bunny again", "b.nvpack", false);
    PetCatalog catalog;
    std::string error;
    NIMVLETS_CHECK(!LoadCatalogFromMemory(buf.data(), buf.size(), catalog, error));
    NIMVLETS_CHECK(error.find("duplicate") != std::string::npos);
    return true;
}

bool TestNoDefaultIsRejected() {
    std::vector<std::uint8_t> buf;
    AppendHeader(buf, kSchema, 2);
    AppendEntry(buf, "a", "", "A", "a.nvpack", false);
    AppendEntry(buf, "b", "", "B", "b.nvpack", false);
    PetCatalog catalog;
    std::string error;
    NIMVLETS_CHECK(!LoadCatalogFromMemory(buf.data(), buf.size(), catalog, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

bool TestMultipleDefaultsIsRejected() {
    std::vector<std::uint8_t> buf;
    AppendHeader(buf, kSchema, 2);
    AppendEntry(buf, "a", "", "A", "a.nvpack", true);
    AppendEntry(buf, "b", "", "B", "b.nvpack", true);
    PetCatalog catalog;
    std::string error;
    NIMVLETS_CHECK(!LoadCatalogFromMemory(buf.data(), buf.size(), catalog, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

}  // namespace

void RegisterPetCatalogLoaderTests(testing::TestRunner& runner) {
    runner.Add("PetCatalogLoader/ValidSingleEntryCatalogLoadsSuccessfully", TestValidSingleEntryCatalogLoadsSuccessfully);
    runner.Add("PetCatalogLoader/SamePetIdDifferentVariantsBothLoad", TestSamePetIdDifferentVariantsBothLoad);
    runner.Add("PetCatalogLoader/InitiallyOwnedFlagRoundTrips", TestInitiallyOwnedFlagRoundTrips);
    runner.Add("PetCatalogLoader/ShopMetadataRoundTrips", TestShopMetadataRoundTrips);
    runner.Add("PetCatalogLoader/PublicWithZeroPriceIsRejected", TestPublicWithZeroPriceIsRejected);
    runner.Add("PetCatalogLoader/StarterRoleRoundTrips", TestStarterRoleRoundTrips);
    runner.Add("PetCatalogLoader/ProductionOnboardingReadyRequiresThreeNormalStarters",
               TestProductionOnboardingReadyRequiresThreeNormalStarters);
    runner.Add("PetCatalogLoader/DevSyntheticOnboardingByte", TestDevSyntheticOnboardingByte);
    runner.Add("PetCatalogLoader/NormalStarterWithVariantIsRejected",
               TestNormalStarterWithVariantIsRejected);
    runner.Add("PetCatalogLoader/SecretRoleDoesNotCountTowardTheTriad",
               TestSecretRoleDoesNotCountTowardTheTriad);
    runner.Add("PetCatalogLoader/SchemaV2IsRejected", TestSchemaV2IsRejected);
    runner.Add("PetCatalogLoader/EntriesLoadInDeterministicOrder", TestEntriesLoadInDeterministicOrder);
    runner.Add("PetCatalogLoader/BadMagicIsRejected", TestBadMagicIsRejected);
    runner.Add("PetCatalogLoader/EmptyBufferIsRejected", TestEmptyBufferIsRejected);
    runner.Add("PetCatalogLoader/TruncatedDataIsRejected", TestTruncatedDataIsRejected);
    runner.Add("PetCatalogLoader/UnsupportedSchemaVersionIsRejected", TestUnsupportedSchemaVersionIsRejected);
    runner.Add("PetCatalogLoader/ZeroEntriesIsRejected", TestZeroEntriesIsRejected);
    runner.Add("PetCatalogLoader/EmptyPetIdIsRejected", TestEmptyPetIdIsRejected);
    runner.Add("PetCatalogLoader/EmptyPackPathIsRejected", TestEmptyPackPathIsRejected);
    runner.Add("PetCatalogLoader/DuplicateIdentityIsRejected", TestDuplicateIdentityIsRejected);
    runner.Add("PetCatalogLoader/NoDefaultIsRejected", TestNoDefaultIsRejected);
    runner.Add("PetCatalogLoader/MultipleDefaultsIsRejected", TestMultipleDefaultsIsRejected);
}

}  // namespace nimvlets::tests
