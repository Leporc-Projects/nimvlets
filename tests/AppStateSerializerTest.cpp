#include "AppStateSerializerTest.h"

#include "persistence/AppStateSerializer.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

using nimvlets::persistence::AppState;
using nimvlets::persistence::DeserializeAppState;
using nimvlets::persistence::SerializeAppState;
using nimvlets::persistence::WindowPosition;

namespace nimvlets::tests {

namespace {

// Construye a mano buffers de bytes "NVSTATE1" que calzan con el
// formato exacto (ver src/persistence/AppStateSerializer.cpp y
// docs/PERSISTENCE.md), la misma técnica que usa
// tests/PetPackLoaderTest.cpp para "NVPACK1" — permite ejercitar el
// parser directamente contra datos sintéticos válidos *y* malformados,
// sin ninguna dependencia del filesystem.

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

void AppendInt32(std::vector<std::uint8_t>& buf, std::int32_t v) {
    AppendBytes(buf, &v, sizeof(v));
}

void AppendString(std::vector<std::uint8_t>& buf, const std::string& s) {
    AppendUint32(buf, static_cast<std::uint32_t>(s.size()));
    AppendBytes(buf, s.data(), s.size());
}

void AppendMagic(std::vector<std::uint8_t>& buf) {
    const char magic[8] = {'N', 'V', 'S', 'T', 'A', 'T', 'E', '1'};
    AppendBytes(buf, magic, sizeof(magic));
}

// Un buffer con el layout que v1 y v2 comparten (magic + schemaVersion
// + cuerpo de Block 03). Con `schemaVersion == 1` es un archivo v1
// completo y válido; con `schemaVersion == 2` está deliberadamente
// truncado (le faltan los campos v2) — útil solo para los tests de
// rechazo, no para un round-trip exitoso. Construido de forma
// independiente para que el test no termine verificando el
// serializador contra sí mismo.
std::vector<std::uint8_t> BuildValidBuffer(
    std::uint32_t schemaVersion,
    std::uint64_t clickBalance,
    const std::string& petId,
    const std::string& variantId,
    bool hasPosition,
    std::int32_t posX,
    std::int32_t posY) {
    std::vector<std::uint8_t> buf;
    AppendMagic(buf);
    AppendUint32(buf, schemaVersion);
    AppendUint64(buf, clickBalance);
    AppendString(buf, petId);
    AppendString(buf, variantId);
    AppendUint8(buf, hasPosition ? 1 : 0);
    AppendInt32(buf, posX);
    AppendInt32(buf, posY);
    return buf;
}

// Un archivo v2 completo y válido (magic + version 2 + cuerpo Block 03
// + bloque v2), SIN el bloque v3 (`language`). Para el test de
// migración v2 -> v3.
std::vector<std::uint8_t> BuildValidV2Buffer(
    std::uint64_t clickBalance, const std::string& petId, bool ownershipSeeded,
    const std::vector<std::string>& ownedPetIds, bool lockPosition, const std::string& sizeChoice,
    std::uint32_t opacityPercent) {
    std::vector<std::uint8_t> buf = BuildValidBuffer(2, clickBalance, petId, "", false, 0, 0);
    AppendUint8(buf, ownershipSeeded ? 1 : 0);
    AppendUint32(buf, static_cast<std::uint32_t>(ownedPetIds.size()));
    for (const std::string& id : ownedPetIds) {
        AppendString(buf, id);
    }
    AppendUint8(buf, lockPosition ? 1 : 0);
    AppendString(buf, sizeChoice);
    AppendUint32(buf, opacityPercent);
    return buf;
}

bool TestDefaultAppStateRoundTrips() {
    const AppState original;  // todo default: version=actual, balance=0, ids vacíos, sin posición
    const std::vector<std::uint8_t> bytes = SerializeAppState(original);

    AppState decoded;
    std::string error;
    NIMVLETS_CHECK(DeserializeAppState(bytes.data(), bytes.size(), decoded, error));
    NIMVLETS_CHECK(decoded == original);
    NIMVLETS_CHECK(!decoded.lastWindowPosition.has_value());
    return true;
}

bool TestFullyPopulatedAppStateRoundTrips() {
    AppState original;
    original.clickBalance = 123456789;
    original.activePetId = "bunny_dev";
    original.activeVariantId = "female";
    original.lastWindowPosition = WindowPosition{-42, 917};

    const std::vector<std::uint8_t> bytes = SerializeAppState(original);
    AppState decoded;
    std::string error;
    NIMVLETS_CHECK(DeserializeAppState(bytes.data(), bytes.size(), decoded, error));
    NIMVLETS_CHECK(decoded == original);
    NIMVLETS_CHECK(decoded.lastWindowPosition.has_value());
    NIMVLETS_CHECK(decoded.lastWindowPosition->x == -42);
    NIMVLETS_CHECK(decoded.lastWindowPosition->y == 917);
    return true;
}

bool TestSerializationIsDeterministic() {
    AppState state;
    state.clickBalance = 42;
    state.activePetId = "bunny_dev";
    state.lastWindowPosition = WindowPosition{10, 20};

    const std::vector<std::uint8_t> first = SerializeAppState(state);
    const std::vector<std::uint8_t> second = SerializeAppState(state);
    NIMVLETS_CHECK(first == second);
    return true;
}

bool TestNoWindowPositionRoundTripsAsNullopt() {
    AppState state;
    state.activePetId = "bunny_dev";
    state.lastWindowPosition = std::nullopt;

    const std::vector<std::uint8_t> bytes = SerializeAppState(state);
    AppState decoded;
    std::string error;
    NIMVLETS_CHECK(DeserializeAppState(bytes.data(), bytes.size(), decoded, error));
    NIMVLETS_CHECK(!decoded.lastWindowPosition.has_value());
    return true;
}

bool TestLargeClickBalanceRoundTrips() {
    AppState state;
    state.clickBalance = std::numeric_limits<std::uint64_t>::max();

    const std::vector<std::uint8_t> bytes = SerializeAppState(state);
    AppState decoded;
    std::string error;
    NIMVLETS_CHECK(DeserializeAppState(bytes.data(), bytes.size(), decoded, error));
    NIMVLETS_CHECK(decoded.clickBalance == std::numeric_limits<std::uint64_t>::max());
    return true;
}

bool TestBadMagicIsRejected() {
    std::vector<std::uint8_t> buf = BuildValidBuffer(AppState::kCurrentSchemaVersion, 5, "p", "", false, 0, 0);
    buf[0] = 'X';  // corrompe el magic

    AppState decoded;
    std::string error;
    NIMVLETS_CHECK(!DeserializeAppState(buf.data(), buf.size(), decoded, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

bool TestEmptyBufferIsRejected() {
    std::vector<std::uint8_t> buf;
    AppState decoded;
    std::string error;
    NIMVLETS_CHECK(!DeserializeAppState(buf.data(), buf.size(), decoded, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

bool TestTruncatedHeaderIsRejected() {
    std::vector<std::uint8_t> buf = BuildValidBuffer(AppState::kCurrentSchemaVersion, 5, "p", "", false, 0, 0);
    buf.resize(10);  // corta a mitad de camino de schemaVersion/clickBalance

    AppState decoded;
    std::string error;
    NIMVLETS_CHECK(!DeserializeAppState(buf.data(), buf.size(), decoded, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

bool TestTruncatedMidStringIsRejected() {
    std::vector<std::uint8_t> buf = BuildValidBuffer(AppState::kCurrentSchemaVersion, 5, "bunny_dev", "", false, 0, 0);
    // Trunca unos pocos bytes dentro del payload declarado de 9 bytes
    // del string "bunny_dev" — el prefijo de longitud dice que siguen
    // 9 bytes, pero en realidad hay menos.
    const std::size_t magicHeaderAndBalanceSize = 8 + 4 + 8;  // magic + schemaVersion + clickBalance
    buf.resize(magicHeaderAndBalanceSize + 4 /*longitud del string*/ + 3 /*solo 3 de 9 chars*/);

    AppState decoded;
    std::string error;
    NIMVLETS_CHECK(!DeserializeAppState(buf.data(), buf.size(), decoded, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

bool TestUnsupportedSchemaVersionIsRejected() {
    std::vector<std::uint8_t> buf = BuildValidBuffer(/*schemaVersion=*/999, 5, "p", "", false, 0, 0);

    AppState decoded;
    std::string error;
    NIMVLETS_CHECK(!DeserializeAppState(buf.data(), buf.size(), decoded, error));
    NIMVLETS_CHECK(error.find("schema") != std::string::npos);
    return true;
}

// --- Block 06: schema v2 + migración hacia adelante desde v1 -------

// Un archivo v1 real (Block 03/04/05) se sigue leyendo: el balance y
// el pet activo sobreviven, los campos v2 quedan en su default, y el
// schema queda marcado como el actual para que el próximo Save() lo
// reescriba como v2.
bool TestV1BufferMigratesForward() {
    std::vector<std::uint8_t> buf =
        BuildValidBuffer(/*schemaVersion=*/1, /*clickBalance=*/999, "frin", "male", /*hasPosition=*/true, 7, 9);

    AppState decoded;
    std::string error;
    NIMVLETS_CHECK(DeserializeAppState(buf.data(), buf.size(), decoded, error));
    NIMVLETS_CHECK(error.empty());
    NIMVLETS_CHECK(decoded.clickBalance == 999);
    NIMVLETS_CHECK(decoded.activePetId == "frin");
    NIMVLETS_CHECK(decoded.activeVariantId == "male");
    NIMVLETS_CHECK(decoded.lastWindowPosition.has_value());
    NIMVLETS_CHECK(decoded.lastWindowPosition->x == 7 && decoded.lastWindowPosition->y == 9);
    // Campos v2 en su default.
    NIMVLETS_CHECK(!decoded.ownershipSeeded);
    NIMVLETS_CHECK(decoded.ownedPetIds.empty());
    NIMVLETS_CHECK(!decoded.lockPosition);
    NIMVLETS_CHECK(decoded.sizeChoice.empty());
    NIMVLETS_CHECK(decoded.opacityPercent == 0);
    // Campo v3 en su default.
    NIMVLETS_CHECK(decoded.language.empty());
    // Marcado como schema actual -> el próximo Save() escribe el formato actual.
    NIMVLETS_CHECK(decoded.schemaVersion == AppState::kCurrentSchemaVersion);
    return true;
}

// --- Block 06.1: schema v3 (language) + migración desde v2 ---------

// Un archivo v2 (Block 06) se sigue leyendo: propiedad, tamaño,
// opacidad y todo lo anterior sobreviven; `language` queda "" (src/app
// lo resolverá desde el locale del OS), y el schema queda marcado como
// el actual para que el próximo Save() lo reescriba como v3.
bool TestV2BufferMigratesForward() {
    const std::vector<std::uint8_t> buf =
        BuildValidV2Buffer(/*clickBalance=*/500, "nidir", /*ownershipSeeded=*/true,
                           {"bunny", "frin"}, /*lockPosition=*/true, "large", 70);

    AppState decoded;
    std::string error;
    NIMVLETS_CHECK(DeserializeAppState(buf.data(), buf.size(), decoded, error));
    NIMVLETS_CHECK(error.empty());
    NIMVLETS_CHECK(decoded.clickBalance == 500);
    NIMVLETS_CHECK(decoded.activePetId == "nidir");
    NIMVLETS_CHECK(decoded.ownershipSeeded);
    NIMVLETS_CHECK((decoded.ownedPetIds == std::vector<std::string>{"bunny", "frin"}));
    NIMVLETS_CHECK(decoded.lockPosition);
    NIMVLETS_CHECK(decoded.sizeChoice == "large");
    NIMVLETS_CHECK(decoded.opacityPercent == 70);
    // Campo v3 en su default -> "sin preferencia explícita".
    NIMVLETS_CHECK(decoded.language.empty());
    NIMVLETS_CHECK(decoded.schemaVersion == AppState::kCurrentSchemaVersion);
    return true;
}

bool TestV3LanguageRoundTrips() {
    AppState original;
    original.clickBalance = 77;
    original.activePetId = "frin";
    original.activeVariantId = "female";
    original.ownershipSeeded = true;
    original.ownedPetIds = {"bunny", "frin"};
    original.sizeChoice = "medium";
    original.opacityPercent = 100;
    original.language = "es";

    const std::vector<std::uint8_t> bytes = SerializeAppState(original);
    AppState decoded;
    std::string error;
    NIMVLETS_CHECK(DeserializeAppState(bytes.data(), bytes.size(), decoded, error));
    NIMVLETS_CHECK(decoded.language == "es");
    NIMVLETS_CHECK(decoded == original);

    // "" (sin preferencia) también hace round-trip.
    original.language.clear();
    const std::vector<std::uint8_t> bytes2 = SerializeAppState(original);
    AppState decoded2;
    NIMVLETS_CHECK(DeserializeAppState(bytes2.data(), bytes2.size(), decoded2, error));
    NIMVLETS_CHECK(decoded2.language.empty());
    NIMVLETS_CHECK(decoded2 == original);
    return true;
}

bool TestV2FieldsRoundTrip() {
    AppState original;
    original.clickBalance = 4242;
    original.activePetId = "bunny";
    original.ownershipSeeded = true;
    original.ownedPetIds = {"frin", "bunny"};  // desordenado a propósito
    original.lockPosition = true;
    original.sizeChoice = "large";
    original.opacityPercent = 70;

    const std::vector<std::uint8_t> bytes = SerializeAppState(original);
    AppState decoded;
    std::string error;
    NIMVLETS_CHECK(DeserializeAppState(bytes.data(), bytes.size(), decoded, error));
    NIMVLETS_CHECK(decoded.ownershipSeeded);
    // Normalizado: ordenado ascendente, sin duplicados.
    NIMVLETS_CHECK((decoded.ownedPetIds == std::vector<std::string>{"bunny", "frin"}));
    NIMVLETS_CHECK(decoded.lockPosition);
    NIMVLETS_CHECK(decoded.sizeChoice == "large");
    NIMVLETS_CHECK(decoded.opacityPercent == 70);
    // operator== compara ownedPetIds -- original está desordenado, así
    // que hay que normalizarlo antes de comparar el struct entero.
    original.ownedPetIds = {"bunny", "frin"};
    NIMVLETS_CHECK(decoded == original);
    return true;
}

// ownedPetIds sale siempre en orden canónico y sin string vacío,
// venga como venga en el AppState en memoria -> el formato sigue
// siendo determinista byte a byte.
bool TestOwnedPetIdsNormalizedOnSerialize() {
    AppState a;
    a.ownedPetIds = {"nidir", "bunny", "bunny", "", "frin"};
    AppState b;
    b.ownedPetIds = {"frin", "nidir", "bunny"};

    NIMVLETS_CHECK(SerializeAppState(a) == SerializeAppState(b));

    AppState decoded;
    std::string error;
    const std::vector<std::uint8_t> bytes = SerializeAppState(a);
    NIMVLETS_CHECK(DeserializeAppState(bytes.data(), bytes.size(), decoded, error));
    NIMVLETS_CHECK((decoded.ownedPetIds == std::vector<std::string>{"bunny", "frin", "nidir"}));
    return true;
}

}  // namespace

void RegisterAppStateSerializerTests(testing::TestRunner& runner) {
    runner.Add("AppStateSerializer/DefaultAppStateRoundTrips", TestDefaultAppStateRoundTrips);
    runner.Add("AppStateSerializer/FullyPopulatedAppStateRoundTrips", TestFullyPopulatedAppStateRoundTrips);
    runner.Add("AppStateSerializer/SerializationIsDeterministic", TestSerializationIsDeterministic);
    runner.Add("AppStateSerializer/NoWindowPositionRoundTripsAsNullopt", TestNoWindowPositionRoundTripsAsNullopt);
    runner.Add("AppStateSerializer/LargeClickBalanceRoundTrips", TestLargeClickBalanceRoundTrips);
    runner.Add("AppStateSerializer/BadMagicIsRejected", TestBadMagicIsRejected);
    runner.Add("AppStateSerializer/EmptyBufferIsRejected", TestEmptyBufferIsRejected);
    runner.Add("AppStateSerializer/TruncatedHeaderIsRejected", TestTruncatedHeaderIsRejected);
    runner.Add("AppStateSerializer/TruncatedMidStringIsRejected", TestTruncatedMidStringIsRejected);
    runner.Add("AppStateSerializer/UnsupportedSchemaVersionIsRejected", TestUnsupportedSchemaVersionIsRejected);
    runner.Add("AppStateSerializer/V1BufferMigratesForward", TestV1BufferMigratesForward);
    runner.Add("AppStateSerializer/V2FieldsRoundTrip", TestV2FieldsRoundTrip);
    runner.Add("AppStateSerializer/OwnedPetIdsNormalizedOnSerialize", TestOwnedPetIdsNormalizedOnSerialize);
    runner.Add("AppStateSerializer/V2BufferMigratesForward", TestV2BufferMigratesForward);
    runner.Add("AppStateSerializer/V3LanguageRoundTrips", TestV3LanguageRoundTrips);
}

}  // namespace nimvlets::tests
