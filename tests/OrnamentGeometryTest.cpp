#include "OrnamentGeometryTest.h"

#include <cmath>

#include "productui/OrnamentGeometry.h"

using nimvlets::productui::DiamondHalfWidthAt;
using nimvlets::productui::HeadingMotifAdvance;
using nimvlets::productui::OrnamentalDividerRuleLen;
using nimvlets::productui::SparkleBounds;
using nimvlets::productui::UiRect;

namespace nimvlets::tests {

namespace {

bool Near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

// El spark queda centrado en (cx, cy) y sus puntas llegan exactamente a
// ±radius en los dos ejes — nunca "de un solo lado" (brief §31).
bool TestSparkleBoundsCenteredAndSymmetric() {
    const UiRect b = SparkleBounds(100.0f, 50.0f, 6.0f);
    NIMVLETS_CHECK(Near(b.CenterX(), 100.0f));
    NIMVLETS_CHECK(Near(b.CenterY(), 50.0f));
    NIMVLETS_CHECK(Near(b.w, 12.0f) && Near(b.h, 12.0f));
    NIMVLETS_CHECK(Near(b.x, 94.0f) && Near(b.y, 44.0f));
    return true;
}

// El semi-ancho del rombo es 0 en las puntas, máximo (w/2) en el medio,
// y simétrico alrededor de t = 0.5.
bool TestDiamondHalfWidthProfile() {
    NIMVLETS_CHECK(Near(DiamondHalfWidthAt(10.0f, 0.0f), 0.0f));
    NIMVLETS_CHECK(Near(DiamondHalfWidthAt(10.0f, 1.0f), 0.0f));
    NIMVLETS_CHECK(Near(DiamondHalfWidthAt(10.0f, 0.5f), 5.0f));
    NIMVLETS_CHECK(Near(DiamondHalfWidthAt(10.0f, 0.25f), DiamondHalfWidthAt(10.0f, 0.75f)));
    NIMVLETS_CHECK(DiamondHalfWidthAt(10.0f, 0.25f) > 0.0f &&
                   DiamondHalfWidthAt(10.0f, 0.25f) < 5.0f);
    // Nunca negativo, ni siquiera fuera de rango.
    NIMVLETS_CHECK(DiamondHalfWidthAt(10.0f, -0.3f) >= 0.0f);
    NIMVLETS_CHECK(DiamondHalfWidthAt(10.0f, 1.7f) >= 0.0f);
    return true;
}

// Los dos segmentos de regla del divisor ornamental son iguales
// (simétrico), nunca negativos, y se encogen a medida que crece el
// hueco central.
bool TestOrnamentalDividerRuleLength() {
    NIMVLETS_CHECK(Near(OrnamentalDividerRuleLen(200.0f, 9.0f), 91.0f));
    NIMVLETS_CHECK(OrnamentalDividerRuleLen(200.0f, 9.0f) > OrnamentalDividerRuleLen(200.0f, 20.0f));
    NIMVLETS_CHECK(OrnamentalDividerRuleLen(10.0f, 40.0f) == 0.0f);  // banda muy chica: sin regla, no negativo
    return true;
}

bool TestHeadingMotifAdvanceIsPositiveAndGrowsWithDiamond() {
    NIMVLETS_CHECK(HeadingMotifAdvance(6.0f) > 0.0f);
    NIMVLETS_CHECK(HeadingMotifAdvance(8.0f) > HeadingMotifAdvance(6.0f));
    return true;
}

}  // namespace

void RegisterOrnamentGeometryTests(testing::TestRunner& runner) {
    runner.Add("OrnamentGeometry/SparkleBoundsCenteredAndSymmetric",
               TestSparkleBoundsCenteredAndSymmetric);
    runner.Add("OrnamentGeometry/DiamondHalfWidthProfile", TestDiamondHalfWidthProfile);
    runner.Add("OrnamentGeometry/OrnamentalDividerRuleLength", TestOrnamentalDividerRuleLength);
    runner.Add("OrnamentGeometry/HeadingMotifAdvanceIsPositiveAndGrowsWithDiamond",
               TestHeadingMotifAdvanceIsPositiveAndGrowsWithDiamond);
}

}  // namespace nimvlets::tests
