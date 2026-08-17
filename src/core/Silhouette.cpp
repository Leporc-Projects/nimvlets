#include "core/Silhouette.h"

#include <cmath>

namespace nimvlets::core {

Point BlobSilhouette::BodyCenter() const {
    return Point{windowWidth / 2.0, (windowHeight / 2.0) + 10.0};
}

Point BlobSilhouette::HeadCenter(double phaseSeconds) const {
    const Point body = BodyCenter();
    const double bob = std::sin(phaseSeconds * bobAngularSpeed) * bobAmplitudePx;
    return Point{body.x + headOffsetX, body.y + headOffsetY + bob};
}

bool BlobSilhouette::Contains(Point point, double phaseSeconds) const {
    const Point body = BodyCenter();
    if (DistanceSquared(point, body) <= bodyRadius * bodyRadius) {
        return true;
    }

    const Point head = HeadCenter(phaseSeconds);
    if (DistanceSquared(point, head) <= headRadius * headRadius) {
        return true;
    }

    return false;
}

BlobSilhouette BlobSilhouette::Default() {
    return BlobSilhouette{};
}

}  // namespace nimvlets::core
