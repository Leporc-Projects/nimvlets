#include "PetPackLoaderTest.h"

#include "NvPack2TestBuilder.h"
#include "content/PetPackLoader.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using nimvlets::content::Direction;
using nimvlets::content::LoadPetPackFromMemory;
using nimvlets::content::PetDefinition;

namespace nimvlets::tests {

namespace {

using namespace nvpack2;  // AppendBytes/AppendUint8/AppendUint32/AppendFloat64/AppendString/AppendMagic/...

bool ValidMinimalPackLoadsSuccessfully() {
    const std::vector<std::uint8_t> bytes = BuildMinimalPackBytes("p", 0x00, 2, 2);
    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(LoadPetPackFromMemory(bytes.data(), bytes.size(), pet, error));
    NIMVLETS_CHECK(error.empty());
    NIMVLETS_CHECK(pet.id == "p");
    NIMVLETS_CHECK(pet.canvasWidth == 2 && pet.canvasHeight == 2);
    NIMVLETS_CHECK(pet.alphaHitThreshold == 128);
    NIMVLETS_CHECK(pet.visualScale == 1.0);
    NIMVLETS_CHECK(pet.states.size() == 1);
    NIMVLETS_CHECK(pet.states[0].id == "default");
    NIMVLETS_CHECK(pet.states[0].baseAnimation.frames.size() == 1);
    NIMVLETS_CHECK(pet.states[0].clickActions.size() == 1);
    NIMVLETS_CHECK(pet.states[0].ambientActions.empty());
    NIMVLETS_CHECK(pet.states[0].hoverUsesAmbientActions);
    return true;
}

bool BadMagicIsRejected() {
    std::vector<std::uint8_t> bytes = BuildMinimalPackBytes("p", 0x00);
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
    std::vector<std::uint8_t> bytes = BuildMinimalPackBytes("p", 0x00);
    bytes.resize(bytes.size() / 2);  // cut off partway through — must fail, not read garbage
    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(!LoadPetPackFromMemory(bytes.data(), bytes.size(), pet, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

bool TruncatedMidPixelDataIsRejected() {
    std::vector<std::uint8_t> bytes = BuildMinimalPackBytes("p", 0x00);
    bytes.pop_back();  // drop the very last pixel byte only
    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(!LoadPetPackFromMemory(bytes.data(), bytes.size(), pet, error));
    return true;
}

bool MismatchedFrameDimensionsAreRejected() {
    std::vector<std::uint8_t> buf;
    AppendMagic(buf);
    AppendString(buf, "p");
    AppendString(buf, "p");
    AppendString(buf, "");
    AppendUint32(buf, 1);
    AppendUint32(buf, 1);
    AppendUint8(buf, 128);
    AppendFloat64(buf, 1.0);
    AppendString(buf, "");
    AppendUint32(buf, 1);  // stateCount
    AppendString(buf, "default");

    // base animation with two frames of DIFFERENT dimensions.
    AppendString(buf, "base");
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
    AppendMagic(buf);
    AppendString(buf, "p");
    AppendString(buf, "p");
    AppendString(buf, "");
    AppendUint32(buf, 1);
    AppendUint32(buf, 1);
    AppendUint8(buf, 128);
    AppendFloat64(buf, 1.0);
    AppendString(buf, "");
    AppendUint32(buf, 1);
    AppendString(buf, "default");
    AppendString(buf, "base");
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
    AppendMagic(buf);
    AppendString(buf, "p");
    AppendString(buf, "p");
    AppendString(buf, "");
    AppendUint32(buf, 1);
    AppendUint32(buf, 1);
    AppendUint8(buf, 128);
    AppendFloat64(buf, 1.0);
    AppendString(buf, "");
    AppendUint32(buf, 1);
    AppendString(buf, "default");
    AppendString(buf, "base");
    AppendUint8(buf, 99);  // not a valid PlaybackKind value (0/1/2)

    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(!LoadPetPackFromMemory(buf.data(), buf.size(), pet, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

bool FramesLoadInDeterministicOrder() {
    std::vector<std::uint8_t> buf;
    AppendMagic(buf);
    AppendString(buf, "p");
    AppendString(buf, "p");
    AppendString(buf, "");
    AppendUint32(buf, 1);
    AppendUint32(buf, 1);
    AppendUint8(buf, 128);
    AppendFloat64(buf, 1.0);
    AppendString(buf, "");
    AppendUint32(buf, 1);
    AppendString(buf, "default");
    AppendAnimationBlock(buf, "base", 0, 1, 1, 1, 0x00);
    AppendEmptyDirectionOverrides(buf);
    AppendFloat64(buf, 300.0);
    AppendUint32(buf, 0);  // ambientActionCount
    AppendUint8(buf, 1);   // hoverUsesAmbientActions
    AppendUint32(buf, 0);  // hoverActionCount
    AppendUint32(buf, 1);  // clickActionCount
    AppendWeightedActionHeader(buf, "click", 1.0, "default");
    // click animation with 3 frames, each tagged with a distinct fill byte.
    AppendString(buf, "click_anim");
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
    AppendEmptyDirectionOverrides(buf);

    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(LoadPetPackFromMemory(buf.data(), buf.size(), pet, error));
    const auto& frames = pet.states[0].clickActions[0].animation.frames;
    NIMVLETS_CHECK(frames.size() == 3);
    NIMVLETS_CHECK(frames[0].pixels[0] == 10);
    NIMVLETS_CHECK(frames[1].pixels[0] == 20);
    NIMVLETS_CHECK(frames[2].pixels[0] == 30);
    return true;
}

bool PackWithoutDirectionOverridesLeavesThemEmpty() {
    const std::vector<std::uint8_t> bytes = BuildMinimalPackBytes("p", 0x00);
    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(LoadPetPackFromMemory(bytes.data(), bytes.size(), pet, error));
    NIMVLETS_CHECK(pet.states[0].baseAnimationDirectionOverrides.empty());
    return true;
}

bool PackWithDirectionalBaseOverrideLoadsCorrectly() {
    const std::vector<std::uint8_t> bytes = BuildDirectionalPackBytes("p", 0x00, 0x42);
    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(LoadPetPackFromMemory(bytes.data(), bytes.size(), pet, error));
    NIMVLETS_CHECK(error.empty());
    NIMVLETS_CHECK(pet.states[0].baseAnimation.frames[0].pixels[0] == 0x00);  // canónico (kRight) sin tocar
    NIMVLETS_CHECK(pet.states[0].baseAnimationDirectionOverrides.size() == 1);
    NIMVLETS_CHECK(pet.states[0].baseAnimationDirectionOverrides[0].direction == Direction::kLeft);
    NIMVLETS_CHECK(pet.states[0].baseAnimationDirectionOverrides[0].animation.frames[0].pixels[0] == 0x42);
    return true;
}

bool InvalidDirectionByteInOverrideIsRejected() {
    std::vector<std::uint8_t> buf;
    AppendMagic(buf);
    AppendString(buf, "p");
    AppendString(buf, "p");
    AppendString(buf, "");
    AppendUint32(buf, 1);
    AppendUint32(buf, 1);
    AppendUint8(buf, 128);
    AppendFloat64(buf, 1.0);
    AppendString(buf, "");
    AppendUint32(buf, 1);
    AppendString(buf, "default");
    AppendAnimationBlock(buf, "base", 0, 1, 1, 1, 0x00);
    AppendUint32(buf, 1);  // baseAnimationDirectionOverrideCount = 1
    AppendUint8(buf, 99);  // direction inválida (solo 0/1 son válidos)

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
    AppendString(buf, "p");
    AppendString(buf, "");
    AppendUint32(buf, 0);  // canvasWidth == 0, invalid
    AppendUint32(buf, 2);
    AppendUint8(buf, 128);
    AppendFloat64(buf, 1.0);
    AppendString(buf, "");
    AppendUint32(buf, 1);
    AppendString(buf, "default");
    AppendAnimationBlock(buf, "base", 0, 1, 1, 1, 0x00);
    AppendEmptyDirectionOverrides(buf);
    AppendFloat64(buf, 300.0);
    AppendUint32(buf, 0);
    AppendUint8(buf, 1);
    AppendUint32(buf, 0);
    AppendUint32(buf, 0);  // clickActionCount = 0 (no click needed for this check)

    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(!LoadPetPackFromMemory(buf.data(), buf.size(), pet, error));
    return true;
}

bool InvalidVisualScaleIsRejected() {
    std::vector<std::uint8_t> buf = BuildMinimalPackBytes("p", 0x00);
    // visualScale lives right after alphaHitThreshold in the header —
    // overwrite it in place with 0.0 (invalid, must be > 0).
    // Layout: magic(8) + string("p")=4+1 + string("p")=4+1 +
    // string("")=4 + canvasW(4) + canvasH(4) + alphaThreshold(1) + visualScale(8 @ this offset).
    const std::size_t visualScaleOffset = 8 + (4 + 1) + (4 + 1) + 4 + 4 + 4 + 1;
    double zero = 0.0;
    std::memcpy(buf.data() + visualScaleOffset, &zero, sizeof(zero));

    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(!LoadPetPackFromMemory(buf.data(), buf.size(), pet, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

bool UnknownTargetStateIdIsRejected() {
    std::vector<std::uint8_t> buf;
    AppendMagic(buf);
    AppendString(buf, "p");
    AppendString(buf, "p");
    AppendString(buf, "");
    AppendUint32(buf, 1);
    AppendUint32(buf, 1);
    AppendUint8(buf, 128);
    AppendFloat64(buf, 1.0);
    AppendString(buf, "");
    AppendUint32(buf, 1);  // stateCount = 1 -- but the click action below targets a state that doesn't exist
    AppendString(buf, "default");
    AppendAnimationBlock(buf, "base", 0, 1, 1, 1, 0x00);
    AppendEmptyDirectionOverrides(buf);
    AppendFloat64(buf, 300.0);
    AppendUint32(buf, 0);
    AppendUint8(buf, 1);
    AppendUint32(buf, 0);
    AppendUint32(buf, 1);  // clickActionCount = 1
    AppendWeightedActionHeader(buf, "click", 1.0, "nonexistent_state");
    AppendAnimationBlock(buf, "click_anim", 0, 1, 1, 1, 0x00);
    AppendEmptyDirectionOverrides(buf);

    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(!LoadPetPackFromMemory(buf.data(), buf.size(), pet, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

bool HoverUsingAmbientPoolWithNonEmptyHoverActionsIsRejected() {
    std::vector<std::uint8_t> buf;
    AppendMagic(buf);
    AppendString(buf, "p");
    AppendString(buf, "p");
    AppendString(buf, "");
    AppendUint32(buf, 1);
    AppendUint32(buf, 1);
    AppendUint8(buf, 128);
    AppendFloat64(buf, 1.0);
    AppendString(buf, "");
    AppendUint32(buf, 1);
    AppendString(buf, "default");
    AppendAnimationBlock(buf, "base", 0, 1, 1, 1, 0x00);
    AppendEmptyDirectionOverrides(buf);
    AppendFloat64(buf, 300.0);
    AppendUint32(buf, 0);
    AppendUint8(buf, 1);   // hoverUsesAmbientActions = true ...
    AppendUint32(buf, 1);  // ... but hoverActionCount = 1 too -- ambiguous, must be rejected
    AppendWeightedActionHeader(buf, "hover", 1.0, "default");
    AppendAnimationBlock(buf, "hover_anim", 0, 1, 1, 1, 0x00);
    AppendEmptyDirectionOverrides(buf);
    AppendUint32(buf, 0);  // clickActionCount

    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(!LoadPetPackFromMemory(buf.data(), buf.size(), pet, error));
    NIMVLETS_CHECK(!error.empty());
    return true;
}

// Un pack "forma Frin": dos estados (seated/lying) con una transición
// real de seated -> lying, ejercitando stateCount > 1 y un
// targetStateId que SÍ es distinto del estado de origen.
bool MultiStatePackWithRealTransitionLoadsCorrectly() {
    std::vector<std::uint8_t> buf;
    AppendMagic(buf);
    AppendString(buf, "frin_test");
    AppendString(buf, "Frin Test");
    AppendString(buf, "frin");
    AppendUint32(buf, 4);
    AppendUint32(buf, 4);
    AppendUint8(buf, 128);
    AppendFloat64(buf, 1.0);
    AppendString(buf, "");
    AppendUint32(buf, 2);  // stateCount = 2

    // seated: base + ambient(sit_to_lie -> lying) + click(howl -> seated)
    AppendString(buf, "seated");
    AppendAnimationBlock(buf, "seated_base", 0, 1, 1, 1, 0x01);
    AppendEmptyDirectionOverrides(buf);
    AppendFloat64(buf, 200.0);
    AppendUint32(buf, 1);  // ambientActionCount
    AppendWeightedActionHeader(buf, "sit_to_lie", 1.0, "lying");
    AppendAnimationBlock(buf, "sit_to_lie_anim", 2, 1, 1, 1, 0x02);
    AppendEmptyDirectionOverrides(buf);
    AppendUint8(buf, 0);   // hoverUsesAmbientActions = false
    AppendUint32(buf, 0);  // hoverActionCount
    AppendUint32(buf, 1);  // clickActionCount
    AppendWeightedActionHeader(buf, "howl", 1.0, "seated");
    AppendAnimationBlock(buf, "howl_anim", 2, 1, 1, 1, 0x03);
    AppendEmptyDirectionOverrides(buf);

    // lying: base only, click(lie_to_sit -> seated), sin ambient.
    AppendString(buf, "lying");
    AppendAnimationBlock(buf, "lying_base", 0, 1, 1, 1, 0x04);
    AppendEmptyDirectionOverrides(buf);
    AppendFloat64(buf, 999.0);
    AppendUint32(buf, 0);  // ambientActionCount = 0 -- sin timer mientras lying
    AppendUint8(buf, 0);
    AppendUint32(buf, 0);
    AppendUint32(buf, 1);
    AppendWeightedActionHeader(buf, "lie_to_sit", 1.0, "seated");
    AppendAnimationBlock(buf, "lie_to_sit_anim", 2, 1, 1, 1, 0x05);
    AppendEmptyDirectionOverrides(buf);

    PetDefinition pet;
    std::string error;
    NIMVLETS_CHECK(LoadPetPackFromMemory(buf.data(), buf.size(), pet, error));
    NIMVLETS_CHECK(error.empty());
    NIMVLETS_CHECK(pet.states.size() == 2);
    NIMVLETS_CHECK(pet.states[0].id == "seated");
    NIMVLETS_CHECK(pet.states[0].ambientActions[0].targetStateId == "lying");
    NIMVLETS_CHECK(pet.states[0].clickActions[0].targetStateId == "seated");
    NIMVLETS_CHECK(pet.states[1].id == "lying");
    NIMVLETS_CHECK(pet.states[1].ambientActions.empty());
    NIMVLETS_CHECK(pet.states[1].clickActions[0].targetStateId == "seated");
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
    runner.Add("PetPackLoader/InvalidVisualScaleIsRejected", InvalidVisualScaleIsRejected);
    runner.Add("PetPackLoader/PackWithoutDirectionOverridesLeavesThemEmpty", PackWithoutDirectionOverridesLeavesThemEmpty);
    runner.Add("PetPackLoader/PackWithDirectionalBaseOverrideLoadsCorrectly", PackWithDirectionalBaseOverrideLoadsCorrectly);
    runner.Add("PetPackLoader/InvalidDirectionByteInOverrideIsRejected", InvalidDirectionByteInOverrideIsRejected);
    runner.Add("PetPackLoader/UnknownTargetStateIdIsRejected", UnknownTargetStateIdIsRejected);
    runner.Add(
        "PetPackLoader/HoverUsingAmbientPoolWithNonEmptyHoverActionsIsRejected", HoverUsingAmbientPoolWithNonEmptyHoverActionsIsRejected);
    runner.Add("PetPackLoader/MultiStatePackWithRealTransitionLoadsCorrectly", MultiStatePackWithRealTransitionLoadsCorrectly);
}

}  // namespace nimvlets::tests
