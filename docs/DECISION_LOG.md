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

### DEC-025 — `src/persistence`: una nueva librería sin SDL para el estado local
**Status:** DECIDIDO · Block 03 — ver `docs/PERSISTENCE.md`.

Block 03 necesitaba una pequeña capa de persistencia (click balance,
pet/variante activos, última posición de ventana) que se mantuviera
testeable sin display, según AGENTS.md §12 y el patrón ya establecido
de este proyecto (`src/core`, `src/content` son ambos puros/sin SDL por
exactamente esta razón). En vez de meter la lógica de storage dentro de
`src/app/SpikeApp` (acoplado a SDL) o dentro de `src/content` (una
preocupación distinta — content describe *cómo se ve un pet*, no *qué
hizo el usuario*), se creó una nueva librería de nivel superior,
replicando el precedente de Block 02 de una librería enfocada por cada
feature grande:

- `persistence::AppState` — un struct simple, sin SDL, sin I/O de
  archivos. Genérico por construcción: `activePetId`/`activeVariantId`
  son strings, no un enum, así que un nuevo pet o variante nunca
  requiere un cambio de schema ni de código.
- `persistence::AppStateSerializer` — (de)serialización pura hacia/
  desde un buffer de bytes en memoria, reflejando la propia separación
  de `content::PetPackLoader` entre parseo y acceso a archivos.
- `persistence::AppStateStore` — la única pieza que toca un filesystem;
  recibe una ruta de directorio como argumento del constructor en vez
  de resolverla ella misma, así los tests la apuntan a directorios
  temporales y la app real la apunta al resultado de
  `SDL_GetPrefPath()` — el mismo camino nunca aparece dos veces.
- `persistence::PersistenceScheduler` — timing puro de debounce/dirty-
  flag, testeado con timestamps fabricados exactamente igual que
  `core::FrameScheduler`.

`nimvlets_persistence` no enlaza nada propio del proyecto (ni siquiera
`nimvlets_core`) — ver `src/persistence/CMakeLists.txt` — depende solo
de la standard library de C++ (`<filesystem>`, `<optional>`, ...).

---

### DEC-026 — Dos contadores de click separados; ubicación de storage vía `SDL_GetPrefPath` + override DEV
**Status:** DECIDIDO · Block 03 — ver `docs/PERSISTENCE.md` §§2, 7.

`clickCount_` (el diagnóstico existente de solo-sesión de Block 01/02,
logueado al shutdown) y `AppState::clickBalance` (la nueva moneda de
producto, persistida y acumulada — AGENTS.md §2) se mantienen como dos
campos distintos en vez de reutilizar uno: responden preguntas
distintas ("cuántos clicks en esta corrida" vs. "cuántos clicks en
total, gastables después") y mezclarlos haría que el balance de un
futuro Shop dependiera de código de logging de diagnóstico de sesión.

Ubicación de storage: `SDL_GetPrefPath("Leporc Projects", "Nimvlets")`
— el propio resolutor multiplataforma de SDL para app-data por usuario,
que ya maneja por completo la diferencia entre macOS/Windows, así que
no hizo falta código nuevo en `src/platform/*` (a diferencia de la
transparencia/click-through de ventana, que SDL no puede abstraer del
todo — ver `docs/PLATFORM_SPIKE.md`). `NIMVLETS_DEV_APPDATA_DIR`
(verificado antes de llamar a `SDL_GetPrefPath()`) le permite a la QA
manual y a los smoke tests no interactivos propios de este bloque
redirigir la persistencia a un directorio temporal aislado — reflejando
exactamente el patrón de `NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS` de
Block 02. Esto fue necesario en la práctica: un intento temprano de
smoke test que probó sobrescribir la variable de entorno `HOME` en su
lugar **no** redirigió `SDL_GetPrefPath()` en macOS (su backend de
Cocoa resuelve el directorio de app-support vía APIs de Foundation, no
la variable de entorno `HOME`), escribiendo brevemente un archivo real
bajo el directorio real por usuario antes de agregar este override —
detectado de inmediato, el archivo perdido se eliminó, y ningún otro
test de este bloque toca la ubicación real.

---

### DEC-027 — Política de escritura con debounce y escrituras atómicas basadas en rename
**Status:** DECIDIDO · Block 03 — ver `docs/PERSISTENCE.md` §§4, 6.

Los clicks rápidos no deben convertirse en una escritura a disco cada
uno. `PersistenceScheduler` implementa un debounce de ventana fija
(2000ms, `kDefaultDebounceMs`): el primer cambio que marca dirty arma
un único deadline; los cambios posteriores antes de que dispare
actualizan el estado en memoria pero nunca empujan el deadline — esto
es lo que hace que la actividad continua se coalesca en escrituras
periódicas en vez de inundar el disco o dejar la persistencia sin
escribir indefinidamente. Un flush fallido queda dirty y se reintenta
un intervalo de debounce más tarde, nunca de inmediato (acota la
frecuencia de reintento bajo un fallo persistente) y nunca se descarta
en silencio. El shutdown limpio flushea incondicionalmente, ignorando
el deadline.

Las escrituras son atómicas vía la técnica estándar de
escribir-a-temp-y-luego-renombrar (`AppStateStore::Save()`): el archivo
real `state.nvstate` solo se reemplaza mediante un único
`std::filesystem::rename()` dentro del mismo directorio, nunca se abre
para escritura directamente, así que una escritura que falla en
cualquier paso anterior deja el save previo válido completamente
intacto — verificado directamente en `tests/AppStateStoreTest.cpp` vía
una técnica portátil de simulación de fallos (ocupar la ruta del
archivo temporal con un directorio, lo cual falla al abrirse como
archivo regular de forma idéntica en cada plataforma que este proyecto
soporta, evitando trucos frágiles basados en `chmod`).

---

### DEC-028 — Fix de capacidad de respuesta ante shutdown del event loop: espera máxima acotada
**Status:** DECIDIDO · Block 03 (corrección de bug, encontrada vía los
propios tests automatizados de este bloque) — ver `docs/PERSISTENCE.md`
§9.

La restricción de "sin QA manual" de este bloque exigía smoke tests no
interactivos genuinamente confiables, incluyendo dejar que la app se
asentara del todo en idle estático antes de enviarle `SIGTERM` — un
escenario que ningún test de un bloque anterior había ejercitado (cada
medición previa de CPU/comportamiento o bien enviaba la señal poco
después del arranque, mientras todavía llegaban eventos relacionados
con el startup, o corría bajo un override de intervalo DEV corto que
mantenía al loop despertando con frecuencia). Eso expuso una
característica real y preexistente: un `SIGINT`/`SIGTERM` entregado no
interrumpe por sí mismo un `SDL_WaitEventTimeout` bloqueante en esta
plataforma — el loop solo vuelve a chequear `ShutdownRequested()`
cuando su propia espera retorna naturalmente. Sin nada más programado
por minutos (quedando solo el deadline de acción pasiva de ~300s como
límite), una señal de terminación podía en principio tardar ese tanto
en notarse.

Corregido con un tope duro (`kMaxWaitMs = 1000.0`) sobre el tiempo de
espera calculado del event loop, aplicado después de cualquier otro
deadline. Un despertar que no encuentra nada pendiente no hace ningún
trabajo de redraw/hit-mask/disco antes de volver a dormir, así que esto
no reintroduce el tick de render fijo de Block 01 ni regresiona el CPU
en idle estático — re-medido en ≈0.0% después del fix (ver
docs/PERFORMANCE_BUDGETS.md). La latencia de shutdown después de este
fix queda acotada a aproximadamente un segundo, confirmado
reproduciendo exactamente el escenario que antes se colgaba (una
corrida en idle completamente asentada, con un estado persistido que
ya coincidía y estaba sincronizado) y observando una salida limpia poco
después del `SIGTERM`.

---

### DEC-029 — `src/catalog`: nueva librería para identidad + catálogo de pets, sin SDL
**Status:** DECIDIDO · Block 04 — ver `docs/CATALOG.md`.

Block 04 necesitaba saber qué Nimvlets existen y resolver/cambiar cuál
mostrar, sin ninguna rama de C++ específica de un pet (AGENTS.md §13) y
sin reescribir la capa de persistencia de Block 03. Siguiendo el
precedente de una librería enfocada por cada feature grande
(`src/content` en Block 02, `src/persistence` en Block 03), se creó
`src/catalog`:

- `catalog::PetIdentity` — un struct plano (`petId` + `variantId`
  opcional), sin ningún enum de Nimvlets conocidos, con
  igualdad/orden/hash.
- `catalog::PetCatalog`/`CatalogEntry` — el catálogo ya validado
  (`Find`/`Default`/`Entries`), construido a partir de una lista que el
  llamador garantiza válida — el mismo reparto de responsabilidades que
  `content::PetDefinition` (no se autovalida) y `content::PetPackLoader`
  (sí valida) ya establecieron.
- `catalog::PetCatalogLoader` — parseo puro del formato binario
  "NVCATLG1" desde un buffer en memoria, sin I/O de archivos en la
  lógica central.
- `catalog::ActivePetResolution` — `ResolveActiveSelection()` (puro,
  sin filesystem) y `LoadPetForIdentity()` (reutiliza
  `content::LoadPetPackFromFile`), la única función que tanto la
  resolución de arranque como el switching en runtime comparten.

`nimvlets_catalog` enlaza `nimvlets_content` PUBLIC (necesita
`PetDefinition` + `LoadPetPackFromFile`) pero nada de SDL ni de
`nimvlets_persistence` — la conexión entre selección de catálogo y
persistencia la hace `src/app`, no esta librería, igual que
`src/app` ya era el único lugar que conectaba clicks/drag con
`persistence::AppState` en Block 03.

---

### DEC-030 — Formato "NVCATLG1": validación estricta en el parser, existencia de `packPath` solo como chequeo de tooling
**Status:** DECIDIDO · Block 04 — ver `docs/CATALOG.md` §3.

Mismo patrón de formato binario custom que "NVPACK1"/"NVSTATE1"
(magic + strings con prefijo de longitud + campos de ancho fijo,
little-endian, compilado por un script Python sin dependencias de
terceros) por consistencia con el pipeline de assets ya establecido —
"mantener el formato/tooling simple y consistente" (block brief §2).

El loader en C++ (`catalog::LoadCatalogFromMemory`) rechaza: cero
entradas, `petId`/`packPath` vacíos, una identidad `(petId, variantId)`
duplicada, y cualquier cantidad de entradas `isDefault` distinta de
exactamente una — cubriendo "duplicados rechazados", "default único
resoluble", y "referencia de pack inválida rechazada claramente" del
block brief (§2) de la forma estructural, en memoria, que el brief
exige. Deliberadamente NO verifica que cada `packPath` apunte a un
archivo que realmente existe: eso requeriría tocar el filesystem desde
un parser que de otro modo es puro y testeable con buffers sintéticos,
y verificarlo cargando cada pack violaría "el runtime no debe cargar
todos los packs al arranque" (block brief §2). Un pack faltante o
corrupto se descubre y reporta claramente recién cuando algo
efectivamente intenta cargarlo — vía el mismo camino
`content::LoadPetPackFromFile` que ya falla claramente en ese caso
desde Block 02, sin duplicar esa lógica.

`tools/compile_pet_catalog.py` sí verifica `pack_path` en tiempo de
*compilación* (existencia relativa al directorio de trabajo, mismo
supuesto "correr desde la raíz del repo" que el resto de `tools/`) —
un chequeo de cordura para quien autora el catálogo, complementario al
runtime, no un reemplazo de él.

---

### DEC-031 — Resolución de arranque con cadena de fallback: persistido -> default -> fallo genuino
**Status:** DECIDIDO · Block 04 — ver `docs/CATALOG.md` §5.

El block brief exige explícitamente "no debe crashear porque un pet
guardado ya no existe" (§3). `SpikeApp::Init()` intenta cargar el pack
de la entrada que `ResolveActiveSelection()` resolvió (calce exacto o
ya el default); si esa carga falla y la entrada no era ya el default,
reintenta una vez más con el default del catálogo antes de rendirse.
Solo si *ambos* intentos fallan es un fallo de arranque genuino,
reportado tan ruidosamente como un catálogo ausente (mismo principio
de "fail loudly" que Block 02 estableció para packs individuales —
DEC-023) — no hay una tercera capa de fallback inventada que no pidió
el brief.

Cuando se termina usando algo distinto de lo persistido — identidad
desconocida, vacía (primera ejecución), o el pack guardado dejó de
cargar — `appState_.activePetId`/`activeVariantId` se reparan en
memoria para reflejar la verdad y se marca el scheduler de
persistencia dirty (reutilizando exactamente el mecanismo de debounce
de Block 03, sin ninguna escritura especial). Esto reemplaza la
sincronización más simple que Block 03 hacía (comparar solo contra
`pet_.id`) con la versión completa que ya anticipaba su propio
comentario: "un valor real y con sentido que la UI de selección de un
bloque futuro pueda leer."

---

### DEC-032 — `TrySwitchActivePet`: cargar antes de descartar, reset del controller antes de reemplazar `pet_`, redimensionar ventana si el canvas cambia
**Status:** DECIDIDO · Block 04 — ver `docs/CATALOG.md` §6.

El pack nuevo siempre se carga en un `content::PetDefinition` local
*antes* de tocar cualquier estado vivo — solo tras confirmar éxito se
sueltan las texturas del pet anterior y se reemplaza `pet_`. Esto es lo
que garantiza, sin ningún código adicional, que "un switch fallido
preserva el pet activo previo" (block brief §4): en el camino de
fallo, literalmente nada se tocó todavía.

`animController_.reset()` ocurre explícitamente *antes* de reasignar
`pet_` (y se vuelve a construir después, vía `.emplace(pet_)`, ya
apuntando al contenido nuevo). Razón concreta: `AnimationController`
guarda una referencia al `PetDefinition` activo y un puntero interno a
la animación actual, que puede apuntar dentro de
`pet_.passiveActions[i]` — un `std::vector`, cuyo buffer puede
reubicarse al reasignarse si el pet nuevo tiene una cantidad distinta
de acciones pasivas que el viejo. Sin el reset explícito, ese puntero
podría quedar colgando en el instante entre "`pet_` ya cambió" y "el
controller todavía no se reconstruyó". Con el reset, nunca existe un
`AnimationController` vivo mientras el contenido de `pet_` está en
proceso de reemplazo.

El tamaño lógico de la ventana y `SDL_SetRenderLogicalPresentation` se
reaplican incondicionalmente después de cada switch exitoso (no solo
si el tamaño de canvas efectivamente cambió) — más simple que rastrear
el tamaño anterior, y sin costo real cuando no cambió. No estaba
listado explícitamente en el brief ("texture/hit mask/window shape"),
pero es necesario para que "reemplazar el contenido activo" en efecto
funcione correctamente para el caso general que el catálogo está
diseñado para soportar — sin esto, un futuro pet con un canvas de
tamaño distinto al de Bunny se vería mal escalado tras un switch.

El mecanismo solo-DEV de smoke test (`NIMVLETS_DEV_SWITCH_TEST_COUNT`)
ejecuta sus intentos sincrónicamente, una sola vez, antes del loop
principal — nunca agrega un timer ni un tick recurrente al scheduler
del event loop, preservando "sin polling" (block brief §5) incluso
cuando está activo.

### DEC-033 — Linux: transparencia/always-on-top/not-focusable no necesitan código nativo
**Status:** DECIDIDO · Block 04.1 — ver `docs/LINUX_PLATFORM.md` §3.1.

Antes de escribir cualquier código Xlib/Wayland, se leyó directamente
la fuente pineada de SDL 3.4.12 (bajo `_deps/sdl3-src/`, mismo método
que Block 01 usó para macOS). Hallazgo: tanto `X11_CreateWindow` como
el manejo de `SDL_WINDOW_TRANSPARENT` de Wayland
(`SetSurfaceOpaqueRegion`) ya aplican transparencia real sin ningún
llamado adicional; `X11_CreateWindow` también aplica
`SDL_WINDOW_ALWAYS_ON_TOP` y traduce `SDL_WINDOW_NOT_FOCUSABLE` a WM
hints ICCCM estándar directamente. Conclusión: `ConfigureCompanionWindow()`
en Linux no necesita ningún llamado nativo para estos tres requisitos
-- a diferencia de macOS (necesitó `NSWindow.opaque`/`hasShadow`/etc.
explícitos) y Windows (`WS_EX_LAYERED`/`HWND_TOPMOST` explícitos).

### DEC-034 — X11 reutiliza el mecanismo de shape de macOS; Wayland no tiene ninguno con la SDL pineada
**Status:** DECIDIDO · Block 04.1 — ver `docs/LINUX_PLATFORM.md` §3.2.

`device->UpdateWindowShape` (el puntero de función que
`SDL_SetWindowShape()` despacha) está wireado a
`X11_UpdateWindowShape` (`src/video/x11/SDL_x11shape.c`, usa
`XShapeCombineMask`/`Region` con `ShapeInput` -- solo hit-testing,
nunca compone pixeles renderizados) pero **nunca** se asigna para el
driver Wayland en absoluto -- no existe ningún `SDL_waylandshape.c` en
el árbol fuente. X11 pasa a usar el mismo tier que macOS
(`NativeShapeHitTestIsRenderSafe() == true`, event-driven, sin
polling). Wayland queda en `false`, y además se determinó que ningún
mecanismo de polling alternativo serviría ahí tampoco -- ver DEC-035.

### DEC-035 — `ClickThroughPollingIsMeaningful()`: nueva función del seam compartido
**Status:** DECIDIDO · Block 04.1 — ver `docs/LINUX_PLATFORM.md` §4.

Antes de este bloque, `usingNativeShapeHitTest_` (un solo booleano)
alcanzaba para decidir "shape nativo vs. polling" porque, con solo
macOS/Windows, "sin shape nativo" y "el polling funcionaría" siempre
coincidían. La investigación de Wayland (DEC-034) encontró dos hechos
adicionales de la fuente pineada que rompen esa coincidencia:
`Wayland_GetGlobalMouseState()` solo retorna una posición útil con
foco propio, y no existe ninguna forma pública de restringir la input
region de una superficie (`wl_surface_set_input_region` es interno,
sin `wl_compositor` expuesto) -- ningún polling podría cambiar el
click-through ahí. Se agregó `platform::ClickThroughPollingIsMeaningful()`
al header compartido (con implementaciones triviales y correctas en
macOS/Windows también, documentadas explícitamente como "nunca
consultadas en la práctica" ahí) y un nuevo miembro
`usingPollDrivenClickThrough_` en `SpikeApp` que reemplaza los dos
sitios de `Run()` que antes chequeaban `!usingNativeShapeHitTest_`
directamente. Sin este cambio, Linux/Wayland heredaría un loop de
polling ~60Hz permanente y comprobadamente inútil -- exactamente lo
que el brief §8 y AGENTS.md §2 prohíben.

### DEC-036 — Wayland: sin hack de posicionamiento; SpikeApp ahora chequea el retorno de SDL_SetWindowPosition
**Status:** DECIDIDO · Block 04.1 — ver `docs/LINUX_PLATFORM.md` §3.3/§6.

`Wayland_SetWindowPosition()` retorna literalmente
`SDL_SetError("wayland cannot position non-popup windows")` para
cualquier toplevel normal -- una limitación del protocolo `xdg-shell`
en sí, no de SDL ni de este proyecto. Se decidió explícitamente NO
implementar ningún protocolo específico de compositor para forzar
esto (el brief lo prohíbe: "do not implement compositor-specific hacks
merely to force absolute positioning"). En cambio, `SpikeApp::Init()`
ahora chequea el valor de retorno real de `SDL_SetWindowPosition()`
(antes de este bloque no se chequeaba en ninguna plataforma) y loguea
explícitamente cuándo no pudo aplicar una posición guardada, citando
`SDL_GetError()` -- genérico, sin ningún `#ifdef` de plataforma;
`appState_.lastWindowPosition` se sigue guardando y preservando igual
en cualquier backend.

### DEC-037 — Opciones de CMake de SDL3 para Linux: mínimas a propósito, con riesgo de FATAL_ERROR si no
**Status:** DECIDIDO · Block 04.1 — ver `docs/LINUX_PLATFORM.md` §2.

`SDL_missing_dependency()` (`cmake/macros.cmake` en la fuente pineada)
hace `FATAL_ERROR` -- no un warning silencioso -- en Linux cuando una
sub-feature X11 que SDL deja en ON por defecto no encuentra su paquete
de desarrollo. En vez de instalar en CI el listado "todas las
features" que `docs/README-linux.md` de SDL documenta,
`cmake/FetchSDL3.cmake` apaga explícitamente lo que este proyecto no
usa (Xcursor/Xdbe/Xfixes/Xscrnsaver/Xsync/Xtest, `SDL_WAYLAND_LIBDECOR`
-- nuestra ventana siempre es `SDL_WINDOW_BORDERLESS`, así que la ruta
de decoración cliente-side de Wayland nunca se selecciona) y mantiene
solo XShape (mecanismo de click-through, DEC-034), XInput2 (ruta de
entrada moderna, no verificable sin hardware real así que se prefirió
el default documentado de SDL en vez de apostar a que el protocolo
core alcanza) y XRandr (consulta de escala/DPI precisa).

### DEC-038 — CI de Linux: smoke de X11 bloqueante, smoke de Wayland intento real pero no bloqueante
**Status:** DECIDIDO · Block 04.1 — ver `docs/LINUX_PLATFORM.md` §9.

El brief §6 pide explícitamente smoke no interactivo de X11 bajo Xvfb
sin condicionales, pero para Wayland permite documentar la brecha en
vez de fingir un PASS si la validación headless no es confiable. Este
bloque se corrió enteramente en un host macOS sin forma de ejecutar
(`push` está prohibido para esta sesión) el workflow y observar si
Weston headless (`--backend=headless-backend.so`) de verdad levanta en
el runner `ubuntu-24.04` real -- hay fricciones conocidas en el
ecosistema con `XDG_RUNTIME_DIR`/D-Bus/logind en imágenes CI mínimas
que no se pudieron descartar sin poder correr esto. Se decidió: paso
de X11 bloqueante (Xvfb+X11+SDL es un patrón de CI extremadamente
estándar, riesgo bajo); paso de Wayland con `continue-on-error: true`
más un intento real (no un placeholder), imprimiendo explícitamente
`PASS`/`INCONCLUSIVE/FAIL` en el log en vez de forzar verde. El
build/código Wayland en sí (`SDL_WAYLAND=ON`, la rama Wayland del
adapter) queda completo y testeado independientemente del resultado de
ese paso.

### DEC-039 — Extensión aditiva y retrocompatible del formato "NVPACK1" para direcciones
**Status:** DECIDIDO · Block 04.2 — ver `docs/NIDIR_CONTENT.md` §5.

En vez de reemplazar `PetDefinition::idle` por una lista genérica
indexada por dirección (lo que habría exigido tocar cada sitio que ya
leía `pet.idle`/`pet_.idle` — `AnimationController`, `SpikeApp`, y una
docena de fixtures de test), se agregó `idleDirectionOverrides` como
campo puramente aditivo: `idle` conserva su significado exacto de
antes (el idle canónico, kRight por convención), y una sección final
OPCIONAL del formato binario ("NVPACK1", sin bump de magic ni de
schema version) codifica cualquier variante direccional extra.
`ByteReader::HasMoreData()` distingue un pack viejo (termina justo
después de `passiveActions`, sin bytes extra) de uno nuevo (sí quedan
bytes) sin ambigüedad. Resultado directo: `assets/dev/bunny_pack.nvpack`
no necesitó recompilarse — su manifest nunca menciona
`idle_direction_overrides`, así que `tools/compile_pet_pack.py` no
escribe ni un byte de más, y el pack compilado es idéntico al de antes
de este bloque.

### DEC-040 — Placeholder de un solo frame para el click_reaction de Nidir
**Status:** DECIDIDO · Block 04.2 — ver `docs/NIDIR_CONTENT.md` §6.

El export real que el owner proveyó solo cubre la animación de idle
-- no existe arte de click dedicado para Nidir todavía. Como
`content::PetDefinition::clickReaction` es un campo obligatorio del
esquema actual, se completó con un placeholder estructural mínimo:
un solo frame (reutiliza `idle/right/frames/frame_000.png`), `one_shot`,
~100ms, `returns_to_idle: true`. Debe ser `one_shot`, nunca `static`:
`AnimationController::Advance()` solo transiciona de vuelta a Idle
cuando una animación `kOneShot` termina naturalmente -- una animación
`kStatic` nunca dispara esa transición, así que un click_reaction
`static` habría dejado al controller trabado en `ClickReaction` para
siempre después del primer click, bloqueando clicks y acciones
pasivas futuras. Documentado explícitamente como placeholder, no como
contenido terminado.

### DEC-041 — fps del idle loop de Nidir: 6.0, elegido por medición real, no por el export
**Status:** DECIDIDO · Block 04.2 — ver `docs/PERFORMANCE_BUDGETS.md`, "Mediciones reales de Block 04.2".

El export de Ludo.ai no trae ninguna cadencia de reproducción
indicada. Se midió contra el binario Release real antes de fijar un
valor: a 12fps, el idle loop de Nidir (25 frames, canvas nativo
513×525) promedia ~11-12% CPU en steady state; a 6fps, ~4-5.5%. Se
priorizó el costo de CPU más bajo sobre una cadencia más fluida, ya
que el brief de este bloque tiene un requisito explícito de recursos
(§10) y ninguno de fluidez visual específica. Ambos números exceden el
objetivo de ~1% que `docs/PERFORMANCE_BUDGETS.md` documenta para Bunny
-- documentado como una limitación real, no ocultado (ver "Bugs/debt/
limitations" del informe final de este bloque).

### DEC-042 — Canvas de Nidir a resolución nativa (513×525), sin reescalar
**Status:** DECIDIDO · Block 04.2 — ver `docs/NIDIR_CONTENT.md` §7.

A diferencia de Bunny (reescalado a 160×160 como fixture de dev), el
canvas de Nidir usa exactamente la resolución nativa de los frames
importados. Instrucción explícita del block brief §4: "Do NOT silently
crop/resize/recenter unless required by the runtime contract" -- el
contrato de runtime no lo exige (`SDL_RenderTexture` ya escala
cualquier resolución nativa al tamaño de canvas del pet). Se identificó
-- pero no se resolvió unilateralmente, ya que hacerlo habría sido
exactamente el tipo de reescalado silencioso que el brief prohíbe --
una tensión real con el invariante de producto "ventana pequeña"
(AGENTS.md §2), y con el costo de CPU medido en DEC-041. Queda como
una decisión pendiente para un futuro bloque/el owner, no resuelta acá.

### DEC-043 — `~/Downloads/Nidir.png` usado como `master.png`
**Status:** DECIDIDO · Block 04.2 — ver `docs/NIDIR_CONTENT.md` §7.

No listado explícitamente entre los dos folders que el block brief §2
nombra, pero coincide en nombre, ubicación (junto a los dos exports
requeridos, dejado por el owner al mismo tiempo), y rol (una única
imagen de referencia estática, sin alpha real, 1254×1254 -- exactamente
lo que `assets/source/nimvlets/README.md` ya definía como `master.png`
desde Block 02) -- evidencia suficiente para una inferencia razonable,
no una adivinanza. El brief solo pedía STOP ante folders faltantes/
ambiguos entre los dos requeridos explícitamente, y ninguno de esos dos
lo estaba.
