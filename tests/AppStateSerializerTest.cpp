#include "AppStateSerializerTest.h"

#include "persistence/AppStateSerializer.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

using nimvlets::persistence::AppState;
using nimvlets::persistence::DeserializeAppState;
using nimvlets::persistence::OwnedEntitlement;
using nimvlets::persistence::SerializeAppState;
using nimvlets::persistence::WindowPosition;

using Ents = std::vector<OwnedEntitlement>;

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

// Un archivo v2 o v3 con propiedad en el formato VIEJO (`ownedPetIds`,
// petIds sueltos). `schemaVersion` == 2 -> sin `language`; == 3 -> con
// `language`. Para los tests de migración hacia v4.
std::vector<std::uint8_t> BuildLegacyOwnershipBuffer(
    std::uint32_t schemaVersion, std::uint64_t clickBalance, const std::string& petId,
    bool ownershipSeeded, const std::vector<std::string>& ownedPetIds, bool lockPosition,
    const std::string& sizeChoice, std::uint32_t opacityPercent, const std::string& language = "") {
    std::vector<std::uint8_t> buf = BuildValidBuffer(schemaVersion, clickBalance, petId, "", false, 0, 0);
    AppendUint8(buf, ownershipSeeded ? 1 : 0);
    AppendUint32(buf, static_cast<std::uint32_t>(ownedPetIds.size()));
    for (const std::string& id : ownedPetIds) {
        AppendString(buf, id);
    }
    AppendUint8(buf, lockPosition ? 1 : 0);
    AppendString(buf, sizeChoice);
    AppendUint32(buf, opacityPercent);
    if (schemaVersion >= 3) {
        AppendString(buf, language);
    }
    return buf;
}

// Cuerpo v4+: propiedad como pares (petId, variantId) + lock/size/
// opacity + language. `schemaVersion` == 4 -> archivo v4 (sin byte de
// lifecycle); >= 5 -> se le agrega el byte de onboardingLifecycle.
std::vector<std::uint8_t> BuildOwnershipPairBuffer(
    std::uint32_t schemaVersion, std::uint64_t clickBalance, const std::string& petId,
    const std::string& variantId, bool ownershipSeeded, const Ents& ownedEntitlements, bool lockPosition,
    const std::string& sizeChoice, std::uint32_t opacityPercent, const std::string& language,
    std::uint8_t onboardingLifecycleByte) {
    std::vector<std::uint8_t> buf =
        BuildValidBuffer(schemaVersion, clickBalance, petId, variantId, false, 0, 0);
    AppendUint8(buf, ownershipSeeded ? 1 : 0);
    AppendUint32(buf, static_cast<std::uint32_t>(ownedEntitlements.size()));
    for (const OwnedEntitlement& e : ownedEntitlements) {
        AppendString(buf, e.petId);
        AppendString(buf, e.variantId);
    }
    AppendUint8(buf, lockPosition ? 1 : 0);
    AppendString(buf, sizeChoice);
    AppendUint32(buf, opacityPercent);
    AppendString(buf, language);
    if (schemaVersion >= 5) {
        AppendUint8(buf, onboardingLifecycleByte);
    }
    return buf;
}

// Un archivo v4 completo (sin lifecycle) — para los tests de migración
// v4 -> v5.
std::vector<std::uint8_t> BuildValidV4Buffer(
    std::uint64_t clickBalance, const std::string& petId, const std::string& variantId,
    bool ownershipSeeded, const Ents& ownedEntitlements, bool lockPosition,
    const std::string& sizeChoice, std::uint32_t opacityPercent, const std::string& language) {
    return BuildOwnershipPairBuffer(4, clickBalance, petId, variantId, ownershipSeeded,
                                    ownedEntitlements, lockPosition, sizeChoice, opacityPercent,
                                    language, /*onboardingLifecycleByte=*/0);
}

// Un archivo v5 completo, con el byte de onboardingLifecycle.
std::vector<std::uint8_t> BuildValidV5Buffer(
    std::uint64_t clickBalance, const std::string& petId, const std::string& variantId,
    bool ownershipSeeded, const Ents& ownedEntitlements, bool lockPosition,
    const std::string& sizeChoice, std::uint32_t opacityPercent, const std::string& language,
    std::uint8_t onboardingLifecycleByte) {
    return BuildOwnershipPairBuffer(5, clickBalance, petId, variantId, ownershipSeeded,
                                    ownedEntitlements, lockPosition, sizeChoice, opacityPercent,
                                    language, onboardingLifecycleByte);
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

// --- Migración hacia adelante desde v1/v2/v3 (Block 07: propiedad
//     pasa de `ownedPetIds` a `ownedEntitlements`) -------------------

// Un archivo v1 real (Block 03/04/05) se sigue leyendo: el balance y
// el pet activo sobreviven, los campos posteriores quedan en su
// default, y el schema queda marcado como el actual para que el
// próximo Save() lo reescriba.
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
    NIMVLETS_CHECK(!decoded.ownershipSeeded);
    NIMVLETS_CHECK(decoded.ownedEntitlements.empty());
    NIMVLETS_CHECK(!decoded.lockPosition);
    NIMVLETS_CHECK(decoded.sizeChoice.empty());
    NIMVLETS_CHECK(decoded.opacityPercent == 0);
    NIMVLETS_CHECK(decoded.language.empty());
    NIMVLETS_CHECK(decoded.schemaVersion == AppState::kCurrentSchemaVersion);
    return true;
}

// Un archivo v2 (Block 06): la lista de propiedad (`ownedPetIds`) se
// parsea PROVISIONALMENTE a `{petId, ""}` por cada petId. El serializer
// NO tiene catálogo, así que no puede saber que "frin" tiene variantes
// — `src/app` reconcilia ese `{frin, ""}` con una tabla histórica
// CONGELADA (ExpandHistoricalWholePetEntitlements -> `{frin, "male"} +
// {frin, "female"}`, sin mirar el catálogo actual; ver
// EntitlementMigrationTest y DEC-128 / DEC-129). Acá solo se verifica el
// parseo del serializer. `language` queda "".
bool TestV2BufferParsesLegacyOwnershipProvisionally() {
    const std::vector<std::uint8_t> buf = BuildLegacyOwnershipBuffer(
        /*schemaVersion=*/2, /*clickBalance=*/500, "nidir", /*ownershipSeeded=*/true,
        {"bunny", "frin"}, /*lockPosition=*/true, "large", 70);

    AppState decoded;
    std::string error;
    NIMVLETS_CHECK(DeserializeAppState(buf.data(), buf.size(), decoded, error));
    NIMVLETS_CHECK(error.empty());
    NIMVLETS_CHECK(decoded.clickBalance == 500);
    NIMVLETS_CHECK(decoded.activePetId == "nidir");
    NIMVLETS_CHECK(decoded.ownershipSeeded);
    NIMVLETS_CHECK((decoded.ownedEntitlements ==
                    Ents{OwnedEntitlement{"bunny", ""}, OwnedEntitlement{"frin", ""}}));
    NIMVLETS_CHECK(decoded.lockPosition);
    NIMVLETS_CHECK(decoded.sizeChoice == "large");
    NIMVLETS_CHECK(decoded.opacityPercent == 70);
    NIMVLETS_CHECK(decoded.language.empty());
    NIMVLETS_CHECK(decoded.schemaVersion == AppState::kCurrentSchemaVersion);
    return true;
}

// Un archivo v3 (Block 06.1): idem parseo provisional; `language` SÍ se
// conserva.
bool TestV3BufferParsesLegacyOwnershipProvisionally() {
    const std::vector<std::uint8_t> buf = BuildLegacyOwnershipBuffer(
        /*schemaVersion=*/3, /*clickBalance=*/12, "bunny", /*ownershipSeeded=*/true, {"frin", "bunny"},
        /*lockPosition=*/false, "medium", 85, /*language=*/"es");

    AppState decoded;
    std::string error;
    NIMVLETS_CHECK(DeserializeAppState(buf.data(), buf.size(), decoded, error));
    NIMVLETS_CHECK(error.empty());
    NIMVLETS_CHECK((decoded.ownedEntitlements ==
                    Ents{OwnedEntitlement{"bunny", ""}, OwnedEntitlement{"frin", ""}}));
    NIMVLETS_CHECK(decoded.language == "es");
    NIMVLETS_CHECK(decoded.schemaVersion == AppState::kCurrentSchemaVersion);
    return true;
}

// Round-trip v5 completo, incluida una autorización de VARIANTE concreta
// ({"frin","male"}) junto a una de pet entero ({"bunny",""}), y el
// lifecycle de onboarding.
bool TestV5RoundTrips() {
    AppState original;
    original.clickBalance = 77;
    original.activePetId = "frin";
    original.activeVariantId = "male";
    original.ownershipSeeded = true;
    original.ownedEntitlements = {OwnedEntitlement{"bunny", ""}, OwnedEntitlement{"frin", "male"}};
    original.sizeChoice = "medium";
    original.opacityPercent = 100;
    original.language = "en";
    original.onboardingLifecycle = nimvlets::persistence::OnboardingLifecycle::kCompleted;

    const std::vector<std::uint8_t> bytes = SerializeAppState(original);
    AppState decoded;
    std::string error;
    NIMVLETS_CHECK(DeserializeAppState(bytes.data(), bytes.size(), decoded, error));
    NIMVLETS_CHECK(decoded == original);

    // Y un buffer v5 hecho a mano parsea igual (byte 2 == kCompleted).
    const std::vector<std::uint8_t> handmade = BuildValidV5Buffer(
        77, "frin", "male", true, {OwnedEntitlement{"bunny", ""}, OwnedEntitlement{"frin", "male"}},
        false, "medium", 100, "en", /*onboardingLifecycleByte=*/2);
    AppState fromHandmade;
    NIMVLETS_CHECK(DeserializeAppState(handmade.data(), handmade.size(), fromHandmade, error));
    NIMVLETS_CHECK(fromHandmade == original);
    return true;
}

// Un default AppState{} (== "ningún save") es kPending; CUALQUIER archivo
// v1/v2/v3/v4 migra a kLegacyComplete (usuario existente, ya
// onboardeado — brief §4/§27, DEC-131). Un v5 conserva su byte.
bool TestOnboardingLifecycleMigration() {
    using nimvlets::persistence::OnboardingLifecycle;

    NIMVLETS_CHECK(AppState{}.onboardingLifecycle == OnboardingLifecycle::kPending);

    AppState decoded;
    std::string error;

    // v1
    {
        const auto buf = BuildValidBuffer(1, 9, "bunny", "", false, 0, 0);
        NIMVLETS_CHECK(DeserializeAppState(buf.data(), buf.size(), decoded, error));
        NIMVLETS_CHECK(decoded.onboardingLifecycle == OnboardingLifecycle::kLegacyComplete);
    }
    // v2 / v3
    for (std::uint32_t v : {2u, 3u}) {
        const auto buf =
            BuildLegacyOwnershipBuffer(v, 9, "bunny", true, {"bunny"}, false, "medium", 100, "es");
        NIMVLETS_CHECK(DeserializeAppState(buf.data(), buf.size(), decoded, error));
        NIMVLETS_CHECK(decoded.onboardingLifecycle == OnboardingLifecycle::kLegacyComplete);
    }
    // v4
    {
        const auto buf = BuildValidV4Buffer(9, "bunny", "", true, {OwnedEntitlement{"bunny", ""}},
                                            false, "medium", 100, "en");
        NIMVLETS_CHECK(DeserializeAppState(buf.data(), buf.size(), decoded, error));
        NIMVLETS_CHECK(decoded.onboardingLifecycle == OnboardingLifecycle::kLegacyComplete);
    }
    // v5: cada valor del byte se conserva; uno fuera de rango -> kLegacyComplete (no destructivo).
    for (auto [byteVal, expected] :
         {std::pair<std::uint8_t, OnboardingLifecycle>{0, OnboardingLifecycle::kPending},
          {1, OnboardingLifecycle::kLegacyComplete},
          {2, OnboardingLifecycle::kCompleted},
          {7, OnboardingLifecycle::kLegacyComplete}}) {
        const auto buf = BuildValidV5Buffer(9, "bunny", "", true, {OwnedEntitlement{"bunny", ""}},
                                            false, "medium", 100, "en", byteVal);
        NIMVLETS_CHECK(DeserializeAppState(buf.data(), buf.size(), decoded, error));
        NIMVLETS_CHECK(decoded.onboardingLifecycle == expected);
    }
    return true;
}

// Un v5 truncado justo antes del byte de lifecycle se RECHAZA (no se
// adivina).
bool TestTruncatedV5LifecycleByteRejected() {
    std::vector<std::uint8_t> buf = BuildValidV5Buffer(
        1, "bunny", "", true, {OwnedEntitlement{"bunny", ""}}, false, "medium", 100, "en", 2);
    buf.pop_back();  // el byte de lifecycle

    AppState decoded;
    std::string error;
    NIMVLETS_CHECK(!DeserializeAppState(buf.data(), buf.size(), decoded, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

// `ownedEntitlements` sale siempre en orden canónico (por (petId,
// variantId)) y sin petId vacío, venga como venga en memoria -> el
// formato sigue siendo determinista byte a byte, y los duplicados se
// normalizan.
bool TestOwnedEntitlementsNormalizedOnSerialize() {
    AppState a;
    a.ownedEntitlements = {OwnedEntitlement{"nidir", ""}, OwnedEntitlement{"bunny", ""},
                           OwnedEntitlement{"bunny", ""}, OwnedEntitlement{"", "x"},
                           OwnedEntitlement{"frin", "male"}};
    AppState b;
    b.ownedEntitlements = {OwnedEntitlement{"frin", "male"}, OwnedEntitlement{"nidir", ""},
                           OwnedEntitlement{"bunny", ""}};

    NIMVLETS_CHECK(SerializeAppState(a) == SerializeAppState(b));

    AppState decoded;
    std::string error;
    const std::vector<std::uint8_t> bytes = SerializeAppState(a);
    NIMVLETS_CHECK(DeserializeAppState(bytes.data(), bytes.size(), decoded, error));
    NIMVLETS_CHECK((decoded.ownedEntitlements ==
                    Ents{OwnedEntitlement{"bunny", ""}, OwnedEntitlement{"frin", "male"},
                         OwnedEntitlement{"nidir", ""}}));
    return true;
}

// Un v4 cortado a mitad de un par (petId, variantId) se rechaza en vez
// de inventar una autorización.
bool TestV4TruncatedOwnershipIsRejected() {
    std::vector<std::uint8_t> buf = BuildValidV4Buffer(
        1, "bunny", "", true, {OwnedEntitlement{"bunny", ""}, OwnedEntitlement{"frin", "male"}}, false,
        "", 0, "");
    buf.resize(buf.size() - 20);  // recorta dentro del bloque de autorizaciones

    AppState decoded;
    std::string error;
    NIMVLETS_CHECK(!DeserializeAppState(buf.data(), buf.size(), decoded, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

// El out-param `outOnDiskSchemaVersion` reporta la versión que traía el
// archivo EN DISCO — ANTES de que `schemaVersion` se normalice a la
// actual. src/app decide con eso si corre la reconciliación de propiedad
// legacy (solo cuando la versión en disco < la actual — DEC-129). En un
// parseo fallido NO se escribe: queda el valor que el caller haya puesto.
bool TestOnDiskSchemaVersionOutParam() {
    AppState decoded;
    std::string error;

    for (std::uint32_t v : {1u, 2u, 3u}) {
        const std::vector<std::uint8_t> buf =
            (v == 1) ? BuildValidBuffer(1, 10, "bunny", "", false, 0, 0)
                     : BuildLegacyOwnershipBuffer(v, 10, "bunny", true, {"bunny"}, false, "medium", 100, "es");
        std::uint32_t onDisk = 999;
        NIMVLETS_CHECK(DeserializeAppState(buf.data(), buf.size(), decoded, error, &onDisk));
        NIMVLETS_CHECK(onDisk == v);
        // El estado en memoria SÍ queda normalizado a la versión actual.
        NIMVLETS_CHECK(decoded.schemaVersion == AppState::kCurrentSchemaVersion);
    }

    // v4: el out-param reporta 4 (ANTES de normalizar a la actual). El
    // gate de reconciliación legacy de src/app es
    // `< kFirstExplicitEntitlementSchema` (== 4), así que un v4 NO se
    // reconcilia — pero el out-param igual dice la verdad sobre el disco.
    {
        const std::vector<std::uint8_t> buf = BuildValidV4Buffer(
            10, "bunny", "", true, {OwnedEntitlement{"bunny", ""}}, false, "medium", 100, "en");
        std::uint32_t onDisk = 0;
        NIMVLETS_CHECK(DeserializeAppState(buf.data(), buf.size(), decoded, error, &onDisk));
        NIMVLETS_CHECK(onDisk == 4);
    }
    // v5: reporta 5.
    {
        const std::vector<std::uint8_t> buf = BuildValidV5Buffer(
            10, "bunny", "", true, {OwnedEntitlement{"bunny", ""}}, false, "medium", 100, "en", 2);
        std::uint32_t onDisk = 0;
        NIMVLETS_CHECK(DeserializeAppState(buf.data(), buf.size(), decoded, error, &onDisk));
        NIMVLETS_CHECK(onDisk == AppState::kCurrentSchemaVersion);
        NIMVLETS_CHECK(onDisk == 5);
    }

    // Parseo fallido (magic malo): el out-param queda intacto.
    {
        std::vector<std::uint8_t> buf = BuildValidBuffer(1, 10, "bunny", "", false, 0, 0);
        buf[0] = 'X';
        std::uint32_t onDisk = 42;
        NIMVLETS_CHECK(!DeserializeAppState(buf.data(), buf.size(), decoded, error, &onDisk));
        NIMVLETS_CHECK(onDisk == 42);
    }
    return true;
}

// `language` sigue haciendo round-trip en v4.
bool TestV4LanguageRoundTrips() {
    AppState original;
    original.ownershipSeeded = true;
    original.ownedEntitlements = {OwnedEntitlement{"bunny", ""}};
    original.language = "es";

    const std::vector<std::uint8_t> bytes = SerializeAppState(original);
    AppState decoded;
    std::string error;
    NIMVLETS_CHECK(DeserializeAppState(bytes.data(), bytes.size(), decoded, error));
    NIMVLETS_CHECK(decoded.language == "es");
    NIMVLETS_CHECK(decoded == original);

    original.language.clear();
    const std::vector<std::uint8_t> bytes2 = SerializeAppState(original);
    AppState decoded2;
    NIMVLETS_CHECK(DeserializeAppState(bytes2.data(), bytes2.size(), decoded2, error));
    NIMVLETS_CHECK(decoded2.language.empty());
    NIMVLETS_CHECK(decoded2 == original);
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
    runner.Add("AppStateSerializer/V2BufferParsesLegacyOwnershipProvisionally",
               TestV2BufferParsesLegacyOwnershipProvisionally);
    runner.Add("AppStateSerializer/V3BufferParsesLegacyOwnershipProvisionally",
               TestV3BufferParsesLegacyOwnershipProvisionally);
    runner.Add("AppStateSerializer/V5RoundTrips", TestV5RoundTrips);
    runner.Add("AppStateSerializer/OnboardingLifecycleMigration", TestOnboardingLifecycleMigration);
    runner.Add("AppStateSerializer/TruncatedV5LifecycleByteRejected", TestTruncatedV5LifecycleByteRejected);
    runner.Add("AppStateSerializer/OwnedEntitlementsNormalizedOnSerialize", TestOwnedEntitlementsNormalizedOnSerialize);
    runner.Add("AppStateSerializer/V4TruncatedOwnershipIsRejected", TestV4TruncatedOwnershipIsRejected);
    runner.Add("AppStateSerializer/OnDiskSchemaVersionOutParam", TestOnDiskSchemaVersionOutParam);
    runner.Add("AppStateSerializer/V4LanguageRoundTrips", TestV4LanguageRoundTrips);
}

}  // namespace nimvlets::tests
