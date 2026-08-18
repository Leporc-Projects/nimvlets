#include "persistence/PersistenceScheduler.h"

namespace nimvlets::persistence {

PersistenceScheduler::PersistenceScheduler(double debounceMs) : debounceMs_(debounceMs) {}

void PersistenceScheduler::MarkDirty(double nowMs) {
    if (dirty_) {
        return;  // ya hay un flush programado; que cubra también este cambio
    }
    dirty_ = true;
    deadlineMs_ = nowMs + debounceMs_;
}

std::optional<double> PersistenceScheduler::NextFlushDeadlineMs() const {
    if (!dirty_) {
        return std::nullopt;
    }
    return deadlineMs_;
}

void PersistenceScheduler::OnFlushSucceeded() {
    dirty_ = false;
}

void PersistenceScheduler::OnFlushFailed(double nowMs) {
    dirty_ = true;
    deadlineMs_ = nowMs + debounceMs_;
}

}  // namespace nimvlets::persistence
