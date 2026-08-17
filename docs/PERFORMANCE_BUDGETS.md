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
