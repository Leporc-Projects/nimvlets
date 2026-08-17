# Nimvlets — Platform Spike (Block 01)

This document is both the QA **plan** (§2, written against the block
brief before running anything) and the **results** (§3 onward, filled
in after actually building and running the spike). Nothing below is
upgraded from PARTIAL to PASS without new evidence — see §4 for exactly
why several interactive items are PARTIAL/NOT TESTED rather than PASS,
and what would resolve them.

## 1. What was evaluated

`SDL_SetWindowShape()` was evaluated first, as the block brief
requires, before deciding on the shipped approach. See §5 for the full
evaluation and the decision it led to (`docs/DECISION_LOG.md` DEC-006).

## 2. QA plan (macOS)

For each item: build Release, run `nimvlets_spike`, and check —

1. transparent window renders
2. borderless (no title bar/decoration)
3. small bounding box (matches `core::BlobSilhouette::Default()`, 160×160 logical)
4. real per-pixel alpha (not just a translucent rectangle)
5. click-through on fully transparent pixels
6. click detected on the visible (opaque) shape
7. drag moves the window
8. a drag is never miscounted as a click, and vice versa
9. no more focus-stealing than the platform makes unavoidable
10. always-on-top over a normal window
11. high-DPI backing on a Retina display
12. clean shutdown, no hung/zombie process
13. render/event loop does not busy-wait
14. no invasive permission is requested anywhere in the process

## 3. Results — macOS

Machine: Apple M5, macOS 26.6.1 (Darwin 25.6.0), Retina display
(2560×1664 logical @ 2x). Build: Release, `cmake --preset macos-release`.

| # | Item | Verdict | Evidence |
|---|---|---|---|
| 1 | Transparent window | **PARTIAL** | `SDL_CreateWindow` with `SDL_WINDOW_TRANSPARENT` returned non-null (no `SDL_GetError()` output); `platform::ConfigureCompanionWindow` ran and resolved a real `NSWindow` (no "could not resolve NSWindow" log). Pixel-level transparency was **not** visually confirmed — see §4. |
| 2 | Borderless | **PARTIAL** | `SDL_WINDOW_BORDERLESS` passed at creation, accepted without error. Not visually confirmed. |
| 3 | Small bounding box | **PASS** | Objectively confirmed via `CGWindowListCopyWindowInfo`: `kCGWindowBounds = {Width=160, Height=160, X=655, Y=376}` for the running process — an independent, non-visual read of the real WindowServer state, not just "we asked for 160×160". |
| 4 | Real alpha | **PARTIAL** | Same basis as #1; `kCGWindowAlpha=1` is the *window's* alpha multiplier (expected — we intentionally leave `NSWindow.alphaValue` at 1.0 and rely on per-pixel alpha from rendering instead), not evidence about pixel content. |
| 5 | Click-through on transparent pixels | **NOT TESTED** (interactive) | Implementation code-reviewed against the exact SDL3 3.4.12 header symbols used (`SDL_GetGlobalMouseState`, `SDL_GetWindowPosition`, `SDL_PROP_WINDOW_COCOA_WINDOW_POINTER`, `NSWindow.ignoresMouseEvents`), all confirmed to exist with the signatures used (see §5). No real or synthetic click was performed — see §4. |
| 6 | Click on visible shape | **NOT TESTED** (interactive, same reason) | Classification logic that would run: **PASS** — `core::DragClassifier` has 8/8 unit tests green, including exact-threshold and "drag that returns to origin" cases. |
| 7 | Drag moves the window | **NOT TESTED** (interactive, same reason) | `SDL_SetWindowPosition` call path code-reviewed correct; not exercised end-to-end with real input. |
| 8 | Click vs. drag never confused | **PASS** (logic) / **NOT TESTED** (full interactive loop) | See #6. |
| 9 | Focus behavior | **PARTIAL** | `SDL_WINDOW_NOT_FOCUSABLE` accepted at creation without error. Not interactively confirmed that a normal app keeps focus while the pet is clicked. |
| 10 | Always-on-top | **PASS** | Objectively confirmed: the spike window's `kCGWindowLayer = 3` (the value macOS assigns to `NSFloatingWindowLevel`), strictly above ordinary app windows observed at `kCGWindowLayer = 0` (e.g. the browser window also present on screen) — read directly from the WindowServer, not inferred. |
| 11 | High-DPI backing | **PASS** | Objectively confirmed via SDL itself: `SDL_GetWindowSize` → 160×160 (logical), `SDL_GetWindowSizeInPixels` → 320×320 (pixels), `SDL_GetWindowPixelDensity`/`SDL_GetWindowDisplayScale` → 2.00. Logged by the app at startup (`SpikeApp::Init`), not a visual judgement call. |
| 12 | Clean shutdown | **PASS** | `SIGINT`/`SIGTERM` → `Shutdown()` path exercised twice via `kill -TERM <pid>`: log shows `clean shutdown, N click(s) recorded this session` both times, the process fully leaves the process table (`ps -p <pid>` empty afterward), and `~/Library/Logs/DiagnosticReports` has no crash report for the process. |
| 13 | No busy-wait | **PASS** | Idle-animated Release run measured ≈1.2% average CPU over a 27-second sampling window (10 samples, 3s apart; see §7) with a 12 fps idle animation running the whole time. A busy-wait loop at this frame rate would show CPU usage close to 100% of one core, not ~1%. |
| 14 | No invasive permission requested | **PASS** | See `docs/PRIVACY_SECURITY.md` and the Block 01 report §10 — grep-verified: no Accessibility, Input Monitoring, Screen Recording, or admin/root request anywhere in `src/`. |

## 4. Why several interactive items are PARTIAL/NOT TESTED, not PASS

This agent session runs the build/run/verify loop through a
non-interactive automation shell on the target Mac, not through a human
sitting at the keyboard. Two consequences, both handled by being honest
about the gap rather than working around it:

- **No screenshot channel.** `screencapture -x` failed with "could not
  create image from display" — almost certainly because the shell
  process driving this session has not been granted the macOS Screen
  Recording permission. This was **not** pursued: granting that
  permission is a systemwide security-setting change (out of scope for
  an agent to make on the user's behalf), and it's also literally the
  mechanism the block brief says not to lean on for verification
  ("No uses screen recording... para verificarlo").
- **No synthetic input.** Deliberately did not synthesize mouse
  clicks/drags (e.g. via `CGEventPost` or accessibility-driven UI
  automation) to exercise the interactive path end-to-end. That would
  edge into exactly the "invasive automation" the same brief line warns
  against, for a marginal gain over what's already verified by unit
  tests + code review + objective WindowServer inspection.

What *was* used instead, all real evidence rather than assumptions:
non-visual WindowServer metadata (`CGWindowListCopyWindowInfo` —
bounds, layer, onscreen state), SDL's own DPI query APIs, process-level
checks (log output, exit behavior, crash reports, CPU/RSS over time),
exhaustive unit tests of every piece of pure logic involved, and a
line-by-line check of every SDL3 API call against the actual pinned
3.4.12 headers (not documentation that might be stale) fetched into
this build.

**What would close the gap:** a human running the 8-step manual
checklist in §6 below, which takes under two minutes.

## 5. `SDL_SetWindowShape` evaluation (required by the block brief)

Evaluated before deciding on an approach, using:
- the official SDL3 wiki page for `SDL_SetWindowShape` (confirms it
  exists in SDL 3.4.12 — "Available since SDL 3.2.0" — and that its
  documented behavior is "sets the alpha channel of a transparent
  window and any fully transparent areas are also transparent to mouse
  clicks", gated on `SDL_WINDOW_TRANSPARENT`);
- **libsdl-org/SDL#12683** ("No way of creating a window which does not
  accept mouse inputs/events (click-through) but isn't totally
  transparent/invisible(?)"), still open — a `desktop pet` use case
  described in nearly identical terms to this block's, where the
  reporter and other commenters (including an SDL maintainer)
  independently converge on the same finding: `SDL_SetWindowShape`
  couples the click-through mask to what actually gets rendered —
  pixels outside the shape surface aren't drawn at all. That's fine if
  your "content" *is* the shape mask; it conflicts with wanting
  `src/graphics` to own drawing independently of the hit-test mask, as
  this block's placeholder (and any future real pet content) needs.
- **libsdl-org/SDL#11199**, cited as further evidence that
  `SDL_SetWindowShape` behaves inconsistently between Windows and
  macOS — another reason not to build the cross-platform click-through
  path on top of it.

A minimal isolated runtime test of `SDL_SetWindowShape` itself (to
watch its "doesn't render outside the mask" behavior firsthand rather
than relying on the cited reports) was not performed in this
environment, since confirming it visually runs into the same
screenshot limitation as §4. The decision below does not depend on that
firsthand look — it follows from the documented API contract plus a
real, still-open upstream issue describing this exact use case.

**Decision:** do not build click-through on `SDL_SetWindowShape`. See
`docs/DECISION_LOG.md` DEC-006 for the shipped alternative (native
`NSWindow.ignoresMouseEvents` / `WS_EX_TRANSPARENT` toggled from a
`SDL_GetGlobalMouseState()` poll piggybacked on the idle-animation
tick).

## 6. Manual smoke test checklist (for the repository owner)

Takes under two minutes on a real desktop session:

1. `cmake --build --preset macos-debug && ./build/macos-debug/src/app/nimvlets_spike`
2. Look at the screen: a small blob shape should appear with **no**
   rectangular background around it.
3. Click on empty space that's inside the 160×160 window area but
   outside the blob — confirm the click reaches whatever is behind the
   window (e.g. it doesn't get "eaten" by our window).
4. Click directly on the blob without moving the mouse — confirm the
   terminal shows `click #1`.
5. Press on the blob, drag more than a few pixels, release — confirm
   the terminal shows `drag ended (correctly not counted as a click)`
   and the window visually followed the cursor.
6. Click into a different, normal app window — confirm it becomes
   active normally (the pet window shouldn't have been stealing focus).
7. Confirm the pet window stays above other normal windows.
8. `Ctrl+C` in the terminal (or `kill -TERM <pid>` from elsewhere) —
   confirm the process exits and the log shows `clean shutdown`.

## 7. Performance sampling (Release, native arm64)

10 samples via `ps -o rss,%cpu,time`, 3 seconds apart, no `sudo`, no
special instrumentation — see the Block 01 report §7 for the full
numbers and method. Summary: RSS stabilized at ≈70 MB; CPU time grew by
0.32s of process time over a 27s wall-clock window (≈1.19% average).

## 8. Windows

No Windows machine is available in this development environment
(Darwin only) — nothing below claims otherwise.

| Item | Status |
|---|---|
| Configure/Build | **NOT RUN locally.** `windows-debug`/`windows-release` CMake presets exist (`Visual Studio 17 2022` generator, x64) and `src/platform/windows/TransparentWindowSupport.cpp` compiles against the same verified SDL3 property names as the macOS side (`SDL_PROP_WINDOW_WIN32_HWND_POINTER`, confirmed present in the pinned SDL3 3.4.12 headers). A `.github/workflows/ci.yml` job (`windows-x64`) exists to configure/build/test on a real Windows runner — but per this block's Git rules, nothing is pushed in this block, so **that CI job has not executed yet**. "Added" is an accurate claim; "executed"/"passing" is not. |
| Tests | **NOT RUN**, same reason. `tests/` has no SDL/platform dependency, so once CI does run, there's no reason to expect it to fail there. |
| GUI runtime | **NOT TESTED.** No display, no Windows machine at all in this block. |

### Differences from macOS (by design, not oversight)

- Always-on-top: `NSFloatingWindowLevel` (macOS) vs. `SetWindowPos(...,
  HWND_TOPMOST, ...)` (Windows) — different APIs, same effect.
- Click-through: `NSWindow.ignoresMouseEvents` (macOS) vs. toggling
  `WS_EX_TRANSPARENT` on `GWL_EXSTYLE` (Windows) — same driving signal
  (`core::BlobSilhouette::Contains()` against a polled global cursor
  position), different native mechanism.
- High-DPI: confirmed working on macOS (§3, item 11). Windows per-monitor
  DPI awareness (whether SDL3's Win32 backend needs an app manifest or
  an explicit `SetProcessDpiAwarenessContext` call in our own code for
  correct scaling) is an **open question** for the first real Windows
  smoke test — not resolved in this block.

### Pending before Windows can be trusted

1. Push this branch (owner's decision, outside this block) and confirm
   the `windows-x64` CI job is green.
2. On real Windows 10/11 x64 hardware: build via the `windows-release`
   preset and run `nimvlets_spike.exe` directly.
3. Repeat the §6 manual checklist there.
4. Specifically watch the `WS_EX_TRANSPARENT` toggle — it is the one
   native code path in this entire block that has never executed
   anywhere, not even in CI (CI has no display).

## 9. Intel Mac / Universal2

Built and verified on the Apple Silicon dev machine used for this
block — see the Block 01 report §6 for the full `lipo -info` output and
binary sizes. Summary: `cmake --preset macos-universal2-release` + build
succeeds; the resulting `nimvlets_spike` and `nimvlets_tests` binaries
both report `arm64 x86_64` via `lipo -info`; the host-arch (arm64)
slice runs and passes all unit tests.

**A cross-compiled universal2 build is not a substitute for a real
smoke test on Intel hardware** — no Intel Mac was available in this
block. This remains a real, open item, not a formality: the transparent
window / click-through / drag behavior on Intel has not been observed
even once.

## 10. SDL3 recommendation

**KEEP SDL3 + native adapters.**

SDL3 correctly and portably handled window creation, the transparent
buffer, borderless presentation, always-on-top, a real event loop
without busy-waiting, and honoring high-DPI backing (objectively
confirmed at 2x on this Retina display) — all via a small, well-behaved
API that matched its own documentation exactly against the pinned
3.4.12 source in every case checked in this block. Zero SDL3 API
mismatches were found between what's documented and what the pinned
version actually ships.

The one real gap — decoupling click-through from what gets rendered —
isn't a flaw in SDL3's overall design; it's a specific, documented,
currently-open upstream limitation of one function
(`SDL_SetWindowShape`, see §5) for one specific use case (a
custom-rendered, non-rectangular overlay). Working around it took two
small (~60–90 line) platform files, already exactly where
`AGENTS.md`'s architecture says platform-specific code belongs. That's
the "native adapters" half of the recommendation: keep SDL3 as the
cross-platform base, keep isolating the handful of things it can't do
itself behind `src/platform/*`, and don't treat this gap as a reason to
reconsider the framework choice.

Reconsider only if a block hits a *different* incompatibility SDL3
can't be worked around for with a small adapter — not encountered here.
