#include "FrameSchedulerTest.h"

#include "core/FrameScheduler.h"

using nimvlets::core::FrameScheduler;

namespace nimvlets::tests {

namespace {

// All of these use fabricated timestamps and never call any sleep/clock
// API — the point of FrameScheduler is that its deadline math is
// testable without dormir realmente (without really sleeping).

bool FirstDeadlineIsImmediate() {
    FrameScheduler scheduler(16.0);
    NIMVLETS_CHECK(scheduler.NextFrameDeadline(1000.0) == 1000.0);
    NIMVLETS_CHECK(scheduler.MillisUntilNextFrame(1000.0) == 0.0);
    return true;
}

bool DeadlineIsIntervalAfterLastPresent() {
    FrameScheduler scheduler(16.0);
    scheduler.OnFramePresented(1000.0);
    NIMVLETS_CHECK(scheduler.NextFrameDeadline(1000.0) == 1016.0);
    NIMVLETS_CHECK(scheduler.MillisUntilNextFrame(1000.0) == 16.0);
    return true;
}

bool WaitShrinksAsTimePasses() {
    FrameScheduler scheduler(16.0);
    scheduler.OnFramePresented(1000.0);
    NIMVLETS_CHECK(scheduler.MillisUntilNextFrame(1010.0) == 6.0);
    return true;
}

bool WaitNeverGoesNegativePastDeadline() {
    FrameScheduler scheduler(16.0);
    scheduler.OnFramePresented(1000.0);
    // Far past the deadline (e.g. the process was suspended/blocked
    // briefly) — must clamp to 0, never a negative "wait".
    NIMVLETS_CHECK(scheduler.MillisUntilNextFrame(5000.0) == 0.0);
    return true;
}

bool PresentingAgainAdvancesTheDeadline() {
    FrameScheduler scheduler(16.0);
    scheduler.OnFramePresented(1000.0);
    scheduler.OnFramePresented(1016.0);
    NIMVLETS_CHECK(scheduler.NextFrameDeadline(1016.0) == 1032.0);
    return true;
}

}  // namespace

void RegisterFrameSchedulerTests(testing::TestRunner& runner) {
    runner.Add("FrameScheduler/FirstDeadlineIsImmediate", FirstDeadlineIsImmediate);
    runner.Add("FrameScheduler/DeadlineIsIntervalAfterLastPresent", DeadlineIsIntervalAfterLastPresent);
    runner.Add("FrameScheduler/WaitShrinksAsTimePasses", WaitShrinksAsTimePasses);
    runner.Add("FrameScheduler/WaitNeverGoesNegativePastDeadline", WaitNeverGoesNegativePastDeadline);
    runner.Add("FrameScheduler/PresentingAgainAdvancesTheDeadline", PresentingAgainAdvancesTheDeadline);
}

}  // namespace nimvlets::tests
