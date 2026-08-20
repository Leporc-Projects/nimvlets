#pragma once

// Hand-builds "NVPACK2" byte buffers (see src/content/PetPackLoader.cpp
// and docs/ANIMATION_RUNTIME.md for the exact on-disk format) so the
// C++ parser — and anything that loads a pack from disk, like
// catalog::LoadPetForIdentity — can be tested directly against
// synthetic valid/malformed data. Shared by PetPackLoaderTest.cpp and
// PetSwitchingTest.cpp so the byte-layout knowledge lives in one place.

#include <cstdint>
#include <string>
#include <vector>

namespace nimvlets::tests::nvpack2 {

inline void AppendBytes(std::vector<std::uint8_t>& buf, const void* data, std::size_t n) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    buf.insert(buf.end(), bytes, bytes + n);
}

inline void AppendUint8(std::vector<std::uint8_t>& buf, std::uint8_t v) {
    buf.push_back(v);
}

inline void AppendUint32(std::vector<std::uint8_t>& buf, std::uint32_t v) {
    AppendBytes(buf, &v, sizeof(v));
}

inline void AppendFloat64(std::vector<std::uint8_t>& buf, double v) {
    AppendBytes(buf, &v, sizeof(v));
}

inline void AppendString(std::vector<std::uint8_t>& buf, const std::string& s) {
    AppendUint32(buf, static_cast<std::uint32_t>(s.size()));
    AppendBytes(buf, s.data(), s.size());
}

inline void AppendMagic(std::vector<std::uint8_t>& buf) {
    const char magic[8] = {'N', 'V', 'P', 'A', 'C', 'K', '2', '\0'};
    AppendBytes(buf, magic, sizeof(magic));
}

// AnimationBlock: id, kind, fps(0 -> per-frame duration), returnsToIdle,
// frameCount, frames[{w,h,anchorX=0,anchorY=0,durationMs,pixels}].
// Every frame is `frameCount` copies of a single w*h RGBA8 buffer
// filled with `fillByte` (alpha forced to 255) — enough to identify
// which animation/frame ended up where without caring about real art.
inline void AppendAnimationBlock(
    std::vector<std::uint8_t>& buf, const std::string& id, std::uint8_t kindByte, std::uint32_t frameCount, std::uint32_t w,
    std::uint32_t h, std::uint8_t fillByte, double durationMs = 100.0, bool returnsToIdle = true) {
    AppendString(buf, id);
    AppendUint8(buf, kindByte);
    AppendFloat64(buf, 0.0);  // fps == 0 -> use per-frame durationMs
    AppendUint8(buf, returnsToIdle ? 1 : 0);
    AppendUint32(buf, frameCount);
    const std::vector<std::uint8_t> px = {fillByte, fillByte, fillByte, 255};
    for (std::uint32_t i = 0; i < frameCount; ++i) {
        AppendUint32(buf, w);
        AppendUint32(buf, h);
        AppendFloat64(buf, 0.0);  // anchorX
        AppendFloat64(buf, 0.0);  // anchorY
        AppendFloat64(buf, durationMs);
        for (std::uint32_t p = 0; p < w * h; ++p) {
            AppendBytes(buf, px.data(), px.size());
        }
    }
}

// DirectionalAnimationOverride[] section: count + {direction, AnimationBlock} * count.
inline void AppendEmptyDirectionOverrides(std::vector<std::uint8_t>& buf) {
    AppendUint32(buf, 0);
}

inline void AppendOneDirectionOverride(
    std::vector<std::uint8_t>& buf, std::uint8_t directionByte, const std::string& id, std::uint8_t kindByte,
    std::uint32_t frameCount, std::uint32_t w, std::uint32_t h, std::uint8_t fillByte) {
    AppendUint32(buf, 1);
    AppendUint8(buf, directionByte);
    AppendAnimationBlock(buf, id, kindByte, frameCount, w, h, fillByte);
}

// WeightedActionBlock: id, weight, targetStateId, AnimationBlock,
// directionOverrideCount(0, unless the caller appends its own after).
inline void AppendWeightedActionHeader(
    std::vector<std::uint8_t>& buf, const std::string& id, double weight, const std::string& targetStateId) {
    AppendString(buf, id);
    AppendFloat64(buf, weight);
    AppendString(buf, targetStateId);
}

// A complete single-state pack ("Bunny/Nidir-shaped"): one
// BehaviorState "default" with a static 1-frame base pose, one click
// action (self-loop), no ambient/hover actions. `fillByte` identifies
// the base pose's pixel so a test can confirm exactly which pack ended
// up loaded.
inline std::vector<std::uint8_t> BuildMinimalPackBytes(
    const std::string& id, std::uint8_t fillByte, std::uint32_t canvasW = 1, std::uint32_t canvasH = 1) {
    std::vector<std::uint8_t> buf;
    AppendMagic(buf);
    AppendString(buf, id);
    AppendString(buf, id);  // displayName, doesn't matter for these tests
    AppendString(buf, "");  // variantGroup
    AppendUint32(buf, canvasW);
    AppendUint32(buf, canvasH);
    AppendUint8(buf, 128);      // alphaHitThreshold
    AppendFloat64(buf, 1.0);    // visualScale
    AppendString(buf, "");      // contentVersion

    AppendUint32(buf, 1);  // stateCount
    AppendString(buf, "default");
    AppendAnimationBlock(buf, "base", 0 /*static*/, 1, canvasW, canvasH, fillByte);
    AppendEmptyDirectionOverrides(buf);           // baseAnimationDirectionOverrides
    AppendFloat64(buf, 300.0);                    // ambientIntervalSeconds
    AppendUint32(buf, 0);                         // ambientActionCount
    AppendUint8(buf, 1);                           // hoverUsesAmbientActions = true
    AppendUint32(buf, 0);                         // hoverActionCount
    AppendUint32(buf, 1);                         // clickActionCount
    AppendWeightedActionHeader(buf, "click", 1.0, "default");
    AppendAnimationBlock(buf, "click_anim", 0 /*static*/, 1, canvasW, canvasH, fillByte);
    AppendEmptyDirectionOverrides(buf);           // click action's own directionOverrides

    return buf;
}

// Variant carrying a real kLeft override on the base pose ("Nidir-
// shaped") — everything else identical to BuildMinimalPackBytes().
inline std::vector<std::uint8_t> BuildDirectionalPackBytes(
    const std::string& id, std::uint8_t rightFillByte, std::uint8_t leftFillByte, std::uint32_t canvasW = 1,
    std::uint32_t canvasH = 1) {
    std::vector<std::uint8_t> buf;
    AppendMagic(buf);
    AppendString(buf, id);
    AppendString(buf, id);
    AppendString(buf, "");
    AppendUint32(buf, canvasW);
    AppendUint32(buf, canvasH);
    AppendUint8(buf, 128);
    AppendFloat64(buf, 1.0);
    AppendString(buf, "");

    AppendUint32(buf, 1);  // stateCount
    AppendString(buf, "default");
    AppendAnimationBlock(buf, "base", 0, 1, canvasW, canvasH, rightFillByte);
    AppendOneDirectionOverride(buf, 1 /*kLeft*/, "base_left", 0, 1, canvasW, canvasH, leftFillByte);
    AppendFloat64(buf, 300.0);
    AppendUint32(buf, 0);
    AppendUint8(buf, 1);
    AppendUint32(buf, 0);
    AppendUint32(buf, 1);
    AppendWeightedActionHeader(buf, "click", 1.0, "default");
    AppendAnimationBlock(buf, "click_anim", 0, 1, canvasW, canvasH, rightFillByte);
    AppendEmptyDirectionOverrides(buf);

    return buf;
}

}  // namespace nimvlets::tests::nvpack2
