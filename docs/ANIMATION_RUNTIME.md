# Nimvlets — Animation Runtime (Block 02)

This describes the small, data-driven content+animation runtime built in
Block 02 on top of Block 01's platform spike. It replaces Block 01's
single hardcoded visual (the analytic placeholder / the "Bunny" QA
fixture loaded as one static texture) with a general system: **any**
`content::PetDefinition` — idle, click reaction, sparse passive actions,
per-frame alpha hit-testing — with zero pet-specific C++ in the engine.

See also: `docs/PET_CONTENT_SPEC.md` (the longer-term content contract
this partially implements), `docs/DECISION_LOG.md` DEC-021 through
DEC-024 (why these choices were made), `docs/PERFORMANCE_BUDGETS.md`
(the CPU numbers this design produces), and `docs/PLATFORM_SPIKE.md`
(the underlying window/click-through mechanism, unchanged by this
block).

## 1. Scope

Implemented this block:

- A pure, SDL-free content model (`src/content/AnimationDefinition.h`):
  `PlaybackKind` (static / loop / one-shot), `FrameDefinition`,
  `AnimationDefinition`, `PetDefinition`.
- A pure, SDL-free state machine (`content::AnimationController`):
  Idle / ClickReaction / PassiveAction, deadline-driven.
- A binary pack format ("NVPACK1") and loader
  (`content::PetPackLoader`) that parses an in-memory or on-disk pack
  into one `PetDefinition`, failing loudly on any structural problem.
- A Python asset pipeline (`tools/compile_pet_pack.py`) that compiles a
  JSON manifest + a folder of source PNG frames into a `.nvpack` file —
  no runtime PNG/JSON dependency needed in the C++ binary.
- A deterministic "Bunny DEV" animation pack
  (`tools/generate_bunny_dev_pack.py` → `assets/dev/bunny_pack.nvpack`)
  exercising every playback kind and a genuinely silhouette-changing set
  of frames, without depending on finished AI-generated art.
- `src/app/SpikeApp` rewired to load one pack at startup, drive
  `AnimationController` with a true deadline-driven event loop (no fixed
  render tick), and rebuild the click-through hit-mask only when the
  displayed frame actually changes.

Explicitly **not** in this block (see the block brief's NON-SCOPE list):
Shop, Collection, onboarding/starter-selection UI, the 44-second easter
egg, hidden shop, wallet/click persistence, global click mode, audio,
notifications, final content for any of the 8 Nimvlets, UI redesign.

## 2. Content model

```
PetDefinition
  id, displayName, variantGroup
  canvasWidth, canvasHeight       — logical size every frame is drawn into
  alphaHitThreshold                — per-pet, not a global constant (default 128)
  idle: AnimationDefinition        — required, at least 1 frame; the pet's canonical idle
  idleDirectionOverrides: [DirectionalAnimationOverride]  — Block 04.2, zero or more; see below
  clickReaction: AnimationDefinition — required, at least 1 frame
  passiveActions: [AnimationDefinition]  — zero or more
  passiveIntervalSeconds           — target average seconds between passive actions (default 300.0)
  contentVersion                   — schema-only, not read by anything yet

AnimationDefinition
  id, kind (static | loop | one_shot), fps, returnsToIdle
  frames: [FrameDefinition]

FrameDefinition
  width, height, anchor, durationMs, pixels (RGBA8), rendererHandle (opaque)

DirectionalAnimationOverride (Block 04.2 — see docs/NIDIR_CONTENT.md)
  direction (Direction::kRight | kLeft), animation: AnimationDefinition
```

**Dirección (Block 04.2).** `content::Direction` es un enum genérico
(`kRight`/`kLeft`), no un concepto por-pet. `content::ResolveIdleAnimation(pet,
direction)` retorna la entrada dedicada de `idleDirectionOverrides` si
existe una para `direction`, si no cae a `pet.idle` — un pet sin
ninguna entrada (Bunny) siempre resuelve a `pet.idle` sin importar la
dirección pedida. `content::AnimationController::SetDirection()`
consulta esto mismo. Ver `docs/NIDIR_CONTENT.md` §5 para el diseño
completo y por qué es una extensión aditiva del modelo anterior, no un
reemplazo de `idle`.

No pet identity ever appears as a C++ enum value or `if (petId ==
"bunny_dev")` branch anywhere in `src/content` or `src/app`. Swapping
which `.nvpack` file `SpikeApp::kPetPackPath` points at swaps the pet
with zero other code changes — the Bunny DEV pack is what happens to be
loaded in this block, not a special case the runtime knows about.

## 3. Animation Player / State Machine

`content::AnimationController` (pure, unit-tested in
`tests/AnimationControllerTest.cpp` with fabricated timestamps, no real
clock/sleep — the same testing pattern `core::FrameScheduler` used in
Block 01):

- **Idle** plays `pet.idle`. If it's `PlaybackKind::kStatic` (the Bunny
  DEV pack's idle is exactly one frame),
  `NextFrameDeadlineMs()` returns `std::nullopt` forever — there is
  never a reason to wake the event loop just to re-check a static idle.
  This is the mechanism behind §6's CPU result.
- **`TriggerClick(nowMs)`** starts `pet.clickReaction` from frame 0,
  *unless* a click reaction is already playing, in which case it's a
  no-op for animation state — the visual never restarts mid-reaction.
  **Click counting is entirely separate from this and never
  conditional on it**: `SpikeApp::HandleEvent()` increments
  `clickCount_` unconditionally on every valid click gesture, then
  calls `TriggerClick()` regardless of what it does to animation state.
  Repeated clicks during an active reaction all count; only the visual
  coalesces. See `tests/ClickAccountingTest.cpp`.
- **`TriggerClick()` interrupts an in-progress passive action**
  immediately — click reaction outranks passive action.
- **`TriggerPassiveAction(index, nowMs)`** only has an effect when
  currently Idle; arriving while a click reaction (or another passive
  action) is playing is silently ignored. Passive action never
  interrupts anything.
- A finished one-shot animation with `returnsToIdle == true` transitions
  back to Idle the moment `Advance()` observes it; `Advance()` loops
  internally so a caller that was asleep long enough to cross more than
  one frame boundary (or finish a whole one-shot) catches all the way up
  in one call, never needing to be called once per elapsed frame.

## 4. Pack format ("NVPACK1")

Producer: `tools/compile_pet_pack.py`. Consumer:
`content::LoadPetPackFromMemory` / `LoadPetPackFromFile`
(`src/content/PetPackLoader.cpp`). All integers/floats little-endian
(every platform this project targets — x86_64, arm64 — is little-endian;
no byte-swapping is implemented, the same assumption Block 01's
`DevSprite` raw format made).

```
magic                    : 8 bytes, "NVPACK1\0"
petId                    : string   (uint32 byte-length + UTF-8 bytes)
displayName               : string
variantGroup              : string
canvasWidth, canvasHeight : uint32, uint32
alphaHitThreshold          : uint8
passiveIntervalSeconds     : float64
contentVersion             : string
idle                       : AnimationBlock
clickReaction              : AnimationBlock
passiveActionCount         : uint32
passiveActions[count]      : AnimationBlock, repeated

directionalIdleOverrideCount : uint32   -- Block 04.2, OPTIONAL trailing section
directionalIdleOverrides[count]:         -- absent entirely in a pre-04.2 pack (e.g.
  direction : uint8 (0=right, 1=left)    -- assets/dev/bunny_pack.nvpack, never recompiled
  animation : AnimationBlock             -- for this) — see docs/NIDIR_CONTENT.md §5

AnimationBlock:
  id            : string
  kind          : uint8   (0=static, 1=loop, 2=one_shot)
  fps           : float64
  returnsToIdle : uint8   (0/1)
  frameCount    : uint32
  frames[count] : FrameBlock, repeated

FrameBlock:
  width, height : uint32, uint32
  anchorX, anchorY : float64, float64
  durationMs       : float64
  pixels           : width * height * 4 raw RGBA8 bytes (row-major, top-to-bottom, straight alpha)
```

**Fails loudly, never silently invents/clamps/truncates data.** Both the
compiler and the loader reject, with a specific message naming the
animation/frame at fault:

- a bad magic value or truncated buffer (loader),
- a referenced source PNG that doesn't exist (compiler),
- any frame whose dimensions differ from that animation's first frame,
- an animation with zero frames,
- an invalid playback-kind byte (loader) / string (compiler),
- a non-positive canvas size,
- an empty pet id (loader) / missing required manifest field (compiler).

`content::LoadPetPackFromMemory` is pure in-memory parsing — no file
I/O — specifically so `tests/PetPackLoaderTest.cpp` can exercise all of
the above with small hand-built byte buffers and zero filesystem/CWD
dependency (a deliberate improvement over Block 01's `DevSprite`, which
was never tested at the file-loading layer for exactly that reason).

## 5. Asset pipeline

```
source PNG(s) ──▶ manifest.json ──▶ tools/compile_pet_pack.py ──▶ *.nvpack
```

`tools/prep_dev_sprite.py` supplies the shared, dependency-free PNG
codec (`read_png_rgba` / `write_png_rgba`, struct + zlib only, no
third-party library) that both `compile_pet_pack.py` and
`generate_bunny_dev_pack.py` reuse — per AGENTS.md §10, "reuse, don't
rewrite working PNG tooling," this is the same decoder Block 01 wrote
for `bunny_source.png`, now also driving a real PNG *encoder* path for
the first time.

`tools/generate_bunny_dev_pack.py` is a deterministic *dev-only*
generator, not a stand-in for a real content pipeline: it derives 7
frames from the existing Block 01 `bunny_source.png` fixture via plain
nearest-neighbor pixel transforms (squash/stretch scale, horizontal
shear/lean) — no rotation/interpolation library, no new artistic
dependency — specifically so this block does not depend on finished
AI-generated art to exercise the runtime. Re-running it against an
unchanged `bunny_source.png` produces byte-identical output. See that
tool's docstring for the exact frame list and manifest.

`tools/compile_pet_pack.py` compiles *any* `content::PetDefinition`-
shaped manifest, not just Bunny's — but the manifest format and this
tool remain development tooling, not the production content pipeline
`docs/PET_CONTENT_SPEC.md` describes.

## 5.1 Where master art lives

`assets/source/nimvlets/README.md` documents the target contract for
real Nimvlet master art and its exported animation sequences — one
directory per pet, `master.png` + `animations/<name>/<direction>/`
(individual PNG frames as the canonical source, a spritesheet as a
secondary artifact), plus a small per-pet `provenance.json` schema.
Block 04.2 populated the first real entry (`nidir/`) and refined this
contract with real data — see `docs/NIDIR_CONTENT.md` for the full
pipeline (import, normalization, deterministic left-direction mirror,
pack compilation). `click` and `passive` remain open-ended categories,
not a hard limit — passive actions are already a data-driven list
today.

## 6. Scheduler behavior — the CPU result

Block 01's spike ran a fixed ~12 fps (`1000/12` ms) render tick
unconditionally, whether or not anything on screen was actually
changing. Block 02's event loop (`SpikeApp::Run()`) instead computes,
every wake, the *actual* next deadline:

```
waitMs = min(
    nextPassiveDeadlineMs - now,                 // always present, ~300s out by default
    animController.NextFrameDeadlineMs() - now,   // absent (nullopt) while idle is static
    hoverPollDeadline - now,                      // Windows fallback only; absent on macOS
)
```

and blocks in `SDL_WaitEventTimeout()` for exactly that long — no
fallback fixed tick exists anymore. A real input event (including the
mouse-moved events SDL's own Cocoa backend needs to keep click-through
correct — see `docs/PLATFORM_SPIKE.md` §5.1) wakes the wait immediately
regardless of how long the timeout is; the timeout only bounds how long
the process blocks when nothing happens. A frame is redrawn — and the
click-through hit-mask rebuilt (`ApplyCurrentHitMask()`) — only when
`needsRedraw_` is set: a frame-advance, a `TriggerClick`/
`TriggerPassiveAction` call, or an `SDL_EVENT_WINDOW_EXPOSED` (the OS
asking for a repaint, e.g. after being uncovered) actually happened.
Static idle therefore renders **nothing** for as long as it stays idle.

Measured (Release, native arm64, `ps -o rss,%cpu,time`, 3-second
intervals over a 27–30s window, no `sudo` — same method Block 01 used):

| Scenario | CPU (average, steady state) | RSS |
|---|---|---|
| Block 01, Bunny fixture, fixed ~12fps tick (baseline) | ≈2% | ≈72 MB |
| Block 02, static idle (production default, no interaction) | **≈0.0%** | ≈73 MB |
| Block 02, passive action forced every ~2s (`NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS=2`, an artificial stress scenario ~150x more frequent than production) | <1% (peak 1.0%, mostly 0.0–0.5%) | ≈74 MB |

Static idle CPU is materially better than Block 01's baseline, and even
the artificial worst-case stress scenario — passive actions firing two
orders of magnitude more often than the real ~300s default — stays
comfortably under `docs/PERFORMANCE_BUDGETS.md`'s 1% target. See that
document for the full budget table and methodology rules.

## 7. Per-frame alpha hit-testing

`core::AlphaMask::FromAlphaChannel(rgba, srcW, srcH, targetW, targetH,
threshold)` (new this block) builds a hit-test grid from a frame's real
RGBA8 pixel data by nearest-neighbor sampling each target cell back to
its source pixel and comparing alpha against `threshold` (inclusive:
`alpha >= threshold`). This is the **one** place any hit-mask is
derived from pixel data — both the macOS `SDL_SetWindowShape` path and
the Windows poll-driven fallback read `SpikeApp::activeHitMask_`, which
`ApplyCurrentHitMask()` rebuilds from this same function every time the
displayed frame changes — so rendering and click-through can never
disagree, for any frame of any pet.

`PetDefinition::alphaHitThreshold` is per-pet data (default 128, the
standard antialiased-edge midpoint — see DEC-018's histogram analysis
for the Bunny asset specifically), not a global constant, so a future
pet with different edge/antialiasing characteristics can tune it without
an engine change.

On macOS, `SDL_SetWindowShape()` is called again only inside
`ApplyCurrentHitMask()` — i.e. only on an actual frame change — never
on a fixed schedule; no hover polling is introduced on macOS by this
block (matching Block 01's finding that SDL's own Cocoa backend
re-evaluates hit-testing against the shape on every real mouse-moved
event with zero polling from us). The Windows fallback
(`PollHover()`/`UpdateClickThrough()`) is unchanged in mechanism from
Block 01 — still a bounded ~60Hz `SDL_WaitEventTimeout` wakeup, never a
busy-wait — and simply reads whatever `activeHitMask_` currently holds.

## 8. DEV passive-interval override

Manual QA cannot reasonably wait a real ~300 seconds to see a passive
action fire. `SpikeApp::ComputeEffectivePassiveIntervalSeconds()` reads
the environment variable `NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS` once at
startup: if it parses as a valid positive number, that value is used as
*this run's* passive interval and logged clearly (including the real
default, for contrast); an unset or invalid value silently falls back to
`pet.passiveIntervalSeconds` (the pack's authored default, 300.0 for the
Bunny DEV pack). **The pack's own default is never mutated** — this is
purely an opt-in override for one run's scheduling, not a change to
production behavior:

```bash
NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS=5 ./build/macos-debug/src/app/nimvlets_spike
```

## 9. Fail-loudly content loading

`SpikeApp::Init()` loads `assets/dev/bunny_pack.nvpack` before creating
any window. If it can't be loaded (missing file, wrong working
directory, corrupt pack), `Init()` logs a specific fatal error and the
process exits with a non-zero status — there is no hardcoded
analytic-shape fallback anymore (contrast Block 01, which fell back to
`core::BlobSilhouette` if `bunny.rgba` failed to load). A data-driven
runtime with no content to show is a real problem to surface, not paper
over — consistent with the asset pipeline's own "fail loudly" contract
(§4 above) applied at the app level too. See DEC-023.

## 10. Running

```bash
# from the repository root — the pack path resolves from CWD
./build/macos-debug/src/app/nimvlets_spike

# QA convenience: passive action every ~5s instead of ~300s
NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS=5 ./build/macos-debug/src/app/nimvlets_spike

# regenerate the Bunny DEV pack after changing assets/dev/bunny_source.png
python3 tools/generate_bunny_dev_pack.py
```
