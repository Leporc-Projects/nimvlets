#include "core/DragClassifier.h"

namespace nimvlets::core {

DragClassifier::DragClassifier(DragClassifierConfig config) : config_(config) {}

void DragClassifier::Begin(Point origin) {
    origin_ = origin;
    active_ = true;
    isDragging_ = false;
    maxDistanceSq_ = 0.0;
}

bool DragClassifier::Update(Point current) {
    if (!active_) {
        return false;
    }

    const double distSq = DistanceSquared(origin_, current);
    if (distSq > maxDistanceSq_) {
        maxDistanceSq_ = distSq;
    }

    const double thresholdSq = config_.distanceThresholdPx * config_.distanceThresholdPx;
    if (!isDragging_ && maxDistanceSq_ > thresholdSq) {
        isDragging_ = true;
        return true;
    }
    return false;
}

PointerGesture DragClassifier::End(Point point) {
    Update(point);
    const PointerGesture gesture = isDragging_ ? PointerGesture::kDrag : PointerGesture::kClick;
    active_ = false;
    isDragging_ = false;
    return gesture;
}

}  // namespace nimvlets::core
