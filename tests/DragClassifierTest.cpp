#include "DragClassifierTest.h"

#include "core/DragClassifier.h"

using nimvlets::core::DragClassifier;
using nimvlets::core::DragClassifierConfig;
using nimvlets::core::Point;
using nimvlets::core::PointerGesture;

namespace nimvlets::tests {

namespace {

bool NoMovementIsClick() {
    DragClassifier classifier;
    classifier.Begin(Point{10.0, 10.0});
    const PointerGesture gesture = classifier.End(Point{10.0, 10.0});
    NIMVLETS_CHECK(gesture == PointerGesture::kClick);
    NIMVLETS_CHECK(!classifier.IsActive());
    return true;
}

bool MovementWithinThresholdIsClick() {
    DragClassifier classifier{DragClassifierConfig{/*distanceThresholdPx=*/4.0}};
    classifier.Begin(Point{0.0, 0.0});
    classifier.Update(Point{3.0, 0.0});
    const PointerGesture gesture = classifier.End(Point{3.0, 0.0});
    NIMVLETS_CHECK(gesture == PointerGesture::kClick);
    return true;
}

bool MovementBeyondThresholdIsDrag() {
    DragClassifier classifier{DragClassifierConfig{/*distanceThresholdPx=*/4.0}};
    classifier.Begin(Point{0.0, 0.0});
    const bool crossedOnUpdate = classifier.Update(Point{10.0, 0.0});
    NIMVLETS_CHECK(crossedOnUpdate);
    NIMVLETS_CHECK(classifier.IsDragging());
    const PointerGesture gesture = classifier.End(Point{10.0, 0.0});
    NIMVLETS_CHECK(gesture == PointerGesture::kDrag);
    return true;
}

bool ThresholdBoundaryExactlyAtLimitIsClick() {
    // Distance exactly equal to the threshold does not count as a drag
    // (strict greater-than is the drag condition) — this is the
    // boundary test required by the block brief.
    DragClassifier classifier{DragClassifierConfig{/*distanceThresholdPx=*/4.0}};
    classifier.Begin(Point{0.0, 0.0});
    const PointerGesture gesture = classifier.End(Point{4.0, 0.0});
    NIMVLETS_CHECK(gesture == PointerGesture::kClick);
    return true;
}

bool ThresholdBoundaryJustOverLimitIsDrag() {
    DragClassifier classifier{DragClassifierConfig{/*distanceThresholdPx=*/4.0}};
    classifier.Begin(Point{0.0, 0.0});
    const PointerGesture gesture = classifier.End(Point{4.01, 0.0});
    NIMVLETS_CHECK(gesture == PointerGesture::kDrag);
    return true;
}

bool UpdateReportsCrossingExactlyOnce() {
    DragClassifier classifier{DragClassifierConfig{/*distanceThresholdPx=*/4.0}};
    classifier.Begin(Point{0.0, 0.0});
    NIMVLETS_CHECK(!classifier.Update(Point{1.0, 0.0}));   // still within threshold
    NIMVLETS_CHECK(classifier.Update(Point{10.0, 0.0}));   // crosses now
    NIMVLETS_CHECK(!classifier.Update(Point{20.0, 0.0}));  // already dragging, no re-report
    return true;
}

bool DragThatReturnsToOriginIsStillADrag() {
    // Regression guard: classification must be based on the *maximum*
    // distance reached during the gesture, not the final displacement.
    DragClassifier classifier{DragClassifierConfig{/*distanceThresholdPx=*/4.0}};
    classifier.Begin(Point{0.0, 0.0});
    classifier.Update(Point{50.0, 0.0});
    const PointerGesture gesture = classifier.End(Point{0.0, 0.0});
    NIMVLETS_CHECK(gesture == PointerGesture::kDrag);
    return true;
}

bool InactiveClassifierIgnoresUpdates() {
    DragClassifier classifier;
    NIMVLETS_CHECK(!classifier.IsActive());
    NIMVLETS_CHECK(!classifier.Update(Point{100.0, 100.0}));
    return true;
}

}  // namespace

void RegisterDragClassifierTests(testing::TestRunner& runner) {
    runner.Add("DragClassifier/NoMovementIsClick", NoMovementIsClick);
    runner.Add("DragClassifier/MovementWithinThresholdIsClick", MovementWithinThresholdIsClick);
    runner.Add("DragClassifier/MovementBeyondThresholdIsDrag", MovementBeyondThresholdIsDrag);
    runner.Add("DragClassifier/ThresholdBoundaryExactlyAtLimitIsClick", ThresholdBoundaryExactlyAtLimitIsClick);
    runner.Add("DragClassifier/ThresholdBoundaryJustOverLimitIsDrag", ThresholdBoundaryJustOverLimitIsDrag);
    runner.Add("DragClassifier/UpdateReportsCrossingExactlyOnce", UpdateReportsCrossingExactlyOnce);
    runner.Add("DragClassifier/DragThatReturnsToOriginIsStillADrag", DragThatReturnsToOriginIsStillADrag);
    runner.Add("DragClassifier/InactiveClassifierIgnoresUpdates", InactiveClassifierIgnoresUpdates);
}

}  // namespace nimvlets::tests
