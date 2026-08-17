#pragma once

#include "core/Geometry.h"

namespace nimvlets::core {

enum class PointerGesture {
    kClick,
    kDrag,
};

struct DragClassifierConfig {
    // Pointer movement beyond this many logical pixels (measured from the
    // press origin) reclassifies the in-progress gesture as a drag instead
    // of a click. Distance is the metric (see docs/PLATFORM_SPIKE.md for
    // why time-based thresholds were considered and dropped as redundant
    // for this spike).
    double distanceThresholdPx = 4.0;
};

// Pure, SDL-free classification of a press/move/release pointer sequence
// into a click or a drag.
//
// This exists as its own class specifically so it can be unit tested
// without SDL, a window, or real event timing (see
// tests/DragClassifierTest.cpp) — the windowing layer
// (src/app/SpikeApp.cpp) only feeds it Points from real SDL mouse events
// and reacts to its verdicts.
class DragClassifier {
public:
    explicit DragClassifier(DragClassifierConfig config = {});

    // Starts tracking a new press at `origin`.
    void Begin(Point origin);

    // Feeds a pointer-moved sample while the button is held. Returns true
    // exactly once per gesture: the moment movement first crosses the
    // drag threshold. Callers use that edge to start actually moving the
    // window instead of doing it on every motion sample.
    bool Update(Point current);

    // Ends the gesture at `point` and returns the final classification.
    // The classifier returns to an inactive state afterwards.
    PointerGesture End(Point point);

    // True once Update()/End() has observed movement past the drag
    // threshold for the gesture currently in progress.
    bool IsDragging() const { return isDragging_; }

    bool IsActive() const { return active_; }

    // Furthest squared distance reached from the press origin so far.
    // Exposed mainly for tests.
    double MaxDistanceSquared() const { return maxDistanceSq_; }

private:
    DragClassifierConfig config_;
    Point origin_{};
    bool active_ = false;
    bool isDragging_ = false;
    double maxDistanceSq_ = 0.0;
};

}  // namespace nimvlets::core
