#include "PetPackLoaderTest.h"

#include "content/PetPackLoader.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using nimvlets::content::LoadPetPackFromMemory;
using nimvlets::content::PetDefinition;

namespace nimvlets::tests {

namespace {

// Hand-builds pack byte buffers matching the exact "NVPACK1" format
// (see src/content/PetPackLoader.cpp and docs/ANIMATION_RUNTIME.md) so
// the C++ parser can be tested directly against synthetic valid *and*
// malformed data — no filesystem, no Python compiler, no CWD
// dependency.

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

void AppendMagic(std::vector<std::uint8_t>& buf) {
    const char magic[8] = {'N', 'V', 'P', 'A', 'C', 'K', '1', '\0'};
    AppendBytes(buf, magic, sizeof(magic));
}

void AppendPetHeader(std::vector<std::uint8_t>& buf, const std::string& id = "p") {
    AppendMagic(buf);
    AppendString(buf, id);
    AppendString(buf, "P");
    AppendString(buf, "");  // variantGroup
    AppendUint32(buf, 2);   // canvasWidth
    AppendUint32(buf, 2);   // canvasHeight
    AppendUint8(buf, 128);  // alphaHitThreshold
    AppendFloat64(buf, 300.0);  // passiveIntervalSeconds
    AppendString(buf, "");  // contentVersion
}

// Appends one AnimationBlock with `frameCount` frames, all `w` x `h`,
// each frame's pixels filled with `fillByte` (so tests can identify
// which frame ended up where after loading).
void AppendAnimation(
    std::vector<std::uint8_t>& buf,
    const std::string& id,
    std::uint8_t kind,
    std::uint32_t frameCount,
    std::uint32_t w,
    std::uint32_t h,
    std::uint8_t fillByte) {
    AppendString(buf, id);
    AppendUint8(buf, kind);
    AppendFloat64(buf, 0.0);  // fps == 0 -> use per-frame durationMs
    AppendUint8(buf, 1);      // returnsToIdle
    AppendUint32(buf, frameCount);
    for (std::uint32_t i = 0; i < frameCount; ++i) {
        AppendUint32(buf, w);
        AppendUint32(buf, h);
        AppendFloat64(buf, 0.0);    // anchorX
        AppendFloat64(buf, 0.0);    // anchorY
        AppendFloat64(buf, 100.0);  // durationMs
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(w) * h * 4, fillByte);
        AppendBytes(buf, pixels.data(), pixels.size());
    }
}

std::vector<std::uint8_t> BuildMinimalValidPack() {
    std::vector<std::uint8_t> buf;
    AppendPetHeader(buf);
    AppendAnimation(buf, "idle", 0 /*static*/, 1, 1, 1, 0x00);
    AppendAnimation(buf, "click_reaction", 2 /*one-shot*/, 1, 1, 1, 0xFF);
    AppendUint32(buf, 0);  // zero passive actions
    return buf;
}

bool ValidMinimalPackLoadsSuccessfully() {
    const std::vector<std::uint8_t> bytes = BuildMinimalValidPack();
    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(LoadPetPackFromMemory(bytes.data(), bytes.size(), pet, error));
    NIMVLETS_CHECK(error.empty());
    NIMVLETS_CHECK(pet.id == "p");
    NIMVLETS_CHECK(pet.canvasWidth == 2 && pet.canvasHeight == 2);
    NIMVLETS_CHECK(pet.alphaHitThreshold == 128);
    NIMVLETS_CHECK(pet.idle.frames.size() == 1);
    NIMVLETS_CHECK(pet.clickReaction.frames.size() == 1);
    NIMVLETS_CHECK(pet.passiveActions.empty());
    return true;
}

bool BadMagicIsRejected() {
    std::vector<std::uint8_t> bytes = BuildMinimalValidPack();
    bytes[0] = 'X';
    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(!LoadPetPackFromMemory(bytes.data(), bytes.size(), pet, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

bool EmptyBufferIsRejected() {
    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(!LoadPetPackFromMemory(nullptr, 0, pet, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

bool TruncatedDataIsRejected() {
    std::vector<std::uint8_t> bytes = BuildMinimalValidPack();
    bytes.resize(bytes.size() / 2);  // cut off partway through — must fail, not read garbage
    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(!LoadPetPackFromMemory(bytes.data(), bytes.size(), pet, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

bool TruncatedMidPixelDataIsRejected() {
    std::vector<std::uint8_t> bytes = BuildMinimalValidPack();
    bytes.pop_back();  // drop the very last pixel byte only
    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(!LoadPetPackFromMemory(bytes.data(), bytes.size(), pet, error));
    return true;
}

bool MismatchedFrameDimensionsAreRejected() {
    std::vector<std::uint8_t> buf;
    AppendPetHeader(buf);

    // idle animation with two frames of DIFFERENT dimensions.
    AppendString(buf, "idle");
    AppendUint8(buf, 1);  // loop; playback kind is irrelevant to this check
    AppendFloat64(buf, 0.0);
    AppendUint8(buf, 1);
    AppendUint32(buf, 2);  // frameCount = 2
    AppendUint32(buf, 1);
    AppendUint32(buf, 1);  // frame 0: 1x1
    AppendFloat64(buf, 0.0);
    AppendFloat64(buf, 0.0);
    AppendFloat64(buf, 100.0);
    std::vector<std::uint8_t> px1(1 * 1 * 4, 0);
    AppendBytes(buf, px1.data(), px1.size());
    AppendUint32(buf, 2);
    AppendUint32(buf, 2);  // frame 1: 2x2 — MISMATCH
    AppendFloat64(buf, 0.0);
    AppendFloat64(buf, 0.0);
    AppendFloat64(buf, 100.0);
    std::vector<std::uint8_t> px2(2 * 2 * 4, 0);
    AppendBytes(buf, px2.data(), px2.size());

    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(!LoadPetPackFromMemory(buf.data(), buf.size(), pet, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

bool ZeroFrameAnimationIsRejected() {
    std::vector<std::uint8_t> buf;
    AppendPetHeader(buf);
    AppendString(buf, "idle");
    AppendUint8(buf, 0);
    AppendFloat64(buf, 0.0);
    AppendUint8(buf, 1);
    AppendUint32(buf, 0);  // frameCount = 0 — an animation with no frames is malformed

    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(!LoadPetPackFromMemory(buf.data(), buf.size(), pet, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

bool InvalidPlaybackKindByteIsRejected() {
    std::vector<std::uint8_t> buf;
    AppendPetHeader(buf);
    AppendString(buf, "idle");
    AppendUint8(buf, 99);  // not a valid PlaybackKind value (0/1/2)
    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(!LoadPetPackFromMemory(buf.data(), buf.size(), pet, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

bool FramesLoadInDeterministicOrder() {
    std::vector<std::uint8_t> buf;
    AppendPetHeader(buf);
    AppendAnimation(buf, "idle", 0, 1, 1, 1, 0x00);

    // click reaction with 3 frames, each tagged with a distinct fill
    // byte so load order can be checked precisely.
    AppendString(buf, "click_reaction");
    AppendUint8(buf, 2);  // one-shot
    AppendFloat64(buf, 0.0);
    AppendUint8(buf, 1);
    AppendUint32(buf, 3);
    for (std::uint8_t fill : {static_cast<std::uint8_t>(10), static_cast<std::uint8_t>(20), static_cast<std::uint8_t>(30)}) {
        AppendUint32(buf, 1);
        AppendUint32(buf, 1);
        AppendFloat64(buf, 0.0);
        AppendFloat64(buf, 0.0);
        AppendFloat64(buf, 100.0);
        const std::vector<std::uint8_t> px = {fill, fill, fill, 255};
        AppendBytes(buf, px.data(), px.size());
    }
    AppendUint32(buf, 0);

    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(LoadPetPackFromMemory(buf.data(), buf.size(), pet, error));
    NIMVLETS_CHECK(pet.clickReaction.frames.size() == 3);
    NIMVLETS_CHECK(pet.clickReaction.frames[0].pixels[0] == 10);
    NIMVLETS_CHECK(pet.clickReaction.frames[1].pixels[0] == 20);
    NIMVLETS_CHECK(pet.clickReaction.frames[2].pixels[0] == 30);
    return true;
}

// Block 04.2: la sección final opcional de idleDirectionOverrides (ver
// docs/NIDIR_CONTENT.md). BuildMinimalValidPack() de arriba NO la
// incluye -- cada test que la usa (todos los de arriba de este
// comentario) ya ejercita implícitamente "un pack sin la sección
// nueva carga igual que antes"; el test explícito de abajo lo deja
// documentado como un caso con nombre, no solo incidental.

bool PackWithoutTrailingSectionLeavesOverridesEmpty() {
    const std::vector<std::uint8_t> bytes = BuildMinimalValidPack();
    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(LoadPetPackFromMemory(bytes.data(), bytes.size(), pet, error));
    NIMVLETS_CHECK(pet.idleDirectionOverrides.empty());
    return true;
}

bool PackWithDirectionalIdleOverrideLoadsCorrectly() {
    std::vector<std::uint8_t> buf = BuildMinimalValidPack();  // idle fillByte=0x00, click fillByte=0xFF
    AppendUint32(buf, 1);  // directionalIdleOverrideCount = 1
    AppendUint8(buf, 1);   // direction = kLeft
    AppendAnimation(buf, "idle_left", 1 /*loop*/, 1, 1, 1, 0x42);

    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(LoadPetPackFromMemory(buf.data(), buf.size(), pet, error));
    NIMVLETS_CHECK(error.empty());
    NIMVLETS_CHECK(pet.idle.frames[0].pixels[0] == 0x00);  // idle canónico (kRight) sin tocar
    NIMVLETS_CHECK(pet.idleDirectionOverrides.size() == 1);
    NIMVLETS_CHECK(pet.idleDirectionOverrides[0].direction == nimvlets::content::Direction::kLeft);
    NIMVLETS_CHECK(pet.idleDirectionOverrides[0].animation.frames[0].pixels[0] == 0x42);
    return true;
}

bool InvalidDirectionByteInOverrideIsRejected() {
    std::vector<std::uint8_t> buf = BuildMinimalValidPack();
    AppendUint32(buf, 1);   // directionalIdleOverrideCount = 1
    AppendUint8(buf, 99);   // direction inválida (solo 0/1 son válidos)

    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(!LoadPetPackFromMemory(buf.data(), buf.size(), pet, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

bool InvalidCanvasSizeIsRejected() {
    std::vector<std::uint8_t> buf;
    AppendMagic(buf);
    AppendString(buf, "p");
    AppendString(buf, "P");
    AppendString(buf, "");
    AppendUint32(buf, 0);  // canvasWidth == 0, invalid
    AppendUint32(buf, 2);
    AppendUint8(buf, 128);
    AppendFloat64(buf, 300.0);
    AppendString(buf, "");
    AppendAnimation(buf, "idle", 0, 1, 1, 1, 0x00);
    AppendAnimation(buf, "click_reaction", 2, 1, 1, 1, 0x00);
    AppendUint32(buf, 0);

    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(!LoadPetPackFromMemory(buf.data(), buf.size(), pet, error));
    return true;
}

}  // namespace

void RegisterPetPackLoaderTests(testing::TestRunner& runner) {
    runner.Add("PetPackLoader/ValidMinimalPackLoadsSuccessfully", ValidMinimalPackLoadsSuccessfully);
    runner.Add("PetPackLoader/BadMagicIsRejected", BadMagicIsRejected);
    runner.Add("PetPackLoader/EmptyBufferIsRejected", EmptyBufferIsRejected);
    runner.Add("PetPackLoader/TruncatedDataIsRejected", TruncatedDataIsRejected);
    runner.Add("PetPackLoader/TruncatedMidPixelDataIsRejected", TruncatedMidPixelDataIsRejected);
    runner.Add("PetPackLoader/MismatchedFrameDimensionsAreRejected", MismatchedFrameDimensionsAreRejected);
    runner.Add("PetPackLoader/ZeroFrameAnimationIsRejected", ZeroFrameAnimationIsRejected);
    runner.Add("PetPackLoader/InvalidPlaybackKindByteIsRejected", InvalidPlaybackKindByteIsRejected);
    runner.Add("PetPackLoader/FramesLoadInDeterministicOrder", FramesLoadInDeterministicOrder);
    runner.Add("PetPackLoader/InvalidCanvasSizeIsRejected", InvalidCanvasSizeIsRejected);
    runner.Add("PetPackLoader/PackWithoutTrailingSectionLeavesOverridesEmpty", PackWithoutTrailingSectionLeavesOverridesEmpty);
    runner.Add("PetPackLoader/PackWithDirectionalIdleOverrideLoadsCorrectly", PackWithDirectionalIdleOverrideLoadsCorrectly);
    runner.Add("PetPackLoader/InvalidDirectionByteInOverrideIsRejected", InvalidDirectionByteInOverrideIsRejected);
}

}  // namespace nimvlets::tests
