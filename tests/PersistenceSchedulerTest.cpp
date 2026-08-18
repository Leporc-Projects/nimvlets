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

// El comportamiento central de "los clicks rápidos se coalescen en una
// escritura": llamadas repetidas a MarkDirty() antes de que dispare el
// deadline no deben empujarlo más lejos, o la actividad continua
// podría dejar la persistencia sin escribir indefinidamente.
bool TestRepeatedMarkDirtyCallsDoNotExtendTheDeadline() {
    PersistenceScheduler scheduler(2000.0);
    scheduler.MarkDirty(1000.0);  // arma el deadline en 3000.0
    scheduler.MarkDirty(1500.0);  // ya estaba dirty -> no hace nada
    scheduler.MarkDirty(2900.0);  // sigue dirty -> no hace nada, aunque esté cerca del deadline
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

// Un flush que falla no debe descartar en silencio el cambio
// pendiente, pero tampoco debe reintentar en el despertar
// inmediatamente siguiente del event loop — se reprograma otro
// debounceMs más adelante.
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
    scheduler.MarkDirty(10000.0);  // un cambio totalmente nuevo, bastante después del primer ciclo
    NIMVLETS_CHECK(scheduler.IsDirty());
    NIMVLETS_CHECK(*scheduler.NextFlushDeadlineMs() == 12000.0);
    return true;
}

bool TestDefaultDebounceIntervalMatchesDocumentedConstant() {
    PersistenceScheduler scheduler;  // usa kDefaultDebounceMs
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
