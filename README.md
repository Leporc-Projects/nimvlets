# Nimvlets

Lightweight, native, cross-platform desktop companion. A small
transparent window shows one creature on your desktop; drag it around,
click it to earn clicks (the only currency), spend clicks to unlock more
creatures permanently.

This repository is currently in **Block 01 — Foundation + Platform
Feasibility Spike**: a disciplined repo bootstrap plus a proof that the
core windowing/transparency/hit-testing/drag approach is viable. It is
explicitly *not* the finished product yet — see
[`docs/PLATFORM_SPIKE.md`](docs/PLATFORM_SPIKE.md) for what's been
verified and what hasn't, and `AGENTS.md` for the permanent engineering
contracts this and every future block follow.

## Requirements

- CMake ≥ 3.25
- A C++20 compiler (Apple Clang / Clang / MSVC — see below)
- macOS: Xcode Command Line Tools (`xcode-select --install`)
- Windows: Visual Studio 2022 with the "Desktop development with C++"
  workload
- Python 3 (for `tools/stats_loc.py` only — no packages needed)
- No manual SDL3 install required — see below.

## Build

SDL3 (pinned to `release-3.4.12`) is fetched automatically via CMake
`FetchContent` on first configure; there's nothing to install by hand.

```bash
# macOS (native host architecture)
cmake --preset macos-debug        # or macos-release
cmake --build --preset macos-debug

# macOS universal2 (Apple Silicon + Intel in one binary)
cmake --preset macos-universal2-release
cmake --build --preset macos-universal2-release
lipo -info build/macos-universal2-release/src/app/nimvlets_spike

# Windows x64 (from a Developer PowerShell / VS environment)
cmake --preset windows-debug      # or windows-release
cmake --build --preset windows-debug
```

Build directories live under `build/<preset-name>/`, never inside the
source tree.

## Run the foundation spike

```bash
./build/macos-debug/src/app/nimvlets_spike
```

This opens a small (160×160), borderless, always-on-top, transparent
window with a simple two-circle placeholder shape — deliberately not
final art, see [`docs/PET_CONTENT_SPEC.md`](docs/PET_CONTENT_SPEC.md).
Click the shape to log an in-memory click count to stdout; drag it to
move the window.

The window is intentionally borderless and non-focusable (that's the
product requirement, not a bug), so it has no close button. Quit it from
the terminal you launched it from with **Ctrl+C**, or from elsewhere
with:

```bash
pkill -TERM -f nimvlets_spike
```

## Test

```bash
ctest --preset macos-debug --output-on-failure
```

Tests are pure `src/core` logic (gesture classification, frame-timing
math, hit-testing geometry) — no SDL, no display required, so they run
the same in CI as on a dev machine.

## LOC stats

```bash
python3 tools/stats_loc.py
```

Reproducible, dependency-free line count for this repo's own
Application/Tooling/Tests/Documentation, excluding SDL3 and build
output. See `AGENTS.md` §7 and `tools/stats_loc.py`'s own docstring.

## Repository layout

```
src/core       pure C++20 logic, no SDL — unit tested in isolation
src/graphics   SDL rendering of the placeholder shape
src/platform   native macOS (AppKit) / Windows (Win32) window glue
src/app        the spike executable: event loop + wiring
tests/         CTest-driven unit tests for src/core
tools/         dev tooling (stats_loc.py)
cmake/         CMake helper modules (warnings, SDL3 fetch)
docs/          product + engineering contracts (see AGENTS.md §17)
```

See [`AGENTS.md`](AGENTS.md) for the full engineering contract
(architecture, privacy/security rules, dependency rules, Git workflow).
