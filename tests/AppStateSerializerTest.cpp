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

// Hand-builds "NVSTATE1" byte buffers matching the exact format (see
// src/persistence/AppStateSerializer.cpp and docs/PERSISTENCE.md), the
// same technique tests/PetPackLoaderTest.cpp uses for "NVPACK1" — lets
// the parser be exercised directly against synthetic valid *and*
// malformed data with zero filesystem dependency.

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

// A valid, fully-formed buffer with the given fields — mirrors exactly
// what SerializeAppState() itself would produce for an AppState with
// these values, but built independently so the test doesn't just
// check the serializer against itself.
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

bool TestDefaultAppStateRoundTrips() {
    const AppState original;  // all defaults: version=current, balance=0, empty ids, no position
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
    buf[0] = 'X';  // corrupt the magic

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
    buf.resize(10);  // cuts off partway through schemaVersion/clickBalance

    AppState decoded;
    std::string error;
    NIMVLETS_CHECK(!DeserializeAppState(buf.data(), buf.size(), decoded, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

bool TestTruncatedMidStringIsRejected() {
    std::vector<std::uint8_t> buf = BuildValidBuffer(AppState::kCurrentSchemaVersion, 5, "bunny_dev", "", false, 0, 0);
    // Truncate a few bytes into the "bunny_dev" string's declared
    // 9-byte payload — the length prefix says 9 bytes follow, but
    // fewer are actually present.
    const std::size_t magicHeaderAndBalanceSize = 8 + 4 + 8;  // magic + schemaVersion + clickBalance
    buf.resize(magicHeaderAndBalanceSize + 4 /*string length*/ + 3 /*only 3 of 9 chars*/);

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
}

}  // namespace nimvlets::tests
