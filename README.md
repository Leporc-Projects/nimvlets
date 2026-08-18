# Nimvlets

Lightweight, native, cross-platform desktop companion. A small
transparent window shows one creature on your desktop; drag it around,
click it to earn clicks (the only currency), spend clicks to unlock more
creatures permanently.

This repository is currently in **Block 03 — Local State Persistence**,
built on Block 02's Content + Animation Foundation (a small, data-driven
content+animation runtime — see
[`docs/ANIMATION_RUNTIME.md`](docs/ANIMATION_RUNTIME.md)) and Block 01's
Foundation + Platform Feasibility Spike (disciplined repo bootstrap plus
a proof that the core windowing/transparency/hit-testing/drag approach
is viable). Block 03 adds a small local, offline-only persistence layer
— click balance, active pet id, and last window position survive a
restart — see [`docs/PERSISTENCE.md`](docs/PERSISTENCE.md). This is
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
# run from the repo root — the pet pack below is loaded via a
# relative path that resolves from the current working directory
./build/macos-debug/src/app/nimvlets_spike

# QA convenience: passive action every ~5s instead of the real ~300s
# default (production behavior is unchanged — see docs/ANIMATION_RUNTIME.md §8)
NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS=5 ./build/macos-debug/src/app/nimvlets_spike

# QA convenience: persist to an isolated directory instead of the real
# per-user app-data location (see docs/PERSISTENCE.md §2)
NIMVLETS_DEV_APPDATA_DIR=/tmp/nimvlets_dev_state ./build/macos-debug/src/app/nimvlets_spike
```

This opens a small (160×160), borderless, always-on-top, transparent
window showing **Bunny** — a deterministically-generated DEV animation
pack (`assets/dev/bunny_pack.nvpack`) derived from a real illustrated
QA fixture the repository owner supplied in Block 01, *not* final
content (see [`docs/PET_CONTENT_SPEC.md`](docs/PET_CONTENT_SPEC.md) and
[`docs/ANIMATION_RUNTIME.md`](docs/ANIMATION_RUNTIME.md)). The pack is
required, not optional — if it can't be loaded (e.g. not run from the
repo root), the app logs a specific error and exits rather than
falling back to any placeholder. Click the visible region to increment
the click balance (persisted locally — see
[`docs/PERSISTENCE.md`](docs/PERSISTENCE.md)) and play a short click
reaction; every ~300s (or the DEV override above) it also plays a
short passive action on its own. Drag it to move the window (the new
position is persisted too); clicking the transparent area passes
through to whatever's beneath. Closing and reopening the window
reopens where you left it, with your click balance intact.

On macOS, click-through hit-testing is handed to SDL's own
`SDL_SetWindowShape()` mechanism (event-driven, no polling) — see
`docs/PLATFORM_SPIKE.md` §5.1 for why a manual approach was tried
first and replaced. A poll-driven fallback exists for Windows, where
that mechanism isn't verified safe yet.

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
src/core         pure C++20 logic, no SDL — unit tested in isolation
src/content      pure, data-driven content model + animation controller + pack loader (no SDL)
src/persistence  pure local state model + serializer + atomic-write store + debounce scheduler (no SDL)
src/graphics     SDL rendering: turns a content::FrameDefinition into a texture
src/platform     native macOS (AppKit) / Windows (Win32) window glue
src/app          the spike executable: event loop + wiring
tests/           CTest-driven unit tests for src/core, src/content, and src/persistence
tools/           dev tooling (stats_loc.py, prep_dev_sprite.py, compile_pet_pack.py, generate_bunny_dev_pack.py)
assets/dev/      dev-only placeholder assets (see assets/dev/README.md)
cmake/           CMake helper modules (warnings, SDL3 fetch)
docs/            product + engineering contracts (see AGENTS.md §17)
```

See [`AGENTS.md`](AGENTS.md) for the full engineering contract
(architecture, privacy/security rules, dependency rules, Git workflow).
