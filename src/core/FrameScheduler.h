#pragma once

namespace nimvlets::core {

// Pure timing helper for an event-driven render loop.
//
// The spike's main loop blocks in SDL_WaitEventTimeout() rather than
// spinning at a fixed rate (see AGENTS.md's "no busy-wait" rule and
// PLATFORM_SPIKE.md item 13). This class answers "how long is it safe to
// block?" so the loop wakes immediately on real input and otherwise wakes
// right when the next idle-animation frame is due — never sooner.
//
// Deliberately has no dependency on wall-clock sleeping so it can be unit
// tested with fabricated timestamps (see tests/FrameSchedulerTest.cpp) —
// "testear deadline sin dormir realmente".
class FrameScheduler {
public:
    // frameIntervalMs must be > 0.
    explicit FrameScheduler(double frameIntervalMs);

    // Timestamp (ms, same clock/epoch as `nowMs`) at which the next frame
    // should be presented.
    double NextFrameDeadline(double nowMs) const;

    // How long (ms, always >= 0) the caller should block waiting for
    // events before it needs to wake up and render the next idle frame.
    double MillisUntilNextFrame(double nowMs) const;

    // Records that a frame was presented at `nowMs`, so the next deadline
    // is computed relative to it.
    void OnFramePresented(double nowMs);

    double FrameIntervalMs() const { return frameIntervalMs_; }

private:
    double frameIntervalMs_;
    double lastFrameMs_ = 0.0;
    bool hasPresented_ = false;
};

}  // namespace nimvlets::core
