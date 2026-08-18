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
loading, no bitmap asset. The same math is rasterized into the shared
`core::AlphaMask` hit-test representation (see DEC-017, DEC-018), so
rendering and click-through can never disagree with each other. Not
final art — see `docs/PET_CONTENT_SPEC.md`. Still the fallback visual
when the Bunny QA fixture (DEC-018) isn't available, and still what
`tests/SilhouetteTest.cpp` exercises directly.

---

### DEC-006 — Click-through via native API + cursor polling, not `SDL_SetWindowShape`
**Status:** SUPERSEDED by DEC-017 (macOS) — the poll-driven mechanism
described below remains DECIDIDO as the **Windows fallback only**; see
DEC-017.

`SDL_SetWindowShape()` was evaluated first, per the block brief. On SDL
3.4.12 it was believed to couple the click-through mask to what
actually gets rendered (pixels outside the shape surface aren't drawn
at all), based on community reports (libsdl-org/SDL#12683, #11199) —
**this belief turned out to be Windows-specific, not true on macOS; see
DEC-017.** The shipped approach instead:

1. renders normally via `src/graphics`, with per-pixel alpha;
2. on macOS, toggles `NSWindow.ignoresMouseEvents`; on Windows, toggles
   `WS_EX_TRANSPARENT` on the extended window style (Windows path
   compiled but not yet run on real hardware — see PLATFORM_SPIKE.md);
3. decides which way to toggle by evaluating the active hit-test
   source against the cursor position from `SDL_GetGlobalMouseState()`,
   polled (no global input hook, no new permission).

Interactive macOS QA later found this **did not work reliably on
macOS** — see DEC-017 for the root cause and the fix. It remains
exactly as described above for Windows, where it is still the only
verified-safe mechanism (see DEC-017's Windows note).

---

### DEC-017 — macOS click-through: `SDL_SetWindowShape`, not manual polling
**Status:** DECIDIDO · Block 01 — full investigation, source citations,
and owner-confirmed QA results in `docs/PLATFORM_SPIKE.md` §5.1.

DEC-006's poll-driven mechanism was reported broken by the repository
owner in interactive macOS QA — a click on a transparent point near the
visible shape did not reach the application underneath, twice, even
after moving the poll to its own faster (~60Hz) schedule. Root cause,
found by reading the pinned **SDL 3.4.12 Cocoa backend source directly**
(`src/video/cocoa/SDL_cocoawindow.m`): SDL's own `-mouseMoved:` handler
calls `updateIgnoreMouseState:` on every real mouse-moved event for any
`SDL_WINDOW_TRANSPARENT` window, and that function **unconditionally
resets `NSWindow.ignoresMouseEvents` to `NO`** unless an
`SDL_SetWindowShape()` surface is set — silently undoing DEC-006's own
assignment on essentially every mouse movement, including the movement
immediately preceding a click.

Re-reading `SDL_SetWindowShape`'s actual macOS implementation
(`src/video/cocoa/SDL_cocoashape.m`, `Cocoa_UpdateWindowShape`) showed
DEC-006's original rejection reason didn't hold there: on macOS, that
function **only ever touches `ignoresMouseEvents`** — it does not
composite, clip, or otherwise touch rendered pixels. (The community
reports DEC-006 cited remain accurate for **Windows**, where the
classic `UpdateLayeredWindow` technique does use the shape bitmap as
the actual rendered content — see the Windows note below.)

**Decision:** on macOS, `SpikeApp::Init()` rasterizes whichever visual
is active (Bunny's real alpha channel, or the analytic placeholder —
see DEC-018) into an `SDL_Surface` and calls `SDL_SetWindowShape()`
once at startup. SDL's own event-driven `updateIgnoreMouseState:` then
handles all click-through toggling automatically, with zero polling
from the app. `platform::NativeShapeHitTestIsRenderSafe()` gates this:
`true` on macOS (verified via source), `false` on Windows (conservative
default — DEC-006's poll-driven mechanism remains the Windows path,
unverified either way on real hardware).

**Owner-confirmed QA result:** click on visible region → registers;
click on transparent region near the visible shape → reaches the
application underneath; drag → moves the window, not counted as a
click. See `docs/PLATFORM_SPIKE.md` §3 and §6.

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

---

### DEC-018 — Bunny: a real-asset QA fixture for hit-testing, not a content system
**Status:** DECIDIDO · Block 01 (closure QA scope only) — see
`docs/PLATFORM_SPIKE.md` §6.

The analytic placeholder (DEC-005) is mathematically exact but can't
validate hit-testing against real alpha data (antialiasing, a
non-convex silhouette, an actual texture). For final macOS closure QA,
the repository owner supplied a real illustrated asset ("Bunny") as a
**temporary fixture only** — explicitly not the start of the content
system `docs/PET_CONTENT_SPEC.md` describes, which still has zero
implementation in Block 01.

- **No new runtime PNG/image dependency:** `tools/prep_dev_sprite.py`
  (dependency-free Python, reusing this block's own PNG-decoding logic)
  converts the source PNG offline into `assets/dev/bunny.rgba`, a
  trivial uncompressed format (magic + width + height + raw RGBA8) the
  C++ side (`graphics::DevSprite`) reads with no PNG decoder and no
  `SDL_image` — keeping AGENTS.md §10's "no dependency without a
  concrete, current reason" intact.
- **Hit-testing threshold — `DevSprite::kHitTestAlphaThreshold = 128`
  (50%):** chosen from this asset's actual alpha histogram, not
  guessed: background pixels are exactly alpha=0 (60.6% of the image);
  interior/"visible" pixels cluster tightly at alpha≈253–254 (98% of
  all non-zero-alpha pixels are ≥128); only a thin antialiased edge
  band falls in between, which is exactly what a threshold should
  decide. 128 is also the standard antialiased-edge midpoint
  convention.
- **One hit-test source of truth:** `DevSprite::BuildAlphaMask()`
  rasterizes the loaded image's real alpha channel into the same
  `core::AlphaMask` type the analytic placeholder rasterizes into (see
  DEC-017) — both the `SDL_SetWindowShape` surface and the
  MOUSE_BUTTON_DOWN defense-in-depth check read from whichever mask is
  active, so rendering and click-through can never disagree, exactly
  matching DEC-005's original guarantee for the placeholder.
- **Fallback, not replacement:** if the fixture file can't be loaded,
  `SpikeApp` logs that and falls back to the unchanged analytic
  placeholder — Bunny never became a hard dependency of the spike
  building or running.

**Owner-confirmed QA result:** see `docs/PLATFORM_SPIKE.md` §6 —
click-visible, click-through-transparent, and drag-not-click all
confirmed with Bunny as the closure fixture.

---

### DEC-019 — High-DPI render scale fix: `SDL_SetRenderLogicalPresentation`
**Status:** DECIDIDO · Block 01 (bug fix) — see
`docs/PLATFORM_SPIKE.md` §5.2.

Found by the agent pixel-inspecting a captured frame (not visually
reported by the owner): on this 2x Retina display, the rendered visual
filled only the window's top-left quadrant at half its intended size.
`SDL_SetRenderLogicalPresentation()` was never called, so the default
`SDL_LOGICAL_PRESENTATION_DISABLED` mapped render coordinates 1:1 to
physical backbuffer pixels instead of scaling from the logical 160×160
space `core::BlobSilhouette` (and, later, the Bunny fixture's
destination rect) is authored in. Fixed with one call
(`SDL_SetRenderLogicalPresentation(renderer, 160, 160,
SDL_LOGICAL_PRESENTATION_LETTERBOX)`) right after creating the
renderer. Pixel-confirmed fixed for both visuals.

---

### DEC-020 — Focus-steal fix: `SDL_HINT_MAC_BACKGROUND_APP`
**Status:** DECIDIDO · Block 01 (bug fix) — see
`docs/PLATFORM_SPIKE.md` §5.3.

Found by the agent via objective frontmost-app inspection (not visually
reported by the owner): launching the spike made it the
frontmost/active application, even though `SDL_WINDOW_NOT_FOCUSABLE`
already correctly kept the *window* from becoming key —
`SDL_WINDOW_NOT_FOCUSABLE` only affects window-level key status, not
app-level activation, and SDL's Cocoa backend calls `[NSApp
activateIgnoringOtherApps:YES]` on startup by default. Fixed with
`SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP, "1")` before `SDL_Init()` —
an official SDL hint documented for exactly this case. Confirmed fixed
objectively: the frontmost app was unchanged (`Claude`, the owner's
active app) immediately before and immediately after launching the
spike.

---

### DEC-021 — Deadline-driven scheduler replaces the fixed ~12fps render tick
**Status:** DECIDIDO · Block 02 — see `docs/ANIMATION_RUNTIME.md` §6.

Block 01's spike redrew unconditionally on a fixed `core::FrameScheduler`
tick (~12fps) whether or not anything on screen was actually changing —
acceptable for a two-circle placeholder, but not something Block 02
could keep once "static idle that can truly settle" became a
requirement. `content::AnimationController::NextFrameDeadlineMs()`
returns `std::nullopt` whenever the currently playing animation is
`PlaybackKind::kStatic`, which `SpikeApp::Run()` uses directly: the
event loop's `SDL_WaitEventTimeout` bound is computed each wake from
`min(passive-action deadline, animation frame deadline if any,
Windows-fallback hover-poll deadline if applicable)` — with no
animation deadline and the passive deadline ~300s out by default, a
truly idle pet can block for minutes at a stretch. A frame is redrawn,
and the click-through hit-mask rebuilt, only when something that
affects the picture actually happened (a frame advance, a
click/passive trigger, or an `SDL_EVENT_WINDOW_EXPOSED` repaint
request) — never on a fixed cadence. `core::FrameScheduler` itself is
unchanged and still backs the Windows poll-driven click-through
fallback's own (independent, ~60Hz) schedule; it just no longer drives
rendering.

**Measured result:** static-idle CPU dropped from Block 01's Bunny
baseline (≈2% average) to ≈0.0% average (Release, native arm64, same
`ps`-based method) — see `docs/ANIMATION_RUNTIME.md` §6 and
`docs/PERFORMANCE_BUDGETS.md` for the full numbers and methodology.

---

### DEC-022 — "NVPACK1" binary pack format + Python compiler pipeline
**Status:** DECIDIDO · Block 02 — see `docs/ANIMATION_RUNTIME.md` §§4–5.

A data-driven runtime needs *some* on-disk format for
`content::PetDefinition`, and the block brief explicitly required no
runtime PNG/JSON dependency in the C++ binary. Chose a small custom
binary format (magic + length-prefixed strings + fixed-width numeric
fields + raw RGBA8 frame data, little-endian) compiled offline by
`tools/compile_pet_pack.py` from a JSON manifest + source PNGs, and
parsed at runtime by `content::PetPackLoader` — pure in-memory buffer
parsing, no file I/O in the core logic, so `tests/PetPackLoaderTest.cpp`
exercises every failure mode (bad magic, truncation, mismatched frame
dimensions, zero-frame animations, invalid playback-kind bytes, invalid
canvas size) with small hand-built byte buffers and zero filesystem/CWD
dependency — the same lesson Block 01's untested `DevSprite` file-load
path motivated fixing here. `tools/prep_dev_sprite.py`'s existing PNG
decoder is reused (not reimplemented) by the compiler, and a new PNG
*encoder* was added to the same module so
`tools/generate_bunny_dev_pack.py` can materialize its derived frames as
real PNG files and run them through the same compiler path any future
real content would use — no shortcut taken. This format and pipeline
remain development tooling, not the production content pipeline
`docs/PET_CONTENT_SPEC.md` describes.

---

### DEC-023 — Fail-loudly pet loading replaces the analytic-shape fallback; `BlobRenderer`/`DevSprite` retired
**Status:** DECIDIDO · Block 02 — see `docs/ANIMATION_RUNTIME.md` §9.

Block 01's `SpikeApp` fell back to the hardcoded analytic placeholder
(`core::BlobSilhouette` via `graphics::BlobRenderer`) if the Bunny QA
fixture couldn't load. Once the runtime became genuinely data-driven,
keeping that fallback would have reintroduced exactly the kind of
hardcoded-shape special-casing the block's content model exists to
eliminate — and silently swapping in different content on a load
failure is the opposite of "fail loudly," the same principle the asset
pipeline itself already follows (DEC-022). `SpikeApp::Init()` now loads
its one pet pack before creating any window and, on failure, logs a
specific error and exits non-zero — no window, no silent substitute.
`src/graphics/BlobRenderer.{h,cpp}` and `src/graphics/DevSprite.{h,cpp}`
(and the superseded `assets/dev/bunny.rgba` fixture, which nothing
loads anymore) were removed as dead code now that nothing references
them. `core::BlobSilhouette` itself (`src/core/Silhouette.h/.cpp`) is
**kept**: it's a pure, tested geometry utility
(`tests/SilhouetteTest.cpp` still exercises it directly) with zero
knowledge of rendering or SDL — not "pet-specific C++ in the engine,"
just an unused-by-the-app-now, still-valid pure-math type.
`graphics::FrameTexture` (`AttachFrameTexture`/`ReleaseFrameTexture`)
replaces `DevSprite`'s texture-creation role, generalized from one
hardcoded fixture to any `content::FrameDefinition`.

---

### DEC-024 — DEV-only passive-interval override, production default untouched
**Status:** DECIDIDO · Block 02 — see `docs/ANIMATION_RUNTIME.md` §8.

Manual QA of the sparse passive action can't reasonably wait a real
~300 seconds per attempt, but the block brief was explicit that the
production/default interval must not change merely to make QA
convenient. `SpikeApp::ComputeEffectivePassiveIntervalSeconds()` reads
`NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS` once at startup — a valid
positive value overrides *this run's* scheduling only and is logged
alongside the pack's real default for contrast; unset or invalid falls
back to `pet.passiveIntervalSeconds` untouched.
`PetDefinition::passiveIntervalSeconds` (the value compiled into the
pack, 300.0 for the Bunny DEV pack) is never mutated by this mechanism —
there is exactly one place production behavior is defined, and the
override reads around it rather than through it.

---

### DEC-025 — `src/persistence`: a new, SDL-free library for local state
**Status:** DECIDIDO · Block 03 — see `docs/PERSISTENCE.md`.

Block 03 needed a small persistence layer (click balance, active pet
id/variant, last window position) that stays testable without a
display, per AGENTS.md §12 and this project's established pattern
(`src/core`, `src/content` are both pure/SDL-free for exactly this
reason). Rather than fold storage logic into `src/app/SpikeApp` (SDL-
coupled) or into `src/content` (a different concern — content
describes *what a pet looks like*, not *what the user has done*), a
new top-level library was created, mirroring Block 02's precedent of
one focused library per major feature:

- `persistence::AppState` — a plain struct, no SDL, no file I/O.
  Generic by construction: `activePetId`/`activeVariantId` are strings,
  not an enum, so a new pet or variant never requires a schema or code
  change.
- `persistence::AppStateSerializer` — pure (de)serialization to/from
  an in-memory byte buffer, mirroring `content::PetPackLoader`'s own
  separation between parsing and file access.
- `persistence::AppStateStore` — the only piece that touches a
  filesystem; takes a directory path as a constructor argument rather
  than resolving one itself, so tests point it at temp directories and
  the real app points it at `SDL_GetPrefPath()`'s result — the same
  path never appears twice.
- `persistence::PersistenceScheduler` — pure debounce/dirty-flag
  timing, tested with fabricated timestamps exactly like
  `core::FrameScheduler`.

`nimvlets_persistence` links nothing project-specific (not even
`nimvlets_core`) — see `src/persistence/CMakeLists.txt` — it depends
only on the C++ standard library (`<filesystem>`, `<optional>`, ...).

---

### DEC-026 — Two separate click counters; storage location via `SDL_GetPrefPath` + a DEV override
**Status:** DECIDIDO · Block 03 — see `docs/PERSISTENCE.md` §§2, 7.

`clickCount_` (Block 01/02's existing session-only diagnostic, logged
at shutdown) and `AppState::clickBalance` (the new, persisted,
cumulative product currency — AGENTS.md §2) are kept as two distinct
fields rather than repurposing one: they answer different questions
("how many clicks this run" vs. "how many clicks ever, spendable
later") and conflating them would make a future Shop's balance
depend on session-diagnostic logging code.

Storage location: `SDL_GetPrefPath("Leporc Projects", "Nimvlets")` —
SDL's own cross-platform per-user app-data resolver, which already
fully handles the macOS/Windows difference, so no new
`src/platform/*` code was needed (unlike window transparency/click-
through, which SDL can't fully abstract — see `docs/PLATFORM_SPIKE.md`).
`NIMVLETS_DEV_APPDATA_DIR` (checked before `SDL_GetPrefPath()` is ever
called) lets manual QA and this block's own non-interactive smoke
tests redirect persistence to an isolated temp directory — mirroring
Block 02's `NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS` pattern exactly.
This was necessary in practice: an early smoke-test attempt that tried
overriding the `HOME` environment variable instead did **not** redirect
`SDL_GetPrefPath()` on macOS (its Cocoa backend resolves the app-
support directory via Foundation APIs, not the `HOME` env var),
briefly writing a real file under the actual per-user directory before
this override was added — caught immediately, the stray file was
deleted, and no other test in this block touches the real location.

---

### DEC-027 — Debounced write policy with atomic rename-based writes
**Status:** DECIDIDO · Block 03 — see `docs/PERSISTENCE.md` §§4, 6.

Rapid clicks must not become one disk write each. `PersistenceScheduler`
implements a fixed-window debounce (2000ms,
`kDefaultDebounceMs`): the first dirty-marking change arms a single
deadline; further changes before it fires update the in-memory state
but never push the deadline out — this is what makes continuous
activity coalesce into periodic writes instead of either flooding the
disk or starving persistence indefinitely. A failed flush stays dirty
and is retried one debounce interval later, never immediately (bounds
retry frequency under a persistent failure) and never silently
dropped. Clean shutdown flushes unconditionally, ignoring the deadline.

Writes are atomic via the standard write-to-temp-then-rename technique
(`AppStateStore::Save()`): the real `state.nvstate` file is only ever
replaced by one same-directory `std::filesystem::rename()`, never
opened for writing directly, so a write that fails at any earlier step
leaves the previous valid save completely untouched — verified
directly in `tests/AppStateStoreTest.cpp` via a portable failure-
simulation technique (occupying the temp-file path with a directory,
which fails to open as a regular file identically on every platform
this project targets, avoiding fragile `chmod`-based tricks).

---

### DEC-028 — Event-loop shutdown-responsiveness fix: capped maximum wait
**Status:** DECIDIDO · Block 03 (bug fix, found via this block's own
automated testing) — see `docs/PERSISTENCE.md` §9.

This block's "no manual QA" constraint required genuinely reliable
non-interactive smoke tests, including letting the app fully settle
into static idle before sending it `SIGTERM` — a scenario no earlier
block's testing had exercised (every prior CPU/behavior measurement
either sent the signal shortly after launch, while startup-related
events were still trickling in, or ran under a short DEV interval
override that kept the loop waking frequently). That exposed a real,
pre-existing characteristic: a delivered `SIGINT`/`SIGTERM` does not
itself interrupt a blocking `SDL_WaitEventTimeout` on this platform —
the loop only re-checks `ShutdownRequested()` when its own wait
naturally returns. With nothing else scheduled for minutes (the ~300s
passive-action deadline being the only remaining bound), a termination
signal could in principle take that long to be noticed.

Fixed with a hard cap (`kMaxWaitMs = 1000.0`) on the event loop's
computed wait time, applied after every other deadline. A wake that
finds nothing due does zero redraw/hit-mask/disk work before sleeping
again, so this does not reintroduce Block 01's fixed render tick or
regress static-idle CPU — re-measured at ≈0.0% after the fix (see
docs/PERFORMANCE_BUDGETS.md). Shutdown latency after this fix is
bounded to about one second, confirmed by reproducing the exact
previously-hanging scenario (a fully-settled idle run with a matching,
already-synced persisted state) and observing a clean exit shortly
after `SIGTERM`.
