# Nimvlets — Decision Log

Each decision has a stable ID (`DEC-NNN`) that never gets reused or
renumbered, even after the decision is superseded. Status values:

- **DECIDIDO** — settled, current, binding.
- **CANDIDATO** — a working choice for now, explicitly open to
  revisiting with evidence (most spike-driven technical picks start
  here).
- **ABIERTO** — genuinely undecided; do not implement as if it were
  settled.
- **SUPERSEDED** — was decided, no longer holds; superseding decision
  is cross-referenced.

---

### DEC-001 — Core stack: C++20, CMake, macOS + Windows for v1
**Status:** DECIDIDO · Block 01

C++20 as the implementation language, CMake ≥ 3.25 as the build system.
v1 platforms are macOS (Apple Silicon + Intel, universal2) and Windows.
Linux is out of scope for v1. No embedded web runtime.

---

### DEC-002 — Single companion window, no fullscreen overlay
**Status:** DECIDIDO · pre-Block 01 (reaffirmed)

Exactly one Nimvlet visible at a time, in a small transparent borderless
window — never a fullscreen overlay, never a rectangular visible
background. Fully transparent pixels are click-through.

---

### DEC-003 — Clicks are a spendable wallet, not a threshold gate
**Status:** DECIDIDO · Block 01

Clicks are the only currency. Starting balance is 0. Buying a Nimvlet
subtracts its price from the balance; ownership is permanent; switching
between owned Nimvlets is free. No notification on earning, in the
current version. Shop and Collection are separate future UI areas.

**Supersedes:** DEC-009 ("click threshold auto-unlocks Nimvlets").

---

### DEC-004 — SDL3, pinned exact version, evaluated as a spike candidate
**Status:** CANDIDATO · Block 01 — see `docs/PLATFORM_SPIKE.md` §"SDL3
recommendation" for the evidence-based verdict at the close of this
block.

SDL3 pinned to `release-3.4.12` via CMake `FetchContent`, built as a
static library from source, is used for the Block 01 spike. This is
explicitly **not** a final architecture commitment — see the block
brief's framing ("SDL3 es CANDIDATO, no decisión arquitectónica
final"). A future block may reconsider if a concrete incompatibility
shows up.

---

### DEC-005 — Placeholder rendering: analytic shape, no image assets
**Status:** DECIDIDO · Block 01 (spike scope)

The spike's placeholder creature is two overlapping filled circles
defined by plain math (`core::BlobSilhouette`), rendered via manual
scanline fill through `SDL_RenderFillRect` — no `SDL_image`, no texture
loading, no bitmap asset. The same math is reused for hit-testing (see
DEC-006), so rendering and click-through can never disagree with each
other. Not final art — see `docs/PET_CONTENT_SPEC.md`.

---

### DEC-006 — Click-through via native API + cursor polling, not `SDL_SetWindowShape`
**Status:** DECIDIDO · Block 01 (spike scope) — full evaluation and
evidence in `docs/PLATFORM_SPIKE.md`.

`SDL_SetWindowShape()` was evaluated first, per the block brief. On SDL
3.4.12 it couples the click-through mask to what actually gets
rendered (pixels outside the shape surface aren't drawn at all), which
conflicts with wanting `src/graphics` to own drawing independently —
and is a known upstream limitation, not specific to Nimvlets
(libsdl-org/SDL#12683: "No way of creating a window which does not
accept mouse inputs/events (click-through) but isn't totally
transparent/invisible"). The shipped approach instead:

1. renders normally via `src/graphics`, with per-pixel alpha;
2. on macOS, toggles `NSWindow.ignoresMouseEvents`; on Windows, toggles
   `WS_EX_TRANSPARENT` on the extended window style (Windows path
   compiled but not yet run on real hardware — see PLATFORM_SPIKE.md);
3. decides which way to toggle by evaluating
   `core::BlobSilhouette::Contains()` against the cursor position from
   `SDL_GetGlobalMouseState()`, polled once per idle-animation tick (no
   extra wakeups, no global input hook, no new permission).

---

### DEC-007 — Manual window drag, not `SDL_HITTEST_DRAGGABLE`
**Status:** DECIDIDO · Block 01

Dragging the pet is implemented by tracking raw mouse button/motion
events ourselves and calling `SDL_SetWindowPosition` once the drag
threshold is crossed, rather than marking the window region
`SDL_HITTEST_DRAGGABLE` and letting the OS move it. The OS-drag approach
would hand the whole gesture to the window manager, and the app would
never see the press/move/release sequence `core::DragClassifier` needs
to tell a click from a drag.

---

### DEC-008 — Global click mode: real, future, explicitly opt-in
**Status:** DECIDIDO (scope only — not implemented) · Block 01

A future, separate, opt-in feature may count clicks anywhere on the
system. It must explain the exact OS permission it needs before
requesting it, must be mouse-only, must never inspect screen content or
track app identity, and must never store coordinate/click-history data.
Not implemented and no permission requested in Block 01. See
`docs/PRIVACY_SECURITY.md`.

**Supersedes:** DEC-010 ("global clicks will never exist").

---

### DEC-009 — Click threshold auto-unlocks Nimvlets
**Status:** SUPERSEDED by DEC-003

Old idea: crossing a click-count threshold automatically unlocked the
next Nimvlet, with no spendable balance. Replaced by the wallet model.

---

### DEC-010 — Global clicks will never exist
**Status:** SUPERSEDED by DEC-008

Old idea: system-wide click counting was ruled out entirely as a
product direction. Replaced by "future, explicitly opt-in feature" —
see DEC-008 and `docs/PRIVACY_SECURITY.md`.

---

### DEC-011 — Starter onboarding: 3 visible + 1 secret at 44s
**Status:** DECIDIDO (product intent) — **NOT IMPLEMENTED**, out of
scope for Block 01.

First launch offers Artu, Rato, Rin Rin. An unanswered choice screen
for 44 seconds reveals a fourth, secret starter, Tan (white wolf), as a
structural easter egg using no third-party franchise assets/branding.
Tan's persistence/reappearance semantics are ABIERTO — see DEC-012.

---

### DEC-012 — Tan reappearance/persistence semantics
**Status:** ABIERTO

Whether missing the 44-second window is one-time, repeats every launch,
or something else, is not decided. Do not implement or assume an
answer.

---

### DEC-013 — Remaining v1 Nimvlets beyond the 4 named starters
**Status:** ABIERTO

v1 targets 8 functional Nimvlets; only Artu, Rato, Rin Rin, and Tan are
named. The remaining roster is not decided — do not invent names,
species, or art direction ahead of that decision.

---

### DEC-014 — No third-party test framework for Block 01
**Status:** CANDIDATO · Block 01

A ~40-line homemade test micro-harness (`tests/TestRunner.h`) is used
instead of adding Catch2/GoogleTest, since the block's test surface is
small (gesture classification, frame timing, hit-testing geometry) and
adding a dependency for it would violate AGENTS.md §10's "don't add a
dependency without a concrete, current reason." Revisit if a future
block's test surface grows enough to justify richer fixtures/matchers.

---

### DEC-015 — Signal-based dev shutdown for the borderless spike window
**Status:** DECIDIDO · Block 01 (spike scope only)

The spike window is borderless, `SDL_WINDOW_NOT_FOCUSABLE`, and
`SDL_WINDOW_UTILITY` by product design (see DEC-002), so it has no
close button and can never receive Cmd+Q. `src/app/SpikeApp` installs
`SIGINT`/`SIGTERM` handlers so `Ctrl+C` / `kill -TERM <pid>` triggers
the same clean `Shutdown()` path (destroy renderer/window, `SDL_Quit`)
used by a normal window-close event — this was necessary to make "clean
shutdown, no hung process" testable at all in this block, and is a
locally-made decision (see the Block 01 report, §13). The real product
will eventually expose a proper quit affordance (tray/menu bar); that's
explicitly NON-SCOPE for this block.

---

### DEC-016 — macOS window collection behavior: all Spaces, not fullscreen
**Status:** CANDIDATO · Block 01 (locally-made presentation choice)

`src/platform/macos` sets `NSWindowCollectionBehaviorCanJoinAllSpaces |
NSWindowCollectionBehaviorStationary` so the pet follows the user across
ordinary virtual desktops (reasonable for a persistent companion)
without requesting `NSWindowCollectionBehaviorFullScreenAuxiliary`
(presence over fullscreen apps, which is explicitly future/configurable
per the PRD and out of scope here). Not specified verbatim in the block
brief — recorded here for visibility, not implied to be final product
behavior.
