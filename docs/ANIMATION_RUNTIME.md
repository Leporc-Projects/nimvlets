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
  width, height, anchor, durationMs, pixels (RGBA8), rendererHandle (opaque)

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
reuses. None of this changed in Block 05 — only the manifest shape
`compile_pet_pack.py` walks did (§2/§4 above): every compilable
animation now lives under `states[i].base_animation` or one of
`states[i].{ambient,hover,click}_actions[j]`, instead of the old flat
`idle`/`click_reaction`/`passive_actions` keys.

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
                                                    // "lying"), present otherwise (~15s Bunny/Nidir,
                                                    // ~45s Frin "seated")
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

Manual QA cannot reasonably wait a real ~15-45s (per-state ambient
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

## 8.1 Política de hover (Block 04.3, corregida en Block 05)

El owner pidió que reposar el cursor sobre el Nimvlet — sin hacer
click — también pueda disparar una acción, sin "spamear" mientras el
mouse se queda quieto encima, Y (requisito nuevo, Block 05) **sin
quedar bloqueado solo porque el timer ambient todavía no venció**.

**Detección de flanco, no de estado sostenido** (sin cambios desde
Block 04.3). `core::HoverPassiveGate` (`src/core/HoverPassiveGate.h/.cpp`,
testeada en aislamiento en `tests/HoverPassiveGateTest.cpp`) solo
responde "¿el cursor ACABA de entrar a la región interactiva?" — el
flanco de subida, nunca un hover sostenido.

**Cooldown INDEPENDIENTE (Block 05, corrección de comportamiento real).**
Block 04.3 compartía deliberadamente el mismo deadline entre el timer
ambient y el disparo por hover — pero eso significaba que, si el timer
ambient acababa de disparar, hover quedaba BLOQUEADO hasta que el
intervalo ambient completo (ahora 15s/45s) volviera a vencer, algo que
el owner pidió explícitamente corregir: "hover must not be blocked
simply because the ambient timer has not expired". `SpikeApp::
MaybeTriggerHoverAction()` ahora usa `hoverCooldownUntilMs_`, un
deadline propio y chico (2s, `kHoverCooldownSeconds`), completamente
independiente de `ambientDeadlineMs_` — ninguno de los dos caminos
consulta el reloj del otro. "Leaving and re-entering should re-arm
hover after a small independent cooldown": el flanco de subida sigue
siendo necesario, pero además debe haber pasado al menos ese cooldown
chico desde el ÚLTIMO disparo por hover (nunca desde el último disparo
ambient) para que uno nuevo cuente.

**Prioridad click/drag > hover/pasiva > estática** (sin cambios). Un
gesto de click/drag en curso nunca alimenta `hoverPassiveGate_`.
`TriggerHoverAction()` es, por diseño, un no-op fuera de `kBase` —
click siempre gana sobre cualquier disparo pasivo pendiente.

**Verificado** con `NIMVLETS_DEV_HOVER_TEST_COUNT` contra el binario
real (mismo mecanismo que antes, ahora ejercitando el cooldown
independiente en vez del compartido) y con `tests/StatefulBehaviorTest.cpp`
(`InvalidHoverTriggerIsNoOpWhenStateDefinesNoHoverPool` — Frin no
define ningún pool de hover todavía).

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

## 9.1 Texture attachment coverage — a silent failure mode `PetPackLoader` can't catch

A pack that fails to *load* fails loudly (§9). A pack that loads fine
but has one animation collection whose frames never get an SDL texture
attached does **not** fail loudly by default — `graphics::FrameTexture`
only turns pixel data into a texture when `SpikeApp::AttachAllTextures()`
explicitly walks that collection. Block 05's `AttachAllTextures()`/
`ReleaseAllTextures()` walk EVERY `BehaviorState`'s `baseAnimation` +
its overrides, and every `ambientActions`/`hoverActions`/`clickActions`
entry's `animation` + its own `directionOverrides` — generically, via
one shared lambda per collection kind, so a future `WeightedAction`
list added anywhere in the graph is covered by construction rather
than needing its own new loop. `content::AnimationController` has no
visibility into this at all (`ResolveAnimation()` resolves correctly
regardless) — this remains, as documented since Block 04.2, a class of
bug that can only be caught by actually exercising the content, not by
a unit test (see `docs/NIDIR_CONTENT.md` §6 for the real bug that
motivated this warning).

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

Valores actuales: Bunny `1.0` (tamaño actual aprobado por el owner, sin
cambio), Nidir `1.10` (candidato conservador de QA — "somewhat larger"
— resultado exacto 176×173 nativo → 194×190 efectivo), Frin `1.0`
(sin pedido de ajuste en este bloque). Ver `docs/NIDIR_CONTENT.md`/
`docs/BUNNY_CONTENT.md`/`docs/FRIN_CONTENT.md` y el informe de Block 05
para el detalle completo.
