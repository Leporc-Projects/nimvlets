#pragma once

#include <optional>

namespace nimvlets::persistence {

// Debounced dirty-flag scheduler for AppState writes — pure timing
// logic, no file I/O, no SDL, testable with fabricated timestamps
// exactly like core::FrameScheduler (see
// tests/PersistenceSchedulerTest.cpp).
//
// The problem this solves: clicks can arrive many times per second,
// but writing to disk once per click would be wasteful and pointless
// (see docs/PERSISTENCE.md, "write policy"). Instead, the first change
// after a clean/flushed state arms a single deadline `debounceMs` in
// the future; any further changes before that deadline fires are
// coalesced into whatever gets written at that one deadline — they do
// not each arm their own write, and they do not push the deadline
// further out (so continuous activity can never starve persistence
// indefinitely).
class PersistenceScheduler {
 public:
    // A short, deliberately small delay: long enough that a burst of
    // rapid clicks collapses into one write, short enough that a crash
    // shortly after the last change loses at most this much progress
    // (clean shutdown always flushes immediately regardless of this
    // deadline — see src/app/SpikeApp.cpp). See docs/PERSISTENCE.md
    // for the rationale.
    static constexpr double kDefaultDebounceMs = 2000.0;

    explicit PersistenceScheduler(double debounceMs = kDefaultDebounceMs);

    // Marks state as needing a flush. A no-op if already dirty — the
    // deadline already scheduled from the *first* pending change is
    // what governs when the (single, coalesced) write happens.
    void MarkDirty(double nowMs);

    bool IsDirty() const { return dirty_; }

    // The timestamp (same clock/epoch as `nowMs` everywhere in this
    // class) at which a pending flush should happen — nullopt when
    // nothing is dirty. This is what src/app/SpikeApp.cpp folds into
    // its event-loop wait-time calculation alongside the animation and
    // passive-action deadlines.
    std::optional<double> NextFlushDeadlineMs() const;

    // Call after a flush attempt succeeds: clears dirty state.
    void OnFlushSucceeded();

    // Call after a flush attempt fails: state remains dirty (the
    // pending change is not silently dropped), but the next retry is
    // deferred a further `debounceMs` rather than being retried on the
    // very next event-loop wake — bounds retry frequency to at most
    // once per debounce interval even under a persistent failure.
    void OnFlushFailed(double nowMs);

 private:
    double debounceMs_;
    bool dirty_ = false;
    double deadlineMs_ = 0.0;
};

}  // namespace nimvlets::persistence
