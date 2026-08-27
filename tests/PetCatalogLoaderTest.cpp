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

void AppendString(std::vector<std::uint8_t>& buf, const std::string& s) {
    AppendUint32(buf, static_cast<std::uint32_t>(s.size()));
    AppendBytes(buf, s.data(), s.size());
}

void AppendMagic(std::vector<std::uint8_t>& buf) {
    const char magic[8] = {'N', 'V', 'C', 'A', 'T', 'L', 'G', '1'};
    AppendBytes(buf, magic, sizeof(magic));
}

// Schema actual del formato "NVCATLG1" — Block 06 lo subió a 2
// (agrega el byte `initiallyOwned` por entrada).
constexpr std::uint32_t kSchema = 2;

void AppendHeader(std::vector<std::uint8_t>& buf, std::uint32_t schemaVersion, std::uint32_t entryCount) {
    AppendMagic(buf);
    AppendUint32(buf, schemaVersion);
    AppendUint32(buf, entryCount);
}

void AppendEntry(
    std::vector<std::uint8_t>& buf,
    const std::string& petId,
    const std::string& variantId,
    const std::string& displayName,
    const std::string& packPath,
    bool isDefault,
    bool initiallyOwned = false) {
    AppendString(buf, petId);
    AppendString(buf, variantId);
    AppendString(buf, displayName);
    AppendString(buf, packPath);
    AppendUint8(buf, isDefault ? 1 : 0);
    AppendUint8(buf, initiallyOwned ? 1 : 0);
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
