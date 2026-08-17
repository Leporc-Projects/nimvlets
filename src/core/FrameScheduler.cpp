#include "core/FrameScheduler.h"

namespace nimvlets::core {

FrameScheduler::FrameScheduler(double frameIntervalMs) : frameIntervalMs_(frameIntervalMs) {}

double FrameScheduler::NextFrameDeadline(double nowMs) const {
    if (!hasPresented_) {
        return nowMs;
    }
    return lastFrameMs_ + frameIntervalMs_;
}

double FrameScheduler::MillisUntilNextFrame(double nowMs) const {
    const double deadline = NextFrameDeadline(nowMs);
    const double remaining = deadline - nowMs;
    return remaining > 0.0 ? remaining : 0.0;
}

void FrameScheduler::OnFramePresented(double nowMs) {
    lastFrameMs_ = nowMs;
    hasPresented_ = true;
}

}  // namespace nimvlets::core
