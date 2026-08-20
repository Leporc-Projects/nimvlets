# AGENTS.md — Nimvlets engineering contract

This file is the **authoritative** source of permanent contracts for any
coding agent (or human) working on this repository. `CLAUDE.md` points
here and adds only tool-specific notes — if the two ever disagree, this
file wins.

Nimvlets is developed in discrete **blocks**. Each block has its own
brief with its own scope; this file only records what's meant to survive
across all of them. Block-specific status, spike results, and
in-progress decisions live in `docs/` (see "Documentation map" below).

## 1. Purpose

Nimvlets is a lightweight, native, cross-platform desktop companion
("pet") product built by Leporc Projects. A small transparent window
shows one creature on the user's desktop at a time; the user can drag it
around and click it to earn clicks, which are the game's only currency,
spent permanently unlocking more creatures. See `docs/PRD_V1.md` for the
actual product scope and `docs/DECISION_LOG.md` for how we got there.

## 2. Product invariants

These hold across every block unless a future block explicitly
supersedes one in `docs/DECISION_LOG.md`:

- Exactly one Nimvlet is visible at a time.
- The pet lives in a small, transparent, borderless window — never a
  fullscreen overlay, never a rectangular visible background.
- Fully transparent pixels around the creature are click-through: the
  app underneath receives the click, not us.
- The pet can be dragged.
- Normal interaction should not steal OS focus more than the platform
  makes unavoidable.
- Local-first and offline-capable. No required account. No behavioral
  telemetry by default. No screen capture. No reading of personal
  files. No keylogging. No ads. No mandatory subscription. No embedded
  web runtime (Electron/Chromium/etc).
- Low resource consumption is a product requirement, not a later
  optimization — see `docs/PERFORMANCE_BUDGETS.md`.
- Event-driven scheduling. Do not run a permanent 60/144 FPS game loop
  when nothing is changing on screen.
- Clicks are the only currency; starting balance is 0; unlocking a
  creature spends clicks permanently; switching between owned creatures
  is free. See `docs/PRD_V1.md` and `docs/DECISION_LOG.md`.
- The click counter is never a permanent desktop overlay; it only
  appears inside product UI once that UI exists.

## 3. Architecture and dependency direction

```
src/core       <- pure C++20, no SDL, no platform headers. Testable in
                   isolation (tests/). Geometry, gesture classification,
                   frame-timing math, content-shape math.
src/graphics   <- SDL-aware rendering. Depends on core. No platform
                   (AppKit/Win32) headers.
src/platform/* <- native OS glue (Objective-C++ on macOS, Win32 on
                   Windows, X11/Wayland detection on Linux), behind a
                   small shared header
                   (src/platform/TransparentWindowSupport.h). Depends on
                   core + SDL. Nothing outside src/platform/* includes
                   AppKit or <windows.h>. src/platform/LinuxBackendPolicy.h
                   is the one exception to "one adapter dir per OS": it's
                   pure X11-vs-Wayland capability logic with no SDL/X11/
                   Wayland headers of its own, so it's built and tested
                   on every host — see docs/LINUX_PLATFORM.md §4.
src/app        <- the executable: owns the window/event loop, wires
                   core + graphics + platform together.
```

Dependencies only point downward through that list — `core` never
depends on `graphics`, `platform`, or `app`; `graphics` never depends on
`platform` or `app`; platform adapters never depend on `app`.

**One core, not two codebases.** macOS and Windows share every line of
`src/core`, `src/graphics`, and `src/app`. Platform-specific code is
isolated behind `src/platform/macos/` or `src/platform/windows/`, both
implementing the same header. If you find yourself writing `#ifdef
_WIN32` / `#ifdef __APPLE__` inside `src/app` or `src/core`, that's a
sign the platform seam is in the wrong place — move the branch into
`src/platform/*` instead.

Release artifacts may eventually be split per platform/architecture
under `dist/`, but `dist/` never contains versioned build output —
see `.gitignore`.

## 4. Platform rules

- Objective-C++/AppKit only behind `src/platform/macos/`, and only when
  SDL3's cross-platform API can't do the job (documented per-case in
  `docs/PLATFORM_SPIKE.md`).
- Win32 only behind `src/platform/windows/`.
- X11/Wayland-specific code only behind `src/platform/linux/`, detected
  at runtime via `SDL_GetCurrentVideoDriver()` — never force one
  backend globally. Only add native X11/Wayland code where reading the
  pinned SDL3 source first showed SDL's cross-platform API genuinely
  insufficient — see `docs/LINUX_PLATFORM.md` §3 for the methodology
  and its findings.
- Never claim a platform behavior (transparency, hit-testing, focus,
  drag, always-on-top, GUI runtime in general) is verified unless it was
  actually run on that OS. `docs/PLATFORM_SPIKE.md` tracks PASS / FAIL /
  PARTIAL / NOT TESTED per item, per platform — CI compiling something
  is not the same claim as a human having watched it run. Do not claim
  parity where the window system itself imposes a limitation (e.g.
  Wayland's `xdg-shell` has no client-requestable always-on-top or
  absolute toplevel positioning — see `docs/LINUX_PLATFORM.md` §6);
  document the limitation instead of hacking around it.
- macOS supports both Apple Silicon and Intel (`universal2`). Linux
  x86_64 (X11 and Wayland) is supported since Block 04.1 — see
  `docs/LINUX_PLATFORM.md`; other Linux architectures and
  packaging/distribution formats remain out of scope.

## 5. Privacy & security

- Standard interaction path only ever reads input aimed at our own
  window/UI (SDL window events) or polls cursor *position*
  (`SDL_GetGlobalMouseState`, `NSEvent.mouseLocation`-equivalent —
  position only, not content). It never installs a global input hook.
- **Do not request Accessibility, Input Monitoring, Screen Recording,
  or admin/root permissions.** If some feature seems to need one, stop
  and document the blocker instead of silently requesting it — see
  `docs/PRIVACY_SECURITY.md`.
- No screen capture, no keyboard logging, no enumerating other apps for
  behavioral purposes, no network sockets, no telemetry, no
  runtime-downloaded assets.
- **Global click counting is a future, explicitly opt-in feature.** Do
  not implement it, and do not request any permission for it, until a
  block brief explicitly says to. When it does land, it must be
  mouse-only (no keyboard), must not do content/screen inspection or
  app-name tracking, and must not store coordinate/history data — only
  increment a counter. See `docs/PRIVACY_SECURITY.md`.

## 6. Performance

See `docs/PERFORMANCE_BUDGETS.md` for the actual numbers. In short:
small RAM/CPU/binary footprint is a hard requirement, measured on
Release builds only, never asserted from a single 1-second sample.

## 7. Build & test commands

Requires CMake ≥ 3.25 and a C++20 compiler. SDL3 is fetched
automatically (pinned version — see §9).

```bash
# Configure + build (pick the preset for your platform)
cmake --preset macos-debug            # or macos-release, windows-debug, windows-release, linux-debug, linux-release
cmake --build --preset macos-debug

# macOS universal2 (arm64 + x86_64)
cmake --preset macos-universal2-release
cmake --build --preset macos-universal2-release
lipo -info build/macos-universal2-release/src/app/nimvlets_spike

# Tests
ctest --preset macos-debug --output-on-failure

# Run the foundation spike
./build/macos-debug/src/app/nimvlets_spike
# (borderless/utility/non-focusable window by design — quit with
# Ctrl+C or `kill -TERM <pid>` in the terminal you launched it from,
# see docs/PLATFORM_SPIKE.md)

# Reproducible LOC stats
python3 tools/stats_loc.py
```

## 8. C++ style

- C++20. `#pragma once` for headers.
- 4-space indent, no tabs (see `.editorconfig`).
- Namespaces mirror directories: `nimvlets::core`, `nimvlets::graphics`,
  `nimvlets::platform`, `nimvlets::app`.
- Prefer `const`/`constexpr`, pass small value types (e.g.
  `core::Point`) by value, pass owning types by reference.
- Reasonable warnings are on by default (see
  `cmake/NimvletsWarnings.cmake`); fix warnings rather than silencing
  them unless there's a documented reason.

## 9. Ownership / RAII

- No manual `new`/`delete`. SDL resources (`SDL_Window*`,
  `SDL_Renderer*`, …) are owned by exactly one object
  (`app::SpikeApp`), created in `Init()` and destroyed in `Shutdown()`
  in reverse order, unconditionally, even on early-exit paths.
- Native platform objects (`NSWindow*`, `HWND`) are borrowed, never
  owned, by `src/platform/*` — SDL/AppKit/Win32 own the real lifetime;
  our code only configures them.
- No global mutable state, with one narrow, documented exception:
  `src/app/SpikeApp.cpp`'s `std::atomic<bool>` shutdown flag, which
  exists because POSIX signal handlers cannot be member functions —
  see the comment on `app::ShutdownRequested()`.

## 10. Dependency rules

- SDL3 is the only third-party dependency, pinned to an exact release
  tag (never a branch or "latest") via CMake `FetchContent` — see
  `cmake/FetchSDL3.cmake` and `docs/DECISION_LOG.md`.
- Don't add SDL_image, SDL_ttf, SDL_mixer, or any other library without
  a documented, concrete reason tied to a block's actual requirements.
- No ECS, no dependency-injection framework, no plugin system, no
  scripting engine, no database, no generic "engine" abstraction layer.
  Build only what the current block's spec actually needs.
- Third-party source lives under the build directory (FetchContent's
  `_deps`), never copied into the versioned tree, and is excluded from
  LOC counting (`tools/stats_loc.py`) and from warnings-as-errors.

## 11. Asset rules

- No audio or branding in this repo yet — see each block's NON-SCOPE
  list. Real (non-placeholder) visual art started landing in Block
  04.2 (Nidir — see `docs/NIDIR_CONTENT.md`); only introduce real art
  for a specific pet when a block brief actually supplies/requires it,
  never speculatively.
- Development placeholders (still used for anything without real art
  yet) are procedurally generated or simple geometric shapes by
  default, never anything imitating a third-party franchise. A real
  (non-procedural) asset used purely as a **QA fixture** (not a
  Nimvlet) is only acceptable narrow and explicitly-scoped — supplied
  deliberately by the repository owner, clearly documented as
  temporary/non-canonical, and never introduced unprompted by an
  agent. See `docs/DECISION_LOG.md` DEC-018 (the "Bunny" QA fixture)
  for that precedent. Real pet art (Nidir onward) is a different,
  permanent category — see the next bullet and
  `docs/NIDIR_CONTENT.md` for its source/import/normalization
  contract.
- When real pet content lands, its shape/contract is
  `docs/PET_CONTENT_SPEC.md`, and its source-art convention (canonical
  individual PNG frames, spritesheet as a secondary artifact,
  `DESCRIPTION.txt` for stable physical traits, explicit right/left
  directional sets) is `docs/NIDIR_CONTENT.md` §1 — don't hardcode a
  specific creature's behavior in engine code if it can be data
  instead (see §13).

## 12. Test rules

- Favor a small number of high-value tests over snapshot tests or
  redundant matrices (see block briefs' "high-value tests" sections).
- Anything that's pure math/logic (gesture classification, frame-timing
  deadlines, hit-testing geometry) belongs in `src/core` specifically
  *so that* it's unit-testable without SDL, a window, or a display —
  see `tests/`.
- Timing logic must be testable with fabricated timestamps, never by
  actually sleeping in a test.
- No mocking frameworks for features that don't exist yet.
- All tests run through CTest (`ctest --preset <preset>`).

## 13. No hardcoded per-creature logic

Don't special-case an individual Nimvlet's behavior in engine/runtime
code. A frog's hop, a wolf's idle animation, etc. should be expressible
as content/data (see `docs/PET_CONTENT_SPEC.md`) once the content system
exists. Nothing in this block hardcodes creature identity — the spike's
placeholder shape is a generic, unnamed "blob," not a Nimvlet.

## 14. Global input monitoring

Do not implement global (system-wide) click or keyboard monitoring of
any kind, and do not request the permissions it would need
(Accessibility / Input Monitoring), until a future block brief
explicitly authorizes it as the separate, opt-in "global click mode"
feature described in `docs/PRIVACY_SECURITY.md`. This is a hard rule,
not a default that changes because it would be convenient.

## 15. Git workflow

- One local branch per block: `block/NN-short-name`.
- Commit in a handful of logical commits (roughly one per coherent
  piece of work), not one-commit-per-file and not a single "updates"
  commit.
- No `git commit --amend` on commits you didn't just make in this
  session, no squashing, no force push.
- **Coding agents do not merge to `main` and do not push/publish**,
  unless a future prompt explicitly changes this rule for a specific
  session. Land at a clean working tree on the block branch and report;
  the repository owner runs integration/publish commands manually after
  review.

## 16. Don't build ahead of scope

Don't add product-facing features just because the code would make it
easy (a Shop screen, persistence, tray icon, etc.) — check the current
block's NON-SCOPE list first. Speculative future-proofing that isn't
asked for is itself a form of scope creep.

## 17. Language conventions (code comments & documentation)

Established starting Block 03; applied going forward, not as a
retroactive rewrite of untouched Block 01/02 content:

- First-party identifiers and code stay in English: variable/function/
  class/type/namespace/file names, commit messages, log strings, CLI
  flags/env vars, on-disk format magic (e.g. `"NVPACK1"`, `"NVSTATE1"`),
  and anything an API, build script, or CI depends on matching exactly.
- New, meaningful code comments are written in Spanish. A comment
  earns its place by explaining *why* — rationale, an invariant, a
  non-obvious behavior, a decision that isn't derivable from reading
  the next line of code — never by restating what that syntax already
  says.
- New or materially updated project documentation (`docs/*.md`,
  `README.md`, etc.) is written in Spanish where practical, while
  preserving exact API names, file paths, commands, flags, and other
  technical strings verbatim — never translated, never paraphrased.
- Applying this does not require translating existing English
  comments/docs wholesale. Convert a file's comments when that file is
  materially touched for other reasons; don't open unrelated,
  untouched files just to translate them.
- Every final block report must include the complete
  `tools/stats_loc.py` breakdown, not a subset: Application, Tooling,
  Tests, CODE TOTAL, Editorial/data, Documentation, RELEVANT TOTAL,
  Delta CODE TOTAL, Delta RELEVANT TOTAL.

## 18. Documentation map

| File | Purpose |
|---|---|
| `AGENTS.md` | This file — permanent engineering contracts. |
| `CLAUDE.md` | Claude Code-specific pointer; defers to this file. |
| `README.md` | Human quickstart: build, run, test. |
| `docs/PRD_V1.md` | Current product scope. |
| `docs/DECISION_LOG.md` | Dated decisions with stable IDs and status. |
| `docs/PLATFORM_SPIKE.md` | Per-block platform QA plan + results. |
| `docs/PERFORMANCE_BUDGETS.md` | Resource budgets and how they're measured. |
| `docs/PRIVACY_SECURITY.md` | What we access, what we never access. |
| `docs/PET_CONTENT_SPEC.md` | Data contract for a Nimvlet's content. |
| `docs/ANIMATION_RUNTIME.md` | Content model, pack format, and animation/scheduler design (Block 02). |
| `docs/PERSISTENCE.md` | Local state model, storage format, and write policy (Block 03). |
| `docs/CATALOG.md` | Pet identity, catalog format, and runtime switching design (Block 04). |
| `docs/LINUX_PLATFORM.md` | Linux (X11/Wayland) build, platform adapter, and CI smoke design (Block 04.1). |
| `docs/NIDIR_CONTENT.md` | Real asset source convention, import/normalization/mirror pipeline, and the directional content model (Block 04.2). |
| `docs/BUNNY_CONTENT.md` | Bunny's migration to real production art, canonical-direction inversion, and the QA-driven downscale-quality/70-30-hover/rest-of-size corrections (Block 04.3). |
| `docs/FRIN_CONTENT.md` | Frin's real male/female import and the named-state behavior graph (seated/lying transitions) it introduced (Block 05). |
