#include "AlphaMaskTest.h"

#include "core/AlphaMask.h"

#include <cstdint>

using nimvlets::core::AlphaMask;
using nimvlets::core::Point;

namespace nimvlets::tests {

namespace {

bool NewMaskIsFullyTransparent() {
    AlphaMask mask(4, 4);
    NIMVLETS_CHECK(!mask.Contains(Point{0.0, 0.0}));
    NIMVLETS_CHECK(!mask.Contains(Point{2.5, 2.5}));
    return true;
}

bool SetOpaqueMakesThatCellInteractive() {
    AlphaMask mask(4, 4);
    mask.SetOpaque(2, 1, true);
    NIMVLETS_CHECK(mask.Contains(Point{2.5, 1.5}));   // inside cell (2,1)
    NIMVLETS_CHECK(!mask.Contains(Point{1.5, 1.5}));  // inside cell (1,1), untouched
    return true;
}

bool ContainsFloorsToNearestCell() {
    AlphaMask mask(4, 4);
    mask.SetOpaque(1, 1, true);
    // Any point within [1,2)x[1,2) maps to cell (1,1).
    NIMVLETS_CHECK(mask.Contains(Point{1.0, 1.0}));
    NIMVLETS_CHECK(mask.Contains(Point{1.99, 1.99}));
    NIMVLETS_CHECK(!mask.Contains(Point{2.0, 1.0}));  // now cell (2,1), untouched
    return true;
}

bool OutOfBoundsIsNeverOpaque() {
    AlphaMask mask(4, 4);
    for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 4; ++y) {
            mask.SetOpaque(x, y, true);
        }
    }
    NIMVLETS_CHECK(!mask.Contains(Point{-0.5, 1.0}));
    NIMVLETS_CHECK(!mask.Contains(Point{1.0, -0.5}));
    NIMVLETS_CHECK(!mask.Contains(Point{4.0, 1.0}));   // exactly at width == out of bounds
    NIMVLETS_CHECK(!mask.Contains(Point{1.0, 4.0}));   // exactly at height == out of bounds
    NIMVLETS_CHECK(!mask.Contains(Point{100.0, 100.0}));
    return true;
}

bool SetOpaqueOutOfBoundsIsIgnoredNotUndefined() {
    // Must not crash/corrupt memory; simply a no-op.
    AlphaMask mask(4, 4);
    mask.SetOpaque(-1, 0, true);
    mask.SetOpaque(0, -1, true);
    mask.SetOpaque(4, 0, true);
    mask.SetOpaque(0, 4, true);
    NIMVLETS_CHECK(!mask.Contains(Point{0.0, 0.0}));
    return true;
}

bool SetOpaqueCanClearAPreviouslySetCell() {
    AlphaMask mask(4, 4);
    mask.SetOpaque(0, 0, true);
    NIMVLETS_CHECK(mask.Contains(Point{0.0, 0.0}));
    mask.SetOpaque(0, 0, false);
    NIMVLETS_CHECK(!mask.Contains(Point{0.0, 0.0}));
    return true;
}

bool DimensionsAreReportedCorrectly() {
    AlphaMask mask(7, 3);
    NIMVLETS_CHECK(mask.Width() == 7);
    NIMVLETS_CHECK(mask.Height() == 3);
    return true;
}

// --- FromAlphaChannel (Block 02: per-frame, threshold-configurable masks) ---

bool FromAlphaChannel_Threshold127Vs128Boundary() {
    // A single pixel with alpha exactly 127.
    const std::uint8_t pixel[4] = {255, 255, 255, 127};

    const AlphaMask maskAt128 = AlphaMask::FromAlphaChannel(pixel, 1, 1, 1, 1, /*alphaThreshold=*/128);
    NIMVLETS_CHECK(!maskAt128.Contains(Point{0.0, 0.0}));  // 127 < 128 -> transparent

    const AlphaMask maskAt127 = AlphaMask::FromAlphaChannel(pixel, 1, 1, 1, 1, /*alphaThreshold=*/127);
    NIMVLETS_CHECK(maskAt127.Contains(Point{0.0, 0.0}));  // 127 >= 127 -> opaque (inclusive)
    return true;
}

bool FromAlphaChannel_DifferentFramesProduceDifferentMasks() {
    const std::uint8_t opaqueFrame[2 * 2 * 4] = {
        0, 0, 0, 255, 0, 0, 0, 255,
        0, 0, 0, 255, 0, 0, 0, 255,
    };
    const std::uint8_t transparentFrame[2 * 2 * 4] = {
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
    };

    const AlphaMask maskA = AlphaMask::FromAlphaChannel(opaqueFrame, 2, 2, 2, 2, 128);
    const AlphaMask maskB = AlphaMask::FromAlphaChannel(transparentFrame, 2, 2, 2, 2, 128);

    NIMVLETS_CHECK(maskA.Contains(Point{0.5, 0.5}));
    NIMVLETS_CHECK(!maskB.Contains(Point{0.5, 0.5}));
    return true;
}

bool FromAlphaChannel_DifferentThresholdsSupportedPerPet() {
    // A pixel at alpha=150 — "visible" under a lenient (lower) threshold,
    // "transparent" under a strict (higher) one, on the exact same source
    // data. Demonstrates the threshold is a per-call parameter, not a
    // baked-in constant — see content::PetDefinition::alphaHitThreshold.
    const std::uint8_t pixel[4] = {10, 20, 30, 150};

    const AlphaMask lenient = AlphaMask::FromAlphaChannel(pixel, 1, 1, 1, 1, /*alphaThreshold=*/100);
    NIMVLETS_CHECK(lenient.Contains(Point{0.0, 0.0}));

    const AlphaMask strict = AlphaMask::FromAlphaChannel(pixel, 1, 1, 1, 1, /*alphaThreshold=*/200);
    NIMVLETS_CHECK(!strict.Contains(Point{0.0, 0.0}));
    return true;
}

bool FromAlphaChannel_UpsamplesWithNearestNeighbor() {
    // 2x2 source: opaque left column, transparent right column.
    const std::uint8_t src[2 * 2 * 4] = {
        0, 0, 0, 255, /* (1,0) */ 0, 0, 0, 0,
        0, 0, 0, 255, /* (1,1) */ 0, 0, 0, 0,
    };
    const AlphaMask mask = AlphaMask::FromAlphaChannel(src, 2, 2, /*targetWidth=*/4, /*targetHeight=*/4, 128);
    NIMVLETS_CHECK(mask.Width() == 4 && mask.Height() == 4);
    // Left half of the upsampled target should be opaque, right half transparent.
    NIMVLETS_CHECK(mask.Contains(Point{0.5, 0.5}));
    NIMVLETS_CHECK(mask.Contains(Point{1.5, 2.5}));
    NIMVLETS_CHECK(!mask.Contains(Point{2.5, 0.5}));
    NIMVLETS_CHECK(!mask.Contains(Point{3.5, 3.5}));
    return true;
}

bool FromAlphaChannel_DegenerateInputsProduceEmptyMaskNotCrash() {
    const std::uint8_t pixel[4] = {0, 0, 0, 255};
    const AlphaMask nullSrc = AlphaMask::FromAlphaChannel(nullptr, 1, 1, 2, 2, 128);
    NIMVLETS_CHECK(!nullSrc.Contains(Point{0.5, 0.5}));

    const AlphaMask zeroSize = AlphaMask::FromAlphaChannel(pixel, 0, 0, 2, 2, 128);
    NIMVLETS_CHECK(!zeroSize.Contains(Point{0.5, 0.5}));

    const AlphaMask zeroTarget = AlphaMask::FromAlphaChannel(pixel, 1, 1, 0, 0, 128);
    NIMVLETS_CHECK(zeroTarget.Width() == 0 && zeroTarget.Height() == 0);
    return true;
}

}  // namespace

void RegisterAlphaMaskTests(testing::TestRunner& runner) {
    runner.Add("AlphaMask/NewMaskIsFullyTransparent", NewMaskIsFullyTransparent);
    runner.Add("AlphaMask/SetOpaqueMakesThatCellInteractive", SetOpaqueMakesThatCellInteractive);
    runner.Add("AlphaMask/ContainsFloorsToNearestCell", ContainsFloorsToNearestCell);
    runner.Add("AlphaMask/OutOfBoundsIsNeverOpaque", OutOfBoundsIsNeverOpaque);
    runner.Add("AlphaMask/SetOpaqueOutOfBoundsIsIgnoredNotUndefined", SetOpaqueOutOfBoundsIsIgnoredNotUndefined);
    runner.Add("AlphaMask/SetOpaqueCanClearAPreviouslySetCell", SetOpaqueCanClearAPreviouslySetCell);
    runner.Add("AlphaMask/DimensionsAreReportedCorrectly", DimensionsAreReportedCorrectly);
    runner.Add("AlphaMask/FromAlphaChannel_Threshold127Vs128Boundary", FromAlphaChannel_Threshold127Vs128Boundary);
    runner.Add("AlphaMask/FromAlphaChannel_DifferentFramesProduceDifferentMasks", FromAlphaChannel_DifferentFramesProduceDifferentMasks);
    runner.Add("AlphaMask/FromAlphaChannel_DifferentThresholdsSupportedPerPet", FromAlphaChannel_DifferentThresholdsSupportedPerPet);
    runner.Add("AlphaMask/FromAlphaChannel_UpsamplesWithNearestNeighbor", FromAlphaChannel_UpsamplesWithNearestNeighbor);
    runner.Add("AlphaMask/FromAlphaChannel_DegenerateInputsProduceEmptyMaskNotCrash", FromAlphaChannel_DegenerateInputsProduceEmptyMaskNotCrash);
}

}  // namespace nimvlets::tests
