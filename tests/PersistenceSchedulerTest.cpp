#include "PersistenceSchedulerTest.h"

#include "persistence/PersistenceScheduler.h"

using nimvlets::persistence::PersistenceScheduler;

namespace nimvlets::tests {

namespace {

bool TestFreshSchedulerIsNotDirty() {
    const PersistenceScheduler scheduler(2000.0);
    NIMVLETS_CHECK(!scheduler.IsDirty());
    NIMVLETS_CHECK(!scheduler.NextFlushDeadlineMs().has_value());
    return true;
}

bool TestMarkDirtyArmsADeadlineDebounceMsInTheFuture() {
    PersistenceScheduler scheduler(2000.0);
    scheduler.MarkDirty(1000.0);
    NIMVLETS_CHECK(scheduler.IsDirty());
    const auto deadline = scheduler.NextFlushDeadlineMs();
    NIMVLETS_CHECK(deadline.has_value());
    NIMVLETS_CHECK(*deadline == 3000.0);
    return true;
}

// The core "rapid clicks coalesce into one write" behavior: repeated
// MarkDirty() calls before the deadline fires must not push it further
// out, or continuous activity could starve persistence indefinitely.
bool TestRepeatedMarkDirtyCallsDoNotExtendTheDeadline() {
    PersistenceScheduler scheduler(2000.0);
    scheduler.MarkDirty(1000.0);  // arms deadline at 3000.0
    scheduler.MarkDirty(1500.0);  // already dirty -> no-op
    scheduler.MarkDirty(2900.0);  // still already dirty -> no-op, even though close to the deadline
    NIMVLETS_CHECK(scheduler.IsDirty());
    NIMVLETS_CHECK(*scheduler.NextFlushDeadlineMs() == 3000.0);
    return true;
}

bool TestOnFlushSucceededClearsDirtyState() {
    PersistenceScheduler scheduler(2000.0);
    scheduler.MarkDirty(1000.0);
    scheduler.OnFlushSucceeded();
    NIMVLETS_CHECK(!scheduler.IsDirty());
    NIMVLETS_CHECK(!scheduler.NextFlushDeadlineMs().has_value());
    return true;
}

// A flush that fails must not silently drop the pending change, but
// must also not retry on literally the next event-loop wake — it's
// rescheduled a further debounceMs out.
bool TestOnFlushFailedKeepsDirtyAndReschedules() {
    PersistenceScheduler scheduler(2000.0);
    scheduler.MarkDirty(1000.0);  // deadline = 3000.0
    scheduler.OnFlushFailed(3000.0);
    NIMVLETS_CHECK(scheduler.IsDirty());
    const auto deadline = scheduler.NextFlushDeadlineMs();
    NIMVLETS_CHECK(deadline.has_value());
    NIMVLETS_CHECK(*deadline == 5000.0);
    return true;
}

bool TestCustomDebounceIntervalIsRespected() {
    PersistenceScheduler scheduler(500.0);
    scheduler.MarkDirty(0.0);
    NIMVLETS_CHECK(*scheduler.NextFlushDeadlineMs() == 500.0);
    return true;
}

bool TestMarkDirtyAfterFlushSucceededArmsANewDeadline() {
    PersistenceScheduler scheduler(2000.0);
    scheduler.MarkDirty(1000.0);
    scheduler.OnFlushSucceeded();
    scheduler.MarkDirty(10000.0);  // a brand new change, well after the first cycle
    NIMVLETS_CHECK(scheduler.IsDirty());
    NIMVLETS_CHECK(*scheduler.NextFlushDeadlineMs() == 12000.0);
    return true;
}

bool TestDefaultDebounceIntervalMatchesDocumentedConstant() {
    PersistenceScheduler scheduler;  // uses kDefaultDebounceMs
    scheduler.MarkDirty(0.0);
    NIMVLETS_CHECK(*scheduler.NextFlushDeadlineMs() == PersistenceScheduler::kDefaultDebounceMs);
    return true;
}

}  // namespace

void RegisterPersistenceSchedulerTests(testing::TestRunner& runner) {
    runner.Add("PersistenceScheduler/FreshSchedulerIsNotDirty", TestFreshSchedulerIsNotDirty);
    runner.Add("PersistenceScheduler/MarkDirtyArmsADeadlineDebounceMsInTheFuture", TestMarkDirtyArmsADeadlineDebounceMsInTheFuture);
    runner.Add("PersistenceScheduler/RepeatedMarkDirtyCallsDoNotExtendTheDeadline", TestRepeatedMarkDirtyCallsDoNotExtendTheDeadline);
    runner.Add("PersistenceScheduler/OnFlushSucceededClearsDirtyState", TestOnFlushSucceededClearsDirtyState);
    runner.Add("PersistenceScheduler/OnFlushFailedKeepsDirtyAndReschedules", TestOnFlushFailedKeepsDirtyAndReschedules);
    runner.Add("PersistenceScheduler/CustomDebounceIntervalIsRespected", TestCustomDebounceIntervalIsRespected);
    runner.Add("PersistenceScheduler/MarkDirtyAfterFlushSucceededArmsANewDeadline", TestMarkDirtyAfterFlushSucceededArmsANewDeadline);
    runner.Add("PersistenceScheduler/DefaultDebounceIntervalMatchesDocumentedConstant", TestDefaultDebounceIntervalMatchesDocumentedConstant);
}

}  // namespace nimvlets::tests
