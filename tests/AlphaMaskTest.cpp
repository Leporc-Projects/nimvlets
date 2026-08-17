#include "AlphaMaskTest.h"

#include "core/AlphaMask.h"

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

}  // namespace

void RegisterAlphaMaskTests(testing::TestRunner& runner) {
    runner.Add("AlphaMask/NewMaskIsFullyTransparent", NewMaskIsFullyTransparent);
    runner.Add("AlphaMask/SetOpaqueMakesThatCellInteractive", SetOpaqueMakesThatCellInteractive);
    runner.Add("AlphaMask/ContainsFloorsToNearestCell", ContainsFloorsToNearestCell);
    runner.Add("AlphaMask/OutOfBoundsIsNeverOpaque", OutOfBoundsIsNeverOpaque);
    runner.Add("AlphaMask/SetOpaqueOutOfBoundsIsIgnoredNotUndefined", SetOpaqueOutOfBoundsIsIgnoredNotUndefined);
    runner.Add("AlphaMask/SetOpaqueCanClearAPreviouslySetCell", SetOpaqueCanClearAPreviouslySetCell);
    runner.Add("AlphaMask/DimensionsAreReportedCorrectly", DimensionsAreReportedCorrectly);
}

}  // namespace nimvlets::tests
