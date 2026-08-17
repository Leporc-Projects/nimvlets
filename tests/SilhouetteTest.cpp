#include "SilhouetteTest.h"

#include "core/Silhouette.h"

using nimvlets::core::BlobSilhouette;
using nimvlets::core::Point;

namespace nimvlets::tests {

namespace {

bool BodyCenterIsInside() {
    const BlobSilhouette blob = BlobSilhouette::Default();
    NIMVLETS_CHECK(blob.Contains(blob.BodyCenter(), /*phaseSeconds=*/0.0));
    return true;
}

bool WindowCornerIsOutside() {
    const BlobSilhouette blob = BlobSilhouette::Default();
    NIMVLETS_CHECK(!blob.Contains(Point{0.0, 0.0}, /*phaseSeconds=*/0.0));
    NIMVLETS_CHECK(!blob.Contains(Point{blob.windowWidth, blob.windowHeight}, /*phaseSeconds=*/0.0));
    return true;
}

bool HeadCenterIsInsideAtPhaseZero() {
    const BlobSilhouette blob = BlobSilhouette::Default();
    NIMVLETS_CHECK(blob.Contains(blob.HeadCenter(0.0), /*phaseSeconds=*/0.0));
    return true;
}

bool BoundaryPointExactlyAtBodyRadiusIsInside() {
    const BlobSilhouette blob = BlobSilhouette::Default();
    const Point body = blob.BodyCenter();
    const Point onEdge{body.x + blob.bodyRadius, body.y};
    NIMVLETS_CHECK(blob.Contains(onEdge, /*phaseSeconds=*/0.0));
    return true;
}

bool PointJustOutsideBodyRadiusIsOutside() {
    const BlobSilhouette blob = BlobSilhouette::Default();
    const Point body = blob.BodyCenter();
    const Point justOutside{body.x + blob.bodyRadius + 0.5, body.y};
    // The head circle is offset upward and doesn't reach the point we
    // probe here (directly to the right of the body), so this is a valid
    // "outside both circles" check.
    NIMVLETS_CHECK(!blob.Contains(justOutside, /*phaseSeconds=*/0.0));
    return true;
}

bool AnimationPhaseMovesTheHead() {
    const BlobSilhouette blob = BlobSilhouette::Default();
    const Point headAtPhaseZero = blob.HeadCenter(0.0);
    const Point headAtPhaseOne = blob.HeadCenter(1.0);
    NIMVLETS_CHECK(headAtPhaseZero.y != headAtPhaseOne.y);
    return true;
}

}  // namespace

void RegisterSilhouetteTests(testing::TestRunner& runner) {
    runner.Add("Silhouette/BodyCenterIsInside", BodyCenterIsInside);
    runner.Add("Silhouette/WindowCornerIsOutside", WindowCornerIsOutside);
    runner.Add("Silhouette/HeadCenterIsInsideAtPhaseZero", HeadCenterIsInsideAtPhaseZero);
    runner.Add("Silhouette/BoundaryPointExactlyAtBodyRadiusIsInside", BoundaryPointExactlyAtBodyRadiusIsInside);
    runner.Add("Silhouette/PointJustOutsideBodyRadiusIsOutside", PointJustOutsideBodyRadiusIsOutside);
    runner.Add("Silhouette/AnimationPhaseMovesTheHead", AnimationPhaseMovesTheHead);
}

}  // namespace nimvlets::tests
