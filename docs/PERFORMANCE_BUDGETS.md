# Nimvlets — Performance Budgets

These are **engineering budgets**, not commercial promises. They're
targets to design and measure against, not marketing claims about the
finished product — actual numbers only become a promise once measured
on final, signed/notarized Release builds with real content.

## Budgets

| Metric | Target | Aspiration | Notes |
|---|---|---|---|
| RAM, resident, normal operation | < 100 MB | < 64 MB where reasonable | Measured on Release, after the process stabilizes (not the first-second spike). |
| CPU, idle with animation running | < 1% average | — | Measured over a real window (tens of seconds), not a 1-second sample. Reference hardware = whatever machine measured it; see the Block 01 report for the actual machine. |
| CPU, hidden/inactive (future) | ~0% except for real events/timers | — | Not applicable yet — Block 01 has no "hidden" state. |
| GPU | Trivial | — | A handful of filled circles at low frame rate; not meaningfully measured in Block 01. |
| Network, base | 0 bytes | — | No network activity is initiated by the app at runtime. |
| Install size, v1 | < 150 MB | smaller | Not applicable to Block 01 (no installer/packaging yet — see NON-SCOPE). |
| Startup, future | ≤ 1 s | — | On reasonable hardware; approximate, not instrumented precisely in Block 01. |

## Methodology rules (binding, not optional)

- **Release only.** Debug builds (unoptimized, with extra runtime
  checks) are never used to conclude anything about these budgets.
- **No single-sample conclusions.** A metric is measured over a real
  time window with multiple samples, not one snapshot.
- **No sudo / no special instrumentation.** Measurements use ordinary,
  unprivileged OS tools (`ps`, `top`, process listing) so the method is
  reproducible by anyone building this repo, not just on an
  instrumented dev machine.
- **Honest gaps.** If a metric can't be measured reliably without
  privileged tools or extra instrumentation this block doesn't have,
  it's marked `NOT MEASURED` with a reason — never guessed.

## Block 01's actual measurements

See the Block 01 report, §7 "Performance," for this block's concrete
numbers (RSS, idle CPU, startup, binary size) and the exact method used
to obtain each one, plus any caveats (e.g. measured on Apple Silicon
only — no Intel Mac or Windows machine was available for a real
hardware run in this block; see `docs/PLATFORM_SPIKE.md`).

## Block 02's actual measurements

Same method as Block 01 (`ps -o rss,%cpu,time`, 3-second intervals over
a 27–30s window, Release build, native arm64, no `sudo`). See
`docs/ANIMATION_RUNTIME.md` §6 for the full write-up of *why* these
numbers changed (the deadline-driven scheduler replacing Block 01's
fixed ~12fps tick — DEC-021).

| Scenario | CPU (average, steady state) | RSS |
|---|---|---|
| Block 01, Bunny fixture, fixed ~12fps tick (baseline) | ≈2% | ≈72 MB |
| Block 02, static idle (production default: no passive action due, no interaction) | **≈0.0%** | ≈73 MB |
| Block 02, passive action forced every ~2s (`NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS=2` — an artificial stress scenario, ~150x more frequent than the real ~300s default) | <1% (peak 1.0%, mostly 0.0–0.5%) | ≈74 MB |

The "CPU, idle with animation running" budget row above (< 1% average)
is now met with considerable margin at true static idle (≈0.0%, not
just under budget), and even the artificial worst-case passive-action
stress scenario stays under the 1% target. Click-reaction CPU was not
measured as a separate scenario in this block (it shares the exact same
render/hit-mask-update code path as passive actions — see
`docs/ANIMATION_RUNTIME.md` §3 — so the passive-action stress number
above is a reasonable proxy, not a claim of an identically-measured
data point). RSS remains well within the < 100 MB budget across all
three scenarios.

## Block 03's actual measurements

Persistence (`src/persistence`) adds a debounced disk write and, per
`docs/PERSISTENCE.md` §9, a capped 1-second maximum event-loop wait
(fixing a shutdown-latency bug that block's own testing found — see
`docs/DECISION_LOG.md` DEC-028). Re-measured with the same method as
Block 02, Release, native arm64, persistence active
(`NIMVLETS_DEV_APPDATA_DIR` pointed at an isolated temp directory):

| Scenario | CPU (average, steady state) | RSS |
|---|---|---|
| Block 03, static idle, persistence active, no pending writes | **≈0.0%** | ≈73–74 MB |

Unchanged from Block 02 within measurement noise — the 1-second wait
cap and the (idle, non-dirty) persistence scheduler add a wake that
does no work the overwhelming majority of the time, not a busy-wait or
periodic render tick. A single debounced write (a few dozen bytes,
atomically renamed) is not large enough to register on a 3-second-
interval `ps` sample; no dedicated per-write CPU/latency measurement
was made in this block, since none of the block's requirements called
for one and the mechanism (one small buffered write + one rename) has
no realistic path to being a measurable cost at this scale.
