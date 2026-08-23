# Nimvlets — Animation Runtime (Block 02, generalizado en Block 05)

This describes the data-driven content+animation+behavior runtime.
Block 02 built the original system: **any** `content::PetDefinition` —
idle, click reaction, sparse passive actions, per-frame alpha
hit-testing — with zero pet-specific C++ in the engine. **Block 05
generalizes the model** from a fixed idle/click/passive shape to a
**named-state behavior graph** (`content::BehaviorState`), so a pet
with real posture transitions (Frin: seated/lying — see
`docs/FRIN_CONTENT.md`) is expressible with the exact same mechanism a
single-state pet (Bunny, Nidir) already used — see §2/§3 below for the
new model and `docs/DECISION_LOG.md` for why the binary format bumped
from "NVPACK1" to "NVPACK2" rather than extending the old one
additively.

See also: `docs/PET_CONTENT_SPEC.md` (the longer-term content contract
this partially implements), `docs/DECISION_LOG.md` (why these choices
were made), `docs/PERFORMANCE_BUDGETS.md` (the CPU numbers this design
produces), and `docs/PLATFORM_SPIKE.md` (the underlying window/click-
through mechanism, unchanged by this block).

## 1. Scope (Block 02, still current)

- A pure, SDL-free content model (`src/content/AnimationDefinition.h`):
  `PlaybackKind` (static / loop / one-shot), `FrameDefinition`,
  `AnimationDefinition`, `PetDefinition`.
- A pure, SDL-free state machine (`content::AnimationController`):
  deadline-driven, generic over any behavior graph.
- A binary pack format ("NVPACK2") and loader
  (`content::PetPackLoader`) that parses an in-memory or on-disk pack
  into one `PetDefinition`, failing loudly on any structural problem.
- A Python asset pipeline (`tools/compile_pet_pack.py`) that compiles a
  JSON manifest + a folder of source PNG frames into a `.nvpack` file —
  no runtime PNG/JSON dependency needed in the C++ binary.
- `src/app/SpikeApp` loads one pack (resolved via the catalog — see
  `docs/CATALOG.md`), drives `AnimationController` with a true
  deadline-driven event loop (no fixed render tick), and rebuilds the
  click-through hit-mask only when the displayed frame actually
  changes.

Explicitly **not** in this project yet: Shop, Collection, onboarding/
starter-selection UI, the 44-second easter egg, hidden shop, global
click mode, audio, notifications.

## 2. Content model (Block 05 — named-state behavior graph)

```
PetDefinition
  id, displayName, variantGroup
  canvasWidth, canvasHeight       — logical size (points) every frame is drawn into;
                                     independent of source art's native resolution
  visualScale                     — per-pet display-size multiplier (Block 05, default
                                     1.0). Applied ONLY at render/window/hit-mask time
                                     (SpikeApp::EffectiveCanvasWidth()/Height() = round
                                     (canvasWidth/Height * visualScale)) — never touches
                                     source art or compiled pixel bytes. All animations of
                                     the same pet share exactly this one scale; there is no
                                     per-animation override. See §11.
  alphaHitThreshold                — per-pet, not a global constant (default 128)
  states: [BehaviorState]          — the behavior graph. states[0] is the startup/default
                                     state. Every WeightedAction::targetStateId must name a
                                     real state id (validated at load — see §4).
  contentVersion                   — schema-only, not read by anything yet

BehaviorState
  id                                — stable, e.g. "default" (a normal pet) or "seated"/
                                       "lying" (Frin)
  baseAnimation                     — the pose shown at rest in this state (typically
                                       PlaybackKind::kStatic — see §3)
  baseAnimationDirectionOverrides   — [DirectionalAnimationOverride], zero or more
  ambientIntervalSeconds            — target average seconds between ambient triggers
                                       *while in this state*; meaningless if ambientActions
                                       is empty (no timer is ever armed for a state with no
                                       ambient actions — e.g. Frin "lying")
  ambientActions: [WeightedAction]  — timer-driven, weighted; zero or more
  hoverUsesAmbientActions           — bool, default true: a hover trigger picks from THIS
                                       state's ambientActions (same pool, no duplicated
                                       frame data) — "hover uses the same available
                                       passive-action pool unless content says otherwise".
                                       If false, hoverActions is used instead (possibly
                                       empty — no hover behavior for this state, Frin's
                                       case today). Setting both true AND a non-empty
                                       hoverActions is rejected at load (ambiguous).
  hoverActions: [WeightedAction]    — pointer-entry-driven, weighted; zero or more
  clickActions: [WeightedAction]    — click-driven, weighted; zero or more

WeightedAction
  id, weight, targetStateId         — which BehaviorState to enter once this one-shot
                                       finishes; the SAME id as the state it's authored
                                       under is a self-loop (the normal case for a
                                       single-state pet's click/ambient — Bunny/Nidir's
                                       click reaction and passive actions are exactly this)
  animation: AnimationDefinition
  directionOverrides: [DirectionalAnimationOverride]

AnimationDefinition
  id, kind (static | loop | one_shot), fps, returnsToIdle
  frames: [FrameDefinition]

FrameDefinition
  width, height, anchor, durationMs, pixels (RGBA8)

DirectionalAnimationOverride
  direction (Direction::kRight | kLeft), animation: AnimationDefinition
```

**Dirección.** `content::Direction` is a generic enum (`kRight`/
`kLeft`), not a per-pet concept. `content::ResolveAnimation(canonical,
overrides, direction)` — one generic function, used both for a
`BehaviorState::baseAnimation` and for any `WeightedAction::animation`
— returns the dedicated override entry for `direction` if one exists,
else falls back to the canonical animation; a pet/state/action with no
directional art (Bunny's original shape) always resolves to the
canonical entry regardless of direction. This replaces Block 04.2's
three separate `ResolveIdleAnimation`/`ResolveClickReaction`/
`ResolvePassiveAction` functions with one generic one, now that "idle"/
"click"/"passive" are no longer distinct field names but instances of
the same `WeightedAction`/`baseAnimation` shape.

**A normal pet reduces to exactly this shape:** one `BehaviorState`
("default"), `clickActions` with one entry (self-loop, weight
irrelevant), `ambientActions` with the pet's weighted passive-action
list (self-loop), `hoverUsesAmbientActions = true`. This is precisely
Bunny/Nidir's manifest today — see `tools/generate_bunny_pack.py`/
`generate_nidir_pack.py`. A **stateful pet** (Frin) has two or more
`BehaviorState`s with real cross-state transitions — see
`docs/FRIN_CONTENT.md`.

**Selección ponderada — política 70/30.**
`content::ChooseWeightedActionIndex(actions, uniformRandom01)`
(`src/content/AnimationDefinition.h/.cpp`) is generic over ANY
`std::vector<WeightedAction>` (ambient, hover, or click — not just
"passive actions" as in Block 04.3) — pure and deterministic: the
CALLER supplies the random value in `[0,1)` (in production,
`SpikeApp::NextUniformRandom01()`, a `std::mt19937` seeded once in
`Init()`; in tests, a fixed value), never generates randomness itself.
If a trigger's action list is empty, the caller-facing `Trigger*()`
methods on `AnimationController` are simply no-ops. See
`tests/AnimationControllerTest.cpp` (`WeightedSelection*`) for the
exact boundary coverage (0.7 is the cutoff for Bunny/Nidir's ambient
70/30, and the same mechanism is what Frin's seated click 70/30
howl/tail-greet uses).

No pet identity ever appears as a C++ enum value or `if (petId ==
"...")` branch anywhere in `src/content` or `src/app`. Swapping which
`.nvpack` file the catalog resolves to swaps the pet with zero other
code changes.

## 3. Animation Player / State Machine

`content::AnimationController` (pure, unit-tested in
`tests/AnimationControllerTest.cpp` and `tests/StatefulBehaviorTest.cpp`
with fabricated timestamps, no real clock/sleep):

- Tracks a current `BehaviorState` index plus a `ControllerMode`:
  `kBase` (showing that state's `baseAnimation` — replaces Block 02-
  04.3's `kIdle`), `kAmbientOrHoverAction` (replaces `kPassiveAction`),
  or `kClickAction` (replaces `kClickReaction`).
- **`kBase`** plays `ResolveAnimation(state.baseAnimation, ...,
  direction)`. If it's `PlaybackKind::kStatic` (every pet's base pose
  today), `NextFrameDeadlineMs()` returns `std::nullopt` forever —
  there is never a reason to wake the event loop just to re-check a
  static base. This is the mechanism behind §6's CPU result.
- **`TriggerClick(uniformRandom01, nowMs)`** picks a weighted entry
  from the active state's `clickActions` and starts it from frame 0,
  *unless* a click action is already playing (coalesce, no-op for
  animation state — the visual never restarts mid-reaction; the caller
  still counts the click unconditionally — see
  `tests/ClickAccountingTest.cpp`). **It interrupts an in-progress
  ambient/hover action immediately** (click outranks ambient/hover).
- **`TriggerAmbientAction`/`TriggerHoverAction(uniformRandom01, nowMs)`**
  only have an effect when currently `kBase`; arriving while a click
  action (or another ambient/hover action) is playing is silently
  ignored — neither ever interrupts anything. `TriggerHoverAction`
  consults `content::EffectiveHoverActions(state)` (`state.hoverActions`
  if the state defines its own, else `state.ambientActions` when
  `hoverUsesAmbientActions`).
- A finished one-shot action (`animation.returnsToIdle == true`)
  transitions the controller to the `BehaviorState` named by its
  `targetStateId` — the SAME state (a self-loop, e.g. any Bunny/Nidir
  action, or Frin's `howl`/`tail_greet` while `seated`) or a DIFFERENT
  one (a real transition, e.g. Frin's `sit_to_lie: seated -> lying`).
  `Advance()` loops internally so a caller that was asleep long enough
  to cross more than one frame boundary (or finish a whole one-shot)
  catches all the way up in one call.
- **Dirección + estado.** `SetDirection()` is generic over whichever
  `BehaviorState` is active — never assumes "the" state of a pet. If
  `kBase`, the frame updates immediately (frame 0 of the new
  direction's `baseAnimation`); if a one-shot is playing (any mode,
  any state), the new direction is recorded but applied only when the
  controller next transitions to a `kBase` state — coherent even when
  that transition also changes WHICH state becomes active (Frin mid-
  `sit_to_lie`: a direction change is deferred and applied against
  `lying`'s base pose once the transition completes, never against an
  unrelated pose) — see `tests/StatefulBehaviorTest.cpp`,
  `DirectionChangeDuringTransitionResolvesCoherentlyAfterCompletion`.

## 4. Pack format ("NVPACK2")

Producer: `tools/compile_pet_pack.py`. Consumer:
`content::LoadPetPackFromMemory` / `LoadPetPackFromFile`
(`src/content/PetPackLoader.cpp`). All integers/floats little-endian.

Block 05 bumps the magic from `"NVPACK1\0"` to `"NVPACK2\0"` — a
genuinely different shape (a named-state graph, not a flat idle/click/
passive record), not an additive extension of the old format, so
reinterpreting old bytes under the same magic would have been
dishonest. Every repo-shipped pack is regenerated from source by its
own `generate_<pet>_pack.py`, so there is no external/shipped content
that needed NVPACK1 read-compatibility.

```
magic                    : 8 bytes, "NVPACK2\0"
petId                    : string   (uint32 byte-length + UTF-8 bytes)
displayName               : string
variantGroup              : string
canvasWidth, canvasHeight : uint32, uint32
alphaHitThreshold          : uint8
visualScale                : float64
contentVersion             : string
stateCount                 : uint32
states[stateCount]:
  id                                    : string
  baseAnimation                         : AnimationBlock
  baseAnimationDirectionOverrideCount   : uint32
  baseAnimationDirectionOverrides[count]: { direction: uint8, animation: AnimationBlock }
  ambientIntervalSeconds                : float64
  ambientActionCount                    : uint32
  ambientActions[count]                 : WeightedActionBlock
  hoverUsesAmbientActions                : uint8 (0/1)
  hoverActionCount                       : uint32
  hoverActions[count]                    : WeightedActionBlock
  clickActionCount                       : uint32
  clickActions[count]                    : WeightedActionBlock

WeightedActionBlock:
  id                    : string
  weight                : float64
  targetStateId         : string
  animation             : AnimationBlock
  directionOverrideCount: uint32
  directionOverrides[count]: { direction: uint8, animation: AnimationBlock }

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

Unlike NVPACK1's optional trailing sections (added incrementally
across Block 04.2/04.3, gated by `ByteReader::HasMoreData()` to stay
backward-compatible), NVPACK2's every count is **always written**
(never conditionally omitted) — a cleaner, simpler shape now that the
format isn't trying to stay byte-compatible with an older one.

**Fails loudly, never silently invents/clamps/truncates data.** Both
the compiler and the loader reject, with a specific message naming the
state/animation/frame at fault:

- a bad magic value or truncated buffer (loader),
- a referenced source PNG that doesn't exist (compiler),
- any frame whose dimensions differ from that animation's first frame,
- an animation with zero frames,
- an invalid playback-kind byte (loader) / string (compiler),
- a non-positive canvas size or `visualScale`,
- an empty pet id, or zero `states`,
- a `WeightedAction::targetStateId` that doesn't match any state id
  (validated once, after the whole pack is parsed — see
  `PetPackLoader.cpp`'s `ValidateTargetStateIds()` — so
  `AnimationController::FindStateIndex()` can never fail at runtime),
- `hoverUsesAmbientActions == true` together with a non-empty
  `hoverActions` (ambiguous — the loader and the compiler both reject
  this).

`content::LoadPetPackFromMemory` is pure in-memory parsing — no file
I/O — specifically so `tests/PetPackLoaderTest.cpp` can exercise all of
the above with small hand-built byte buffers and zero filesystem/CWD
dependency.

## 5. Asset pipeline

```
source PNG(s) ──▶ manifest.json ──▶ tools/compile_pet_pack.py ──▶ *.nvpack
```

`tools/prep_dev_sprite.py` supplies the shared, dependency-free PNG
codec (`read_png_rgba`/`write_png_rgba`), the deterministic horizontal
mirror (`mirror_rgba_horizontal`), the content-anchored shared-canvas
normalization (`compute_content_bbox`/`compose_on_canvas`/
`compute_frame_normalization_plan`), and both resize functions
(`resize_rgba_nearest` for upscales, `resize_rgba_area_average` — a
box filter — for real downscales) that `tools/compile_pet_pack.py`
reuses. The manifest shape `compile_pet_pack.py` walks changed in
Block 05's first pass (§2/§4 above): every compilable animation now
lives under `states[i].base_animation` or one of
`states[i].{ambient,hover,click}_actions[j]`, instead of the old flat
`idle`/`click_reaction`/`passive_actions` keys. Block 05's THIRD
corrective pass (see DEC-075 in `docs/DECISION_LOG.md`) additionally
redesigned `compute_frame_normalization_plan()` itself: it no longer
measures every group's scale against one pet-wide bounding box
(invalid across `BehaviorState`s of genuinely different silhouette
orientation, e.g. Frin "seated" vs. "lying") — it now unifies groups
that share a real source frame file via a union-find (so a state's
`base_animation` that reuses a transition's own first/last frame
inherits that transition's scale BY CONSTRUCTION, never by a fresh
pixel comparison) and, for anything not so linked, compares against
its OWN authoring state's `base_animation` instead of the pet's global
reference. `tools/compile_pet_pack.py`'s `_compile_frame()` also now
combines the content-normalization resize and the
`runtime_max_frame_dimension` downscale into a single resize pass
(less cumulative detail loss than two chained box-filters for the same
net ratio), and fails loudly (`PackCompileError`) instead of silently
cropping if any frame's real content — not just its frame 0 — would
exceed the shared working canvas.

`tools/generate_bunny_pack.py`/`generate_nidir_pack.py` build a
single-state ("default") manifest from real imported PNG frames plus
their deterministically-mirrored opposite direction. `tools/
generate_frin_pack.py` (Block 05) is the first multi-state generator —
see `docs/FRIN_CONTENT.md` for Frin's own import/behavior details, and
that script's own docstring for how a future stateful pet (Artu) would
follow the same pattern.

## 5.1 Where master art lives

`assets/source/nimvlets/README.md` documents the target contract for
real Nimvlet master art: one directory per pet (or per variant, for
Frin), `master.png` + `animations/<name>/<direction>/` (individual PNG
frames as the canonical source, a spritesheet as a secondary
artifact), `DESCRIPTION.txt` for stable physical traits, plus (Block
05) `pack_manifest.json`'s `states: [...]` shape for anything beyond a
single self-looping state.

## 6. Scheduler behavior — the CPU result

`SpikeApp::Run()` computes, every wake, the *actual* next deadline:

```
waitMs = min(
    ambientDeadlineMs_ - now,                     // std::optional (Block 05) — absent entirely
                                                    // for a state with no ambientActions (e.g. Frin
                                                    // "lying"), present otherwise (~12s Bunny/Nidir/
                                                    // Frin "seated" -- unificado, ver DEC-084)
    animController.NextFrameDeadlineMs() - now,    // absent (nullopt) while the base pose is static
    confirmRedrawDeadlineMs_ - now,                // absent unless an animation transition just armed it (§8.2)
    persistenceScheduler.NextFlushDeadlineMs() - now,  // absent unless something is actually dirty
    hoverPollDeadline - now,                       // Windows fallback only; absent on macOS
)
```

and blocks in `SDL_WaitEventTimeout()` for exactly that long — no
fixed tick exists. A real input event wakes the wait immediately
regardless of timeout length; the timeout only bounds how long the
process blocks when nothing happens. A frame is redrawn — and the
click-through hit-mask rebuilt — only when `needsRedraw_` is set: a
frame-advance, any `Trigger*`/direction/pet-switch call, or an
`SDL_EVENT_WINDOW_EXPOSED`. Static base state therefore renders
**nothing** for as long as it stays there. See `docs/PERFORMANCE_BUDGETS.md`
for measured numbers.

## 7. Per-frame alpha hit-testing

`core::AlphaMask::FromAlphaChannel(rgba, srcW, srcH, targetW, targetH,
threshold)` builds a hit-test grid from a frame's real RGBA8 pixel data
by nearest-neighbor sampling. This is the **one** place any hit-mask is
derived from pixel data — both the macOS `SDL_SetWindowShape` path and
the Windows poll-driven fallback read `SpikeApp::activeHitMask_`, which
`ApplyCurrentHitMask()` rebuilds every time the displayed frame
changes, at `SpikeApp::EffectiveCanvasWidth()/Height()` (Block 05 —
`canvasWidth/Height * visualScale`, so a pet's hit region always
matches what's actually drawn on screen regardless of its display-size
multiplier) — so rendering, hit-testing, and window size can never
disagree, for any frame of any pet at any scale.

`PetDefinition::alphaHitThreshold` is per-pet data (default 128), not a
global constant.

## 8. DEV overrides

Manual QA cannot reasonably wait a real ~12s (per-state ambient
interval) or navigate a real product UI (which doesn't exist yet) to
see a specific pet. Two independent, opt-in, env-var-gated DEV
mechanisms:

```bash
# Shorten every state's ambient interval for this run (production per-
# state default is never mutated)
NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS=5 ./build/macos-debug/src/app/nimvlets_spike

# Launch a specific catalog entry without touching persisted state
# (Block 05 — see the "Owner manual QA" section of README.md)
NIMVLETS_DEV_SELECT_PET=frin/male ./build/macos-debug/src/app/nimvlets_spike
```

`SpikeApp::ComputeEffectiveAmbientIntervalSeconds(state)` reads
`NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS` once at startup: if it parses
as a valid positive number, that value replaces *this run's* ambient
interval for EVERY state that has one — the pack's own authored
per-state default is never mutated.

## 8.1 Política de hover — dwell continuo de 0.5s (Block 04.3 → Block 05, cuatro correcciones)

Historial honesto (ninguna pasada se reescribe, cada una superó a la
anterior con un requisito más preciso del owner):
- Block 04.3: hover comparte deadline con el timer ambient (edge +
  cooldown compartido).
- Block 05, primera corrección: cooldown propio de 2s, independiente
  del timer ambient (todavía disparaba EN EL INSTANTE en que el cursor
  entraba).
- Block 05, segunda corrección: dwell continuo real de 5 segundos — el
  cursor debe permanecer **continuamente** sobre la región interactiva
  antes de disparar; si sale antes, el contador se reinicia por
  completo (sin crédito parcial). Un click, un drag, o un cambio real
  de `BehaviorState` TAMBIÉN reinician el contador, incluso si el
  cursor nunca salió físicamente de la región.
- Block 05, tercera corrección: 5s -> 1s ("current 5-second dwell is
  too long") — el MECANISMO no cambia en absoluto, solo
  `SpikeApp::kHoverDwellSeconds` (un único valor). Esa misma pasada
  agrega, además, que el timer ambient se reinicie en cualquier
  interacción real (ver el final de esta sección y §11.1).
- Block 05, **cuarta corrección (estado actual, pasada de resolución
  de renderer)**: 1s -> **0.5s** — pedido de producto explícito (ver
  DEC-084 en `docs/DECISION_LOG.md`), sin relación con el diagnóstico
  de renderer de esta misma pasada. De nuevo, solo el valor cambia.

**Mecanismo: `core::HoverDwellTracker`** (`src/core/HoverDwellTracker.h`,
reemplaza a la vieja `core::HoverPassiveGate`) — pura, sin SDL, testeada
con timestamps fabricados en `tests/HoverDwellTrackerTest.cpp`.
`Update(isOverOpaque, nowMs)` retorna `true` exactamente una vez por
episodio de dwell continuo, en la muestra donde `nowMs -
dwellStartMs_ >= kHoverDwellSeconds*1000` se cumple por primera vez —
nunca antes, nunca de nuevo mientras el cursor sigue quieto encima
después de disparar. Cualquier muestra con `isOverOpaque == false`
reinicia el tracker (`dwellStartMs_` vuelve a `nullopt`). `Reset()`
hace lo mismo explícitamente, sin necesitar una muestra "afuera" — el
mecanismo que usan los resets por click/drag/cambio-de-estado.

**El problema del cursor perfectamente quieto — y por qué hace falta
un deadline propio en el loop principal.** En el camino nativo (macOS/
Linux-X11), hover solo se alimentaba históricamente de eventos
`SDL_EVENT_MOUSE_MOTION` reales — pero si el cursor se queda
PERFECTAMENTE quieto sobre el pet (exactamente el caso que "dwell"
debe detectar), ningún evento de motion nuevo llega para volver a
chequear si ya pasó el umbral. `SpikeApp::hoverDwellDeadlineMs_` (un
`std::optional<double>`, el mismo patrón que
`ambientDeadlineMs_`/`confirmRedrawDeadlineMs_`) se arma en cuanto un
dwell arranca y se agrega al cálculo de `waitMs` del loop principal
(`SpikeApp::Run()`) — así el loop despierta exactamente cuando ese
dwell cruzaría el umbral, sin importar si el mouse se movió o no
durante ese tiempo. Al despertar por ese deadline, el loop vuelve a
MUESTREAR la posición real del cursor (`SampleCursor()`, la misma
función que ya usa el fallback poll-driven de Windows) en vez de
asumir que el cursor sigue encima — así el mecanismo es correcto sin
importar la plataforma o si llegaron eventos de motion intermedios.

**Reset por click/drag/cambio-de-estado (pedido explícito del owner).**
`SpikeApp::ResetHoverDwell()` es el único punto que estos tres casos
usan: (1) `SDL_EVENT_MOUSE_BUTTON_DOWN` sobre la región interactiva
(el inicio de CUALQUIER gesto, sea click o drag, antes incluso de
saber cuál de los dos terminará siendo), y (2) el loop principal, cada
vez que detecta que `AnimationController::CurrentStateId()` cambió de
verdad (una transición real de `BehaviorState`, p. ej. Frin
seated → lying — nunca un self-loop ordinario como un click en Bunny,
que deja el mismo estado activo y por lo tanto NO reinicia ningún
dwell en curso).

**Ambos relojes ("deadline") son independientes -- ninguno consulta el
del otro** — `ambientDeadlineMs_` y `hoverDwellDeadlineMs_` corren cada
uno con su propia matemática, requisito que se mantiene sin cambios
desde la primera corrección de este bloque. Pero (tercera corrección,
ver DEC-078 en `docs/DECISION_LOG.md`) el timer ambient **sí se
reinicia** cuando una interacción de hover ocurre: el cursor entrando
a la región interactiva (un dwell nuevo arrancando, incluso si nunca
llega a disparar) y un disparo de hover completo AMBOS llaman a
`RearmAmbientDeadline()`. Esto no es un acoplamiento de relojes — es
que "el owner acaba de interactuar con el pet" también aplica a hover,
no solo a click/drag (ver §8.1.1 para la lista completa de qué cuenta
como interacción).

**Prioridad click/drag > hover/pasiva > estática** (sin cambios). Un
gesto de click/drag en curso nunca alimenta `hoverDwellTracker_` (lo
resetea en cambio, ver arriba). `TriggerHoverAction()` es, por diseño,
un no-op fuera de `kBase` — click siempre gana sobre cualquier disparo
pasivo pendiente. Y, simétricamente, un `TriggerAmbientAction()`/
`TriggerHoverAction()` que llega mientras la OTRA acción pasiva ya está
en curso también es un no-op (ambas comparten
`ControllerMode::kAmbientOrHoverAction` y el mismo chequeo `mode_ !=
kBase` en `content::AnimationController`) — ninguna de las dos
interrumpe nunca a la otra ni a sí misma. Matriz completa verificada
en `tests/AnimationControllerTest.cpp`: `ClickInterruptsAmbientAction`/
`AmbientActionNeverInterruptsClick` (eje click↔ambient, ya existentes),
más `HoverActionNeverInterruptsClick`/`ClickInterruptsHoverAction`
(eje click↔hover) y
`AmbientActionNeverInterruptsAnInProgressHoverAction`/
`HoverActionNeverInterruptsAnInProgressAmbientAction` (eje
ambient↔hover) — Block 05, tercera pasada.

**Verificado** con `NIMVLETS_DEV_HOVER_TEST_COUNT` contra el binario
real (simula ciclos completos de entrada + dwell completo + salida) y
con `tests/HoverDwellTrackerTest.cpp` (9 casos: no dispara al entrar,
sí exactamente al cruzar el umbral, nunca dos veces mientras se queda
quieto, un reset explícito exige un umbral completo nuevo aunque el
cursor nunca haya salido, ciclos repetidos de entrada/salida cada uno
dispara una vez) — genérico sobre CUALQUIER valor de dwell (los tests
usan su propio `kDwellSeconds` de prueba, no
`SpikeApp::kHoverDwellSeconds`), así que no necesitaron ningún cambio
al bajar el umbral de 5s a 1s.

## 8.1.1 Reinicio del timer ambient por interacción real (Block 05, tercera pasada — DEC-078)

Antes de esta pasada, `RearmAmbientDeadline()` solo se llamaba al
cargar/cambiar de pet y al detectar una transición de `BehaviorState`
real (ver `lastKnownStateId_` en `SpikeApp::Run()`) — un click o un
hover completo interrumpían la animación en curso, pero el conteo de
~12s hacia el PRÓXIMO ambient seguía corriendo desde donde estaba
antes de la interacción. QA manual del owner: "the pet should not
perform an ambient action immediately after the owner just interacted
with it" — un ambient podía dispararse casi inmediatamente después de
que el owner acababa de interactuar, si el deadline anterior ya
estaba por vencer.

`SpikeApp::RearmAmbientDeadline(nowMs)` ahora también se llama en:

- el cursor entrando a la región interactiva (un dwell de hover nuevo
  arrancando -- ver `MaybeTriggerHoverAction()`, detectado como la
  transición "no estaba en dwell -> ahora sí" del propio
  `hoverDwellTracker_`), incluso si el dwell nunca llega a cruzar el
  umbral y disparar una acción;
- un disparo de hover completo (`TriggerHoverAction()` exitoso);
- un click (`TriggerClick()` exitoso, en `SDL_EVENT_MOUSE_BUTTON_UP`);
- el inicio de un drag (`SDL_EVENT_MOUSE_BUTTON_DOWN` sobre la región
  interactiva);
- el fin de un drag (`SDL_EVENT_MOUSE_BUTTON_UP`, rama de drag);
- un cambio de dirección real (`SetActiveDirection()`, incluyendo el
  causado por un drag que cruza la mitad de pantalla -- ver
  `UpdateDirectionFromWindowPosition()`).

El intervalo en sí (12s, ver DEC-066/074/084 en
`docs/DECISION_LOG.md` para su historial completo) es ortogonal a
esta sección -- lo que describe acá es CUÁNDO se reinicia el
conteo. Un ambient cuyo deadline cae DURANTE un click/hover/drag en
curso sigue sin solaparse ni encolarse (comportamiento preexistente,
sin cambios en esta pasada): `TriggerAmbientAction()` es un no-op
fuera de `ControllerMode::kBase`, pero `RearmAmbientDeadline()` se
llama de todas formas justo después de chequear el deadline (ver
`SpikeApp::Run()`), así que el deadline SIEMPRE se reprograma para
~12s más adelante en vez de quedar vencido y disparar apenas termine
la interacción en curso.

## 8.2 Redraw de confirmación — de solo-dirección a toda transición (Block 05)

Block 04.3 armaba un segundo redraw completo ~120ms más tarde
(`confirmRedrawDeadlineMs_`) solo tras un cambio de dirección, como
mitigación de un posible "cold Metal present" (un `SDL_RenderPresent()`
tras un período largo sin presentar nada puede mostrar un drawable
todavía no asentado). Block 05 lo generaliza (`SpikeApp::
MarkNeedsRedraw()`) a CUALQUIER transición de animación real — click,
disparo ambient/hover, switch de pet — no solo cambios de dirección.

Esto se decidió tras un diagnóstico real de Bunny (ver el informe de
Block 05): un dump de frames REALMENTE renderizados
(`NIMVLETS_DEV_DUMP_FRAMES_DIR`, vía `SDL_RenderReadPixels` sobre
nuestro propio render target — nunca una captura de pantalla) mostró,
de forma 100% reproducible en 3/3 corridas, que el PRIMER render de
una sesión (antes de cualquier interacción) se lee de vuelta
completamente vacío — evidencia real, aunque no concluyente sobre si
eso refleja lo que de verdad se presentó en pantalla o solo una
condición de carrera del propio mecanismo de lectura. Dado que ningún
otro punto del pipeline (fuente -> compilación -> render simulado)
mostró pérdida de contenido medible, esta mitigación genérica y barata
es la respuesta responsable: extiende una protección ya existente a
más puntos de transición, sin inventar un fix específico de Bunny.

## 9. Fail-loudly content loading

`SpikeApp::Init()` resuelve el catálogo y carga el pack activo antes de
crear cualquier ventana. Si no se puede cargar, `Init()` loguea un
error fatal específico y el proceso sale con código no-cero — sin
ningún fallback hardcodeado.

## 9.1 Render path — UNA textura reutilizable por pet (Block 05)

`graphics::ActiveFrameTexture` mantiene **una sola** `SDL_Texture`
(`SDL_TEXTUREACCESS_STREAMING`, RGBA32) por pet activo y la actualiza en
el lugar (`SDL_UpdateTexture`) cada vez que
`AnimationController::CurrentFrame()` cambia. Blend mode
(`SDL_BLENDMODE_BLEND`) y scale mode (`SDL_SCALEMODE_LINEAR`) se fijan
explícitamente al crearla.

Esto reemplazó al modelo de Block 02 (`graphics::FrameTexture`), que
creaba y retenía una textura por CADA frame de CADA
animación/dirección/estado — 152 texturas para Bunny/Nidir, 204 para
cada variante de Frin — y cambiaba el objeto `SDL_Texture*` dibujado en
cada avance de frame. Ver DEC-081 en `docs/DECISION_LOG.md` para el A/B
medido que motivó el cambio (equivalencia visual byte a byte contra el
camino viejo; -24% a -27% de RSS según el pet).

Precondición estructural, verificada contra los 4 packs reales antes de
implementar nada: todos los frames de un pet compilado comparten las
mismas dimensiones en píxeles, porque `compose_on_canvas()` los coloca a
todos sobre el mismo canvas de trabajo compartido (§5). `SetFrame()`
igualmente recrea la textura si las dimensiones cambian (p. ej. al
cambiar de pet), así que no es una suposición silenciosa.

**Un modo de fallo silencioso que este modelo ELIMINA:** hasta Block 05
había que acordarse de cubrir cada colección de animaciones nueva en
`SpikeApp::AttachAllTextures()`/`ReleaseAllTextures()`; olvidarse ahí
resolvía bien en `AnimationController` pero renderizaba completamente
transparente en runtime, sin ningún error — pasó de verdad en Block
04.2 (ver `docs/NIDIR_CONTENT.md`, "bug de cobertura de texturas").
Ahora no hay ninguna lista de colecciones que mantener sincronizada: se
sube el frame que el controller esté mostrando, sea cual sea, en el
momento de dibujarlo. Una colección nueva funciona por construcción.

## 10. Running

```bash
# from the repository root — the catalog path resolves from CWD
./build/macos-debug/src/app/nimvlets_spike

# owner manual QA: launch a specific pet without touching persisted state
NIMVLETS_DEV_SELECT_PET=bunny ./build/macos-debug/src/app/nimvlets_spike
NIMVLETS_DEV_SELECT_PET=nidir ./build/macos-debug/src/app/nimvlets_spike
NIMVLETS_DEV_SELECT_PET=frin/male ./build/macos-debug/src/app/nimvlets_spike
NIMVLETS_DEV_SELECT_PET=frin/female ./build/macos-debug/src/app/nimvlets_spike

# QA convenience: ambient action every ~5s instead of the real per-state default
NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS=5 ./build/macos-debug/src/app/nimvlets_spike

# regenerate a pack after changing its source PNGs
python3 tools/generate_bunny_pack.py
python3 tools/generate_nidir_pack.py
python3 tools/generate_frin_pack.py
```

## 11. Escala visual por-pet (Block 05)

`content::PetDefinition::visualScale` (default 1.0) es un multiplicador
de tamaño en pantalla, dato puro por-pet — nunca una rama de código.
`SpikeApp::EffectiveCanvasWidth()/EffectiveCanvasHeight()`
(`round(canvasWidth/Height * visualScale)`) es la ÚNICA fuente de
verdad para el tamaño de ventana (`SDL_CreateWindow`/
`SDL_SetWindowSize`), la presentación lógica del renderer
(`SDL_SetRenderLogicalPresentation`), el rect de destino del sprite
(`RenderFrame()`), y las dimensiones del hit-mask
(`core::AlphaMask::FromAlphaChannel`) — los cuatro se derivan del mismo
cálculo, así que nunca pueden desalinearse entre sí ni con el alto-DPI
(que sigue operando sobre el mismo canvas lógico de siempre, sin
cambios). El arte fuente y los bytes del pack compilado NUNCA se
tocan — es puramente cuánto se estira al dibujar.

Valores actuales (Block 05, tercera pasada de corrección post-QA — ver
DEC-076 en `docs/DECISION_LOG.md`): Bunny `1.0` (tamaño actual
aprobado por el owner, la referencia; sin cambio), Nidir `1.25` (subido
de 1.10 -- QA manual real: seguía sintiéndose más chico que Bunny
incluso con el +10% ya aplicado), Frin (macho Y hembra, un único valor
para ambas variantes) `1.30` (subido de 1.0 -- "feels too small,
should be comparable to Bunny/Nidir"). Estos candidatos se derivaron
midiendo el bounding box de contenido VISIBLE del pack compilado como
fracción de su propio frame (no solo el tamaño del canvas transparente,
que incluye margen) — la "presencia visible efectiva" real en puntos:
Bunny ~114×159pt, Nidir ~154×176pt (antes: ~136×155pt, con la ALTURA
por debajo de Bunny pese al ajuste previo), Frin macho ~111×167pt
(antes: ~85×128pt), Frin hembra ~118×179pt (antes: ~91×138pt). Ver
`docs/NIDIR_CONTENT.md`/`docs/BUNNY_CONTENT.md`/`docs/FRIN_CONTENT.md`
y el informe de Block 05 para el detalle completo.
