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
**Status:** SUPERSEDED por DEC-048 — Block 04.2, tercera pasada. El
click-fire real fue importado, reemplazando el placeholder por
completo.

<details><summary>Texto original (para contexto histórico)</summary>

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

</details>

### DEC-041 — fps del idle loop de Nidir: 6.0, elegido por medición real, no por el export
**Status:** SUPERSEDED por DEC-044 — Block 04.2, segunda pasada. `idle`
dejó de ser un loop continuo (ver DEC-044), así que "fps de CPU más
bajo" dejó de ser la pregunta relevante: el fps ahora se deriva de la
configuración real de export de Ludo.ai, no de una medición de costo.

<details><summary>Texto original (para contexto histórico)</summary>

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

</details>

### DEC-042 — Canvas de Nidir a resolución nativa (513×525), sin reescalar
**Status:** SUPERSEDED por DEC-045 — Block 04.2, segunda pasada. La
tensión con el invariante "ventana pequeña" que esta entrada dejaba
abierta resultó ser un problema real reportado por el owner ("Nidir
currently appears much larger on screen than Bunny"); DEC-045 la
resuelve con una política de canvas lógico genérica.

<details><summary>Texto original (para contexto histórico)</summary>

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

</details>

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

---

### DEC-044 — Semántica corregida de Nidir: base estática + idle periódico one-shot vía el scheduler existente
**Status:** DECIDIDO · Block 04.2, segunda pasada — ver
`docs/NIDIR_CONTENT.md` §5.1. **Supersede DEC-041.**

La primera pasada modeló toda la secuencia de 25 frames de "respiración"
de Nidir como su `idle`, con `PlaybackKind::kLoop` -- reproducción
continua para siempre, exactamente el patrón que Block 02 eliminó de
Bunny (ver DEC-021) y que el owner señaló explícitamente como
incorrecto para el producto real: "Nidir's idle MUST NOT loop
continuously." El modelo correcto, ya probado por Bunny desde Block 02,
no necesitó ningún cambio de arquitectura -- solo una reasignación de
contenido:

- `idle` pasa a ser un solo frame, `PlaybackKind::kStatic` (la pose
  base/maestra) -- `NextFrameDeadlineMs()` vuelve a devolver
  `std::nullopt` para siempre en reposo, igual que Bunny.
- La secuencia completa de 25 frames se reasigna a
  `passiveActions[0]`, `PlaybackKind::kOneShot` -- disparada de forma
  esporádica por el scheduler de acciones pasivas que YA existe desde
  Block 02 (`SpikeApp::nextPassiveDeadlineMs_`,
  `pet.passiveIntervalSeconds`, `TriggerPassiveAction()`), reutilizado
  sin ningún cambio, tal como pedía el brief ("reuse, don't rebuild").
  Al terminar naturalmente, `AnimationController::Advance()` vuelve a
  Idle sola -- el mismo mecanismo que ya hace esto para
  `clickReaction`.
- Un click sigue interrumpiendo una acción pasiva en curso
  limpiamente: `TriggerClick()` ya tenía prioridad sobre
  `TriggerPassiveAction()` desde Block 02 (`ControllerState::kClickReaction`
  reemplaza a `kPassiveAction` sin más ceremonia) -- sin cambios,
  confirmado con un test nuevo dedicado
  (`ClickInterruptsPeriodicIdleAndReturnsToStaticBaseAfterward` en
  `tests/DirectionTest.cpp`).

El intervalo exacto (1/3/5 minutos) sigue siendo política de producto
no decidida en este bloque -- `pet.passiveIntervalSeconds` se dejó en
el mismo valor placeholder (300s) que ya traía la primera pasada, sin
inventar un número nuevo.

### DEC-045 — Canvas lógico genérico, independiente de la resolución nativa del arte fuente
**Status:** DECIDIDO · Block 04.2, segunda pasada — ver
`docs/NIDIR_CONTENT.md` §7. **Supersede DEC-042.**

El owner reportó el problema visual real que DEC-042 había dejado
abierto sin resolver: Nidir ocupaba una ventana visiblemente más
grande que Bunny. La causa no era arquitectónica --
`PetDefinition::canvasWidth/canvasHeight` ya gobernaba tanto el
renderizado como el hit-mask de forma totalmente independiente de la
resolución nativa de cada frame desde Block 02 -- sino el VALOR
elegido en la primera pasada (copiar 513×525 nativo 1:1 en vez de
escalarlo). Se agregó `tools/prep_dev_sprite.compute_logical_canvas_size()`,
una función genérica y reutilizable (sin ninguna rama específica de
Nidir) que deriva un canvas lógico comparable en escala al de Bunny
(160 en el lado más largo, la misma convención que Bunny ya usaba
implícitamente, ahora explícita) preservando el aspect ratio nativo.
Para Nidir: canvas lógico 156×160. Ningún PNG fuente se tocó; el
hit-mask sigue exactamente alineado con el renderizado (ambos siguen
leyendo el mismo `canvasWidth/canvasHeight`); el comportamiento
high-DPI y el switching de dirección no cambiaron. Bunny no se vio
afectado (su canvas ya era 160×160 antes de que esta función
existiera).

### DEC-046 — Downscale opcional en tiempo de compilación (`runtime_max_frame_dimension`)
**Status:** DECIDIDO · Block 04.2, segunda pasada — ver
`docs/NIDIR_CONTENT.md` §8.

El RSS medido de Nidir (~259MB, primera pasada) se investigó en vez de
descartarse como aceptable: la causa era que cada uno de los 25+ frames
se decodificaba y almacenaba en el pack compilado a su resolución
nativa completa (513×525), muy por encima de lo que un canvas lógico
de 156×160 (DEC-045), incluso a 2x Retina, puede llegar a mostrar en
pantalla. Se agregó un campo de manifest opcional,
`runtime_max_frame_dimension` (default `None` = sin cambios de
comportamiento, preserva byte-a-byte el pack de Bunny, verificado con
`cmp`), que `tools/compile_pet_pack.py` aplica de forma genérica a
CUALQUIER frame (de cualquier animación, cualquier override
direccional) cuyo lado más largo exceda el límite -- reescalado
determinista nearest-neighbor (`prep_dev_sprite.resize_rgba_nearest()`,
misma fórmula que `core::AlphaMask::FromAlphaChannel` ya usa) aplicado
solo a los bytes que van al pack compilado, nunca a los PNG fuente en
disco. Para Nidir: límite de 320px (2× el tamaño de referencia lógico
de 160, el mismo factor 2.00 de densidad de pixel que este proyecto ya
mide para Retina desde Block 01). Resultado medido: pack compilado de
~58MB a ~21.6MB; RSS en runtime de ~259MB a ~128MB (ver
`docs/PERFORMANCE_BUDGETS.md`).

### DEC-047 — Blocker de acceso a `~/Downloads`/`~/Documents`: RESUELTO en la tercera pasada
**Status:** DECIDIDO (hallazgo, no una elección de diseño) · Block
04.2 — ver `docs/NIDIR_CONTENT.md` §10 para el registro completo,
incluida la resolución.

El owner exportó la animación real de click-fire
(`~/Downloads/nidir-click-fire-right`/`-spritesheet`) durante la
segunda pasada de este bloque, pero el acceso a `~/Downloads` estuvo
denegado ("Operation not permitted") de forma consistente en más de 6
intentos, con `~/Documents` y el resto del filesystem accesibles con
normalidad en ese momento -- confirmado como un bloqueo específico de
esa carpeta, no un problema de este repositorio. Se decidió no seguir
reintentando indefinidamente y, en cambio, dejar `click_reaction` como
el placeholder existente (DEC-040) y reportar el blocker explícitamente
en vez de omitirlo o fabricar una importación.

Al retomarse la tercera pasada, el mismo tipo de bloqueo (TCC de
macOS, categoría "Documentos") había escalado temporalmente a
`~/Documents` completo vía la herramienta de shell de esta sesión
-- `git`/`cmake`/`ctest`/scripts de Python dejaron de poder listar el
propio repositorio. El owner otorgó Acceso Total al Disco a la app
Claude (Ajustes del Sistema → Privacidad y Seguridad) y reinició la
app; el acceso se restableció de inmediato, sin cambios de este
repositorio. El owner además preparó el export real en
`local_imports/nidir/` (staging local, nunca commiteado) como vía de
acceso alternativa mientras tanto. El click-fire real se importó
exitosamente en esta misma pasada -- ver DEC-048.

---

### DEC-048 — Import del click-fire real de Nidir, reemplazando el placeholder por completo
**Status:** DECIDIDO · Block 04.2, tercera pasada — ver
`docs/NIDIR_CONTENT.md` §6. **Supersede DEC-040.**

Import desde `local_imports/nidir/` (staging local del owner, ver
DEC-047), siguiendo exactamente el mismo pipeline que idle: se
inspeccionó la estructura real antes de asumir nada (el export sí
traía una carpeta anidada extra, `Nidir-a-masculine-b/`, exactamente
como el brief advertía que podía pasar); los 25 frames PNG (ya
deterministas, `frame_000.png`..`frame_024.png`) se copiaron -- nunca
movieron -- a
`assets/source/nimvlets/nidir/animations/click_reaction/right/frames/`;
el spritesheet (3120×3060, grilla 5×5 de frames de 624×612,
consistente con los 25 frames) se copió a
`.../click_reaction/right/spritesheet/spritesheet.png` como referencia
secundaria; `left` se derivó por el mismo espejado horizontal
determinista que ya usaba idle, ahora factorizado en una función
reusada por ambas animaciones (`_derive_left_direction()` en
`tools/generate_nidir_pack.py`). 25 frames reales, nativo 624×612 --
distinto de idle (513×525) porque el efecto de fuego extiende el
bounding box visible. fps derivado de la misma duración de generación
de Ludo.ai que idle (3s) -- asumiendo que el export de click-fire usó
la misma configuración, una suposición explícita y documentada, no
confirmada de forma independiente por el owner para este export
puntual. `local_imports/nidir/` se eliminó tras copiar y verificar
(checksum MD5) todo el contenido en su ubicación canónica -- el
staging ya no era necesario.

### DEC-049 — Fix: `AttachAllTextures()`/`ReleaseAllTextures()` no cubrían los overrides direccionales de click/passive
**Status:** DECIDIDO (corrección de bug) · Block 04.2, tercera pasada
— ver `docs/NIDIR_CONTENT.md` §6.

Al ejercitar el click-fire real en dirección "left" por primera vez
(la primera vez que este código path se ejercita con contenido real
no-placeholder) se encontró que `SpikeApp::AttachAllTextures()`/
`ReleaseAllTextures()` -- que adjuntan/liberan las texturas SDL de
cada frame al cargar/descargar un pet -- nunca fueron actualizadas
cuando la segunda pasada de este bloque agregó
`clickReactionDirectionOverrides`/`passiveActionDirectionOverrides`
al content model. `AnimationController` resolvía el override "left"
correctamente (`ResolveClickReaction()`/`ResolvePassiveAction()`
funcionan bien), pero sus frames nunca tenían una textura adjunta --
`RenderFrame()` los dibujaba completamente transparentes, un bug real
y silencioso (sin crash, sin error visible) donde el pet "desaparece"
durante cualquier click o idle periódico en una dirección no
canónica. El hit-mask no se veía afectado (usa `frame.pixels`
directamente, no la textura), así que el click-through seguía siendo
correcto -- solo el render era el problema. Pasó desapercibido en las
dos pasadas anteriores porque ningún smoke test de ese código
inspeccionaba pixeles reales, solo logs.

Reproducido deliberadamente (revirtiendo temporalmente el fix,
confirmando el síntoma contra el binario real, restaurando el fix)
antes de darlo por corregido -- no solo inferido de leer el código.
Corregido agregando las dos colecciones faltantes a ambas funciones.
Se agregó además un log defensivo permanente en `RenderFrame()`
(`SpikeApp.cpp`) que reporta cualquier frame con pixels reales pero
sin textura adjunta -- detecta automáticamente cualquier regresión
futura de esta misma clase de bug. `PetDefinition`
(`AnimationDefinition.h`) gana un comentario explícito advirtiendo que
cualquier colección de animaciones nueva debe actualizar esas dos
funciones. No se pudo cubrir con un test unitario en `tests/` porque
`SpikeApp` vive en el ejecutable SDL-dependiente `nimvlets_spike`, no
en ninguna librería que `nimvlets_tests` enlace -- consistente con la
convención ya establecida de mantener `tests/` completamente libre de
SDL (DEC-022); la verificación fue reproducir+corregir+re-confirmar
contra el binario real, documentado explícitamente en vez de fingir
cobertura de test que la arquitectura actual no permite.

### DEC-050 — Residencia dual de dirección (right+left) NO optimizada, a propósito
**Status:** DECIDIDO (hallazgo documentado, optimización diferida) ·
Block 04.2, tercera pasada — ver `docs/PERFORMANCE_BUDGETS.md`.

Con el fix de DEC-049, `AttachAllTextures()` ahora mantiene
correctamente residentes en memoria las texturas de AMBAS direcciones
de TODAS las animaciones de Nidir mientras el pet está activo, aunque
solo una dirección se renderiza a la vez -- esto es "unnecessary
simultaneous... directions... retained" en el sentido literal que el
brief de este bloque pide evaluar. RSS estático de Nidir subió de
~127MB (segunda pasada, click-fire aún placeholder) a ~156MB (tercera
pasada, 50 frames reales adicionales de click-fire en ambas
direcciones, correctamente atendidos). Se estima que cargar/descargar
texturas por dirección bajo demanda (en vez de las dos siempre
residentes) ahorraría del orden de ~15MB para Nidir -- una
optimización real, identificada, pero con complejidad/riesgo
desproporcionados para esta pasada de corrección puntual (requeriría
manejar el caso de una animación en reproducción activa justo cuando
cambia la dirección activa, entre otros). Se documenta como hallazgo
honesto y oportunidad real para un bloque futuro, deliberadamente no
implementada acá -- no una omisión sin examinar. La mayoría del
incremento de RSS es contenido real legítimo (50 frames reales
reemplazando un placeholder de 2 frames), no desperdicio: los frames
ya están acotados a 320px por lado (mismo `runtime_max_frame_dimension`
genérico que idle, DEC-046), sin superficies sobredimensionadas ni
datos duplicados.

---

### DEC-051 — Canvas de trabajo compartido, anclado por contenido (`normalize_visual_scale`)
**Status:** DECIDIDO · Block 04.3 — ver `docs/NIDIR_CONTENT.md` §12.

QA manual real del owner sobre Block 04.2 (todavía sin mergear a main)
encontró clipping, pérdida de calidad, y tamaño visual inconsistente
entre idle y click-fire. Investigado con evidencia de pixel, no
supuesto: (1) el "clipping" de la animación de click-fire es real y
está BAKEADO en el export original de Ludo.ai -- 18 de 25 frames
tienen contenido que toca/excede su propio borde nativo (el bounding
box unión de la secuencia completa abarca el 100% del ancho del frame)
-- este pipeline no le agrega ningún recorte adicional
(`SDL_RenderTexture` nunca recorta, siempre pasa el texture completo);
no hay forma de corregir esto sin un nuevo export con más margen, así
que se documenta como limitación real, no se inventa contenido de
reemplazo. (2) El tamaño/calidad inconsistente SÍ era un bug real de
este pipeline: cada animación se estiraba de forma independiente al
mismo canvas lógico fijo vía `SDL_RenderTexture`, y como
click_reaction (frame nativo 624×612) tiene más margen alrededor del
personaje que idle (513×525) -- necesario para el efecto de fuego --
el personaje aparecía visiblemente más chico durante click-fire.
Medido: el personaje en sí ocupa casi el mismo tamaño ABSOLUTO en
pixeles en ambas animaciones (434×498 vs. 435×498) -- el problema era
puramente de encuadre relativo, no de escala real de la ilustración.

Corrección genérica (sin ninguna rama por pet), en
`tools/prep_dev_sprite.py` (`compute_content_bbox()`,
`compose_on_canvas()`, `compute_frame_normalization_plan()`) y
`tools/compile_pet_pack.py` (campo de manifest opcional
`normalize_visual_scale`, default `false` -- verificado byte-a-byte
que el pack de Bunny no cambia): deriva, a partir de los pixeles
reales de cada animación, un `content_scale` por grupo lógico (right/
left de una misma animación SIEMPRE comparten escala) y un canvas de
trabajo COMPARTIDO por todo el pet, alineando el centro del bounding
box de contenido de cada animación al centro de ese canvas -- nunca
recorta, solo agrega margen transparente. `compute_logical_canvas_size()`
(sin cambios, DEC-045) pasa a operar sobre ese canvas de trabajo en
vez de la resolución nativa cruda de idle sola. Para Nidir: canvas de
trabajo 624×612 (dominado por click_reaction), canvas lógico final
160×157 (antes: 156×160), y — resultado directo, no buscado a
propósito — idle y click_reaction ahora compilan a EXACTAMENTE las
mismas dimensiones de runtime (320×314 ambas, antes distintas).
Verificado visualmente (frames compuestos manualmente e inspeccionados
lado a lado) antes de integrar, no solo por diseño teórico.

Trade-off documentado: el presupuesto de `runtime_max_frame_dimension`
(320px, sin cambios) ahora se reparte sobre un canvas de trabajo más
grande para idle, reduciendo su resolución compilada efectiva
(~265×304 -> ~222×255 pixeles de personaje) -- no es una pérdida real
frente a lo que la pantalla necesita (el canvas lógico final a 2x
Retina es exactamente 320×314, lo que ambas animaciones ahora ocupan
con precisión), así que no se cambió `RUNTIME_MAX_FRAME_DIMENSION`.

### DEC-052 — Dirección automática por mitad de pantalla
**Status:** DECIDIDO · Block 04.3 — ver `docs/NIDIR_CONTENT.md` §13.

Requisito nuevo del owner, explícitamente fuera de alcance del brief
original de Block 04.2 ("todavía sin ningún control de UI que lo
dispare"). `SpikeApp::UpdateDirectionFromWindowPosition()` (nueva):
compara el centro de la ventana contra el centro del display que la
contiene (`SDL_GetDisplayForWindow()`/`SDL_GetDisplayBounds()` --
funciona correctamente en multi-monitor, ya que usa los límites del
display específico, no de "la pantalla" genérica) y llama
`SetActiveDirection()` con el resultado; si no se puede determinar el
display (valor de fallo documentado 0), no toca la dirección actual --
nunca asume un lado sin evidencia. Se dispara desde `Init()`
(posicionamiento inicial), `HandleEvent()` ante `SDL_EVENT_WINDOW_MOVED`
(cubre drag en vivo Y reposicionamiento programático con un solo hook,
ya que SDL dispara ese evento ante cualquier cambio de posición sin
importar la causa), y `TrySwitchActivePet()` (el `animController_.emplace()`
de un switch reinicia la dirección a su default `kRight`, así que hay
que reaplicar la política tras cada switch para no mostrar el lado
equivocado hasta el próximo movimiento). Estable por diseño:
`SetActiveDirection()` (sin cambios) ya es un no-op si la dirección no
cambió y nunca interrumpe una animación en curso. Verificado contra el
binario real con posiciones de ventana fabricadas en ambos extremos de
la pantalla (no solo por lectura de código) -- ver el log dedicado
`screen-half direction check`. No cubierto por un test unitario en
`tests/` por la misma razón que DEC-049 (vive en el ejecutable
SDL-dependiente `nimvlets_spike`, fuera del alcance de `nimvlets_tests`).

### DEC-053 — Idle periódico de Nidir: 60 segundos
**Status:** DECIDIDO · Block 04.3 — ver `docs/NIDIR_CONTENT.md` §14.

`passive_interval_seconds` de Nidir pasa de 300s (el placeholder
explícito de Block 04.2, documentado como "no final") a 60s, por
pedido directo del owner -- ya no es un placeholder, es un valor de
producto real para este pet específico. Cambio de un solo valor en
`tools/generate_nidir_pack.py`; ningún mecanismo nuevo (el campo, su
override solo-DEV, y el scheduler que lo lee vienen sin cambios desde
Block 02/04.2). Bunny no se ve afectado (manifest independiente).

---

### DEC-054 — Doble-present + redraw de confirmación programado, corrigiendo el sprite parcial tras un cambio de dirección
**Status:** DECIDIDO (diagnóstico + mitigación, no confirmado
visualmente en este entorno) · Block 04.3, corrección post-QA — ver
`docs/NIDIR_CONTENT.md` §15.

QA manual real encontró un bug reproducible: al cruzar el punto medio
de pantalla arrastrando Nidir, el sprite estático recién mostrado a
veces aparecía parcial/corrupto; reproducir cualquier animación después
lo arreglaba. Se inspeccionó el camino completo antes de tocar código:
`AnimationController::SetDirection()` (correcto, sin cambios),
cobertura de texturas (correcta, ya arreglada en Block 04.2), y --
leyendo directamente la fuente pineada de SDL 3.4.12
(`SDL_cocoashape.m`) -- se confirmó que `SDL_SetWindowShape()` en
macOS únicamente actualiza `NSWindow.ignoresMouseEvents`, nunca
compone/recorta pixeles, descartando ese mecanismo como causa posible
por diseño (reconfirma DEC-017). Con la lógica de contenido descartada,
la explicación mejor sustentada por el patrón reportado (un present
frío incompleto, presentes repetidos lo arreglan) es una clase conocida
de problema macOS/Metal: un `SDL_RenderPresent()` tras un período largo
sin presentar (este runtime es deliberadamente event/deadline-driven,
puede estar minutos sin renderizar) puede mostrar un drawable de
`CAMetalLayer` todavía no asentado en su rotación de buffers.

Corrección genérica (aplica a cualquier pet/redraw, no solo Nidir):
`SpikeApp::RenderFrame()` presenta el mismo contenido dos veces
seguidas; un cambio de dirección que sí cambia el frame mostrado
además arma `confirmRedrawDeadlineMs_`, forzando un SEGUNDO redraw
completo ~120ms después, con separación real de wall-clock -- la
corrección no depende de que una animación futura "arregle" el estado
por casualidad (instrucción explícita del brief). Verificado
mecánicamente contra un build Debug (tres redraws reales del mismo
frame, separados en el tiempo, confirmados vía el diagnóstico `[diag]
animation redraw`) y con una ráfaga de 500 cambios de dirección
automatizados contra el binario Release real (sin errores, sin
crecimiento de RSS). **No se pudo confirmar visualmente que el bug
reportado esté resuelto** -- este entorno no tiene forma de capturar
pixeles realmente presentados en pantalla; queda pendiente de
confirmación en la próxima QA manual del owner, documentado
honestamente como limitación, no como un hecho verificado.

### DEC-055 — Tamaño visual global +5%, candidato de QA reversible
**Status:** CANDIDATO (explícitamente no una decisión de producto
cerrada) · Block 04.3, corrección post-QA — ver
`docs/NIDIR_CONTENT.md` §16.

El owner pidió evaluar todos los Nimvlets ~5% más grandes que el
tamaño ya confirmado bueno de Nidir. Implementado como
`prep_dev_sprite.DISPLAY_SIZE_SCALE_FACTOR = 1.05`, un único valor de
módulo aplicado automáticamente por `compute_logical_canvas_size()` a
cualquier pet que la use (genérico -- ninguna constante propia de
Nidir) -- multiplica el `reference_size` (160, DEC-045) antes de
derivar el canvas lógico final, después del canvas de trabajo
compartido de DEC-051, así que la relación de tamaño/encuadre relativo
entre animaciones no se ve afectada por este factor. Resultado: Nidir
160×157 -> 168×165; Bunny (importado en este mismo bloque, DEC-056)
122×160 -> 128×168. Revertir es cambiar un único valor a `1.0` y
volver a correr los `generate_<pet>_pack.py` -- ningún otro cambio de
código. El parámetro `scale_factor` de la función sigue permitiendo
pasar `1.0` explícitamente (usado por los tests que verifican la
matemática de aspect ratio en sí, independiente del candidato vigente).

### DEC-056 — Migración de Bunny a assets reales, segunda validación del pipeline genérico
**Status:** DECIDIDO · Block 04.3, corrección post-QA — ver
`docs/BUNNY_CONTENT.md`.

El owner exportó el idle y click reales de Bunny
(`local_imports/bunny/`), migrándolo del fixture sintético de Block
01/02 (`tools/generate_bunny_dev_pack.py`) a contenido real, usando el
MISMO pipeline genérico construido para Nidir sin ningún cambio de
runtime ni rama de código por pet -- la validación real de que
`normalize_visual_scale`/`compute_frame_normalization_plan()`/el
formato NVPACK1 de tres secciones son genuinamente genéricos, no solo
"funcionan para Nidir". Hallazgo real que Nidir nunca ejercitó: el
`content_scale` de Bunny's `click_reaction` NO es 1.0 (0.9586 --
Bunny sí tiene al personaje dibujado a una escala ligeramente distinta
entre idle y click dentro de sus propios frames nativos), confirmando
que la rama de reescalado de la política (no solo el posicionamiento)
funciona correctamente con datos reales por primera vez.

Detalle crítico encontrado durante la implementación: el export real
de Bunny nombra su dirección canónica "left" (a diferencia de Nidir,
"right") -- como `ResolveIdleAnimation()`/etc. siempre resuelven el
campo CANÓNICO de `PetDefinition` (sin override) para
`Direction::kRight`, wirear los frames reales "left" directamente en
el campo canónico habría sido un bug real (Bunny se vería con la
dirección equivocada en `kRight`, el default inicial). Corregido
wireando el campo canónico con los frames DERIVADOS (espejados,
"right") y el override "left" con los frames reales -- verificado
contra el binario real y visualmente (frames real/derivado exportados
e inspeccionados, confirmando que son espejos horizontales exactos).

Migración cuidadosa de identidad: `id` se mantiene `"bunny_dev"`,
`assets/dev/bunny_pack.nvpack` se mantiene el mismo path -- ningún
estado persistido ni entrada de catálogo existente se rompe.
`display_name` sí se actualizó ("Bunny (dev fixture)" -> "Bunny", solo
una etiqueta, no una clave de identidad). `tools/generate_bunny_dev_pack.py`
se conserva como artefacto histórico con una advertencia explícita
agregada: correrlo ahora sobrescribiría el pack real con contenido
sintético.

Alcance deliberadamente limitado (instrucción explícita del brief): se
integra solo lo que existe hoy (un idle, un click) -- una segunda
animación de idle, el comportamiento ponderado 70/30, y un disparador
por hover quedan diferidos a un futuro bloque de interacción, después
de que exista el arte nuevo. No se diseñó de forma especulativa.

### DEC-057 — Prioridad click > idle periódico: invariante ya garantizado, sin cambios necesarios
**Status:** DECIDIDO (verificación, no un cambio) · Block 04.3,
corrección post-QA.

El brief de esta corrección pidió verificar (sin implementar el futuro
sistema 70/30) que un idle periódico programado no pueda corromper ni
superponerse a un click en reproducción. Revisado
`content::AnimationController` (sin cambios): `TriggerPassiveAction()`
tiene un guard explícito (`if (state_ != ControllerState::kIdle)
return;`) que la vuelve un no-op completo si un click está en curso --
nunca reemplaza `currentAnimation_` en ese caso. `SpikeApp::Run()`
además chequea `State() == kIdle` ANTES de siquiera llamar
`TriggerPassiveAction()` (guard redundante, doble seguro, no dañino).
Por construcción (`currentAnimation_` es un único puntero, nunca dos
animaciones a la vez), "renderizar concurrentemente" es estructuralmente
imposible. Ya cubierto por tests existentes, sin cambios de este
bloque: `AnimationControllerTest.cpp`'s `PassiveActionNeverInterruptsClickReaction`
(un trigger de acción pasiva durante un click se ignora por completo)
y `ClickReactionInterruptsPassiveAction`/`PassiveActionPlaysAndReturnsToIdle`
(la dirección inversa, y que completar un one-shot siempre vuelve a un
estado Idle coherente). Re-confirmados pasando contra el binario
actual -- ningún cambio de `AnimationController` fue necesario ni se
hizo, siguiendo la instrucción explícita del brief de "avoid
unnecessary changes" cuando el invariante ya está garantizado.

### DEC-058 — Causa raíz real de Bunny "se ve mal": clipping de fuente (no corregible) + nearest-neighbor perdiendo detalle (sí corregible)
**Status:** DECIDIDO · Block 04.3, segunda corrección post-QA — ver
`docs/BUNNY_CONTENT.md` §3.1/§8.

Segunda ronda de QA manual: el owner confirmó que Bunny ya no pierde
partes al cambiar de dirección (DEC-054 funciona), pero sus animaciones
"se ven mal" — pixeles que parecen desaparecer durante la reproducción.
Instrucción explícita: diagnosticar la causa real, no asumir, no
"maquillar" con un hack de Bunny.

Por primera vez en este proyecto, el diagnóstico usó `screencapture`
contra la ventana real corriendo (siempre con capturas acotadas a la
región conocida de la ventana — nunca de pantalla completa, ver nota
de seguridad más abajo), comparado pixel-a-pixel contra los frames
fuente. Encontró DOS causas reales y concurrentes:

1. **Clipping genuino en el export fuente** (la punta de la oreja
   izquierda de Bunny se corta en un borde plano, `minx=0`, en
   `idle/left/frame_019` y el tramo 018-021 consecutivo; también en
   `click_reaction/left/frame_012`). Esto **corrige** la conclusión de
   DEC-056/`docs/BUNNY_CONTENT.md` §3 original ("no se identificó
   ningún hallazgo de clipping real") — esa conclusión se basó solo en
   contar cuántos frames tocan qué borde, sin renderizar y mirar
   ninguno de los frames marcados. No corregible por código; requiere
   un nuevo export con más margen si el owner quiere eliminarlo.
2. **Nearest-neighbor perdiendo detalle fino, genérico y corregible.**
   `tools/compile_pet_pack.py` usaba un solo punto de muestreo por
   pixel destino para dos downscales reales que Bunny es el primer
   caso en ejercitar con `scale < 1.0` de verdad (la normalización de
   escala por contenido, `content_scale=0.9586` en `click_reaction`, y
   `runtime_max_frame_dimension`, que siempre se dispara para Bunny
   porque su canvas de trabajo 428×563 excede 320) — compuesto DOS
   veces seguidas. Nueva función
   `prep_dev_sprite.resize_rgba_area_average()` (box filter
   determinista, RGB ponderado por alpha para evitar fringing de color
   en bordes de transparencia) reemplaza `resize_rgba_nearest` en
   ambos sitios cuando el downscale es real — verificado
   empíricamente: 35,934 → 36,125 pixeles opacos (~0.5% más contenido
   preservado) en un caso de prueba real. El hit-mask de runtime
   (`core::AlphaMask::FromAlphaChannel`) no se toca — solo bytes de
   textura del pack compilado. Genérico (sin rama por pet); beneficia
   también a Nidir (su canvas 624×612 también excede 320).

**Nota de seguridad/privacidad:** la primera captura de este
diagnóstico usó `screencapture` de pantalla completa por error y
expuso brevemente contenido privado no relacionado del usuario (una
conversación de navegador ajena a este proyecto) — el archivo se
eliminó de inmediato al notarlo, se le avisó al usuario en el momento,
y todas las capturas posteriores de esta sesión usan una región
acotada (`-R<x>,<y>,<w>,<h>`) que coincide exactamente con la ventana
propia y controlada de la app, nunca pantalla completa. Práctica
vigente para cualquier captura futura de este proyecto.

### DEC-059 — Segunda animación de idle real para Bunny y Nidir + selección ponderada 70/30 + intervalo de 10 segundos
**Status:** DECIDIDO · Block 04.3, segunda corrección post-QA — ver
`docs/BUNNY_CONTENT.md` §9, `docs/NIDIR_CONTENT.md` §19,
`docs/ANIMATION_RUNTIME.md` §2/§6.

El owner exportó una segunda animación de idle real para cada pet
("groom" para Bunny, "wing_stretch" para Nidir), entregadas vía
`local_imports/` — **instrucción explícita: usar `local_imports/` como
staging de ahora en adelante, nunca más `~/Downloads`** (el blocker
histórico de acceso a `~/Downloads`/`~/Documents` (DEC-047, §10 de
`docs/NIDIR_CONTENT.md`) hacía de esa carpeta un lugar poco práctico
de todos modos. Importadas con el mismo pipeline genérico ya
establecido (checksum MD5 contra staging, espejado determinista para
la dirección derivada, sin tocar `local_imports/` hasta verificar el
import completo).

Pedido explícito: 70% de probabilidad para la primera acción pasiva,
30% para la segunda, con un intervalo objetivo de 10 segundos para
AMBOS pets (reemplaza 300s de Bunny y 60s de Nidir, DEC-053). Se
descartó deliberadamente un ciclado round-robin estricto (lo que este
runtime hacía antes) porque no puede expresar una proporción 70/30 —
requería una selección genuinamente ponderada.

Implementado genéricamente en el modelo de contenido, no ad-hoc en
`SpikeApp`: `content::PetDefinition::passiveActionWeights` (paralelo
por índice a `passiveActions`, vacío = uniforme, DEBE calzar en
tamaño si no está vacío — validado tanto al compilar como al cargar) +
`content::ChooseWeightedPassiveActionIndex(pet, uniformRandom01)`
(pura/determinista — el llamador provee el random, nunca lo genera
ella misma, mismo patrón que `AnimationController::Advance(nowMs)`).
Formato NVPACK1 extendido con una CUARTA sección final opcional,
independiente de las tres `*_direction_overrides` existentes (Block
04.2) — ausente por completo si el manifest no define
`passive_action_weights`, byte-idéntico a cualquier pack compilado
antes de este feature. `SpikeApp` reemplaza el ciclado
`nextPassiveActionIndex_` por un `std::mt19937` sembrado una vez en
`Init()` (`passiveActionRng_`/`NextUniformRandom01()`), usado tanto
por el disparo por timer como por el disparo por hover (DEC-060).

Cobertura de test: 5 tests nuevos en `AnimationControllerTest.cpp`
(`WeightedSelection*` — límite exacto 0.7, determinismo, fallback
uniforme sin pesos, fallback uniforme por tamaño inconsistente,
clamping de input fuera de `[0,1)`).

### DEC-060 — Disparo de acción pasiva por hover, cooldown compartido con el timer ambiente
**Status:** DECIDIDO · Block 04.3, segunda corrección post-QA — ver
`docs/ANIMATION_RUNTIME.md` §8.1.

El owner pidió que reposar el cursor sobre el Nimvlet, sin hacer
click, también pueda disparar una acción pasiva — distinto del click,
sin spamear mientras el mouse queda quieto encima, con prioridad
click/drag > hover/pasiva > estática.

Diseño clave: se evaluó y descartó darle a hover su PROPIO cooldown
independiente del timer ambiente (`nextPassiveDeadlineMs_`) — un
cooldown separado permitiría que ambos caminos dispararan casi
simultáneamente (p. ej. hover dispara en t=9.9s, el timer ya tenía
pendiente t=10.0s, produciendo dos acciones pasivas casi seguidas,
justo el "spam" que el owner pidió evitar). En cambio, hover reutiliza
el MISMO `nextPassiveDeadlineMs_` que el timer — un único intervalo
real de 10s gobierna ambos caminos, cualquiera que dispare primero
reprograma el mismo deadline hacia adelante. `core::HoverPassiveGate`
(nueva clase pura, sin SDL, mismo idioma que `core::DragClassifier`)
tiene una única responsabilidad: detectar el FLANCO de subida ("el
cursor acaba de entrar"), no un hover sostenido — evita que un hover
prolongado (varios `SDL_EVENT_MOUSE_MOTION` por segundo) dispare
repetidamente. Alimentada desde los DOS call sites existentes
(`SDL_EVENT_MOUSE_MOTION` del camino de hit-test nativo, y
`PollHover()` del fallback poll-driven) vía un único helper
compartido, `SpikeApp::MaybeTriggerHoverPassiveAction()`.

Prioridad click/drag > hover garantizada por construcción, sin código
nuevo dedicado: ningún gesto en curso (`dragClassifier_.IsActive()`)
alimenta jamás el gate, y `TriggerPassiveAction()` ya es un no-op
fuera de `Idle` desde Block 02 (mismo invariante que DEC-057 ya
verificó para el timer).

Verificado contra el binario real con un mecanismo solo-DEV nuevo,
`NIMVLETS_DEV_HOVER_TEST_COUNT` (mismo patrón que
`NIMVLETS_DEV_CLICK_TEST_COUNT`): de 8 ciclos simulados de entrada/
salida, exactamente 1 disparo real — confirma que el mecanismo nunca
hace spam. 6 tests nuevos en `tests/HoverPassiveGateTest.cpp` cubren
la detección de flanco en aislamiento.

### DEC-061 — Tamaño visual global: +10% absoluto contra el baseline original, no compuesto
**Status:** DECIDIDO · Block 04.3, segunda corrección post-QA — ver
`docs/NIDIR_CONTENT.md` §18.

El candidato +5% de DEC-055 fue confirmado por el owner; pidió OTRO
+5%, con la intención explícita de terminar en +10% TOTAL respecto del
baseline original (160), no un +5% adicional compuesto
multiplicativamente sobre el +5% ya aplicado (`1.05 * 1.05 = 1.1025`,
un +10.25% que NO es lo pedido). `DISPLAY_SIZE_SCALE_FACTOR` pasa de
`1.05` a `1.10` — por construcción, ya correcto sin ningún cambio de
fórmula: `compute_logical_canvas_size()` siempre multiplica el factor
contra `REFERENCE_LOGICAL_SIZE` (160) directamente, nunca contra un
tamaño intermedio ya escalado, así que asignar `1.10` produce el +10%
absoluto correcto. Verificado con un test dedicado que compara la
constante directamente contra `1.10` y contra `1.05 * 1.05`, en vez de
solo comparar dimensiones de canvas redondeadas (que, para algunas
resoluciones, redondean igual para ambos factores). Resultado: Nidir
168×165 -> 176×173; Bunny 128×168 -> 134×176. Sigue siendo reversible
(cambiar la constante y volver a correr los `generate_<pet>_pack.py`),
mismo mecanismo que DEC-055.

### DEC-062 — Import de contenido nuevo solo desde `local_imports/`, nunca más `~/Downloads`
**Status:** DECIDIDO (preferencia del owner, vigente hacia adelante) ·
Block 04.3, segunda corrección post-QA.

El owner pidió explícitamente que todo staging de exports nuevos de
Ludo.ai use `local_imports/` (ya gitignored, ver `.git/info/exclude`)
en vez de `~/Downloads`/`~/Documents` — el blocker histórico de acceso
a esas carpetas (§10 de `docs/NIDIR_CONTENT.md`) ya las hacía
impracticas de todos modos. Sin cambio de código: el pipeline de
import (checksum MD5, copiado manual documentado, nunca mover/destruir
el staging antes de verificar) es idéntico sin importar de qué
directorio se lea — solo cambia la convención operativa hacia
adelante. Los dos imports de esta corrección (`groom` de Bunny,
`wing_stretch` de Nidir) ya siguieron esta convención.

### DEC-063 — Grafo de comportamiento por-estado ("NVPACK2"), reemplazando idle/click/passive fijo
**Status:** DECIDIDO · Block 05.

El modelo de contenido de Block 02-04.3 (`PetDefinition::idle`/
`clickReaction`/`passiveActions`, un único estado implícito por pet) no
podía expresar Frin (transición real sentado/acostado) sin ramas de
código específicas por especie — prohibido por AGENTS.md §13. Se
generalizó a `content::BehaviorState`: cada pet tiene una lista de
estados con nombre, cada uno con una pose base y tres triggers
(`ambientActions`/`hoverActions`/`clickActions`), cada acción una
`WeightedAction` con su propio `targetStateId` (a qué estado
transicionar al terminar — el MISMO estado es un self-loop, el caso de
todo pet normal; uno DISTINTO es una transición real). Un pet de un
solo estado (Bunny, Nidir) es exactamente el modelo anterior expresado
en la forma nueva — cero cambio de comportamiento, solo de
representación.

Se evaluó extender el formato "NVPACK1" de forma aditiva (como hizo
Block 04.2 con las direcciones) pero se descartó: el layout no es una
extensión, es una reestructuración real (idle/click/passive dejan de
ser campos con nombre fijo). Reinterpretar bytes viejos bajo el mismo
magic habría sido deshonesto: se bumpeó a `"NVPACK2\0"`. Ningún pack
externo/shippeado depende de NVPACK1 — todos se regeneran desde fuente
por su propio `generate_<pet>_pack.py` — así que no hay ningún costo
real de compatibilidad retroactiva que pagar.

Ver `docs/ANIMATION_RUNTIME.md` §2-§4 para el diseño completo,
`src/content/AnimationController.{h,cpp}` para la máquina de estados
genérica, y `docs/FRIN_CONTENT.md` para el primer pet real que necesita
más de un estado.

### DEC-064 — Escala visual por-pet, genérica y data-driven (`visualScale`)
**Status:** DECIDIDO · Block 05.

El owner reportó que Nidir se ve visualmente más chico que Bunny pese a
tener un canvas lógico comparable (mucho detalle fino, Nidir se percibe
"menos presente" en el escritorio). Se agregó
`content::PetDefinition::visualScale` (double, default 1.0) —
puramente runtime: `SpikeApp::EffectiveCanvasWidth()/Height()`
(`round(canvasWidth/Height * visualScale)`) gobierna ventana/render-
target-lógico/hit-mask juntos, sin tocar nunca el arte fuente ni los
bytes del pack compilado. Se evaluó y descartó retirar el mecanismo
existente `DISPLAY_SIZE_SCALE_FACTOR` (un knob global de Python,
compile-time, ya vigente en 1.10 desde Block 04.3) en favor de este
nuevo campo — hacerlo habría exigido recalcular el canvas "base" de
Bunny (actualmente ya incluye ese +10% histórico) para reproducir
exactamente su tamaño actual aprobado, con riesgo real de una
diferencia de redondeo de 1px; en cambio, `visualScale` se trata como
un multiplicador INDEPENDIENTE aplicado ENCIMA del canvas ya derivado
— Bunny queda en el default 1.0 (cero cambio, tamaño actual aprobado
preservado exactamente) y Nidir en 1.10 (candidato conservador de QA,
"+10% relativo a su resultado actual" — resultado exacto: 176×173
nativo -> 194×190 efectivo, verificado contra el binario real). Los dos
mecanismos (`DISPLAY_SIZE_SCALE_FACTOR` global de compile-time,
`visualScale` por-pet de runtime) coexisten deliberadamente, con roles
distintos y documentados — ver `docs/ANIMATION_RUNTIME.md` §11.

### DEC-065 — Cooldown de hover INDEPENDIENTE del timer ambient, supersede DEC-060
**Status:** DECIDIDO · Block 05. Supersede el diseño de DEC-060 (Block
04.3), que queda histórico — no se reescribe, se corrige acá con
evidencia nueva.

El owner especificó explícitamente para este bloque: "hover must NOT
be blocked simply because the ambient 15-second timer has not
expired". Esto es una corrección directa de DEC-060, que deliberadamente
compartía un único deadline entre hover y el timer ambient, precisamente
para evitar que ambos dispararan casi simultáneamente. Ese diseño tenía
un efecto secundario real no anticipado: si el timer ambient acababa de
disparar, hover quedaba bloqueado hasta que el intervalo completo
(ahora 15s) volviera a vencer — un cursor reposando sobre el pet
inmediatamente después de un disparo ambient no obtenía ninguna
reacción visible, contradiciendo "a real mouse/pointer entering the
visible Nimvlet must cause a visible reaction".

Corrección: `hoverCooldownUntilMs_`, un deadline propio y chico (2s,
`kHoverCooldownSeconds`), completamente independiente de
`ambientDeadlineMs_` — ninguno de los dos consulta el reloj del otro.
El riesgo original que motivó DEC-060 (dos acciones casi seguidas) se
mitiga de otra forma: el cooldown de hover es corto pero real (2s), y
como ambos caminos igual respetan `ControllerMode::kBase` antes de
disparar, cualquiera que dispare primero automáticamente bloquea al
otro hasta que la acción en curso termine — nunca hay una colisión
visual, solo la posibilidad (aceptable, y pedida por el owner) de que
ambos caminos disparen relativamente cerca en el tiempo. `core::
HoverPassiveGate` (la detección de flanco en sí) no cambió — sigue
siendo la misma clase pura de Block 04.3.

### DEC-066 — Intervalo ambient sube a 15 segundos (Bunny/Nidir), Frin usa un rest-delay propio de 45s
**Status:** DECIDIDO · Block 05.

Política de producto vigente para este bloque: "target interval is now
15 seconds" — reemplaza los 10s de la corrección post-QA de Block
04.3 (DEC-059). Aplicado vía `BehaviorState::ambientIntervalSeconds`
(ahora por-ESTADO, no un único valor por-pet — ver DEC-063), 15.0 para
el único estado de Bunny/Nidir. Frin usa 45.0 para su estado "seated"
(el "rest delay" antes de `sit_to_lie` — una transición de postura
completa, más significativa que un idle esporádico, así que un ritmo
más pausado es una elección de contenido razonable, no una medida
pedida por el owner) y NINGÚN valor efectivo en "lying" (sin
`ambientActions`, el timer nunca se arma ahí — "No random howl/
tail-greet while lying").

### DEC-067 — Renombre de identidad de catálogo `bunny_dev` -> `bunny`
**Status:** DECIDIDO · Block 05 — ver AGENTS.md §6 del brief
("content/catalog cleanup before product UI").

`bunny_dev` era el id heredado del fixture sintético de Block 01/02.
Bunny tiene arte real de producción desde Block 04.3 — mantener el
sufijo "_dev" en el identificador que terminará persistido en el save
del usuario real (y, más adelante, en datos de ownership/shop) ya no
reflejaba la realidad del contenido, y el momento de corregirlo es
ANTES de que exista UI de producto que cree datos de larga vida
alrededor de él (justo lo que este bloque pide evaluar). Se decidió
migrar: `tools/generate_bunny_pack.py`'s manifest `"id"` y
`assets/dev/pet_catalog_manifest.json`'s `pet_id` pasan a `"bunny"`.

Compatibilidad preservada sin código nuevo: `SpikeApp::Init()` ya
maneja una selección persistida que no calza con ningún id del catálogo
cayendo al default y reparando el save (`usedFallback`, mecanismo
existente desde Block 04, cubierto por
`tests/PetSwitchingTest.cpp`/`ActivePetResolutionTest.cpp`) — un save
real con `activePetId: "bunny_dev"` de antes de este bloque simplemente
no calza más, cae al default (que sigue siendo Bunny), y se repara solo
en el próximo flush. `assets/dev/bunny_pack.nvpack` (la RUTA del
archivo) NO se renombra — solo el identificador lógico interno — para
minimizar el blast radius de este cambio.

Se evaluó y se descartó, en el mismo pase, renombrar el directorio
`assets/dev/` (nombre ya inexacto — contiene arte real de producción
compilado, no solo contenido de dev) y el ejecutable `nimvlets_spike`
(sigue siendo, en los hechos, un spike/foundation runtime — no existe
todavía ninguna UI de producto real). Ambos casos: el riesgo de tocar
rutas hardcodeadas en `src/app/SpikeApp.cpp`, cada `generate_<pet>_
pack.py`, y toda la documentación que las referencia no se justificaba
frente a ningún beneficio real — ninguno de los dos nombres, a
diferencia de `bunny_dev`, va a terminar persistido en datos de usuario
reales. Documentado acá explícitamente, no una omisión no examinada.

### DEC-068 — Retiro del fixture sintético original de Bunny (Block 01/02)
**Status:** DECIDIDO · Block 05.

`tools/generate_bunny_dev_pack.py` (el generador determinista original
de Block 02, derivando 7 frames por transformaciones de pixeles
simples de `bunny_source.png`) y sus artefactos
(`assets/dev/bunny_pack/` — el manifest+frames sintéticos,
`assets/dev/bunny_source.png` — la imagen fuente de 320×320) se
eliminaron. Superseded desde Block 04.3 (Bunny tiene arte real de
producción hace tres bloques) y, tras el bump de formato de DEC-063,
genuinamente roto: ese script sigue construyendo un manifest con la
forma vieja (`idle`/`click_reaction`/`passive_actions` planos) y
`compile_pet_pack.py` ya no la acepta — fallaría con "missing required
field 'states'" si se corriera. Su propio docstring ya lo marcaba
explícitamente como "no correr, sobrescribiría contenido real" desde
Block 04.3. Valor histórico preservado en `git log`/DEC-018/DEC-023 sin
necesidad de mantener código roto corriendo — ver AGENTS.md §6 del
brief de este bloque ("remove truly obsolete generated artifacts/tools
only when they are demonstrably superseded and no longer useful").

### DEC-069 — Frin: import real macho/hembra + primer pet con transición de postura real
**Status:** DECIDIDO · Block 05 — ver `docs/FRIN_CONTENT.md` para el
detalle completo.

Frin (lobo blanco/crema) es el Nimvlet documentado históricamente como
"Tan" en `docs/PRD_V1.md` — nombre corregido en este bloque para
coincidir con el contenido real final del owner. Import real de 8
animaciones (sit_to_lie/lie_to_sit/howl/tail_greet × macho/hembra, 25
frames c/u, sin normalización necesaria) con el mismo pipeline genérico
que Nidir/Bunny ya validaron dos veces. Primer pet en ejercitar
`BehaviorState` con MÁS de un estado y transiciones reales (DEC-063) —
sentado/acostado, con un rest-delay de 45s (DEC-066) y click ponderado
70/30 (howl/tail-greet) mientras sentado. Las poses base de ambos
estados se derivan por REFERENCIA a frames ya existentes de
`sit_to_lie` (frame 0 = sentado, frame final = acostado) — nunca se
inventó ni duplicó ningún asset nuevo.

### DEC-070 — Diagnóstico de Bunny: la fuente NO está defectuosa; el hallazgo de clipping de DEC-058 se retracta parcialmente
**Status:** DECIDIDO · Block 05 — ver el informe de este bloque para
la evidencia pixel-a-pixel completa.

El owner reportó, en QA manual posterior a Block 04.3, que las
animaciones de Bunny seguían mostrando pérdida de pixeles/partes del
cuerpo durante la reproducción, y afirmó explícitamente que los PNG
fuente originales NO son defectuosos — instrucción directa de este
bloque: no asumir que sí lo son, diagnosticar con evidencia real en vez
de repetir un cambio de filtro especulativo.

Se re-midió, pixel a pixel (no solo por conteo de bounding box), el
gradiente de alpha real en las tres ubicaciones que DEC-058 (Block
04.3) había señalado como "clipping real" (oreja de idle/left/frame_019,
borde de click_reaction/left/frame_012, borde de groom/left/frame_020).
Los tres muestran una transición antialiaseada limpia de 2-3 pixeles
(p. ej. alpha 23→163→255) hacia el borde del frame nativo — consistente
con un borde NATURAL que simplemente cae cerca del límite del canvas
exportado, no con un corte plano/mid-forma. **DEC-058 se retracta
parcialmente**: la conclusión "el clipping lateral SÍ es real" no
resiste una medición de gradiente real — el error metodológico exacto
que la propia sección de DEC-058 advertía evitar (confiar en un conteo
de bounding box sin confirmar visualmente) se repitió en esa misma
corrección.

Auditado también, con evidencia: la etapa de composición/normalización
(`compose_on_canvas`/`normalize_visual_scale`) no introduce recorte
adicional (el margen del canvas de trabajo compartido es generoso en
los tres casos medidos), y el downscale de render-time (textura
compilada -> canvas lógico, `SDL_SCALEMODE_LINEAR`) pierde <0.3% de
pixeles opacos frente a un filtro de caja de referencia — no es una
fuente material de pérdida. Un intento de leer pixeles REALMENTE
renderizados (`SDL_RenderReadPixels`, ver `NIMVLETS_DEV_DUMP_FRAMES_DIR`
en `docs/ANIMATION_RUNTIME.md` §8.2) encontró que el PRIMER render de
cada sesión se lee de vuelta vacío de forma 100% reproducible — evidencia
real mitigada de forma genérica (DEC de abajo, ver §8.2), aunque no
concluyente sobre la causa exacta.

**Conclusión:** el pipeline de assets (fuente -> compilación -> carga)
queda exonerado con evidencia concreta — soporta directamente la
afirmación del owner. La causa raíz exacta de lo que el owner percibe
sigue sin confirmarse con certeza total (requiere una observación
visual interactiva real en macOS, que este entorno de agente no tiene)
— ver el informe de Block 05 para el detalle completo y la mitigación
genérica aplicada (§8.2 de `docs/ANIMATION_RUNTIME.md`).

### DEC-071 — Causa raíz real del tamaño visual inconsistente ("quieto chico, animando grande") y la corrupción de Frin al quedar acostado: bug de key-format en `_build_normalization_plan()`/`_compile_weighted_actions()`
**Status:** DECIDIDO · Block 05, pasada de corrección post-QA #2 — ver
el informe de esta pasada para la evidencia numérica completa.

QA manual del owner tras el cierre inicial de Block 05 encontró que
CUALQUIER animación disparada por ambient/hover/click (no la pose base
estática) se mostraba a un tamaño visiblemente distinto — típicamente
más grande, a veces con proporciones distorsionadas — que la pose base,
para los 4 pets reales. Para Frin específicamente, esto se percibía
como corrupción visual al llegar al estado "lying" (un salto real de
tamaño/posición en la transición `sit_to_lie -> lying`).

**Causa raíz, con evidencia:** `tools/compile_pet_pack.py`'s
`_build_normalization_plan()` (el pre-pass que decide, para
`normalize_visual_scale: true`, el content_scale/canvas de trabajo/
offset de cada entrada compilable) guardaba cada `WeightedAction`
(ambient/hover/click) bajo la clave
`f"state[{id}].{trigger}[{action_id}]"`. La pasada REAL de compilación
(`_compile_weighted_action()`) buscaba esa misma entrada bajo
`f"state[{id}].{trigger} ('{action_id}')"` — un formato de string
DISTINTO, escrito a mano por separado en el otro lugar, que nunca
coincidía con el primero. `normalization_plan.get(...)` devolvía
`None` para TODA acción de TODO pet con `normalize_visual_scale: true`
— es decir, desde que Block 05 introdujo el grafo de comportamiento
(`WeightedAction`), NINGUNA acción ambient/hover/click pasaba nunca por
`compose_on_canvas()`: cada una se compilaba a su propio encuadre/
resolución NATIVO (solo el downscale plano de
`runtime_max_frame_dimension`, sin el canvas de trabajo compartido ni
el content_scale relativo a la pose base) — exactamente el defecto de
"cada animación se estira independientemente al mismo canvas fijo" que
la política de canvas de trabajo compartido de Block 04.3 existe
específicamente para prevenir. La pose base (`base_animation`) SÍ
usaba el formato correcto (nunca pasaba por
`_compile_weighted_action()`), así que ERA la única entidad
correctamente normalizada — de ahí el patrón exacto reportado
("quieto" == la pose base, correcta; "animando" == cualquier
`WeightedAction`, incorrecta).

Medido antes de la corrección (bbox de contenido, frame 0 de cada
entrada, tras compilar): Frin macho `seated.base_animation`=196px,
`click_actions[howl]`=317px (+62%), `ambient_actions[sit_to_lie]`=288px
(+47%); Nidir `idle_breathing`=304px vs. `base_animation`=257px (+18%);
Bunny `groom`=314px vs. `base_animation`=289px (+9%). Tras la
corrección, los cuatro pets muestran `frame0_max_dim` esencialmente
idéntico entre `base_animation` y CUALQUIER `WeightedAction` (dentro de
±1px de redondeo) — ver el informe de esta pasada para la tabla
completa.

**Corrección:** `_weighted_action_context(state_id, trigger_name,
action_id)`, una única función que ambas pasadas (`add_actions()` del
pre-pass y `_compile_weighted_actions()` de la compilación real) usan
para construir la clave — hace que este tipo de bug sea
estructuralmente imposible de reintroducir (nunca dos formatos escritos
a mano en dos lugares que puedan divergir). Test de regresión de
integración nuevo,
`tools/test_asset_pipeline.py`'s `CompileWeightedActionNormalizationTest`
— compila un manifest real end-to-end y confirma que una acción
ambient termina con las MISMAS dimensiones que `base_animation`;
confirmado que este test FALLA si se reintroduce el formato viejo
(verificado deliberadamente antes de dar la corrección por buena).

**Nunca fue un problema de la fuente ni de un filtro de resample** —
ninguna de las hipótesis de diagnóstico exploradas en el cierre
anterior de Block 05 (aspect ratio, bilinear vs. box filter, primer
render "frío") causaba esto; era, en los hechos, una regresión de
integración introducida por el propio refactor a `WeightedAction` de
este bloque, nunca ejercitada por ningún test hasta ahora porque
ninguno de los tests Python anteriores compilaba un manifest real de
extremo a extremo con `normalize_visual_scale: true` sobre el nuevo
esquema de estados/acciones.

### DEC-072 — Hover: de "cooldown independiente de 2s" a "dwell continuo de 5s"
**Status:** DECIDIDO · Block 05, pasada de corrección post-QA #2 —
supersede el diseño de DEC-065 (que sigue histórico) — ver
`docs/ANIMATION_RUNTIME.md` §8.1 para el mecanismo completo.

El owner precisó el requisito de hover tras probar el resultado de
DEC-065: no alcanza con "un cooldown corto e independiente" — el
disparo segundo tras entrar sigue siendo demasiado inmediato/
sorpresivo. Requisito nuevo, más preciso: el cursor debe permanecer
CONTINUAMENTE sobre el Nimvlet durante 5 segundos reales antes de que
hover dispare algo; salir antes reinicia el contador por completo; un
click, un drag, o un cambio real de `BehaviorState` TAMBIÉN lo
reinician, incluso si el cursor nunca salió físicamente de la región.

`core::HoverDwellTracker` (nueva, reemplaza a `core::HoverPassiveGate`)
implementa esto de forma pura -- ver su propio comentario de clase y
`tests/HoverDwellTrackerTest.cpp` (9 casos). El desafío real de
integración: en el camino nativo (macOS/Linux-X11), hover solo se
alimentaba de eventos de motion reales -- un cursor perfectamente
quieto sobre el pet nunca generaría un evento nuevo para volver a
chequear el umbral de 5s. Se agregó `SpikeApp::hoverDwellDeadlineMs_`
(mismo patrón que `ambientDeadlineMs_`/`confirmRedrawDeadlineMs_`,
consultado en el cálculo de `waitMs` del loop principal) que arma un
wakeup exacto para ese momento y, al llegar, vuelve a MUESTREAR la
posición real del cursor (reusando `SampleCursor()`, la misma función
que ya usa el fallback poll-driven de Windows) en vez de asumir que
sigue encima.

### DEC-073 — `master.png` pasa a ser una copia real de `frame_000` de la pose base canónica
**Status:** DECIDIDO · Block 05, pasada de corrección post-QA #2 — ver
`prep_dev_sprite.write_master_from_canonical_frame()`.

El owner sospechó que `master.png` debería ser el primer frame real de
la pose base de cada pet. Inspeccionado antes de asumir nada: los 4
`master.png` actuales resultaron ser una ilustración "hero shot"
COMPLETAMENTE SEPARADA de los frames de animación reales (mismo diseño
de personaje, pero un render/encuadre distinto, 1254×1254, fondo
sólido) -- 3 de los 4 (Nidir, Frin macho, Frin hembra) ni siquiera
tienen canal alpha real (colortype 2, RGB puro), así que el decoder
PNG mínimo de este repo (`read_png_rgba()`, que exige colortype 6) no
puede ni abrirlos. Confirmado con evidencia: `master.png` NUNCA se lee
en el pipeline de compilación (`tools/compile_pet_pack.py` solo lee
`animations/**/frames/*.png`), así que esto nunca afectó al runtime —
pero sí es un activo de referencia inconsistente y potencialmente
confuso para cualquier proceso futuro (documentación, QA visual, un
futuro `provenance.json`) que asuma que representa el pet real.

Implementado determinísticamente en cada `generate_<pet>_pack.py` (no
un copy-paste manual): tras compilar el pack, se escribe `master.png`
como una copia decodificada+recodificada (`prep_dev_sprite.
write_master_from_canonical_frame()`, valida que el frame de origen
sea RGBA8 real antes de escribir nada) del frame 0 de la pose base
canónica del pet, en su dirección canónica real (nunca el derivado por
espejo) — Bunny: `idle/left/frame_000`; Nidir: `idle/right/frame_000`;
Frin macho: `sit_to_lie/left/frame_000`; Frin hembra:
`sit_to_lie/right/frame_000`. No afecta el pack compilado ni ningún
frame de animación — puramente una corrección del asset de referencia.

### DEC-074 — Rest-delay de Frin unificado a 15s
**Status:** DECIDIDO · Block 05, pasada de corrección post-QA #2.

DEC-066 había fijado el rest-delay de Frin ("seated" -> `sit_to_lie`)
en 45s, una elección de contenido propia y explícitamente no confirmada
por el owner. El owner pidió, en esta pasada, unificar el "tiempo base
de las animaciones pasivas" a 15 segundos — se interpreta esto como
aplicando también al rest-delay de Frin (mismo mecanismo genérico,
`BehaviorState::ambientIntervalSeconds`), no solo al intervalo ambient
de Bunny/Nidir que ya estaba en 15s. `tools/generate_frin_pack.py`'s
`REST_DELAY_SECONDS` pasa de 45.0 a 15.0 — trivial de diferenciar de
nuevo más adelante si el owner pide un ritmo distinto para la
transición de postura específicamente.

### DEC-075 — Transforma canónica POR ESTADO: por qué comparar bounding boxes entre estados de silueta distinta es matemáticamente inválido, y la corrección real
**Status:** DECIDIDO · Block 05, TERCERA pasada de corrección post-QA
(la QA manual del owner encontró que DEC-071 mejoró el salto de
tamaño pero no lo eliminó) — ver el informe de este bloque para la
evidencia numérica y visual completa.

**QA manual del owner tras DEC-071:** Bunny seguía perdiendo
pixeles/partes del cuerpo en casi todas sus animaciones (excepto una
pasiva); TODOS los pets mostraban una diferencia sutil de escala entre
la pose estática y la animada; Frin macho/hembra seguían
perdiendo/corrompiendo partes al hacer sit-to-lie/lying/lie-to-sit; y
Frin se sentía demasiado chico en su forma estática. El owner aportó
evidencia nueva explícita: exports reales de Bunny de la MISMA pose
conceptual (idle/click_reaction/groom) tienen dimensiones de contenido
visible MATERIALMENTE distintas — un export de Ludo.ai no garantiza la
misma escala de personaje entre secuencias distintas, ni siquiera
cuando el primer frame representa la misma pose.

**Verificación real, no asumida:** se generó una comparación
bottom-aligned, a escala nativa 1:1 (sin ningún resize), de
idle/click_reaction/groom de Bunny — confirmó visualmente que groom
(y, en menor medida, click_reaction) están renderizados a una escala
de personaje REAL Y GENUINAMENTE MÁS GRANDE que idle dentro de su
propio export nativo, no un artefacto de medición. La corrección de
DEC-071 (content_scale por grupo, comparado contra la pose de
referencia) mide esto correctamente CUANDO todos los grupos comparten
la misma orientación de silueta (el caso de Bunny/Nidir: todas las
poses son "sentado", solo cambia cuánto margen nativo rodea al
personaje) — confirmado con los mismos números exactos que antes
(groom content_scale=0.8642, click_reaction=0.9586), sin cambios.

**El bug real remanente:** `compute_frame_normalization_plan()` medía
el "tamaño" de un grupo como el LADO MÁS LARGO de su bounding box de
contenido. Eso asume que todos los grupos comparten orientación de
silueta — cierto para Bunny/Nidir, FALSO entre `BehaviorState`s con
posturas genuinamente distintas: Frin "seated" (alto y angosto, el
lado más largo es la ALTURA) vs. "lying" (bajo y ancho, el lado más
largo es el ANCHO). Comparar "lado más largo" entre los dos compara
EJES DISTINTOS y produce una escala sin sentido — medido en el pack
real: `state[lying].base_animation` compilaba con content_scale=1.31x
(macho)/1.08x (hembra), y `lie_to_sit` con 1.52x (macho)/1.11x
(hembra) — inflación real y visible, exactamente "lying pose gets
scaled differently" que QA reportó. Como consecuencia indirecta, el
canvas de trabajo compartido de Frin también se inflaba de más para
darle espacio a esa "lying" artificialmente agrandada (macho: 689x968
→ 543x815 tras la corrección; hembra: 534x683 → 496x653) — canvas más
chico y ajustado, no una regresión.

Adicionalmente, un chequeo frame-por-frame (no solo frame 0) encontró
que `lie_to_sit` llegaba a exceder por una fracción de pixel (~0.1-
0.2px) el canvas de trabajo derivado SOLO del frame 0 — el canvas
nunca garantizaba que TODOS los frames de una animación (no solo el
primero) entraran sin recorte. `compose_on_canvas()` recorta en
silencio si esto pasa (documentado, nunca falla) — un riesgo real de
"corrupción silenciosa" para cualquier animación futura cuya pose se
extienda más lejos del ancla en un frame intermedio que en el frame 0.

**La corrección real (genérica, sin ninguna rama por pet):** la
pregunta correcta no es "¿cómo comparo mejor dos bounding boxes?" —
es "¿cuándo NO debo comparar bounding boxes en absoluto?". Este
proyecto ya exige el contrato first/last-frame (Block 04.2): el primer
frame de una animación es la pose de reposo, el último es la pose
final. Para Frin, eso significa que `state[seated].base_animation` Y
`state[lying].base_animation` son, LITERALMENTE, el mismo archivo que
el frame 0 y el frame final de `sit_to_lie` respectivamente — no son
dos mediciones independientes que reconciliar, son EL MISMO personaje
en el mismo frame. `tools/prep_dev_sprite.compute_frame_normalization_plan()`
ahora recibe `group_frame_paths` (todas las rutas de frame de cada
grupo, no solo la primera) y aplica un **union-find de archivo
compartido**: dos grupos que comparten CUALQUIER archivo de frame real
se fusionan en un único "scale_group" con una única escala resuelta,
POR CONSTRUCCIÓN — nunca por una nueva comparación de pixeles. Para
una acción sin ese vínculo (p. ej. `lie_to_sit`, un export de reversa
genuinamente distinto), la comparación válida es contra el
`base_animation` de SU PROPIO estado (`base_group_of_state`/
`state_of_group`, nuevos parámetros) — misma orientación de silueta
por construcción, en vez del estado de referencia global del pet (otra
orientación). Resultado medido: `state[lying].base_animation` pasa a
content_scale=1.0000 EXACTO para ambas variantes (idéntico a
`seated`/`sit_to_lie`, garantizando "lying es exactamente el frame
final transformado de sit_to_lie"); `lie_to_sit` pasa de 1.52x/1.11x a
1.16x/1.03x — una corrección real y mucho más modesta, validada
comparando su propia pose "lying" de inicio contra la de
`sit_to_lie` (misma orientación), no contra "seated".

Bunny/Nidir (un solo `BehaviorState` cada uno) son matemáticamente
idénticos al comportamiento anterior por construcción — el
union-find nunca fusiona nada porque sus grupos nunca comparten
archivo, y "el base_animation de su propio estado" ES el único estado
que ya existe. Verificado: los packs recompilados de Bunny/Nidir
producen exactamente los mismos content_scale que antes de este
cambio.

**Salvaguarda adicional (evita que esta clase de bug pueda volver a
corromper contenido en silencio):** `tools/compile_pet_pack.py`'s
`_compile_frame()` ahora verifica, para CADA frame de CADA entrada (no
solo el frame 0 usado para calibrar), que su bounding box de contenido
real, ya escalado y posicionado, entre dentro del canvas de trabajo
final — si no, `PackCompileError` en vez de un recorte silencioso vía
`compose_on_canvas()`. Esto convierte "ningún frame real pierde
contenido" en un invariante que CUALQUIER regeneración futura de
CUALQUIER pet verifica automáticamente, no solo una promesa de
documentación.

**Downscale de dos pasadas -> una sola pasada combinada (causa parcial
del "pixel loss" remanente de Bunny):** `_compile_frame()` aplicaba el
`content_scale` de normalización y el downscale de
`runtime_max_frame_dimension` como dos llamadas SEPARADAS y
SECUENCIALES a `resize_rgba_area_average` (un box-filter cada una).
Dos box-filters encadenados para el MISMO factor de reescalado neto
pierden más detalle fino que uno solo aplicado directamente desde la
resolución nativa — medido en este bloque sobre un frame real de
groom de Bunny: ~1.75% de los pixeles de alpha difieren visiblemente
(>10/255) entre ambos caminos, con un ablandamiento de contorno
perceptible en el de dos pasadas al comparar los PNG resultantes lado
a lado. Corregido: ambos factores se combinan en un único
`combined_scale` (`content_scale * runtime_ratio`), un único resize
directo desde los pixeles nativos. No cambia ninguna geometría/tamaño
resultante — solo reduce la pérdida de detalle acumulada. Este es
probablemente solo una PARTE del "pixel loss" reportado por el owner
para Bunny — ver el informe de este bloque, sección de limitaciones,
para lo que NO se pudo confirmar con evidencia visual directa pese a
captura de pantalla real del binario corriendo.

Tests de regresión nuevos (`tools/test_asset_pipeline.py`):
`MultiStateNormalizationPlanTest` (la función pura, con un
BehaviorState sintético de 2 estados) y
`CompileTwoStateNormalizationTest` (integración end-to-end: un
manifest real de 2 estados compilado a un `.nvpack` real, verificando
que `lying.base_animation` compila pixel-idéntico al frame final de
`sit_to_lie`).

### DEC-076 — Retuning de `visual_scale`: Nidir a 1.25, Frin (ambas variantes) a 1.30
**Status:** DECIDIDO · Block 05, tercera pasada de corrección post-QA.

QA manual del owner: Nidir seguía sintiéndose más chico que Bunny pese
al +10% ya aplicado (DEC-por-visual_scale de la pasada anterior); Frin
macho/hembra se sienten claramente chicos, deberían tener una
presencia de escritorio comparable a Bunny/Nidir. Medido con evidencia
real (no solo el tamaño del canvas transparente, que incluye margen):
el bounding box de contenido visible del pack COMPILADO, como fracción
de su propio frame, multiplicado por `canvas_width/height *
visual_scale` — la "presencia visible efectiva" real en puntos.

| Pet | visual_scale antes | Efectivo antes | visual_scale ahora | Efectivo ahora |
|---|---|---|---|---|
| Bunny (referencia, sin cambio) | 1.0 | ~114x159pt | 1.0 | ~114x159pt |
| Nidir | 1.10 | ~136x155pt (¡más bajo que Bunny!) | 1.25 | ~154x176pt |
| Frin macho | 1.0 | ~85x128pt | 1.30 | ~110x166pt |
| Frin hembra | 1.0 | ~91x138pt | 1.30 | ~118x179pt |

A 1.10, la ALTURA efectiva de Nidir (155pt) quedaba por debajo de la
de Bunny (159pt) pese a ser más ancho — consistente con "sigue
sintiéndose más chico" pese al ajuste anterior. Un único valor de
`visual_scale` para AMBAS variantes de Frin (macho/hembra) — mismo
personaje, misma presencia esperada, sin importar el género. Puramente
runtime (`content::PetDefinition::visualScale`, aplicado solo en
`SpikeApp::EffectiveCanvasWidth()/Height()`) — el arte fuente y el
pack compilado no cambian. Candidatos de QA, no cifras definitivas del
owner — triviales de re-ajustar (un único número por pet en cada
`generate_<pet>_pack.py`) tras la próxima ronda de QA manual real.

### DEC-077 — Hover: dwell de 5s a 1s
**Status:** DECIDIDO · Block 05, tercera pasada de corrección post-QA.

El owner pidió explícitamente bajar el umbral de dwell continuo de
hover (ver DEC-072) de 5 a 1 segundo — "current 5-second dwell is too
long". El MECANISMO no cambia (dwell continuo, reset en salida/click/
drag/cambio de estado, desacoplado del timer ambient, wakeup explícito
para un cursor perfectamente quieto) — solo `SpikeApp::
kHoverDwellSeconds` (5.0 -> 1.0), un único valor.

### DEC-078 — El timer ambient se reinicia en toda interacción real, no solo en cambios de estado
**Status:** DECIDIDO · Block 05, tercera pasada de corrección post-QA.

Antes de esta pasada, `RearmAmbientDeadline()` solo se llamaba al
cargar/cambiar de pet y al detectar una transición de `BehaviorState`
real — un click o un hover completo interrumpían la animación en
curso, pero el conteo de ~15s hacia el PRÓXIMO ambient seguía
corriendo desde donde estaba antes de la interacción, así que un
ambient podía dispararse casi inmediatamente después de que el owner
acababa de interactuar con el pet — "the pet should not perform an
ambient action immediately after the owner just interacted with it"
(owner). Se agregan llamadas a `RearmAmbientDeadline(nowMs)` en: el
cursor entrando a la región interactiva (un dwell de hover nuevo
arrancando, incluso si nunca llega a disparar), un disparo de hover
completo, un click, el inicio de un drag, el fin de un drag, y un
cambio de dirección real (incluyendo el causado por un drag que cruza
la mitad de pantalla). El intervalo en sí sigue en 15s (ver DEC-066/
074) — lo único nuevo es CUÁNDO se reinicia el conteo. Un ambient cuyo
deadline cae durante un click/hover/drag en curso sigue sin
solaparse/encolarse (comportamiento ya existente desde antes de este
bloque: `TriggerAmbientAction()` es un no-op fuera de `kBase`, pero el
deadline SIEMPRE se reprograma) — no cambia. Prioridad sin cambios:
click > hover/ambient > estático (`ClickInterruptsAmbientAction`/
`AmbientActionNeverInterruptsClick` ya existentes, más 4 tests nuevos
en `tests/AnimationControllerTest.cpp` que completan la matriz
click<->hover y ambient<->hover: `HoverActionNeverInterruptsClick`,
`ClickInterruptsHoverAction`,
`AmbientActionNeverInterruptsAnInProgressHoverAction`,
`HoverActionNeverInterruptsAnInProgressAmbientAction`).

### DEC-079 — El contador ambient se reinicia al TERMINAR una acción, no al empezar la interacción
**Status:** DECIDIDO · Block 05, pasada de estabilización.

DEC-078 ya reiniciaba `ambientDeadlineMs_` en cada interacción real
(click, drag, entrada de hover, cambio de dirección). Faltaba la otra
mitad: el reinicio ocurría al EMPEZAR la interacción, así que la
DURACIÓN de la animación resultante se comía parte del intervalo. Un
click a T reiniciaba el contador a T+15s, la animación corría ~3s, y al
volver a la pose base quedaban solo ~12s — visible de inmediato con
`NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS=2`, donde la pasiva llegaba
prácticamente pegada al final del click.

El disparador obvio ("¿cambió `CurrentStateId()`?") NO sirve: la mayoría
de las acciones son self-loops (el click de Bunny/Nidir, howl/tail_greet
de Frin) que terminan en el MISMO estado en que empezaron, así que el id
nunca cambia y la terminación era invisible para `SpikeApp`.

`content::AnimationController` ahora reporta la terminación
explícitamente vía `ActionCompletedDuringLastAdvance()`: un
`std::optional<double>` recalculado en CADA `Advance()`, que devuelve el
instante EXACTO de terminación del contenido (`currentFrameStartMs_`
acumulado) y no el `nowMs` del llamador — si el loop despertó tarde, el
intervalo igual se cuenta desde la terminación real. `SpikeApp::Run()`
lo consume y llama `RearmAmbientDeadline(*completedMs)`. Cubre por el
mismo camino las dos clases de terminación (self-loop y cambio de
estado real), así que el bloque de `lastKnownStateId_` queda solo para
resetear el dwell de hover.

Verificado contra el binario real (`NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS=2`):
click termina en t=3901ms, la pasiva dispara en t=5901ms — 2000ms
exactos desde la TERMINACIÓN. Segundo ciclo: 8902ms -> 10901ms.

### DEC-080 — DRAG es la interacción de máxima prioridad: cancela la acción en curso y arrastra la pose base
**Status:** DECIDIDO · Block 05, pasada de estabilización.

Evidencia del owner: arrastrar a Nidir MIENTRAS una animación se
reproduce produce corrupción visual, que un redraw posterior repara.
Diagnóstico: durante un drag, `SDL_SetWindowPosition()` movía la
ventana en el mismo instante en que el loop presentaba frames nuevos
(y, si el arrastre cruzaba la mitad de pantalla,
`SDL_EVENT_WINDOW_MOVED` -> `UpdateDirectionFromWindowPosition()` ->
cambio de dirección -> otro redraw). Mover una ventana transparente,
per-pixel-shaped y always-on-top mientras se presenta contenido nuevo
es exactamente el escenario donde el compositor puede mostrar contenido
inconsistente.

La respuesta NO es bloquear el arrastre durante ~3s (haría el pet
inmovible justo cuando el owner quiere moverlo). Prioridad establecida:
**DRAG > CLICK > HOVER/AMBIENT > BASE**. Cuando un gesto califica como
drag real (cruza el umbral de `core::DragClassifier`):

- se cancela la acción one-shot en curso vía
  `AnimationController::CancelActionToCurrentState()`, que vuelve a la
  pose base del estado **ACTUAL** — nunca al `targetStateId` pendiente:
  una transición abortada a mitad de camino nunca completó. Para Frin
  eso da exactamente lo pedido sin ninguna rama por pet: `sit_to_lie`
  interrumpido deja `seated`, `lie_to_sit` interrumpido deja `lying`,
  howl/tail_greet interrumpidos dejan `seated`, y ya-`lying` en base
  es un no-op;
- no avanza ningún frame mientras el drag está activo (el loop saltea
  `Advance()`; además la pose base es `kStatic`, así que
  `NextFrameDeadlineMs()` es `nullopt` y el loop ni se despierta);
- no se resuelve dirección durante el arrastre — se resuelve UNA vez al
  soltar, contra la posición final, junto con un reset limpio del dwell
  de hover y un reinicio completo del contador ambient.

Resultado: durante un arrastre hay CERO presentaciones de contenido
nuevo, así que el escenario de corrupción reportado deja de existir por
construcción. 10 tests con timestamps fabricados cubren la matriz.

### DEC-081 — UNA textura reutilizable por pet en vez de una SDL_Texture por frame
**Status:** DECIDIDO · Block 05, pasada de estabilización — adoptado
tras un A/B medido, no por preferencia arquitectónica.

El modelo de Block 02 (`graphics::FrameTexture`) creaba y retenía una
`SDL_Texture` por CADA frame de CADA animación/dirección/estado del pet
activo — 152 texturas para Bunny/Nidir, 204 para cada Frin — y cada
avance de frame cambiaba el `SDL_Texture*` que se dibujaba.

**Precondición verificada primero** (no asumida) contra los 4 packs
reales: todos los frames de un pet compilado comparten EXACTAMENTE las
mismas dimensiones (Nidir 320x314 x152, Bunny 243x320 x152, Frin macho
213x320 x204, hembra 243x320 x204) — consecuencia directa de que
`compose_on_canvas()` los pone a todos sobre el mismo canvas de trabajo
compartido. Una sola textura reutilizable es viable.

`graphics::ActiveFrameTexture`: una única textura
`SDL_TEXTUREACCESS_STREAMING` RGBA32, actualizada en el lugar con
`SDL_UpdateTexture` solo cuando el frame mostrado cambia, con
`SDL_BLENDMODE_BLEND` y `SDL_SCALEMODE_LINEAR` fijados EXPLÍCITAMENTE
(el camino viejo dependía del default implícito de
`SDL_CreateTextureFromSurface`, que resulta ser el mismo — ver
`SDL_render.c:1545` — pero por accidente, no por contrato).

**A/B medido, ambos caminos en el mismo binario** (vía un
`NIMVLETS_DEV_RENDERER` temporal, retirado al adoptar):

- *Equivalencia visual:* volcados de `SDL_RenderReadPixels` de Nidir en
  los dos caminos, alineados por el offset de 1 frame entre dos
  procesos independientes: **23/27 pares byte-idénticos**; los 4
  restantes son el primer frame en blanco y los bordes donde la cadencia
  de present difiere entre procesos. Equivalencia confirmada.
- *Residencia (Release, RSS estable):* Bunny 217.9->166.3 MB (**-23.7%**),
  Nidir 258.1->194.5 MB (**-24.6%**), Frin macho 252.8->183.4 MB
  (**-27.4%**), Frin hembra 267.1->197.7 MB (**-26.0%**).
- *Invariante de recursos:* 50 redraws (5 clicks + 6 cambios de
  dirección + animaciones) crean **1** textura; 4 switches de pet crean
  2 en total (creación perezosa: un pet que nunca se dibuja nunca
  reserva textura).

Esto implementa la optimización #2 que `docs/PERFORMANCE_BUDGETS.md`
había documentado como pendiente. El camino por-frame se retiró
completo (`graphics/FrameTexture.{h,cpp}`,
`SpikeApp::AttachAllTextures()/ReleaseAllTextures()`,
`content::FrameDefinition::rendererHandle`) — el brief pedía
explícitamente no dejar un refactor especulativo conviviendo con el
camino viejo.

**Honesto sobre el criterio que NO se pudo probar:** el A/B no
demuestra que este cambio elimine la corrupción visual que el owner
reporta — eso requiere QA interactiva real. Se adoptó por los dos
criterios que SÍ quedaron probados (equivalencia visual byte a byte y
-24/-27% de residencia), más el hecho de que elimina estructuralmente
el swap de objeto-textura por frame. Si la corrupción persiste tras
esta pasada, este camino no es la causa y el diagnóstico sigue.

### DEC-082 — "No screen capture" es un contrato de PRODUCTO, no una prohibición de herramientas de desarrollo
**Status:** DECIDIDO · Block 05, pasada de estabilización — aclaración
de redacción en `AGENTS.md` §5, sin debilitar nada.

La viñeta "No screen capture" de `AGENTS.md` §5 vive en una lista sobre
lo que hace el RUNTIME, pero leída aislada por un agente hacía que éste
se auto-prohibiera tomar capturas de diagnóstico de nuestra propia
ventana durante QA. Eso costó tiempo real en este bloque: los defectos
visuales de una ventana transparente, per-pixel-shaped y always-on-top
muchas veces no son reproducibles de otra forma.

Se agregó una viñeta que fija el alcance explícitamente: el producto
nunca captura pantalla (contrato permanente, sin cambios); un
desarrollador —o un agente trabajando para el owner, con su
consentimiento— SÍ puede tomar capturas enfocadas de nuestra propia
ventana como diagnóstico de desarrollo. Nunca se envían con el
producto, nunca se automatizan dentro de él, nunca se guardan en el
repo. El contrato de privacidad real no se debilita.

### DEC-083 — macOS: driver de renderer por default pasa a "software" — evidencia de QA real, no preferencia arquitectónica
**Status:** DECIDIDO · Block 05, pasada de resolución de renderer.

QA manual real, interactiva, en la máquina macOS del owner, aisló el
bug de larga data de "pixeles/partes del cuerpo que desaparecen
durante animaciones" (Bunny; Frin en sit-to-lie/lying/lie-to-sit) a la
etapa de PRESENTACIÓN, no al pipeline de contenido: **el driver de
software de SDL renderiza los MISMOS assets correctamente; los drivers
acelerados que macOS elige por default (Metal/OpenGL) no.** Nidir --
el control ya establecido en pasadas anteriores -- se ve bien en
ambos. Esto NO es "el software siempre es más seguro que lo
acelerado" como afirmación general -- es una discrepancia real y
reproducible de un driver acelerado específico contra assets
específicos, en esta máquina, medida por el owner mismo.

Como consecuencia directa, esta pasada NO tocó ningún PNG fuente,
frame derivado left/right, `content_scale`, canvas de trabajo
compartido, ni `master.png` -- el pipeline de assets/contenido queda
exonerado por segunda vez (la primera fue DEC-070/075, que ya había
descartado el pipeline de compilación; ahora se descarta también la
GEOMETRÍA como explicación, porque el mismo binario compilado, con el
mismo pack, se ve distinto según el driver de PRESENTACIÓN).

**Política implementada** (`src/platform/RendererPolicy.h/.cpp`, lógica
pura sin SDL, mismo patrón que `LinuxBackendPolicy.h` -- ver
`tests/RendererPolicyTest.cpp`, 7 tests): `SpikeApp::Init()` pide
`SDL_CreateRenderer(window_, driverName)` con
`driverName = PreferredRendererDriverName(CurrentRendererPlatform(),
devOverride)`. Default por plataforma: macOS -> `"software"`
(`SDL_SOFTWARE_RENDERER`, confirmado string literal contra la fuente
SDL 3.4.12 pineada -- `src/render/software/SDL_render_sw.c`); Windows/
Linux -> `nullptr` (sin cambio, nunca se demostró el mismo bug ahí).
`platform::CurrentRendererPlatform()` es un hecho de compilación,
implementado una vez por cada
`src/platform/{macos,windows,linux}/TransparentWindowSupport.*` --
mismo patrón exacto que `NativeShapeHitTestIsRenderSafe()` ya usaba,
así que `SpikeApp` nunca necesita su propio `#ifdef` de plataforma
(AGENTS.md §3).

**Override DEV preservado**: `NIMVLETS_DEV_RENDERER_DRIVER` (cualquier
nombre de driver -- "metal", "opengl", "software", lo que sea) gana
sobre el default de CUALQUIER plataforma, sin recompilar -- verificado
contra el binario real: default sin la variable selecciona
"software"; con la variable en "opengl"/"metal" selecciona
exactamente eso. Fallback documentado y no-silencioso: si el driver
pedido (preferido o DEV override) falla al crear el renderer,
`SpikeApp::Init()` loguea el fallo específico y reintenta con
`SDL_CreateRenderer(window_, nullptr)` (el default de SDL) en vez de
abortar el arranque -- verificado con un nombre de driver inexistente.
El driver REALMENTE seleccionado (`SDL_GetRendererName()`, no solo lo
pedido) se loguea siempre al arrancar.

**Esto es una política de corrección, no una afirmación de que el
renderizado acelerado nunca podrá funcionar** -- si una investigación
futura confirma y corrige la causa exacta en el driver acelerado (o
una versión más nueva de SDL la resuelve), esta política es un único
punto de cambio (`PreferredRendererDriverName()`), no una reescritura.

Ver el informe de esta pasada para el A/B visual completo (Nidir/
Bunny/Frin bajo software, capturas reales) y las mediciones de
CPU/RSS bajo software.

### DEC-084 — Retuning de timing: hover dwell a 0.5s, intervalo ambient a 12s
**Status:** DECIDIDO · Block 05, pasada de resolución de renderer --
pedido de producto explícito del owner, no derivado de ningún
diagnóstico.

- `SpikeApp::kHoverDwellSeconds`: 1.0 -> 0.5 (historial completo: 5.0
  -> 1.0 -> 0.5, cada pasada un pedido de producto más preciso). El
  MECANISMO (`core::HoverDwellTracker`, reset en salida/click/drag/
  cambio de estado, deadline propio en el loop principal para el caso
  del cursor perfectamente quieto) no cambia -- solo este único valor.
- `ambient_interval_seconds`/`REST_DELAY_SECONDS`: 15.0 -> 12.0 en los
  tres generadores (`tools/generate_bunny_pack.py`,
  `generate_nidir_pack.py`, `generate_frin_pack.py`) -- un único
  intervalo de producto para los cuatro pets, sin excepción. El
  mecanismo de reinicio en interacción real (DEC-078/079: el conteo
  arranca de nuevo en click/drag/hover-completo/cambio de dirección Y
  al TERMINAR una acción, no solo al empezar la interacción) tampoco
  cambia -- solo el número base.

Ambos son datos de contenido/runtime puros (nunca geometría de
assets), así que no entran en conflicto con el alcance de DEC-083 (no
tocar el pipeline de assets para compensar el bug de renderer). Los
cuatro packs se recompilaron para llevar el nuevo
`ambient_interval_seconds` -- verificado que el tamaño en bytes de
cada `.nvpack` no cambia (es metadata de comportamiento, no geometría
de frame).

### DEC-085 — El A/B prometido en DEC-083 encontró un SEGUNDO bug: SDL_SetWindowShape() corrompe el software renderer en macOS; fallback a click-through poll-driven
**Status:** DECIDIDO · Block 05, misma pasada de resolución de renderer
· **CAUSA RAÍZ CORREGIDA POR DEC-086** — el HECHO observable de esta
entrada (una forma instalada corrompe el render bajo el driver
"software", y por lo tanto la ruta de shape no se puede usar ahí) sigue
siendo cierto y sigue vigente. Lo que estaba MAL era la explicación:
esta entrada culpaba a la parte genérica de `SDL_video.c` y afirmaba
que la corrupción era PERMANENTE. Las dos cosas se midieron y son
falsas — ver DEC-086. Se conserva el texto original tal cual, sin
reescribir, para que quede el registro de cómo se llegó hasta acá
-- este es el seguimiento directo de la validación que DEC-083 dejó
pendiente ("Ver el informe de esta pasada para el A/B visual
completo"), no una pasada nueva.

Al ejecutar esa validación en vivo (Nidir bajo el nuevo default
"software" de DEC-083) apareció un bug DISTINTO y más severo que el
que DEC-083 resolvía: cada frame renderizado DESPUÉS del primero se
presentaba como una silueta blanca opaca sólida -- confirmado primero
con una captura de pantalla real, y después con evidencia mucho más
rigurosa (volcado del backbuffer real vía `SDL_RenderReadPixels`,
mecanismo `NIMVLETS_DEV_DUMP_FRAMES_DIR` ya existente): el frame 0
(el render directo de `Init()`) salía correcto; los 27 frames
siguientes de una animación completa de 28 frames -- cada uno con un
`FrameDefinition*` distinto, cada uno con su propio
`SDL_UpdateTexture` fresco -- salían blancos, sin ninguna falla de API
reportada (`SDL_UpdateTexture` siempre devolvía éxito).

Se descartó la hipótesis inicial (el modelo de `ActiveFrameTexture` de
una sola textura reutilizada, actualizada in-place, vs. recrear una
textura nueva por cambio de frame): un cambio de implementación de
prueba a "destruir y recrear vía `SDL_CreateTextureFromSurface` en
cada cambio de frame" NO cambió el resultado -- el frame 1 (idéntica
textura reutilizada del frame 0, sin ningún re-upload de por medio, en
el caso estático) seguía saliendo blanco. Esto probó que el bug no
vive en `ActiveFrameTexture` en absoluto.

La causa real se aisló con un programa SDL3 standalone mínimo
(~50 líneas, cero código de esta app, misma ventana transparente/
borderless/utility que usa SpikeApp): **una sola llamada a
`SDL_SetWindowShape()` corrompe permanentemente todo
`SDL_RenderPresent()` posterior contra el driver "software"**, sin
importar el orden relativo al render ni si se llama una vez o en cada
frame. Un segundo repro standalone acotó la causa aún más: togglear
`NSWindow.ignoresMouseEvents` DIRECTAMENTE (el mecanismo exacto de
`Cocoa_UpdateWindowShape()`, y el mismo que ya usa
`SetWindowClickThrough()`) NO corrompió nada en 4 ciclos consecutivos
contra el mismo renderer -- así que la ruptura no está en el efecto
documentado de `Cocoa_UpdateWindowShape()` (que sigue siendo cierto:
solo toca `ignoresMouseEvents`), sino en la parte genérica de
`SDL_video.c`'s `SDL_SetWindowShape()` (conversión/almacenamiento de
la superficie de forma) que corre antes de llegar a Cocoa.

Esto significa que el mecanismo de click-through PRIMARIO de esta app
en macOS (`SDL_SetWindowShape`, ver
`NativeShapeHitTestIsRenderSafe()`) es incompatible con el nuevo
default "software" de DEC-083 -- usarlo habría dejado la app
visualmente inutilizable (blanco sólido permanente) apenas ocurriera
la primera actualización de hit-mask, que pasa en el primer frame.

**Fix implementado**: `NativeShapeHitTestIsRenderSafe()` y
`ClickThroughPollingIsMeaningful()` (`src/platform/TransparentWindowSupport.h`
y las tres implementaciones por plataforma) ahora reciben un parámetro
`usingSoftwareRenderer` -- un hecho de RUNTIME (qué driver
efectivamente eligió SDL, `SDL_GetRendererName(renderer_) ==
SDL_SOFTWARE_RENDERER`), no de compilación, calculado una vez en
`SpikeApp::Init()` justo después de crear el renderer. En macOS:
`NativeShapeHitTestIsRenderSafe(true) == false` (el shape ya no es
seguro con software) y `ClickThroughPollingIsMeaningful(true) == true`
(SpikeApp cae al fallback poll-driven existente,
`SetWindowClickThrough()`, ya confirmado seguro contra el software
renderer por el segundo repro standalone). Con el driver acelerado
histórico, ambas funciones siguen devolviendo exactamente lo mismo que
antes de este bloque (`true`/`false` respectivamente) -- cero cambio
de comportamiento fuera del caso nuevo. Windows y Linux aceptan el
parámetro por consistencia de firma pero lo ignoran: Windows porque ya
era `false`/`true` sin condición, y `RendererPolicy` nunca fuerza
"software" ahí salvo por el override DEV; Linux por la misma razón,
documentado explícitamente como no verificado si alguna vez se fuerza
software ahí.

**Resultado, con el fix**: se repitió el volcado de frames completo
para Nidir (28 frames, animación ambient real disparada por el
intervalo de 12s de DEC-084), Bunny (28 frames) y Frin macho (28
frames) bajo el driver "software" -- **0 de 84 frames corruptos**,
confirmando que el A/B prometido en DEC-083 ahora sí es limpio para
los tres. `ActiveFrameTexture` no cambió de diseño (revertido al
modelo de una sola textura reutilizada tras descartar esa hipótesis;
el log confirma "creation #1" por sesión, sin crecer por frame).

Ver el informe final de esta pasada para los números de CPU/RSS bajo
software (Nidir/Bunny/Frin macho, estático vs. durante animación) y el
estado de la retest de la transición de dirección (§ notas de UX).

---

### DEC-086 — macOS click-through: Nimvlets es el ÚNICO escritor de NSWindow.ignoresMouseEvents (y la causa raíz real de DEC-085)
**Status:** DECIDIDO · Block 05, pasada de estabilización

Dos preguntas abiertas desde DEC-085 quedaron resueltas acá, las dos con
evidencia medida (lectura de la fuente pineada de SDL 3.4.12 + repros
nativos mínimos), no por inferencia. La entrada es una sola porque las
dos respuestas están acopladas: la primera es la razón por la que no
podemos usar la ruta de shape, y la segunda es la razón por la que la
alternativa venía fallando.

**(1) Por qué una forma corrompe el render bajo el driver "software".**
No es `SDL_video.c` ni nada del backend Cocoa. Es el RENDERER:
`SDL_RenderPresent()` llama a `SDL_RenderApplyWindowShape()`
(`src/render/SDL_render.c:5463`) para toda ventana transparente. Esa
función crea una textura a partir de la superficie de forma y le pide
un blend mode CUSTOM —
`SDL_ComposeCustomBlendMode(ZERO, SRC_ALPHA, ADD, ZERO, SRC_ALPHA, ADD)`,
o sea "multiplicá el destino por el alpha de la forma". El renderer de
software no implementa el hook `SupportsBlendMode`, así que
`IsSupportedBlendMode()` (`SDL_render.c:1409`) rechaza cualquier modo
custom y la llamada FALLA. SDL ignora ese fallo por diseño ("There's
nothing we can do if this fails, so just keep on going"), con lo cual
la textura de forma se queda con su `SDL_BLENDMODE_BLEND` por defecto
— y en vez de enmascarar nuestro contenido, **lo pinta encima**. De ahí
la silueta blanca opaca.

Medido con un programa SDL3 standalone con los flags de ventana de
producción: contra `"software"` el blend mode custom es rechazado
("That operation is not supported") y el pixel central leído de vuelta
pasa de `(0,0,0,0)` a `(255,255,255,255)` en el instante en que se
instala una forma; contra `"metal"` el mismo blend mode es aceptado y
el pixel leído no cambia nunca. Además `SDL_SetWindowShape(w, NULL)`
restaura el render de inmediato: **la corrupción no es permanente**,
dura exactamente lo que dure la forma instalada. DEC-085 afirmaba lo
contrario en los dos puntos.

**(2) Por qué el fallback poll-driven venía perdiendo.** En SDL 3.4.12
hay exactamente DOS escritores de `NSWindow.ignoresMouseEvents`. El
que importa es `-[Cocoa_WindowListener updateIgnoreMouseState:]`
(`src/video/cocoa/SDL_cocoawindow.m:1073`), alcanzable solo desde
`-mouseMoved:` (línea 1893) bajo la guarda
`(window->flags & SDL_WINDOW_TRANSPARENT)`; `-mouseDragged:`,
`-rightMouseDragged:` y `-otherMouseDragged:` reenvían todos ahí. Lee
la forma desde `SDL_PROP_WINDOW_SHAPE_POINTER` y, **si no hay ninguna
instalada, asigna `NO` incondicionalmente**. Medido con los flags de
ventana de producción: UN solo `NSEventTypeMouseMoved` entregado a
nuestra propia ventana da vuelta el valor de YES a NO.

O sea: el poll histórico nunca estuvo mal medido — estaba PELEANDO
contra otro escritor. Ninguna frecuencia de muestreo podía ganar esa
carrera, solo achicarla. Y como `SpikeApp` cacheaba
`currentlyClickThrough_`, cuando SDL pisaba el valor la app ni siquiera
se enteraba: creía que ya estaba en el estado correcto y no volvía a
mirar.

**Decisión.** `platform::MakeClickThroughAuthoritative()` (nueva, en el
seam de siempre, implementada en las tres plataformas) intercepta
`-setIgnoresMouseEvents:` para NUESTRA ventana — agregando el override
a su propia clase vía el runtime de Objective-C, con swizzle in-place
solo si esa clase ya lo implementaba — y descarta toda escritura que no
venga del adaptador. Nimvlets escribe llamando a la IMP original
directamente. Windows/Linux devuelven `false` (no hay otro escritor
contra el que protegerse).

Alternativas evaluadas y descartadas, con motivo:
- **Instalar igual la forma** (para que Cocoa haga el hit-test
  per-pixel solo): imposible sin corromper el render — el renderer lee
  la MISMA propiedad de ventana que Cocoa, no se pueden separar.
- **Parchear la fuente pineada de SDL**: forkea una dependencia pineada
  a un tag exacto (AGENTS.md §10), agrega `PATCH_COMMAND` al
  `FetchContent`, y hay que revalidarlo en cada bump, en cuatro
  plataformas. Desproporcionado para lo que se arregla.
- **Segunda ventana invisible con forma, solo para hit-test**: choca
  con la misma pared — la ventana VISUAL sigue siendo
  `SDL_WINDOW_TRANSPARENT`, así que SDL le seguiría pisando
  `ignoresMouseEvents` igual. Además duplica la gestión de posición/
  nivel/Spaces.
- **Monitor global de NSEvent para el mouse**: prohibido por AGENTS.md
  §5/§14 (hook global de input), y además innecesario.

**Muestreo.** `core::EvaluateClickThrough()` (nueva, pura, con tests)
observa que **el estado de click-through es inobservable mientras el
cursor está FUERA del rectángulo de la ventana** — ningún click de ahí
puede llegarnos, esté como esté. Así que afuera se elige
deliberadamente el estado NO-click-through: eso devuelve la entrega
normal de eventos de mouse, con lo cual el ingreso a la ventana llega
como EVENTO en vez de tener que descubrirlo encuestando. El muestreo
periódico queda armado solo mientras el cursor está DENTRO del
rectángulo. En reposo — el caso dominante — el loop no se despierta ni
una vez por click-through.

Límite honesto: mientras el click-through está ACTIVO la ventana no
recibe eventos de mouse, así que detectar el regreso del cursor exige
muestrear. Eso es inherente al mecanismo de `ignoresMouseEvents`, no
una elección de este diseño (la propia ruta de shape de SDL tiene la
misma forma: reevalúa en `-mouseMoved:`). Un "teletransporte" del
cursor directo a un pixel transparente seguido de un click inmediato,
sin ningún evento de movimiento intermedio, puede perderse por hasta un
intervalo de muestreo.

---

### DEC-087 — Continuidad de punta en transiciones de estado: la colocación se hereda por archivo compartido, no se recalcula
**Status:** **SUPERSEDED por DEC-099** (Block 05, pasada de
simplificación geométrica) -- el anclaje por último frame se eliminó; la
continuidad de punta ahora es exacta por sustitución de frame canónico.
El containment por archivo compartido que esta entrada introdujo SÍ se
conserva. El texto original se preserva abajo sin reescribir.

_Status original:_ DECIDIDO · Block 05, pasada de estabilización

QA manual: "sit_to_lie termina visualmente y, al entrar a la base de
lying, el lobo salta un poco hacia arriba" (y el simétrico al entrar a
seated). Medido sobre el pack compilado real de Frin macho: bounding
box IDÉNTICO, suma de alpha IDÉNTICA, `dy = -62px`. **Traslación pura
de pixeles idénticos** — no era escala ni contenido, era colocación.

Causa: `compute_frame_normalization_plan()` colocaba cada entrada
anclando el centro de contenido de SU PROPIO frame 0. Como
`state[lying].base_animation` es, literalmente, el frame final de
`sit_to_lie` (el contrato first/last-frame que este proyecto ya exige),
el compilador estaba poniendo EL MISMO ARCHIVO en dos lugares
distintos.

Corrección, estructural y no heurística: se extiende el union-find por
archivo compartido de DEC-075 de la ESCALA a la TRASLACIÓN. Una entrada
cuyos archivos de frame son un SUBCONJUNTO de los de otra hereda su
colocación tal cual (match por ruta real, a nivel de entrada, así
right/left se emparejan con su propia dirección). Una transición que
cambia de estado y NO tiene ese vínculo (`lie_to_sit`, un export
inverso independiente) se ancla por su ÚLTIMO frame contra donde
aterriza de verdad la base del estado destino: el instante en que el
personaje QUEDA QUIETO es donde un salto se ve; el arranque ya está en
movimiento y lo disimula.

Resultado, compilado, ambas variantes y ambas direcciones:
- `sit_to_lie` último frame vs. base de `lying`: **idéntico pixel a
  pixel** (antes `dy` -62 macho / -60 hembra).
- `sit_to_lie` primer frame vs. base de `seated`: idéntico (sin
  cambios, ya lo era).
- `lie_to_sit` último frame vs. base de `seated`: centroide a menos de
  1.2px (antes `dy` +52..+55).

**Residual, declarado y no disimulado:** el ARRANQUE de `lie_to_sit`
ahora difiere de la base de `lying` en (25.5, 10.7)px en macho y
(3.0, 5.8)px en hembra. Es ARTE FUENTE, probado sobre los PNG nativos
sin compilador de por medio: `sit_to_lie` mueve al lobo macho
(-23, +158) px nativos mientras `lie_to_sit` lo mueve (-23, -118);
si fueran reversas exactas la suma sería cero, y suman (-46, +40). El
par de la hembra casi coincide ((-3, +13)), que es exactamente por qué
su residual es chico. Ningún offset rígido puede satisfacer las dos
puntas de un desacuerdo así, y deformar la animación autorada para
taparlo estaba fuera de alcance. **Pregunta de CONTENIDO para el
owner**, no deuda de compilador.

Efecto secundario: al no exigir `lying` su propio centrado, el canvas
de trabajo compartido pierde margen muerto (macho 543x815 -> 546x657,
hembra 496x653 -> 495x531). `visual_scale` se RE-DERIVA 1.30 -> 1.05
para dejar el tamaño en pantalla aprobado exactamente igual (la
derivación está escrita en el comentario de la constante), y de yapa
el pet se compila a ~24% más resolución efectiva bajo el mismo tope de
`runtime_max_frame_dimension`.

También se volvió exacto el dimensionado del canvas frente al redondeo
del downscale de runtime (`_grow_working_canvas_for_runtime_rounding()`):
antes tres redondeos independientes podían dejar un frame 1px afuera, y
solo el guard ruidoso de `_compile_frame()` lo atajaba. Es no-op para
Bunny/Nidir.

---

### DEC-088 — El tamaño de un grupo se mide por registración de alpha, no por el lado más largo del bounding box
**Status:** DECIDIDO · Block 05, pasada de estabilización

QA manual: las animaciones de click se ven "un poco más anchas/grandes"
que la pose base aprobada de su estado (Bunny `click`; Frin `howl`/
`tail_greet` sentado).

`compute_frame_normalization_plan()` medía "qué tan grande es este
personaje" como el LADO MÁS LARGO de su bounding box de contenido —
una cifra decidida por dos pixeles extremos. Una pose autorada que
estira una oreja, una cola o un hocico cambia esa medida sin que el
personaje haya cambiado de tamaño. Medido en el contenido real: el
`howl` de Frin macho tiene el bbox +4.5% más ANCHO que la base sentada
con EXACTAMENTE la misma altura.

Se reemplaza por `prep_dev_sprite.alpha_rms_radius()`: radio RMS
ponderado por alpha alrededor del centroide de alpha. Integra TODOS los
pixeles, así que escala linealmente con un reescalado real (lo que
queremos medir) y apenas se mueve con un cambio de pose (lo que no).
Es la "alpha-registration" que el brief pidió explícitamente en vez de
"raw bbox width alone". Con la medida ya estable, la tolerancia baja de
2% a 0.5% — que no cuesta calidad de imagen, porque el `content_scale`
ya se combina con el downscale de runtime en un ÚNICO resize.

Desvío de cada acción contra la pose base de su estado, antes -> después:

| pet | animación | antes | después |
|---|---|---|---|
| Bunny | click | +0.75% | **+0.06%** |
| Bunny | groom (aprobada) | +0.58% | **+0.27%** |
| Nidir | wing_stretch | -0.16% | -0.16% (sin cambio) |
| Nidir | click_fire | +0.12% | +0.12% (sin cambio) |
| Frin macho | howl | +0.98% | **-0.12%** |
| Frin macho | tail_greet | -0.72% | **-0.17%** |
| Frin hembra | tail_greet | -1.43% | **+0.13%** |
| Frin hembra | howl | +0.34% | **+0.28%** |

**El pack de Nidir queda BYTE-IDÉNTICO tras regenerarlo** — el control
dorado no se tocó, y eso es verificable, no una afirmación.

Lo que queda de "más ancho" está AUTORADO en el arte (Bunny click:
bbox +1.9% de ancho; Frin howl: +2.1% de ancho a altura casi igual).
Corregir eso aplastaría una pose real, así que no se hace. Se declara
como hecho de contenido.

---

### DEC-089 — Frin: rest delay a 10s, desacoplado del intervalo ambient de Bunny/Nidir
**Status:** DECIDIDO · Block 05, pasada de estabilización

Pedido de producto explícito del owner. Frin sentado pasa de 12s a
**10s** de reposo genuino antes de `sit_to_lie` -> `lying`.
Bunny y Nidir se quedan en **12s** (DEC-084, sin cambios), y el dwell
de hover se queda en **0.5s** (DEC-084, sin cambios).

Lo relevante más allá del número: DEC-084 había UNIFICADO los tres
valores, y esto los vuelve a separar a propósito. Son dos ritmos
distintos — "cada cuánto el pet hace un gesto ocioso" vs. "cuánto tarda
en cambiar de POSTURA" — y no hay razón de producto para que coincidan.
Que se puedan diferenciar sin tocar una línea de motor es justamente el
punto del modelo: `ambient_interval_seconds` es un dato de CONTENIDO
por-estado (`BehaviorState::ambientIntervalSeconds`), nunca hardcodeado
por especie.

Semánticas de reset sin cambios: click, hover, drag y la terminación de
cualquier acción reinician la cuenta desde ese instante. `lying` sigue
sin `ambient_actions`, así que nunca hay timer armado ahí — nada
despierta el loop por ese motivo mientras el lobo está acostado.

---

### DEC-090 — Hover dwell: 0.5s -> 0.2s
**Status:** DECIDIDO · Block 05, pasada de pulido final

Pedido de producto explícito del owner. `core::kDefaultHoverDwellSeconds`
pasa de 0.5 (DEC-084) a **0.2** segundos -- único valor que cambia; el
mecanismo en sí (dwell CONTINUO sobre pixel visible, reset completo al
salir/click/drag/cambio de estado, un solo disparo por episodio, nunca
spam) sigue exactamente igual, ver `core::HoverDwellTracker`.

Efecto colateral real encontrado al tocar esto: el log de diagnóstico
`"hover-triggered action after %.0fs dwell"` usaba CERO decimales --
con 0.5s eso ya redondeaba a "1s" o "0s" según el caso sin que nadie lo
notara, pero con 0.2s el resultado (`"0s"`) habría sido activamente
engañoso (sugiere "sin dwell alguno" cuando en realidad hubo 200ms
reales). Corregido a `%.1f` -- ver `src/app/SpikeApp.cpp`.

Timing sin cambios: Bunny/Nidir ambient permanece en 12s (DEC-084),
Frin sentado permanece en 10s (DEC-089) -- ninguno de los dos se toca
en esta pasada.

---

### DEC-091 — Frin: inversión de dirección RUNTIME (right<->left), provenance de arte sin cambios
**Status:** DECIDIDO · Block 05, pasada de pulido final

Pedido de producto explícito del owner: TODO lo que hoy se ve corriendo
con `Direction::kRight` debe verse con `Direction::kLeft`, y viceversa
-- para las DOS variantes de Frin (macho y hembra) y para TODO
contenido direccional (pose base sentada, pose base acostada,
`sit_to_lie`, `lie_to_sit`, `howl`, `tail_greet`).

**Lo que NO cambia, a propósito:** las carpetas de import
(`assets/source/nimvlets/frin/{male,female}/animations/<anim>/
{left,right}/frames/`) siguen registrando exactamente la orientación
que el owner exportó de verdad -- PROVENANCE, nunca semántica de
runtime. Ningún archivo se renombra ni se mueve. Esto es deliberado:
mezclar "qué exportó Ludo" con "qué muestra el runtime hoy" en el mismo
nombre de carpeta habría hecho que una futura re-inversión (o una
consulta de "¿qué pasó realmente en el export original?") dependiera de
recordar CUÁL de las dos convenciones estaba vigente en cada momento.

**Implementación, centralizada en un solo lugar** (pedido explícito del
brief: "Do this once in the Frin content-generation layer, not by
duplicating logic for each animation"): `tools/generate_frin_pack.py`'s
`entries_for(anim_id, runtime_direction)` -- la única función por la
que TODO contenido direccional de Frin pasa (poses base vía
`base_pose()`, las cuatro animaciones vía `action()`) -- ahora invierte
`runtime_direction` con `_invert_runtime_direction()` ANTES de
resolverla contra `canonical_direction` (real vs. derivado/espejado).
Ni `action()` ni `base_pose()` ni `_finalize_and_compile()` cambiaron
una sola línea -- ellos siguen pidiendo "dame el contenido para el slot
runtime right/left" exactamente como antes; la inversión vive
enteramente escondida detrás de esa única función.

**Invariante resultante, verificado, variante-independiente:** sin
importar `canonical_direction` (macho "left", hembra "right"), el slot
runtime `Direction::kRight` SIEMPRE termina leyendo de la carpeta
física `.../left/frames/`, y `Direction::kLeft` SIEMPRE de
`.../right/frames/` -- exactamente invertido de lo que un mapeo directo
(sin esta pasada) habría dado. Confirmado dos formas independientes:

1. Estático, contra los "source" paths de los DOS manifests reales
   (antes/después de esta pasada) -- para CADA base/acción, el slot
   runtime-right nuevo apunta a los MISMOS archivos que el slot
   runtime-left viejo, y viceversa. Cero mismatches, macho + hembra,
   base + las 4 animaciones.
2. `tests/test_asset_pipeline.py`'s `FrinRuntimeDirectionInversionTest`
   fija esto como test permanente contra el pack real, más una tabla de
   verdad algebraica aislada de `_invert_runtime_direction()`.

Ambos packs de Frin se regeneraron. Efecto de tamaño de canvas: apenas
perceptible (macho 546x657 -> 548x657, +2px; hembra 495x531 -> 494x531,
-1px) -- la inversión en sí no cambia ningún contenido/escala, solo
reordena qué frames van a qué slot, y ese reordenamiento interactúa de
forma mínima con el redondeo del canvas de trabajo compartido (ver
DEC-092/093 para la otra fuente de cambio de canvas de esta misma
pasada).

**Regresión verificada:** la continuidad de transición de estado de
DEC-087 (`sit_to_lie` -> `lying` pixel-idéntico) sigue pixel-idéntica en
AMBAS direcciones tras la inversión -- esperado, porque el mecanismo de
containment por archivo compartido opera sobre el mismo `entries_for()`
ya invertido, así que la relación "mismo archivo en las dos puntas" se
preserva sin importar qué slot runtime lo muestre.

---

### DEC-092 — `align_endpoint_to_target_base`: extiende el anclaje-por-último-frame de DEC-087 a acciones self-loop, vía un flag de CONTENIDO opcional
**Status:** **SUPERSEDED por DEC-099** (Block 05, pasada de
simplificación geométrica) -- el flag `align_endpoint_to_target_base` se
eliminó por completo. El texto original se preserva abajo sin
reescribir.

_Status original:_ DECIDIDO · Block 05, pasada de pulido final

QA manual, dos reportes relacionados:
> Bunny: "the character feels very slightly wider/larger while
> animation content is active, so returning to the static base can
> reveal a tiny apparent size snap."
> Frin: "seated click actions appear slightly wider/larger than the
> seated base."

**Medido antes de tocar nada** (packs compilados reales, centroide
ponderado por alpha del ÚLTIMO frame mostrado contra la base de su
estado -- no solo el frame 0, que ya estaba cubierto y aprobado por
`CompiledClickScaleTest`/DEC-088):

| acción | delta LAST-vs-BASE (antes) |
|---|---|
| Bunny `groom` (izquierda) | 1.82px |
| Bunny `click` | 0.84-0.85px |
| Frin macho `howl` | 0.83-0.85px |
| Frin macho `tail_greet` | 0.46-0.56px |
| Frin hembra `howl` | 0.81px |
| Frin hembra `tail_greet` | 0.66px |

Ninguno de estos números es "corrupción" -- son todos sub-2px sobre
frames nativos de 250-550px -- pero son reales, medibles, y consistentes
con la queja del owner: la punta de REGRESO a la base (el instante en
que la acción termina y `AnimationController` vuelve a mostrar
`base_animation`) no estaba protegida para NINGUNA acción self-loop
(`target_state_id == state_id`, el caso normal de click/ambient en
Bunny/Frin sentado) -- solo una transición que CAMBIA de estado
(`sit_to_lie`, `lie_to_sit`) se anclaba por su último frame contra su
destino (DEC-087); una acción self-loop se anclaba SIEMPRE por su
PROPIO frame 0, dejando que el personaje derivara libremente hacia
donde sea que la animación real lo llevara para cuando terminara.

**Por qué NO se generalizó el registro incondicionalmente a TODA acción
que retorna** (lo que habría sido el cambio más simple): habría alterado
también el pack de **Nidir** -- que nunca reportó este problema y que
el brief marca explícitamente como "FROZEN GOLDEN CONTROL" ("do not
modify... Nidir generated pack behavior"). Cualquier cambio de
colocación, sin importar cuán pequeño, casi con certeza mueve al menos
un `offset_x`/`offset_y` redondeado a entero.

**Decisión:** un campo NUEVO, opcional, de CONTENIDO --
`align_endpoint_to_target_base` (default `false`) en un
`WeightedActionManifest` (ver `tools/compile_pet_pack.py`, mismo nivel
que `target_state_id`/`returns_to_idle`, mismo patrón que
`normalize_visual_scale`/`hover_uses_ambient_actions`: una propiedad de
DATOS, nunca una rama de código por-pet). `add_actions()` en
`compile_pet_pack.py` amplía su condición de registro de
`changes_state` a `changes_state OR align_endpoint_to_target_base`
(ambas gateadas, como siempre, por `returns_to_idle` -- si la acción
nunca vuelve, no hay ninguna punta de regreso que proteger). El
generador de CADA pet decide, por su propio contenido, si activa el
flag:

- `tools/generate_bunny_pack.py`: `groom` y `click` lo activan (y,
  por consistencia semántica aunque sea un no-op observable,
  `idle_breathing` también -- ver más abajo por qué es un no-op ahí).
- `tools/generate_frin_pack.py`: `action()` gana el parámetro
  `align_endpoint_to_target_base` (default False); solo `howl` y
  `tail_greet` lo pasan en `True`. `sit_to_lie`/`lie_to_sit` lo dejan en
  False -- no hace ninguna diferencia para ellas (`changes_state` ya
  las registra sin condición alguna), así que agregarlo sería ruido
  sin efecto.
- `tools/generate_nidir_pack.py`: **sin tocar, en absoluto.** El flag
  nunca aparece en su manifest -> la condición de registro se reduce
  EXACTAMENTE a `changes_state` (que para Nidir, de un solo estado,
  siempre es False) -> el código toma el MISMO camino que antes de esta
  pasada, por construcción, no por casualidad de números.

**Por qué `idle_breathing` de Bunny es un no-op observable con el flag
activo:** su frame 0 ES, literalmente, el mismo archivo que la pose
base de "default" (mismo import). El mecanismo de containment por
archivo compartido de DEC-087 ya lo ancla EXACTO en la punta de
ENTRADA, y ese containment tiene prioridad sobre el registro de
transición en `place()` -- `_build_normalization_plan()`'s post-pass ya
descarta cualquier acción que comparta un archivo real con su destino
ANTES de que `compute_frame_normalization_plan()` la vea (la misma
regla que ya protegía a `sit_to_lie` de un ciclo de containment
consigo mismo). Verificado: el plan resultante para `idle_breathing`
es byte-idéntico con el flag en `True` o en `False`.

**Resultado medido, packs recompilados:**

| acción | antes | después |
|---|---|---|
| Bunny `groom` (izquierda) | 1.82px | **0.19px** |
| Bunny `groom` (derecha) | 0.88px | 0.88px *(sin cambio -- ver nota)* |
| Bunny `click` (derecha) | 0.84px | **0.67px** |
| Bunny `click` (izquierda) | 0.85px | **0.65px** |
| Frin macho `howl` | 0.83-0.85px | **0.59-0.64px** |
| Frin macho `tail_greet` | 0.46-0.56px | 0.56px *(sin cambio en una dirección)* |
| Frin hembra `howl` | 0.81px | **0.32-0.81px** *(mejora en una dirección)* |
| Frin hembra `tail_greet` | 0.66px | 0.66px *(sin cambio)* |

Nota honesta: dos de los ocho casos (Bunny groom-derecha, Frin macho
tail_greet-izquierda, Frin hembra tail_greet) no cambian en absoluto --
el nuevo offset entero calculado coincide, por redondeo, con el
anterior. Ninguno de los ocho casos EMPEORA. El peor caso final,
0.88px sobre un frame nativo de ~250px, es imperceptible en pantalla.
Ver docs/BUNNY_CONTENT.md/§17 y docs/FRIN_CONTENT.md/§9 para el detalle
completo por-acción y el informe de este bloque para la tabla íntegra.

**Regresión sobre `lie_to_sit` (ya registrada, sin cambios en SU
condición de registro):** su NÚMERO sí cambia, porque comparte la
MISMA rama de `place()` cuyo punto de anclaje pasó de centro-de-bbox a
centroide-de-alpha (ver DEC-093) -- eso es harina de otro costal
(DEC-093 lo documenta con su propia evidencia), no algo que este DEC
introduzca.

**Tests:** `tools/test_asset_pipeline.py`'s `AlignEndpointToTargetBaseTest`
(integración real: compila un manifest+PNG de fixture con el flag en
False/True y verifica placement/escala/no-op vía `compile_pack()` real)
y `CompiledSelfLoopEndpointContinuityTest` (contra los packs reales que
se envían).

---

### DEC-093 — El anclaje por-último-frame se registra por centroide de alpha, no por centro de bounding box
**Status:** **SUPERSEDED por DEC-099** (Block 05, pasada de
simplificación geométrica) -- `alpha_registration_point()` se eliminó
junto con las ramas de anclaje que lo usaban. El texto original se
preserva abajo sin reescribir.

_Status original:_ DECIDIDO · Block 05, pasada de pulido final

Consecuencia directa de implementar DEC-092: la primera versión
(reusar `anchor_of()`, centro de bounding box, sin cambios respecto a
DEC-087) SÍ redujo el delta de bbox-center a casi cero para `groom` de
Bunny -- pero el centroide de ALPHA (el "centro de masa" real de lo
visible, la misma noción que `alpha_rms_radius()` ya usa para medir
TAMAÑO desde DEC-088) empeoró: de 0.88-1.82px a **~4.1-4.3px**. Medido,
no supuesto -- ver el volcado completo en el informe de este bloque.

Causa: el último frame de `groom` tiene margen de contenido asimétrico
(una pose real donde el cuerpo queda desplazado dentro de un contorno
de ancho similar al de la base) -- alinear sus DOS PIXELES EXTREMOS
(bbox) no alinea dónde está realmente distribuido el peso visual. Es
el mismo fenómeno, en COLOCACIÓN, que DEC-088 ya documentó para
TAMAÑO: una medida decidida por extremos es frágil frente a una pose
real; una medida ponderada por TODOS los pixeles no lo es.

**Corrección:** nueva `prep_dev_sprite.registration_point()` (junto con
`alpha_weighted_centroid()`, extraído de `alpha_rms_radius()` para
reusarlo -- refactor puro, sin cambio de comportamiento en
`alpha_rms_radius()` en sí, verificado con los 77 tests preexistentes
sin tocar). `place()`'s rama de anclaje-por-transición ahora registra
por CENTROIDE DE ALPHA (de la punta que se mueve Y del destino) en vez
de por centro de bbox -- ámbito estrictamente acotado a esa rama; la
colocación DEFAULT de cualquier entrada sin destino registrado (la
inmensa mayoría de entradas de TODO pet, incluido el 100% de Nidir)
sigue usando `anchor_of()`/bbox-center, sin cambios.

**Por qué el alcance acotado importa para Nidir:** Nidir nunca entra a
esta rama (sin transiciones de estado, sin ninguna acción con
`align_endpoint_to_target_base`) -- cambiar qué métrica usa esa rama
tiene efecto CERO sobre Nidir, verificado por byte-identidad exacta
(ver el informe de este bloque, sha256 fijado en
`NidirGoldenControlTest`).

**Efecto sobre `lie_to_sit` (Frin, ya registrada desde DEC-087, sin
relación con el flag de DEC-092):** al compartir la misma rama, su
número también se recalcula con el nuevo punto de anclaje. Resultado
mixto y reportado con honestidad: mejora sustancialmente en DOS de los
cuatro casos (macho-derecha 1.21px -> 0.40px, hembra-derecha 1.20px ->
0.25px) y empeora levemente en los otros dos (macho-izquierda 0.44px
-> 0.87px, hembra-izquierda 0.23px -> 0.86px) -- ningún caso supera
1px, muy por debajo del umbral perceptible y muchísimo por debajo del
~53px que existía antes de DEC-087. Se evaluó mantener DOS
convenciones de anclaje distintas (bbox para `changes_state` legado,
alpha-centroide para el flag nuevo de DEC-092) para evitar tocar
`lie_to_sit` en absoluto, y se descartó: introduciría dos mecanismos
conceptualmente idénticos con comportamiento distinto sin ninguna razón
de PRODUCTO para diferenciarlos, puramente para preservar un número que
ya estaba, y sigue estando, dentro de un rango perceptualmente
irrelevante.

**Tests:** los 6 tests preexistentes de `TransitionEndpointContinuityTest`
siguen pasando sin modificar (su fixture usa rectángulos sólidos, donde
centro-de-bbox y centroide-de-alpha coinciden exactamente por
construcción, así que no distinguen las dos convenciones -- documentado
como límite honesto de ESE test específico, no un hueco de cobertura:
`CompiledFrinEndpointContinuityTest` y `CompiledSelfLoopEndpointContinuityTest`
sí ejercitan contenido real con densidad de alpha no uniforme, contra
los packs compilados reales).

---

### DEC-094 — Hover dwell: 0.2s -> 0.4s; Frin rest delay: 10.0s -> 12.0s (revierte DEC-089)
**Status:** DECIDIDO · Block 05, pasada de continuidad de frontera

Dos pedidos de producto explícitos, independientes entre sí, agrupados
en un único DEC porque los dos son "un solo número, mecanismo sin
cambios":

**Hover dwell.** `core::kDefaultHoverDwellSeconds` pasa de 0.2 (DEC-090)
a **0.4** segundos. Mismo mecanismo de siempre (dwell CONTINUO, reset
completo en salida/click/drag/cambio de estado, un disparo por
episodio, nunca spam) -- ver `core::HoverDwellTracker`, sin tocar.

**Frin rest delay.** `REST_DELAY_SECONDS` en
`tools/generate_frin_pack.py` pasa de 10.0 (DEC-089) de vuelta a
**12.0** -- vuelve a coincidir numéricamente con el intervalo ambient
de Bunny/Nidir (DEC-084), aunque conceptualmente siguen siendo dos
relojes independientes (uno por-estado, vía
`BehaviorState::ambientIntervalSeconds`, nunca hardcodeado por
especie) que HOY comparten valor por decisión de producto, no por
restricción del modelo. `lying` sigue sin `ambient_actions` -- nunca
hay timer armado ahí.

Semánticas de reset sin cambios en ninguno de los dos casos.

---

### DEC-095 — Escala de una acción self-loop derivada del RETORNO, no del arranque
**Status:** **SUPERSEDED por DEC-099** (Block 05, pasada de
simplificación geométrica) -- derivar la escala del último frame se
eliminó; la escala vuelve a medirse siempre desde el frame 0. El texto
original se preserva abajo sin reescribir.

_Status original:_ DECIDIDO · Block 05, pasada de continuidad de frontera

DEC-092 (pasada anterior) resolvió la COLOCACIÓN de la punta de regreso
de una acción self-loop (`groom`/`click` de Bunny, `howl`/`tail_greet`
de Frin sentado) anclándola por su último frame contra la base --
pero preservó DELIBERADAMENTE `content_scale`, que seguía
derivándose SIEMPRE del PRIMER frame de la acción contra la base. QA
manual confirmó que eso no alcanzaba: "the stable base pose and the
animation boundary can still represent the same character at slightly
different SCALE" -- un personaje puede estar perfectamente COLOCADO en
la punta de regreso y aun así verse ligeramente más grande o más chico
que la base, si la ESCALA se calibró contra una pose distinta (el
arranque) de la que efectivamente toca la base al terminar.

**Corrección:** `group_content_size()` (dentro de
`compute_frame_normalization_plan()`) mide ahora el ÚLTIMO frame en vez
del primero para cualquier grupo presente en el nuevo parámetro
`scale_from_last_frame_entries` -- poblado, en
`compile_pet_pack.py`'s `add_actions()`, con exactamente las mismas
entradas self-loop que ya tenían `align_endpoint_to_target_base` de
DEC-092 (el mismo flag ahora gobierna PLACEMENT *y* SCALE juntos, sin
un flag nuevo -- una sola palanca de contenido para "este endpoint
representa la base destino").

**Por qué SOLO self-loop, nunca una transición que cambia de estado**
(evidencia, no intuición -- ver el docstring de
`scale_from_last_frame_entries`): en una acción self-loop, el frame 0 Y
el último frame comparten la MISMA postura que la base (sentado todo el
tiempo, "default" todo el tiempo), así que comparar CUALQUIERA de los
dos contra la base compara la MISMA silueta. En `lie_to_sit`
(acostado -> sentado), el último frame tiene una postura GENUINAMENTE
distinta de la base de ORIGEN (`lying`) -- comparar por tamaño ahí sería
exactamente la comparación entre-posturas inválida que DEC-075 ya
prohibió. Para `lie_to_sit` la escala se sigue derivando del PRIMER
frame contra la base de origen, sin cambios -- ver DEC-096 para su
propio mecanismo, centrado en TRASLACIÓN, no escala.

**Resultado medido, packs compilados, radio RMS ponderado por alpha del
ÚLTIMO frame contra la base (antes -> después):**

| acción | antes | después |
|---|---|---|
| Frin macho `howl` | 0.261% off | **0.002% off** |
| Frin macho `tail_greet` | 0.334% off | **0.053% off** |
| Frin hembra `tail_greet` | 0.011% off (ya bueno) | 0.030% off (sigue bueno) |
| Bunny `groom`/`click` | ya dentro de tolerancia (0.5%) en ambas puntas | sin cambio de `content_scale` -- ver nota |

Nota honesta sobre Bunny: su contenido YA caía dentro de la tolerancia
de 0.5% tanto midiendo desde el frame 0 como desde el último, así que
`content_scale` no cambió de valor para ninguna de sus tres acciones
self-loop -- el pedido del brief ("scale ratio as close to 1.000 as
practical") ya estaba satisfecho antes de esta pasada; lo que SÍ mejora
para Bunny es la PLACEMENT (ver el informe de este bloque). Un
residual sub-0.5% para Frin hembra `howl` (0.125% -> 0.391%) resultó
ser ruido de RESAMPLEO -- el canvas de trabajo compartido creció
levemente por DEC-096 (`lie_to_sit` necesitando más margen), lo que
cambia el ratio de downscale de RUNTIME aplicado a TODO el pet
(compartido), no un cambio real en `content_scale` (que se midió en
1.000000 exacto, sin rescale, antes y después) -- documentado, no
escondido.

Uniformidad garantizada por construcción: `content_scale` sigue siendo
UN solo valor por grupo, aplicado a TODOS los frames de esa animación
por igual -- nunca hay compensación de zoom por-frame. Si algún frame
recibiera una escala distinta sus dimensiones compiladas divergirían,
y `_compile_animation()` ya falla fuerte ante eso (invariante
preexistente, reverificado con un test explícito -- ver
`ReturnEndpointScaleContinuityTest`).

`assets/dev/nidir_pack.nvpack` queda BYTE-IDÉNTICO tras
regenerarlo -- Nidir nunca activa `align_endpoint_to_target_base`, así
que `scale_from_last_frame_entries` es siempre el conjunto vacío para
su generador, y la condición de `group_content_size()` se reduce, por
construcción, al comportamiento anterior a esta pasada.

**Tests:** `ReturnEndpointScaleContinuityTest` (packs reales, ambos
pets, tolerancia 1%) y `AlignEndpointToTargetBaseTest` (ya existente,
extendido para cubrir que la escala también cambia con el flag, no
solo la colocación).

---

### DEC-096 — Registro de DOS PUNTAS para `lie_to_sit`, con interpolación lineal de traslación cuando un transform constante no alcanza
**Status:** **SUPERSEDED por DEC-097** (Block 05, pasada de resolución
de root-motion) · fue DECIDIDO · Block 05, pasada de continuidad de
frontera. El MECANISMO descrito abajo se implementó, se envió, y QA
manual del owner lo RECHAZÓ: mover la colocación del sprite
gradualmente a lo largo del clip no se percibe como continuidad, se
percibe como el personaje entero derivando por la ventana. La medición
que lo respaldaba (las dos puntas cerraban a ~1px) era correcta y sigue
siendo correcta -- lo que estaba mal era asumir que dos puntas
numéricamente buenas implican un clip visualmente bueno. El texto
original se conserva sin reescribir; ver DEC-097 para qué lo reemplaza
y para el número que la métrica de dos-puntas no capturaba

QA manual, específico: "when lying and the owner clicks to stand up,
the stable lying pose immediately jumps in position when the FIRST
lie_to_sit frame appears". DEC-087/DEC-092 ya habían anclado la punta
FINAL de `lie_to_sit` (por-último-frame contra `seated_base`) -- pero
nunca la punta INICIAL: sin ningún registro contra `lying_base`,
`lie_to_sit` se colocaba SOLO por su propio frame 0, dejando que
aterrizara donde fuera que su export nativo lo pusiera.

**Medido antes de tocar nada** (centroide de alpha, frame 0 de
`lie_to_sit` contra `lying_base`, packs compilados): sin NINGÚN
registro, la distancia dependía enteramente de dónde el export nativo
de `lie_to_sit` casualmente cayera dentro del canvas de trabajo
compartido -- el salto que el owner reportó.

**Extensión genérica del sistema existente** (no un hack por-pet, per
el brief de este bloque): nuevo campo de contenido
`align_transition_both_endpoints` (WeightedActionManifest, opcional,
default False, solo tiene efecto si `target_state_id` cambia de
estado). Cuando está en `true`, además del registro por-último-frame
que `changes_state` ya activa incondicionalmente, se registra TAMBIÉN
el PRIMER frame contra la base del estado de ORIGEN (`state_id`, el
propio estado bajo el que la acción está autorada) -- mismo mecanismo
de registro (centroide de alpha, `registration_point()`) que la punta
final ya usaba.

**Modelo de resolución, evidencia-primero** (no se asumió que hiciera
falta interpolar): se calculan `start_offset` (registro de frame 0
contra origen) y `end_offset` (registro del último frame contra
destino, sin cambios respecto al mecanismo existente) y se COMPARAN. Si
coinciden dentro de 1px de tolerancia, se usa un ÚNICO offset
constante -- el mismo comportamiento de siempre, sin ningún cambio
observable. Solo si divergen de verdad se interpola:

```
offset(i) = lerp(start_offset, end_offset, i / (frame_count - 1))
```

-- lineal, SOLO traslación (nunca escala, nunca por-eje independiente,
nunca deforma anatomía ni re-temporiza), un offset entero por frame.
`lie_to_sit` SÍ necesitó interpolación en las 4 combinaciones
variante×dirección medidas (divergencia real de 10-50px entre las dos
puntas, muy por encima de la tolerancia de 1px) -- confirmado
directamente contra `_build_normalization_plan()`'s salida, no
supuesto.

`sit_to_lie` NO recibe este flag -- sus dos puntas YA son archivos
compartidos con `seated_base`/`lying_base` (containment las deja
exactas sin ningún registro por contenido, DEC-087), así que
`align_transition_both_endpoints` sería ruido sin efecto ahí. Fijado
como test explícito (`test_sit_to_lie_is_not_flagged_for_two_endpoint_
registration`), no solo por omisión.

**Arquitectura de implementación** (ver
`tools/prep_dev_sprite.py`/`tools/compile_pet_pack.py`):
- `compute_frame_normalization_plan()` gana `source_target_entry`
  (complemento de `transition_target_entry`, apunta al ORIGEN),
  `scale_from_last_frame_entries` (ver DEC-095) y
  `per_frame_offsets_out` (parámetro de salida MUTABLE -- nunca se
  cambió el TIPO de retorno de la función, que sigue siendo el mismo
  `dict[str, 5-tuple]` que sus ~6 call sites existentes ya
  destructuran; el nuevo dato es aditivo e invisible para cualquier
  llamador que no lo pida).
- El canvas de trabajo compartido se agranda para contener el frame en
  AMBAS posiciones (arranque y destino), no solo la de destino --
  verificado: el guard ruidoso de `_compile_frame()` (contenido
  excediendo el canvas) NUNCA se disparó al recompilar el contenido
  real.
- `_compile_animation()` (compile_pet_pack.py) resuelve, por índice de
  frame, el offset efectivo vía `lerp_offset_schedule()` cuando esta
  entrada tiene un schedule de dos puntas -- `content_scale`/
  `working_width`/`working_height` siguen siendo el ÚNICO valor
  uniforme del plan para TODOS los frames; solo `offset_x`/`offset_y`
  varía por índice.
- CERO cambios en `src/` -- el runtime C++ nunca supo ni necesita saber
  que esto existe: el pack compilado ya trae las posiciones correctas
  bakeadas frame por frame, `content::PetPackLoader` las lee
  exactamente igual que cualquier otro frame.

**Resultado medido, packs compilados (centroide de alpha, antes ->
después):**

| medición | antes | después |
|---|---|---|
| `lie_to_sit` frame 0 vs `lying_base` | sin registro (el salto reportado) | **0.80-1.16px** |
| `lie_to_sit` último frame vs `seated_base` | 0.25-0.87px (ya bueno, DEC-087/092) | 0.40-0.87px (se mantiene bueno) |

Movimiento verificado SUAVE sobre los 25 frames reales (sin saltos
grandes escondidos en medio de la interpolación) -- ver
`test_translation_correction_never_produces_a_large_frame_to_frame_jump`.

**Escala de `lie_to_sit`:** NO se tocó, per §9 del brief ("this pass
should primarily solve translation continuity... do not interpolate
zoom unless overwhelming evidence"). Medido antes de decidir: el ratio
de escala implícito en cada punta (radio RMS del frame 0 contra
`lying_base`, y del último frame contra `seated_base`) está a
±0.1-0.7% de 1.0 en las 4 combinaciones -- consistente entre las dos
puntas, sin evidencia de que el export cambie de escala. Se mantiene UN
solo `content_scale` uniforme para toda la transición, derivado (sin
cambios) del primer frame contra la base de origen.

**Tests:** `LerpOffsetScheduleTest`/`TwoEndpointFrameOffsetsTest`
(mecanismo puro, sin manifest), `LieToSitTwoEndpointContinuityTest`
(contra los packs reales que se envían, las 4 combinaciones
variante×dirección).

---

### DEC-097 — La interpolación de traslación por frame se retira: el compilador no inventa root-motion. `lie_to_sit` pasa a anclarse por su ARRANQUE
**Status:** **PARCIALMENTE SUPERSEDED por DEC-099** (Block 05, pasada de
simplificación geométrica) -- el RECHAZO de la interpolación por-frame
sigue vigente y es permanente. Lo superseded es solo
`anchor_start_to_source_base`: la colocación de arranque ya no es un
flag propio, se deriva de `first_frame_is_state_base`. El texto original
se preserva abajo sin reescribir.

_Status original:_ DECIDIDO · Block 05, pasada de resolución de root-motion

**Qué rechazó QA.** DEC-096 hacía que `lie_to_sit` satisficiera sus DOS
puntas interpolando linealmente la traslación por índice de frame
(`offset(i) = lerp(start, end, i/(n-1))`). Métricamente era un éxito:
arranque a 0.80-1.16px de `lying_base`, final a 0.54-0.87px de
`seated_base`. Visualmente fue un fracaso -- el owner reportó que
"mientras Frin se levanta, el sprite entero parece desplazarse por la
ventana".

**Por qué la métrica no lo vio, medido.** Las dos puntas eran lo único
que se estaba midiendo. Lo que faltaba mirar era cuánto RECORRIDO total
hace el personaje durante el clip, comparado con cuánto recorre el arte
autorado:

| variante | recorrido compilado CON interpolación | recorrido con transforma rígida (= autorado) | movimiento INVENTADO |
|---|---|---|---|
| macho | 78.1 px | 62.1 px | **+16.0 px (+26%)** |
| hembra | 72.2 px | 64.7 px | **+7.5 px (+12%)** |

O sea: hasta un cuarto del movimiento aparente del lobo lo estaba
poniendo el compilador, no el arte. Eso es root-motion artificial, y es
exactamente lo que el owner percibió.

**Regla permanente que sale de acá.** El compilador aplica UNA
transforma por animación -- una escala uniforme y una traslación
constante -- y NUNCA introduce movimiento aparente del personaje
completo. Si el arte no cierra geométricamente, el residual se MIDE y
se REPORTA como deuda de contenido; no se disimula. Se retiraron
enteras `compute_two_endpoint_frame_offsets()` y
`lerp_offset_schedule()` de `tools/prep_dev_sprite.py`, junto con todo
el threading de `per_frame_offsets` por `tools/compile_pet_pack.py`
(`_compile_animation()` vuelve a recibir UNA sola `normalization` para
todos sus frames) -- no se dejaron "por si acaso": un mecanismo
rechazado que sigue disponible es un mecanismo que vuelve.

**Qué lo reemplaza:** `anchor_start_to_source_base` (campo de contenido
opcional, solo con efecto en una acción que CAMBIA de estado). Ancla el
clip por su PRIMER frame contra la base del estado de ORIGEN, con una
transforma rígida constante, y REEMPLAZA el anclaje por-último-frame
que esa acción recibiría si no. Deliberadamente no existe un modo "las
dos puntas": es justamente lo que exigiría mover el sprite durante el
clip.

**Por qué el ARRANQUE y no el final.** Con una transforma rígida se
puede satisfacer una punta o la otra, nunca las dos (medido abajo). Un
salto instantáneo en el PRIMER frame ocurre en el instante exacto en
que el owner hace click -- con la atención puesta ahí y el personaje
todavía quieto. Un residual al final ocurre cuando el lobo ya viene en
movimiento. Se eligió proteger el arranque.

**El residual, medido, con las dos prioridades de anclaje** (una
transforma rígida: UNA escala uniforme + UNA traslación constante; el
residual es el mismo vector, solo cambia en qué punta cae):

| variante | dirección | A: anclado al ARRANQUE -> residual al FINAL | B: anclado al FINAL -> residual al ARRANQUE |
|---|---|---|---|
| macho | right | 54.08 px nativos = 26.34 px compilados | idéntico |
| macho | left | 54.08 px nativos = 26.34 px compilados | idéntico |
| hembra | right | 9.67 px nativos = 5.83 px compilados | idéntico |
| hembra | left | 9.67 px nativos = 5.83 px compilados | idéntico |

Verificado luego sobre los packs recompilados: arranque a 0.80-1.13px
(anclado), final a 26.58/25.72px (macho) y 6.75/6.70px (hembra). En
pantalla, con `visual_scale` incluido: **~15pt el macho, ~3.9pt la
hembra**, sobre un pet de 185pt de alto.

**Causa raíz, y por qué NO es un bug del compilador.** El root-motion
interno del export de `lie_to_sit` no es el inverso del de
`sit_to_lie`: medido sobre los PNG NATIVOS, `sit_to_lie` mueve al lobo
macho (-23, +158) px y `lie_to_sit` lo mueve (-23, -118) px. Si fueran
reversas exactas la suma sería (0,0); es (-46, +40). Ninguna transforma
rígida puede reconciliar eso, y ninguna debería intentarlo deformando
o desplazando el personaje.

**Recomendación de CONTENIDO (deuda, no falla de runtime):**
regenerar/reexportar `lie_to_sit` -- sobre todo el del MACHO -- para que
su root-motion cierre contra `sit_to_lie`. El residual de la hembra
(~3.9pt) es mucho menor y podría tolerarse; el del macho (~15pt, ~8% del
alto del pet) es claramente visible. Hasta entonces el residual queda
VISIBLE al final del clip, a propósito, fijado como techo en
`CompiledFrinEndpointContinuityTest.test_lie_to_sit_end_residual_is_known_content_debt`
para que no pueda EMPEORAR sin que un test falle.

`sit_to_lie` no se tocó y queda congelado: sus dos puntas ya son
archivos compartidos con `seated_base`/`lying_base`, containment las
deja exactas, y su comportamiento está aprobado por QA.

---

### DEC-098 — Contrato ESTRICTO de retorno-a-base: sin tolerancia de escala en la frontera
**Status:** **SUPERSEDED por DEC-099** (Block 05, pasada de
simplificación geométrica) -- `strict_scale_entries` se eliminó; en la
frontera ya no hay ninguna escala que ajustar, porque la punta ES el
frame de la base. El texto original se preserva abajo sin reescribir.

_Status original:_ DECIDIDO · Block 05, pasada de resolución de root-motion

QA manual: "durante la animación Bunny/Frin se ven un poco más
gordos/grandes; al terminar y volver a la base estática se vuelven un
poco más finos/chicos".

`scale_tolerance` (0.5%) existe para no resamplear cuando la diferencia
es imperceptible EN AISLAMIENTO -- política correcta para una animación
cualquiera. Pero en la frontera de RETORNO el owner ve dos imágenes
CONSECUTIVAS del mismo personaje, y ahí una diferencia sub-percentual
se lee como "se achicó al terminar".

**Corrección:** para una acción que el CONTENIDO marca con
`align_endpoint_to_target_base` (el contrato "mi último frame ES la
pose base del destino"), la escala se aplica EXACTA -- `raw_scale` tal
cual, sin pasar por la tolerancia. Ese mismo flag ya gobernaba
colocación (DEC-092) y de-dónde-medir (DEC-095); ahora gobierna también
con-cuánta-precisión-aplicar. Sigue siendo UNA escala uniforme por
acción, X e Y idénticas, nunca por frame.

**Efecto real, medido** (radio RMS del último frame compilado contra la
base, antes -> después):

| pet / acción | antes | después |
|---|---|---|
| Frin hembra `howl` | 1.003907 | **0.999985** |
| Frin hembra `tail_greet` | 1.000298 | **0.998843** |
| Frin macho `howl` / `tail_greet` | 1.000016 / 1.000529 | sin cambio (ya exactos) |
| Bunny `idle_breathing` / `groom` / `click` | 0.998861 / 1.002571 / 1.000481 | **sin cambio** |

**Por qué Bunny no cambió, honestamente.** Sus tres acciones ya recibían
su escala exacta antes de esta pasada: `groom` (0.862113) y `click`
(0.953602) están muy fuera de la tolerancia, así que nunca la tocó; e
`idle_breathing` resuelve a 1.0 por CONSTRUCCIÓN, no por la tolerancia
-- su frame 0 ES literalmente el archivo de la pose base (containment,
DEC-087), así que la escala exacta ES 1.0. Derivarla del último frame
en su lugar (1.00112) haría que el frame 0 -- el archivo compartido --
dejara de calzar: se movería el desajuste del final al arranque, peor.
El residual de Bunny que queda (máximo 0.257% en `groom`) es precisión
de resampleo/redondeo entero, no tolerancia: en pantalla son ~0.34pt
sobre un canvas de 134pt.

**Lo que esta pasada NO corrige, a propósito** (ver §5 del brief y
`docs/ANIMATION_RUNTIME.md` §15): la variación de tamaño DURANTE la
animación. Perfilando el radio RMS requerido frame a frame contra la
base, el arte real varía así:

| acción | escala requerida, rango sobre 25 frames | vs. la escala del endpoint aplicada |
|---|---|---|
| Bunny `groom` | 0.8609 - 0.8665 | medio del clip ~0.3% más grande |
| Bunny `click` | 0.9244 - 0.9611 | hasta ~3.2% más grande a mitad de clip |
| Frin macho `howl` | 1.1351 - 1.1620 | medio del clip ~1.4% más grande |
| Frin hembra `tail_greet` | 1.0093 - 1.0702 | medio del clip ~5% más chico |

Eso es SILUETA AUTORADA -- la cabeza sube, la cola se abre, el cuerpo
se estira -- no un desajuste de transforma. Aplastarlo para que el bbox
coincida con la base deformaría poses reales, que es exactamente lo que
el brief prohíbe. Se documenta con números para que quede claro qué se
corrigió (la frontera) y qué no (el interior del clip).

### DEC-099 — Identidad semántica de pose: una punta que ES la pose base se COMPILA desde la base, no se aproxima a ella
**Status:** DECIDIDO · Block 05, pasada de simplificación geométrica y
contratos de pose exactos.

**Contexto.** QA manual del owner, tras seis pasadas consecutivas de
corrección de geometría: Nidir PERFECTO (tamaño estable, animaciones
estables, sin pérdida de pixeles); Bunny y Frin con el defecto residual
de siempre -- "cuando corre una animación se ve un poco más gordo/más
grande que quieto, y al volver a la base se ve más flaco otra vez". Y,
para `lie_to_sit`, tres intentos acumulados y ningún resultado
aceptado: primero saltaba, después la interpolación lo hacía derivar
por la ventana (DEC-096, rechazado), después el anclaje rígido movió la
discontinuidad al final (DEC-097). El owner rechazó explícitamente esa
complejidad acumulada y pidió SIMPLIFICAR: preferir borrar antes que
agregar otra capa matemática.

**Diagnóstico: por qué Nidir era estable y Bunny/Frin no.** Medido, no
supuesto. Los tres exports de Nidir traen al personaje al MISMO tamaño
de mundo -- sus bounding boxes de contenido nativo son 436x500 (idle),
437x500 (click_reaction) y 437x497 (wing_stretch), pese a vivir en
canvas nativos muy distintos (513x525, 624x612, 624x519). La diferencia
entre ellos es del 0.2%, dentro de `scale_tolerance`, así que TODOS los
grupos de Nidir se compilan con `content_scale` exactamente 1.0: Nidir
tiene, de hecho, UN SOLO sistema de coordenadas y ningún reescalado
por-animación.

Bunny y Frin no. Sus exports vienen a escalas de mundo distintas Y con
las dos proporciones en desacuerdo:

| par (base -> acción) | ratio W | ratio H | anisotropía | error con la mejor escala uniforme |
|---|---|---|---|---|
| nidir idle -> click | 0.9977 | 1.0000 | 0.23% | +0.11% / -0.11% |
| nidir idle -> wing_stretch | 0.9977 | 1.0060 | 0.83% | +0.42% / -0.41% |
| bunny idle -> groom | 0.8548 | 0.8642 | 1.08% | +0.55% / -0.54% |
| bunny idle -> click | 0.9359 | 0.9550 | 2.00% | +1.01% / -1.00% |
| frin macho base -> howl | 1.1190 | 1.1605 | **3.58%** | **+1.84% / -1.80%** |
| frin hembra base -> howl | 0.9825 | 1.0000 | 1.75% | +0.89% / -0.88% |

Confirmado por un segundo método independiente (ajuste de IoU con ejes
X/Y libres sobre los frames COMPILADOS): Nidir da anisotropía 0.00%,
mientras Frin macho `howl` da 3.96% y Bunny `click` 2.97% -- y en los
dos casos el IoU MEJORA al soltar los ejes (0.9656 -> 0.9851 para
`howl`), que es la firma de una diferencia real de escala por eje, no
de una diferencia de pose.

Ninguna transforma uniforme puede satisfacer los dos ejes a la vez, y
§7 del brief prohíbe escala no uniforme (nada de squash/stretch: eso
deformaría poses reales). Ese residual -- hasta ±1.8% en el peor caso,
`howl` del macho, contra ±0.42% del peor caso de Nidir -- es DEUDA DE
EXPORT y se reporta como tal. **No se declara resuelto por ser
pequeño**: la QA visual del owner es la única puerta de aceptación.

**Decisión.** Dejar de preguntar "¿qué tan cerca quedó esta punta de la
pose base?" y empezar a declarar "esta punta ES la pose base".

Dos campos nuevos, opcionales, por acción, solo de tiempo de build:
`first_frame_is_state_base` y `last_frame_is_state_base`, cada uno con
el id del estado cuya pose estable representa esa punta. Cuando están,
el compilador toma esa punta del ARCHIVO de esa base y con la
transforma de esa base -- el frame compilado sale idéntico byte a byte
al que el runtime muestra con el pet quieto. Sin métrica, sin residual,
sin tolerancia en la frontera. Los PNG fuente NO se tocan: la
sustitución es una referencia de tiempo de compilación, así que la
provenance del arte se preserva intacta.

**Lo que se ELIMINÓ** (no reemplazado por otra corrección: borrado):

| mecanismo | qué hacía | DEC |
|---|---|---|
| `transition_target_entry`/`last_frames` | anclar por último frame contra la base destino | DEC-087 |
| `align_endpoint_to_target_base` | extender ese anclaje a self-loops | DEC-092 |
| `alpha_registration_point` | punto de registro por centroide de alpha | DEC-093 |
| `scale_from_last_frame_entries` | derivar la escala del último frame | DEC-095 |
| `strict_scale_entries` | saltear `scale_tolerance` en la frontera | DEC-098 |
| `anchor_start_to_source_base` | anclar por primer frame contra la base de origen | DEC-097 |

Los seis existían para el mismo fin -- aproximar una punta a una pose
base -- y ninguno podía llegar a exacto: el resampleo entero deja su
propio residual (medido: hasta 0.26% en Bunny, irreducible por más
precisión de medición que se le ponga).

**La prueba de que el diagnóstico era correcto:** quitar los seis
mecanismos NO movió un solo byte del pack de Nidir (sha256
`594da545…` sin cambios). Nidir nunca los usó. Eran, literalmente, la
diferencia entre el pet estable y los inestables.

**Lo que se CONSERVÓ, y por qué.** El union-find de escala por archivo
compartido, el containment de colocación, el canvas de trabajo
compartido y la escala uniforme por grupo siguen siendo necesarios para
meter exports de distinto canvas nativo en un solo sistema de
coordenadas -- eso es infraestructura, no compensación.

**Un matiz que la primera versión de esta pasada se llevó por delante.**
Eliminar TAMBIÉN el anclaje de arranque resultó ser una simplificación
de más, y se midió: el frame 0 sustituido y el frame 1 autorado de
`lie_to_sit` quedaron a 66.3px (macho) / 61.8px (hembra), y el canvas
de trabajo compartido se infló de 657 a 767px de alto, encogiendo a
Frin ~14% en pantalla. La causa es real y no es una tolerancia: la
colocación por default centra el contenido del frame 0 en el ancla
compartida, lo que asume que la pose de arranque del clip VIVE en esa
ancla. Es cierto para todo pet de un solo estado; es falso para un clip
que arranca en otro estado (la pose acostada hereda la colocación de
`sit_to_lie`, no vive en el ancla).

La corrección NO fue devolver `anchor_start_to_source_base`, sino
DERIVAR la colocación de la declaración que ya existe: si el contenido
dice "mi frame 0 ES la pose base del estado X", entonces dónde vive esa
base es dónde tiene que arrancar el clip. Una sola declaración de
contenido, dos consecuencias coherentes (qué pixeles y qué sistema de
coordenadas), cero flags nuevos. Para un pet de un solo estado es un
no-op exacto -- su base ya está en el ancla compartida --, y eso se
verifica: Bunny recompila byte-idéntico con y sin el mecanismo.

**Resultado en `lie_to_sit`.** Las DOS puntas son ahora exactas, cosa
que ninguna transforma rígida podía lograr. El desajuste real del
export no desapareció -- se absorbió DENTRO del clip, donde el lobo ya
está en movimiento, en vez de en el instante en que queda quieto. Ese
residual interno se mide y se fija en un test; el owner decidió
explícitamente NO regenerar ninguna animación de Ludo, así que queda
como deuda conocida y aceptada.

**Lo que esta pasada NO corrige, a propósito.** La anisotropía de
export de la tabla de arriba. Con escala uniforme obligatoria, `howl`
del macho se muestra ~1.8% más ancho y ~1.8% más bajo que la base
durante todo el clip. Eso es lo que queda del "se ve más gordo", y no
se puede quitar sin deformar el arte. Las opciones honestas son
convivir con ello o re-exportar; el owner eligió no re-exportar.

### DEC-100 — Corrección de ASPECTO de export, constante por secuencia: la escala uniforme dejó de ser sagrada
**Status:** DECIDIDO · Block 05, pasada de consistencia visual final.

**Contexto.** Tras DEC-099 las puntas semánticas son exactas y las seis
capas de aproximación están borradas, pero QA del owner seguía viendo
lo mismo: Bunny "muy levemente más ancho/gordo" mientras anima, y
`howl` de Frin "más gordo" que la pose sentada. `tail_greet`, en
cambio, "geometría BUENA" -- el mejor de Frin. Nidir, perfecto.

**Lo que las pasadas anteriores prohibían.** Escala no uniforme, para
no deformar poses autoradas. Este brief levantó explícitamente esa
restricción *si la medición prueba* que el export entero usa un sistema
de proporciones distinto.

**La medición.** Momentos de segundo orden ponderados por alpha
(`alpha_weighted_sigma()`), sobre los PNG NATIVOS, comparando cada
secuencia contra la pose base estable que su propio contenido declara.
El desacuerdo entre ejes (`sx/sy - 1`) en las DOS puntas de reposo:

| secuencia | punta f000 | punta f024 | ¿consistente? |
|---|---|---|---|
| nidir `click_reaction` | -0.30% | +0.06% | no (ruido, ~0) |
| nidir `wing_stretch` | -0.69% | -0.56% | sí, pequeño |
| bunny `groom` | -1.49% | -1.28% | **sí** |
| bunny `click_reaction` | -2.37% | -2.20% | **sí** |
| frin macho `howl` | **-3.90%** | **-3.90%** | **sí, exacto** |
| frin macho `tail_greet` | +0.20% | +0.21% | sí, ~0 |
| frin hembra `howl` | -2.13% | -2.05% | **sí** |
| frin hembra `tail_greet` | -0.71% | -0.76% | sí, ~0 |
| frin macho `lie_to_sit` | -1.45% | -1.44% | **sí** |

Que dos frames autorados independientes, separados por 3 segundos de
animación, den el MISMO desacuerdo es la firma de una propiedad
constante del export -- no de una pose. Contraste directo:
`idle_breathing` de Bunny, que comparte export con la base, da 0.00% y
+1.19% en sus dos puntas; ahí la diferencia SÍ es pose (el conejo
respira).

**Triangulación (la prueba decisiva).** Frin macho tiene TRES exports
independientes de la misma pose sentada de reposo. Comparados entre sí
por relación de aspecto: base ↔ `tail_greet` = **-0.20%** (coinciden);
base ↔ `howl` = **+4.05%**; `tail_greet` ↔ `howl` = **+4.26%**. Dos
exports independientes coinciden y el tercero discrepa con los dos: el
sistema de proporciones anómalo es el de `howl`, no el de la base.

**Verificación con frames intermedios.** Perfilando el clip entero,
`howl` nunca cruza cero (-2.7% a -5.5% en los 25 frames), mientras
`tail_greet` oscila entre -21% y +0.2% -- su cola se abre, y eso es
animación real. Confirma dos cosas a la vez: que `howl` arrastra un
offset constante, y que **medir frames intermedios para derivar una
corrección sería confundir animación con error**. Por eso la derivación
usa exclusivamente las dos puntas de reposo.

**Decisión.** Nuevo campo declarativo por acción,
`match_aspect_to_stable_poses`. Cuando está, el compilador deriva UN
par constante (scale_x, scale_y) de la correspondencia de poses
estables que el contenido YA declara (`first_frame_is_state_base` /
`last_frame_is_state_base`) y lo aplica a toda la secuencia. Sin número
mágico en ningún generador: el par sale de los pixeles.

Requisitos duros: constante para todos los frames, nunca por frame,
nunca interpolado, nunca progresivo. Esto NO es deformar una pose -- es
lo contrario: devuelve la secuencia al sistema de proporciones de la
pose base, del que su export se había ido.

**Quién lo declara, y quién no.** Bunny `groom` y `click`; Frin
`howl` (macho y hembra) y `lie_to_sit`. NO lo declaran: `tail_greet`
(control negativo -- su geometría está aprobada por QA y su export ya
coincide), `sit_to_lie` (congelado, y sus dos puntas comparten archivo
real con las bases), `idle_breathing` de Bunny (comparte export con la
base; su variación es respiración autorada), y NINGUNA secuencia de
Nidir.

**Resultado medido** (error de aspecto TAL COMO SE MUESTRA, sobre el
pack compilado; positivo = se ve más ancho relativo a alto = "más
gordo"):

| secuencia | antes | después |
|---|---|---|
| frin macho `howl` | +2.7% a +5.5% | **media -0.64%, rango [-1.43, +1.49]** |
| frin hembra `howl` | ~+2.1% | **media +0.84%** |
| bunny `click` | +2.4% a +5.2% | **media -0.57%** |
| bunny `groom` | +1.3% a +3.8% | **media +1.29%** |

**Calibración honesta con el control aprobado.** Las propias
animaciones de Nidir, que el owner llama perfectas, muestran +2.31%
(`wing_stretch`) y +2.70% (`click_fire`) en su frame más parecido al
reposo. Esa es la banda que el owner YA acepta, medida y no inventada.
Las cuatro secuencias corregidas quedan dentro o por debajo. **Eso no
prueba que el defecto se resolvió** -- lo prueba la QA visual del
owner; sirve para saber qué magnitud es plausible.

**Límite que queda.** `groom` de Bunny conserva ~+1.3% de media: sus
puntas de reposo justifican una corrección de -1.39%, pero el cuerpo
del clip pide ~-2.7%. Derivar del promedio del clip está descartado --
`idle_breathing`, cuyo export es el de la base y cuya corrección
correcta es exactamente cero, daría +4.38% por ese método. Las puntas
de reposo son el único estimador sano.

### DEC-101 — `lie_to_sit`: reparación de FRONTERA a nivel de contenido, no de movimiento
**Status:** DECIDIDO · Block 05, pasada de consistencia visual final.

**El defecto, esta vez identificado con precisión.** QA: "cerca del
FINAL de la animación de levantarse, el frame autorado está visiblemente
en otro lugar que la pose sentada final; después aparece la base
sentada en el lugar correcto" -- sin que el usuario moviera la ventana.

Perfilando el pack compilado frame a frame apareció la forma exacta del
problema. El lobo termina de levantarse y **se queda quieto** varios
frames (pasos de centroide <=0.55px desde f017 en el macho, <=1.04px
desde f020 en la hembra) pero quieto a **~26px (macho) / ~6px (hembra)
del lugar** donde está la pose sentada real, porque el root-motion de
este export no cierra contra el de `sit_to_lie`. Recién al terminar
saltaba a su sitio. Casi un segundo inmóvil en el lugar equivocado, y
después un teletransporte: por eso se ve tanto.

**Decisión.** Nuevo campo declarativo `stable_pose_tail_frames`: cuántos
frames FINALES representan la pose estable declarada, no solo el
último. El compilador los compila desde el archivo de esa base.

Macho: 8 (f017..f024). Hembra: 5 (f020..f024). Distintos porque los dos
exports son distintos -- se eligieron del perfil medido, en el primer
frame donde el lobo ya llegó a la pose sentada Y dejó de moverse, de
modo que **ningún frame autorado que todavía se mueva se pierde**.

**Lo que esto NO hace:** no interpola, no suaviza, no recentra
progresivamente, no mueve la ventana, no sintetiza arte nuevo, no toca
un solo PNG fuente. El clip conserva sus 25 frames y sus 3.0s exactos.

**Resultado.** Ya no queda ningún frame en que el lobo esté INMÓVIL en
el lugar equivocado: desde que muestra la pose sentada, no se desplaza
más (medido: 0.00px). El desajuste del export no desapareció -- se
reubicó a UN solo paso (24.46px macho / 5.59px hembra) que ocurre
mientras el lobo todavía está en movimiento (su propio paso autorado
ahí es 2.90px / 2.58px). La magnitud del paso apenas bajó (25.85 ->
24.46 en el macho); **lo que cambió es CUÁNDO ocurre**, y eso es lo que
el owner reportó como el problema. La QA visual decide si alcanza.

### DEC-102 — Ganancia de color constante por secuencia: `tail_greet` del macho viene más oscuro del export
**Status:** DECIDIDO · Block 05, pasada de consistencia visual final.

**Diagnóstico antes de tocar nada.** QA: `tail_greet` tiene buena
geometría pero se ve algo más oscuro que la pose sentada. Se midió el
color medio de los pixeles INTERIORES (alpha>=250) -- deliberadamente
sin el borde antialiaseado, que arrastra la media cuando dos exports
tienen distinta resolución nativa, y que de paso vuelve la medición
inmune a si el borde está premultiplicado o no.

| | R | G | B |
|---|---|---|---|
| macho `tail_greet` f000 vs base | -2.5% | -2.8% | -2.4% |
| macho `tail_greet` f012 vs base | -3.9% | -5.0% | -4.1% |
| macho `howl` f000 vs base | -0.3% | -0.3% | +0.2% |
| hembra `tail_greet` f000 vs base | -0.4% | -0.7% | +0.4% |

**Causa: el PNG fuente (A).** Descartadas las otras: el pipeline de
compilación se midió fiel a **+0.12%/+0.15%/+0.18%** (fuente contra
pack compilado, mismos pixeles interiores), así que no hay
oscurecimiento por downscale ni por compilación; y como la medición es
solo de interior, ni el formato de textura ni el blending ni el
premultiplicado entran en juego. **Ningún cambio bajo `src/`.**

Es un problema del MACHO. La hembra está en -0.5%, dentro del ruido y
del mismo orden que su propio `howl` (que nadie reportó): no se le
aplica nada, porque corregir ahí sería resamplear para nada.

**Decisión.** Campo declarativo `match_color_to_stable_poses`: el
compilador deriva UNA ganancia RGB constante de la misma correspondencia
de poses estables, y la aplica a toda la secuencia sobre los pixeles
nativos. Derivada para el macho: **(1.0265, 1.032, 1.024)**.

Alpha nunca se toca. La ganancia no varía entre frames (nada de
auto-exposición). Las puntas sustituidas NO la reciben: ya SON los
pixeles de la base.

**Resultado.** Frames de reposo del macho: -2.5% -> **+0.20%/+0.14%/
+0.06%**. Mitad de clip: -3.9%/-5.0%/-4.1% -> -1.5%/-2.1%/-1.9%. Ese
resto es sombreado AUTORADO (el lobo gira, la cola tapa luz) y se
conserva a propósito -- aplanarlo sería auto-exposición, que es
justamente lo prohibido.

### DEC-103 — Frin macho: escala visual por-variante, +5% sobre la escala común
**Status:** DECIDIDO · Block 05, pasada de pulido final.

**Contexto.** Pedido de producto explícito: "increase ONLY Frin male's
final on-screen size by exactly +5%... female stays exactly the current
size." `VISUAL_SCALE` (ver DEC-076/087) era hasta acá un único valor
por-pet compartido por las dos variantes de Frin.

**Decisión.** Se generaliza a un multiplicador por-variante,
`VARIANT_VISUAL_SCALE_MULTIPLIER = {"male": 1.05, "female": 1.0}`,
aplicado sobre el `VISUAL_SCALE` común existente (1.05) en el momento
de armar el manifest: `visual_scale = VISUAL_SCALE *
VARIANT_VISUAL_SCALE_MULTIPLIER[variant]`. Resultado: macho 1.1025,
hembra 1.05 sin cambio.

Puramente de runtime -- ningún PNG fuente, ningún `content_scale`,
ningún canvas lógico se toca (verificado: canvas lógico del macho sigue
siendo 153x176, idéntico a 3b4fa66). `visual_scale` es, por diseño
desde DEC-076, el único lugar donde una diferencia de tamaño
puramente de PRODUCTO entre variantes debe vivir (se aplica solo en
`SpikeApp::EffectiveCanvasWidth()/Height()`) -- exactamente el
mecanismo genérico ya existente, generalizado de un escalar a un
diccionario por-variante, sin ninguna rama `if pet.id == "..."` en
runtime C++.

Tamaño en pantalla resultante: macho 161x185pt (3b4fa66) -> **169x194pt**
(+5.0% exacto en los dos ejes, verificado 161*1.05=169.05,
185*1.05=194.25); hembra sin cambio, 168x185pt.

### DEC-104 — `lie_to_sit`: re-derivación rigurosa del corte terminal, y el piso geométrico del macho confirmado
**Status:** DECIDIDO · Block 05, pasada de pulido final.

**Contexto.** QA del owner sobre DEC-101 (Block 05, pasada anterior):
el salto terminal de `lie_to_sit` sigue siendo visible en las dos
variantes. Explícito: "'move the jump earlier while the wolf is
moving' is NOT sufficient. We need the terminal portion itself to LOOK
continuous" -- y el criterio de selección de DEC-101 ("primer frame ya
detenido") queda invalidado por el propio brief como insuficiente.

**Metodología, rigurosa y verificada dos veces.** Búsqueda EXHAUSTIVA
sobre los 25 candidatos posibles de frontera, sobre el pack COMPILADO
con la corrección de aspecto ya aplicada (no sobre PNG nativos
sueltos, para medir exactamente lo que el runtime muestra). Para cada
candidato: distancia de centroide del último frame autorado a la base
sentada, Y el paso propio de entrada a ese frame (cuánto se movió
llegando a él). Un candidato SOLO califica si ese paso es > 0.5px --
cortar sobre un frame ya inmóvil reproduce el patrón "quieto en el
lugar equivocado, después salta" que toda esta familia de
correcciones (DEC-101 en adelante) existe para eliminar. Entre los que
califican, se elige el de menor distancia. La búsqueda se implementó
DOS VECES independientemente (una vez a mano durante el diagnóstico,
otra vez como test automatizado que recompila el manifest con
`stable_pose_tail_frames=1` y reproduce la búsqueda completa) y ambas
coinciden exactamente -- ver
`LieToSitTerminalBoundaryRederivationTest` en
tools/test_asset_pipeline.py.

**Un hallazgo honesto, no cosmético: el MACHO no mejora.**

| first_tail | último autorado | distancia a S | paso propio | ¿califica? |
|---|---|---|---|---|
| 16 | 15 | 26.85px | 0.67px | sí |
| **17** | **16** | **24.46px** | **2.90px** | **sí — mínimo entre los que califican** |
| 18 | 17 | 24.28px | 0.44px | no (casi inmóvil) |
| 19-24 | 18-23 | 24.56-25.37px | 0.07-0.55px | no |

El candidato de 18 (24.28px) es marginalmente más cercano en distancia
pura, pero corta sobre un frame que apenas se movió (0.44px) -- lo
descarta el criterio de "seguir en movimiento visible". Ningún frame
de las 25 posiciones del clip queda genuinamente más cerca de la base
sentada que 24.46px. **3b4fa66 (8 frames de cola) YA ERA el óptimo.**
Esto es un PISO GEOMÉTRICO real de este export (medido, no supuesto):
el root-motion de `lie_to_sit` del macho simplemente no converge más
cerca de la pose sentada real, y ninguna elección de frontera dentro
de las reglas (sin síntesis, sin interpolación, sin re-export) lo
reduce. Se confirmó visualmente: comparando los frames 16/17
(autorados) contra S con una superposición alpha roja/verde, el lobo
está desplazado ~24px hacia la izquierda de forma consistente, sin
diferencia de escala significativa (bounding box dentro del 5%).

**La HEMBRA sí tenía margen real.**

| first_tail | último autorado | distancia a S | paso propio | ¿califica? |
|---|---|---|---|---|
| 19 | 18 | 8.16px | 0.05px | no |
| **20** | **19** | **5.59px** | **2.58px** | **sí (3b4fa66)** |
| **21** | **20** | **5.32px** | **1.02px** | **sí — mínimo, nuevo valor** |
| 22-24 | 21-23 | 5.35-5.39px | 0.09-0.33px | no |

Nuevo valor: 4 frames de cola (antes 5). Reducción real: 5.59px ->
5.32px (~4.8%), y se preserva UN frame autorado más (idx19, antes
sustituido, ahora visible).

**Una opción explícitamente considerada y NO implementada.** El
mecanismo de colocación de `lie_to_sit` ancla el clip por su PRIMER
frame contra `lying_base` (DEC-099, vía `first_frame_is_state_base`).
Anclar por el ÚLTIMO frame en su lugar (contra `seated_base`)
desplazaría el residual completo -- misma magnitud, ~24-26px -- desde
el FINAL del clip hacia el INICIO (la transición frame0->frame1, justo
al hacer click). Se investigó y se descartó deliberadamente: el frame
0 ya es exacto bajo CUALQUIER anclaje (sustitución de frame canónico,
DEC-099, independiente de la colocación del resto del clip), así que
hoy el inicio de `lie_to_sit` es perfectamente continuo; cambiar de
anclaje introduciría un defecto NUEVO exactamente donde hoy no hay
ninguno, a cambio de resolver uno que el owner no reportó ahí. Mover
el defecto no es lo mismo que eliminarlo -- exactamente lo que este
mismo brief advierte explícitamente que no alcanza ("move the jump
earlier... is NOT sufficient"). Documentado acá para que quede
disponible si el owner prefiriera esa opción a futuro, con los números
exactos arriba.

**Conclusión honesta para el informe de este bloque:** el macho
requiere, para eliminar (no solo minimizar) su defecto terminal, un
re-export de `lie_to_sit` en Ludo cuyo root-motion cierre contra
`sit_to_lie` -- la recomendación que DEC-101 ya hacía y que sigue en
pie. La hembra queda con una mejora real y medible, y su residual
(5.32px, ~3.2pt en un pet de 185pt) es probablemente ya imperceptible.

### DEC-105 — `lie_to_sit` terminal: sustitución de cola reemplazada por traslación rígida de los frames autorados reales
**Status:** DECIDIDO · Block 05, pasada de corrección posicional final.

**Contexto.** QA sobre DEC-101 (que substituía los últimos N frames de
`lie_to_sit` por copias congeladas de la base sentada): "the wolf is
already almost/fully seated but is located a few pixels away from the
correct stable seated position... TAKE THE EXISTING AUTHORED FRAME AND
DRAW/COMPOSE IT A FEW PIXELS TO THE CORRECT X/Y LOCATION" -- explícita
la petición de trasladar, no reemplazar. El owner rechaza otro
re-export de Ludo, otra vez.

**Mecanismo, genérico.** Nuevo campo declarativo por-animación,
`terminal_rigid_translation: {start_frame, dx, dy}`. Desde
`start_frame` en adelante, el compilador suma `(dx, dy)` a la
colocación de CADA frame en pixeles del frame COMPILADO -- los mismos
pixeles autorados, compuestos en otro lugar del mismo canvas de
trabajo compartido. Nunca escala, nunca interpola, nunca depende del
índice del frame más allá de este único corte binario: exactamente DOS
offsets posibles en toda la animación (`None` antes de `start_frame`,
`(dx, dy)` desde ahí), verificado interceptando `_compile_frame()`
real. Nunca toca un frame ya cubierto por
`first_frame_is_state_base`/`last_frame_is_state_base` (DEC-099): esos
siguen usando el archivo Y la colocación exactos de su base, sin
intervención.

Esto NO es la interpolación por-frame que DEC-097 retiró: aquella
calculaba un offset DISTINTO para cada frame (una función continua del
índice) para forzar una continuidad de root-motion que el export no
tiene, y produjo el "todo el personaje deriva por la ventana" que QA
rechazó. Acá hay exactamente DOS valores constantes, la misma
distinción que ya regía `content_scale`/`stable_pose_tail_frames`.

**Prueba matemática de por qué el K óptimo es el MISMO que en DEC-104.**
Anclar el frame K contra la base sentada (`dx,dy = base -
centroide(frame K)`, lo que este mecanismo hace) produce en el borde
K-1 -> K EXACTAMENTE el mismo salto (`|base - centroide(frame K-1)|`)
que sustituir K directamente por la base -- la posición de K-1 no
cambia en ninguno de los dos esquemas. Verificado: la búsqueda
exhaustiva de DEC-104 (K=17 macho, K=21 hembra, bajo el mismo criterio
"minimizar distancia sujeto a paso de entrada > 0.5px") sigue siendo
óptima acá, re-confirmada con un test que recompila y reproduce la
búsqueda completa sobre el pack de referencia SIN la corrección.

**dx/dy medidos por dirección, nunca asumidos por espejo.** El brief
lo exige explícitamente ("Do not assume. Verify from the compiled
packs"): se midieron los dos runtime directions por separado sobre el
pack YA compilado.

| variante | dirección | dx | dy |
|---|---|---|---|
| macho | right | +23.444248 | -6.326784 |
| macho | left | -23.516887 | -6.325220 |
| hembra | right | -1.714820 | -5.111141 |
| hembra | left | +1.503224 | -5.095938 |

`dx` se invierte de signo entre direcciones (el espejado horizontal
invierte X), pero NO es exactamente simétrico -- macho: 23.444 vs
23.517, una diferencia real de 0.07px, la asimetría genuina del
contenido, no un artefacto de redondeo. `dy` mantiene signo y magnitud
similar entre direcciones (el espejado es horizontal, no vertical).

**Resultado: el salto NO se redujo -- lo que se muestra después SÍ
cambió.** Midiendo distancia a la base sentada por frame, ANTES (con
DEC-104, sustitución):

| macho | f016 | f017 | f018 | ... | f023 | f024 |
|---|---|---|---|---|---|---|
| distancia | 24.46px | 0.00px (=base) | 0.00px | ... | 0.00px | 0.00px (=base) |

DESPUÉS (esta pasada, traslación):

| macho | f016 | f017 | f018 | f019 | f020 | f021 | f022 | f023 | f024 |
|---|---|---|---|---|---|---|---|---|---|
| distancia | 24.46px | 0.55px | 0.84px | 1.34px | 1.72px | 1.88px | 1.94px | 2.08px | 0.00px (=base) |

El salto en el borde 16->17 sigue midiendo ~24px (right: 23.95px,
left: 24.90px) -- EL PISO GEOMÉTRICO de DEC-104 no desapareció, y no
podía: es el mismo argumento matemático (la posición de frame16 no
cambia bajo ningún esquema). Lo que sí cambió es que, DESDE el frame
17 en adelante, el lobo ya está VISIBLEMENTE cerca de la posición
correcta (0.5-2.1px, no 24-25px) -- y esos pixeles son la animación
REAL, no una copia congelada: los pasos entre 17-23 (0.07-0.56px) son
la misma micro-variación autorada que ya existía en el export
original, ahora VISIBLE en vez de oculta detrás de una sustitución.
Hembra: mismo patrón, salto ~5.3px en el borde 20->21, luego 0.3-0.8px
de distancia hasta el frame final.

**Sobre "does it actually get smaller, not just relocated".** La
respuesta honesta: el ÚNICO salto (16->17 macho, 20->21 hembra) no se
redujo respecto de DEC-104 -- es el mismo piso geométrico. Lo que se
ganó es que YA NO hay una segunda discontinuidad perceptual (una
"meseta" de pixeles congelados en la posición correcta que se sentía
distinta de la animación real que la precedía). Antes: [movimiento real]
-> [salto] -> [pose estática repetida 7-8 veces]. Ahora: [movimiento
real] -> [salto] -> [movimiento real, minúsculo, en la posición
correcta] -> [pose exacta]. Es la misma magnitud de salto, con
animación auténtica a ambos lados en vez de solo uno.

**El canvas de trabajo compartido no necesitó crecer.** El margen ya
calculado por `compute_frame_normalization_plan()` para cada entrada
resultó suficiente para absorber el desplazamiento de ~24-32px
(working-canvas, antes del downscale de runtime) sin disparar la
verificación de "contenido excede el canvas" (`_compile_frame()`,
DEC-075) -- no hizo falta ninguna lógica de crecimiento adicional.

---

### DEC-106 — Product UI: capa SDL propia y chica, no Electron/Qt/ImGui
**Status:** DECIDIDO · Block 06.

**Contexto.** El brief §5 exige un Product UI que se sienta como una app
de escritorio nativa, prohíbe Electron/Chromium/Qt/WebView/motor de
juego/ImGui-en-producción, y pide "solo los componentes que Nimvlets
realmente necesita, no un framework de UI general".

**Decisión.** Se extiende la arquitectura SDL3/nativa existente con
`src/productui/`: una librería PURA (`nimvlets_productui_core`:
`FocusList`, `CollectionLayout`, `Format`) más una capa SDL
(`UiPaint`, `TextCache`, `PetPreviewCache`, `CollectionView`,
`ProductWindow`). `UiPaint` implementa exactamente los primitivos que
la Collection usa: fill/stroke de round-rect por scanline, blit
contain-fit, blit de glyphs, un clip rect. Sin sistema de widgets, sin
data-binding, sin theming en runtime.

**Dependencias agregadas: ninguna.** El texto del sistema se rasteriza
con Core Text (framework del OS, no un paquete — igual que AppKit,
AGENTS.md §10). Ver DEC-107.

**Costura de plataforma.** `src/platform/TextRasterizer.h` y
`src/platform/SystemShell.h` siguen el mismo patrón que
`TransparentWindowSupport.h`: uno de macos/windows/linux se compila
según `CMAKE_SYSTEM_NAME`. macOS real; Windows/Linux stubs honestos
(`false`/no-op), no fingidos (brief §24).

---

### DEC-107 — Texto del sistema vía Core Text, straight alpha, non-ARC
**Status:** DECIDIDO · Block 06.

**Contexto.** El Product UI necesita tipografía del sistema (SF Pro en
macOS) sin `SDL_ttf` (AGENTS.md §10 lo prohíbe sin una razón concreta
documentada) y sin una fuente decorativa embebida (brief §3). El debug
text de SDL (8×8 bitmap) se ve como una herramienta de debug, lo que el
brief prohíbe explícitamente (§6).

**Decisión.** `src/platform/macos/TextRasterizer.mm` usa **Core Text**
sobre `[NSFont systemFontOfSize:weight:]` para rasterizar una string a
un bitmap RGBA8 **straight alpha** (para encajar con
`SDL_BLENDMODE_BLEND`/`SDL_PIXELFORMAT_RGBA32` como el resto del
runtime): peso regular/medium/semibold, escala de DPI, tinte de color,
recorte de una línea con "…" a un ancho máximo en píxeles, y
`MeasureTextWidth` para layout.

**Detalle que costó una pasada de QA.** El target `nimvlets_platform_macos`
NO usa ARC (igual que `TransparentWindowSupport.mm`). La primera
versión hacía `CFRelease` sobre un `NSFont` autoreleased → doble free
al drenar el pool → segfault. Corregido tratando los `NSObject*` como
prestados (los libera el `@autoreleasepool`) y CF-releaseando solo los
objetos CF (`CTLine`, `CGColor`, `CGColorSpace`, `CGContext`).

**Orientación.** El buffer de un `CGBitmapContext` sobre memoria propia
resultó ser **top-first** en esta plataforma (medido: el primer render
salía verticalmente espejado con una inversión de filas manual). Se
copia sin voltear.

**Windows/Linux.** Stubs que devuelven `false`. `src/productui` trata
`false` como "esta plataforma no dibuja texto de producto todavía" sin
crashear. DirectWrite / fontconfig+FreeType son trabajo futuro.

---

### DEC-108 — Propiedad: semilla en el catálogo (v2), autoridad en AppState (v2)
**Status:** DECIDIDO · Block 06.

**Contexto.** El brief §12 pide un estado de propiedad de desarrollo
"limpio, data-driven y testeable", que NO codifique permanentemente los
pets poseídos de hoy en lógica de runtime, y que permita la
inicialización de propiedad de un primer arranque futuro (Block 09) sin
rediseño.

**Decisión.**
- **Semilla**: `catalog::CatalogEntry::initiallyOwned` (bool), schema
  `NVCATLG1` v1→v2 (un byte por entrada). El manifest de dev marca
  Bunny + Frin; Nidir queda sin marcar → **bloqueado**. Así los tres
  estados de propiedad se ejercitan con solo los packs reales que ya
  existen — sin una entrada falsa ni arte nuevo.
- **Autoridad**: `persistence::AppState::ownedPetIds`
  (`std::vector<std::string>` canónico: ordenado, sin duplicados, sin
  vacío) + `ownershipSeeded` (bool), schema `NVSTATE1` v1→v2.
  `SpikeApp::Init()` siembra desde el catálogo **solo cuando
  `ownershipSeeded` es false**, y lo pone en true. "Nunca se
  inicializó" y "posee cero Nimvlets" son estados distintos por ese
  flag.
- **Invariante**: `catalog::EnsureActivePetOwned()` — el pet del
  escritorio siempre es propio (brief §9).
- **Por petId, no por variante**: poseer "frin" da acceso a macho y
  hembra (un Nimvlet lógico).
- **Frontera para el Shop (Block 07)**: comprar = agregar un `petId` a
  `ownedPetIds` + descontar `clickBalance`. `catalog::CollectionModel`
  (`kActive`/`kOwnedInactive`/`kLocked`) NO cambia.

`catalog::CollectionModel` (puro) deriva la vista de álbum: agrupa las
filas del catálogo por `petId` lógico, colapsa las variantes de Frin.
13 tests puros nuevos (`CollectionModelTest` + `CollectionLayoutTest`).

---

### DEC-109 — AppState schema v2 CON migración hacia adelante mínima desde v1
**Status:** DECIDIDO · Block 06. Supersede la nota de Block 03
"sin migración en este bloque" para `persistence::AppState`.

**Contexto.** Block 06 agrega 5 campos a `AppState` (ownedPetIds,
ownershipSeeded, lockPosition, sizeChoice, opacityPercent). El contrato
de Block 03 (`docs/PERSISTENCE.md` §3) era "cualquier `schemaVersion`
distinto de la actual se trata como corrupto → defaults seguros" —
lo que borraría el click balance y la posición de ventana del owner al
actualizar.

**Decisión.** `DeserializeAppState` ahora lee **v1 O v2**. Un archivo
v1 se lee con su layout viejo (magic + version + el cuerpo de Block 03)
y los campos v2 quedan en su default; `outState.schemaVersion` se fija
a la versión actual, así que el próximo `Save()` lo reescribe como v2.
Migración de una sola vez, sin lógica de conversión más allá de "los
campos nuevos arrancan en su default". El balance y la posición
sobreviven la actualización.

Una versión más nueva desconocida (v3+) o basura sigue tratándose como
"no se puede usar este dato" → defaults, igual que antes.

El catálogo (`NVCATLG1`) NO migra: es un artefacto de build que se
recompila desde su manifest en el mismo commit, no datos del usuario.

3 tests nuevos: v1→v2 forward, round-trip de campos v2, normalización
canónica de `ownedPetIds`.

---

### DEC-110 — Tamaño de usuario: multiplicador SOBRE visualScale, no un reemplazo
**Status:** DECIDIDO · Block 06.

**Contexto.** El brief §15 pide un conjunto finito (Small/Medium/Large)
"o un modelo compatible con la arquitectura de `visual_scale`
existente". `visualScale` es dato de CONTENIDO por-pet, congelado en
Block 05 (brief §21).

**Decisión.** `core::PetSizeChoice` (puro) es un MULTIPLICADOR encima
de `visualScale`, nunca lo edita:
`tamaño = canvasW · visualScale · factor`. **Medium = 1.00 exacto** —
un owner que nunca toca el control ve el pet idéntico a antes de Block
06, sin cambio de comportamiento. Small = 0.80, Large = 1.30
(documentado en `docs/PRODUCT_UI.md` §8). Se persiste como string
legible; un valor desconocido → "medium".

Opacidad: conjunto finito {100, 85, 70, 55} %, `SDL_SetWindowOpacity`;
55 % es el piso (por debajo el pet cuesta encontrar/clickear). Un valor
arbitrario se ajusta a la opción más cercana
(`core::NormalizeOpacityPercent`).

---

### DEC-111 — Menú rápido: NSStatusItem, acciones vía SDL_EVENT_USER
**Status:** DECIDIDO · Block 06.

**Contexto.** El brief §14 pide una "presencia real en la barra de
menús de macOS" compacta, con un icono monocromo propio (sin emoji).

**Decisión.** `src/platform/macos/QuickMenu.mm`: un `NSStatusItem` en la
barra del sistema. El `NSMenu` se construye a partir de
`platform::BuildQuickMenuModel(ShellState)` — un modelo **puro**
(`nimvlets_platform_policy`, compila y se testea en cualquier host) que
`tests/QuickMenuModelTest.cpp` cubre etiqueta por etiqueta. Así el test
verifica exactamente la estructura enviada (header con el nombre del
pet, Show/Hide según estado, Collection…, Size ▸, Opacity ▸, Lock
Position checkable, Quit).

**Entrega de acciones.** Cada item accionable empuja un
`SDL_EVENT_USER` (`.type` = `SDL_RegisterEvents(1)`, `.code` =
`int(ShellAction)`) en el hilo principal. `SpikeApp::HandleEvent` lo
despacha en el MISMO event loop — sin threads, sin callbacks, sin
estado global. La ventana de producto se rutea aparte por `windowID` y
NUNCA termina la app (brief §18).

**Icono.** Dibujado por código en `MakeMenuBarIcon()` (silueta de
criatura sentada, `template` para el tinte del tema). Documentado como
**icono de desarrollo reemplazable** — cuando exista arte de marca, se
reemplaza esa función por una carga de recurso.

**Activación de app.** Nimvlets corre como accessory app (sin Dock) para
el pet. `platform::BringApplicationToForeground()`
(`activateIgnoringOtherApps:`, sin cambiar la activation policy) se
llama al abrir la Collection para que esa ventana normal reciba teclado
y clicks de contenido sin el "primer click solo activa" del sistema
(brief §6/§23). Windows/Linux: no-op.

---

### DEC-112 — Product UI event-driven: sin loop de render oculto tras cerrar
**Status:** DECIDIDO · Block 06.

**Contexto.** El brief §19 exige que, con el Product UI cerrado, el idle
del pet siga siendo liviano: "no hidden 60 FPS Product UI renderer, no
background UI polling", y prefiere comportamiento event/deadline-driven.

**Decisión.** `CollectionView` tiene un flag `dirty_` que se activa
SOLO ante un cambio real (hover, foco, scroll, abrir/cerrar detalle,
cambio de modelo, `EXPOSED`). `ProductWindow::RenderIfNeeded()` —
llamada una vez por vuelta del event loop del pet — es un no-op salvo
que `dirty_` esté activo o haya un `EXPOSED` pendiente. NO se agrega
ningún término de deadline para la ventana de producto al cálculo de
`waitMs` del loop; los eventos de SDL despiertan el loop de todos
modos. Con la Collection cerrada, `RenderIfNeeded()` ni se llama
(`if (productWindow_.IsOpen())`), y `Close()` destruye el renderer, las
texturas y los caches.

**Medido** (macOS Release): Product UI abierto en reposo → CPU ≈ 0 %.
Pet-only en reposo tras 3 ciclos open/close → CPU ≈ 0 %, RSS en la
misma banda que un arranque fresco (sin acumulación). Ver
`docs/PERFORMANCE_BUDGETS.md`.

---

### DEC-113 — Previews de la Collection: activo inyectado, inactivo carga-y-suelta, locked sin arte
**Status:** DECIDIDO · Block 06.

**Contexto.** El brief §8 quiere que "el arte del Nimvlet domine cada
entrada". Cargar el pack completo de cada pet (~46–76 MB) solo para un
thumbnail es caro (brief §19).

**Decisión.** `productui::PetPreviewCache`:
- **pet activo**: su frame de reposo se INYECTA desde `src/app` (el
  pack ya está en memoria) — cero costo extra.
- **pet poseído-inactivo**: la primera vez que se dibuja su entrada se
  carga su pack, se copia el frame 0 de la pose base (Direction::kRight)
  a una textura chica, y el `PetDefinition` se descarta de inmediato.
  Costo: una carga de pack transitoria por pet poseído-inactivo,
  pagada una vez mientras la Collection está abierta.
- **pet locked**: SIN arte (brief §8/§9 — el ejemplo del brief,
  "Sweetie / Not in your collection", tampoco tiene). Solo una caja muy
  tenue mantiene el ritmo de las tres columnas.

`Clear()` libera todas las texturas al cerrar la ventana. Con muchos
pets poseídos esto escalaría — un thumbnail precompilado o un loader en
background sería el fix; se registra como limitación conocida.

---

### DEC-114 — Tamaño "Large": 1.30 -> 1.15
**Status:** DECIDIDO · Block 06.1.

**Contexto.** QA del owner sobre Block 06: el preset Large (1.30 encima
de `visualScale`) agranda demasiado los sprites detallados; se ve
estirado.

**Decisión.** `core::PetSizeScaleFactor(kLarge)` pasa de 1.30 a **1.15**.
Small (0.80) y Medium (1.00 exacto — sigue siendo el tamaño canónico
del contenido) no cambian. El id persistido `"large"` no cambia: una
preferencia guardada de Block 06 se re-interpreta sola al nuevo factor
la próxima vez que se lee, sin migración de datos. No se toca ningún
`visualScale` por-pet, ni los assets, ni la normalización de contenido.
El hit-test y el tamaño de ventana siguen la nueva dimensión efectiva
como cualquier otro cambio de tamaño (misma ruta que Block 06).

---

### DEC-115 — Localización EN/ES: catálogo de claves semánticas, pura, en src/core
**Status:** DECIDIDO · Block 06.1.

**Contexto.** El brief pide inglés + español para el texto de interfaz,
persistido, con cambio inmediato, sin `if (lang == es)` desperdigado
por el render, y "Do NOT build a general ICU-like localization
framework".

**Decisión.** `core::Localization` (puro, sin SDL):
- `Language { kEn, kEs }`, ids persistidos `"en"`/`"es"` (nunca
  traducidos — AGENTS.md §17), `ParseLanguage` con fallback a `kEn`
  ("Otherwise default to English", brief §5).
- `enum class StringKey` — una entrada por string traducible de la UI y
  el menú. `Localized(key, lang)` devuelve un `const char*` de una
  tabla estática 2D. Sin plurales gramaticales complejos, sin
  interpolación: donde hace falta un nombre propio se concatena
  (`kUsePetPrefix` + displayName).
- Vive en `src/core` (no en productui ni platform) para que tanto
  `platform::BuildQuickMenuModel` (nimvlets_platform_policy) como
  `CollectionLayout` (nimvlets_productui_core) la consuman, y para que
  los tests corran en cualquier host.

**Qué NO se traduce** (brief §4/§16): los nombres propios de Nimvlet
(Bunny, Nidir, Frin, ...), los términos de marca "Nimvlets"/"Nimvlet",
y "clicks"/"clics" NUNCA se vuelve "coins"/"monedas". Los endónimos de
idioma ("English"/"Español") se muestran siempre en su propio idioma.

**Idioma inicial** cuando el owner nunca eligió: `SpikeApp` mira
`SDL_GetPreferredLocales()` (ya disponible, sin maquinaria nueva) y
distingue solo es/en; el resultado NO se persiste. Una elección
explícita desde el menú Language sí se persiste y gana desde ese
momento. `AppState::language == ""` es justamente "nunca elegido".

**Cambio inmediato:** elegir idioma en el menú actualiza `language_`,
persiste la elección, y re-empuja `ShellState` (el shell reconstruye el
NSMenu) + `ProductWindow::SetLanguage` (la vista redibuja). Sin
reinicio. El id del widget con foco es semántico, así que el foco
sobrevive el re-etiquetado.

---

### DEC-116 — AppState schema v3: idioma persistido
**Status:** DECIDIDO · Block 06.1. Extiende DEC-109.

**Decisión.** `kCurrentSchemaVersion` 2 -> 3; se agrega
`AppState::language` (string, `""` = nunca elegido). `DeserializeAppState`
generaliza la migración hacia adelante de DEC-109: lee cualquier versión
en `[1, kCurrentSchemaVersion]` (hoy 1, 2 y 3). Un archivo v1 o v2 se
lee con su layout, los campos de versiones posteriores quedan en su
default (`language = ""`), y `schemaVersion` se marca como el actual
para que el próximo `Save()` lo reescriba como v3. El click balance, la
posición de ventana, la propiedad y las preferencias de tamaño/opacidad/
lock **sobreviven** la actualización intactas. Una versión más nueva
desconocida (v4+) o basura sigue tratándose como dato inutilizable.

---

### DEC-117 — Collection: composición HERO + GALLERY, con acento de identidad por pet
**Status:** DECIDIDO · Block 06.1. Supersede la parte VISUAL de DEC-113
y el layout de grid uniforme de Block 06 (la funcionalidad de Block 06
—propiedad, switching, variantes, ciclo de vida— NO cambia).

**Contexto.** QA del owner: la Collection de Block 06 funciona bien pero
se ve "como una primera versión funcional, no un producto Nimvlets
distintivo" — un grid plano y uniforme con PNGs insertados.

**Decisión.** El Nimvlet seleccionado se vuelve el PROTAGONISTA visual:

- **Hero**: arte grande a la izquierda sobre una forma orgánica muy
  tenue teñida con el acento del pet (óvalo; round-rect apenas más
  angular para Nidir), nombre grande, etiqueta de especie PROVISIONAL
  (`ProvisionalSpecies` — "Rabbit"/"Black dragon"/"Wolf" tomadas de la
  prosa de PRD_V1 §3, EN+ES; NINGUNA personalidad autorada, brief §14),
  estado, selector de variante TIPOGRÁFICO ("Male · Female" con
  subrayado de acento, no dos botones — brief §13), y una sola acción.
- **Gallery**: los demás Nimvlets en un cluster centrado y discreto —
  arte chico, nombre, estado conciso. Un click promueve a hero.
- **Acento por pet** (`productui::PetAccent`, puro): SOLO tiñe la forma
  del hero, la línea de foco/selección, y el subrayado de variante.
  Bunny apricot, Nidir violeta apagado, Frin azul hielo (brief §9); el
  resto de Nimvlets con ids TENTATIVOS. NUNCA recolorea toda la UI, sin
  gradientes.
- **Locked**: su arte se muestra más callado (alpha 150), NO grayscale
  agresivo ni destruido (brief §12). `PetPreviewCache` ahora carga
  también los packs de pets locked. Sin acción de compra, sin precio.
- Ventana 760x540 -> **800x560** (brief §18); el contenido cabe sin
  scroll.
- Jerarquía por tamaño/peso/espacio, no por más contenedores
  (brief §7/§17). El bloque de texto del hero se centra verticalmente
  contra el arte para no quedar "pesado arriba".

Todo el layout sigue siendo puro y testeable (`CollectionLayout` +
`CollectionLayoutTest`).

---

### DEC-118 — Microinteracción de hover: instantánea, no animación temporizada
**Status:** DECIDIDO · Block 06.1.

**Contexto.** El brief §11 pide un micro-lift de hover de ~2-3pt y una
transición de ~120-160ms, PERO explícitamente condicionado: "if the
current event-driven rendering architecture can support it without
creating a permanent render loop" y "Performance architecture outranks
decorative motion".

**Decisión.** El hover sobre una entrada de la gallery aplica de
inmediato: un lift de 2pt (el layout desplaza `art`/`name`/`status` de
esa entrada hacia arriba cuando `in.hoverPetId` coincide), un wash de
fondo sutil, y más contraste en nombre/estado. **Sin tween temporizado.**

Se DESCARTÓ la animación de 120-160ms: implementarla requeriría un
deadline de render en el loop del pet (o un tick propio de la ventana
de producto) activo mientras dura cada animación de hover. Aunque sea
un deadline acotado y no un loop permanente, agrega estado y superficie
de riesgo a un pase que es de pulido visual, para un beneficio
puramente decorativo que el brief mismo subordina a la arquitectura de
performance. El cambio de estado instantáneo + sutil es la opción que
el brief nombra como preferible en ese caso. El modelo event-driven de
Block 06 (DEC-112) queda intacto: `ProductWindow::RenderIfNeeded()`
sigue siendo un no-op salvo `dirty_`/`EXPOSED`, sin término de deadline
nuevo.

---

### DEC-119 — Previews del Product UI: artefacto compilado liviano `"NVPREV1"`, no el pack completo
**Status:** DECIDIDO · Block 06.2. **Supersede DEC-113** (parte de
carga-y-suelta / locked-sin-arte).

**Contexto.** QA del owner sobre Block 06.1: cambiar de variante Frin
(Macho ⇆ Hembra) en el hero "se siente MUY lento". La instrumentación
del camino real (Release, esta máquina) lo confirmó: `PetPreviewCache`
abría y parseaba el pack de animación COMPLETO del pet
(`frin_male` 72,6 MB / `frin_female` 76,3 MB — todos los frames de
todas las animaciones decodificados a RGBA) solo para quedarse con UN
frame de reposo estático, y después lo descartaba. Por etapa: lectura
de archivo 45–85 ms (peor en frío), parseo 7–12 ms, upload ~0,2 ms —
**55–100 ms por primer cambio a una variante, en el hilo de render.**
No escala al roster de 8 pets; el RSS con la Collection abierta ya
llegaba a ~324 MB con dos packs decodificados transitoriamente.

**Decisión.** Un artefacto de disco por variante de catálogo,
`"NVPREV1"` (`tools/compile_pet_preview.py` / `compile_pet_previews.py`,
lector puro `productui::PreviewArtifact`): magic + versión + petId +
variantId + sourcePack + width + height + pixel_bytes + RGBA8
straight-alpha. Se deriva en el pipeline de assets del frame de reposo
canónico del pack ya compilado (estado 0, `base_animation`, frame 0,
`Direction::kRight` — el MISMO frame que el runtime mostraba), acotado a
≤320 px (paridad con `runtime_max_frame_dimension`). ~0,3–0,4 MB c/u;
**1,44 MB para los 4 vs 244 MB de packs**. Vive al lado del pack con el
mismo nombre y extensión `.nvprev` — `productui::PreviewPathForPack`
usa esa convención en runtime, así que el formato binario del catálogo
NO cambia ni necesita migración.

`PetPreviewCache::LoadBundle` los carga TODOS de una vez al abrir la
ventana (`ProductWindow::Open`); `Get(petId, variantId)` pasa a ser un
lookup en un mapa sobre texturas ya residentes. `SetActive` sigue
inyectando el frame de reposo real a resolución completa del pet activo
(su pack ya está en RAM). Un pet locked ahora SÍ tiene arte (su
`.nvprev`, dibujado más callado) — el `.nvprev` es tan barato que la
distinción "locked sin arte" de DEC-113 ya no se justifica.

**Resultado medido** (Release, esta máquina): RSS con la Collection
abierta 324 MB → **180 MB**; cambio de variante Frin 55–100 ms → **el
fetch es un lookup sub-ms** (el redibujo completo de la Collection
~8–10 ms); 20 ciclos abrir/cerrar sin fuga (RSS asienta ~128 MB, por
debajo del baseline pet-only de 166 MB); CPU 0 % tras cerrar. El
`"Use <pet>"` real sigue cargando el pack de runtime completo de forma
transaccional — eso es correcto (selección de preview ≠ carga del pet
activo).

---

### DEC-120 — Nitidez Retina: el bitmap de glyphs se acota a píxel entero del dispositivo
**Status:** DECIDIDO · Block 06.2.

**Contexto.** QA del owner: "algo de texto se ve levemente borroso en
Retina". Chequeo forense primero (brief §8), con `DrawText`
instrumentado a scale 2.0: la RASTERIZACIÓN ya es correcta — los glyphs
se rasterizan a densidad de backing nativa (25 pt → textura de 59 px;
`TextCache` indexa por `scale`; un cambio de display-scale limpia el
cache) y se blittean 1:1. El defecto es de COLOCACIÓN: `DrawText` ponía
los runs centrados/derechos en `anchorX*scale − glyphW*0.5`, que cae en
`X,5` cuando el ancho del bitmap es impar, así que SDL dibujaba el
bitmap 1:1 con medio píxel de offset y cada texel quedaba a caballo
entre dos píxeles del dispositivo. Medido: `frac=0.500` en
"Use Frin" / "Bunny" / "On desktop" (todos centrados — justo lo que el
owner reportó); `frac=0.000` en todo el texto de cabecera alineado a la
izquierda (que era nítido).

**Decisión.** `productui/TextLayout.h::GlyphBlitOrigin` (puro, testeado)
calcula el origen y lo redondea a píxel entero del dispositivo. La
rasterización no se toca — nada de negrita, nada de reescalado. El
check nativo de texto ahora también verifica la relación de píxeles 1x
vs 2x (~2x) para atrapar una regresión futura de "rasterizar a 1x y
agrandar". Además se subió el contraste del texto secundario
(`kTextMuted` 3,3:1 → ~5,0:1; `kTextFaint` 2,1:1 → ~3,5:1) sin volverlo
casi-negro.

---

### DEC-121 — Acción primaria del hero: relleno/tinta de acento del pet, nunca casi-negro
**Status:** DECIDIDO · Block 06.2. Ajusta DEC-117.

**Contexto.** QA del owner: el botón "Use <pet>" casi-negro
(`#2A2520`) "pesa demasiado" contra la UI clara — es el objeto más
oscuro de la pantalla. Y mostrar la línea de estado "Use" ARRIBA del
botón "Use <pet>" es redundante.

**Decisión.** `productui::PetAccent` gana `softFill` (tinte claro/medio
del tono del pet) y `deepInk` (versión oscura y legible del mismo tono,
contraste verificado ≥ 5,5:1 sobre `softFill`). El botón = `softFill` +
borde `line` + texto `deepInk`. Nunca negro arbitrario. Y `showStatusLine
== !actionEnabled`: la línea de estado y el botón son MUTUAMENTE
excluyentes — activo muestra "● On desktop" sin botón; locked muestra
"Not in your collection" sin botón; poseído-inactivo muestra SOLO el
botón. No se dibuja ningún botón deshabilitado.

También: hero stage más presente (halo asimétrico de primitivas de
primera parte alrededor del arte + lóbulo secundario + regla de acento
fina bajo el nombre, teñido con `shapeTint` a alpha más alto) y la
Collection se lee como DOS planos (gallery sobre `kGalleryShelf`, un
neutro cálido un poco más profundo, bajo el divisor; pedestales de la
gallery con un tinte de identidad muy tenue por pet en vez del mismo
cuadro neutro).

---

### DEC-122 — Copy editorial de Bunny/Nidir/Frin: autorada y aprobada por el owner
**Status:** DECIDIDO · Block 06.2. Supersede la parte "sin personalidad
autorada" de DEC-117.

**Contexto.** Block 06.1 (brief §14) prohibía que el agente inventara
personalidades permanentes; `PetEditorial` solo tenía una etiqueta de
especie provisional y `ShortDescription` siempre `""`. El brief de
Block 06.2 (§14/§15) provee copy bilingüe EXPLÍCITA para los tres pets
con arte real y pide una línea de descripción en el hero.

**Decisión.** `productui::PetEditorial` (puro, tabla por id de catálogo
+ idioma — data-driven, NO hard-codeado en la vista) sirve la especie y
la descripción de una frase aprobadas:

| pet | especie EN / ES | descripción EN / ES |
|---|---|---|
| bunny | Rabbit / Conejo | "Small, curious, and never in a hurry." / "Pequeño, curioso y sin ninguna prisa." |
| nidir | Black dragon / Dragón negro | "Quiet wings. Bright eyes. Fire when it matters." / "Alas quietas. Ojos brillantes. Fuego cuando hace falta." |
| frin | White wolf / Lobo blanco | "Watchful, calm, and happiest close by." / "Atento, tranquilo y más feliz cerca." |

El resto del roster sigue devolviendo `""` hasta que se le escriba copy
propia (el hero omite la línea). Los nombres propios nunca están en la
tabla.

---

### DEC-123 — Modelo de propiedad capaz de variantes: `catalog::PetEntitlement`
**Status:** DECIDIDO · Block 07. **Supersede** el `AppState::ownedPetIds`
(conjunto de `petId`) de Block 06 (DEC-108) para la propiedad; el enum
de estados de `CollectionModel` (`kActive`/`kOwnedInactive`/`kLocked`)
NO cambia.

**Contexto.** Block 06 modelaba la propiedad como un conjunto de
`petId`: poseer `"frin"` daba las dos variantes. La verdad de producto
final (PRD §4) es que el onboarding otorga UNA variante de Frin y la
otra se obtiene por separado en el shop oculto de starters. Un conjunto
de `petId` no puede expresar "posee solo Frin macho".

**Decisión.** `catalog::PetEntitlement { petId, variantId }` (misma
forma que `PetIdentity`, tipo propio por semántica distinta):
`variantId == ""` = **el pet entero** (cualquier variante, presente o
futura); no vacío = **solo esa variante**. `Covers(PetIdentity)`
resuelve el gate de activación. `CanonicalizePetEntitlements` deja la
lista determinista: descarta `petId` vacío, ordena por
`(petId, variantId)`, dedup, y aplica **subsunción** — si existe
`(p, "")` se descartan las `(p, <var>)`. Sin conocimiento del catálogo
(lógica de strings pura), así que sirve igual al serializer y a
`src/app`.

`CollectionModel` consume autorizaciones: cada `CollectionVariant`
lleva un flag `owned`; `CanActivate(model, petId, variantId)` exige la
variante EXACTA. `SeedEntitlementsFromCatalog` siembra **pet entero**
por cada `initiallyOwned` (mismo alcance que el `ownedPetIds` de dev de
Block 06). `EnsureActiveEntitlementOwned` agrega la autorización exacta
de la identidad activa si falta.

Block 07 NO implementa onboarding ni el shop oculto — solo deja el
modelo listo (brief §4/§31).

---

### DEC-124 — AppState schema v4: propiedad como autorizaciones; migración desde v1/v2/v3
**Status:** DECIDIDO · Block 07. Extiende DEC-109/DEC-116.

**Decisión.** `kCurrentSchemaVersion` 3 → 4. `AppState::ownedPetIds`
(`string[]`) → `AppState::ownedEntitlements`
(`persistence::OwnedEntitlement { petId, variantId }[]` — datos planos,
sin dependencia de `src/catalog`; `src/app` puentea a
`catalog::PetEntitlement`). El bloque v2/v3 (lock/size/opacity/
language) NO cambia de layout; solo la lista de propiedad pasa de
`string[]` a pares `(petId, variantId)`.

`DeserializeAppState` generaliza la migración hacia adelante: lee
cualquier versión en `[1, kCurrentSchemaVersion]` (hoy 1, 2, 3 y 4). Un
archivo v2/v3 se lee con su layout y **cada `petId` poseído se migra a
una autorización de PET ENTERO** `(petId, "")` — así un Frin de Block
06 (que exponía macho y hembra) sigue dando las dos variantes (brief
§5). El click balance, la posición de ventana, el idioma, las
preferencias de tamaño/opacidad/lock y el pet/variante activo
**sobreviven** intactos. `schemaVersion` se marca como el actual para
que el próximo `Save()` lo reescriba como v4. Una versión más nueva
desconocida (v5+) o basura sigue tratándose como dato inutilizable.

`NormalizeOwnedEntitlements` (en el serializer) impone orden + dedup
sobre una copia para que el formato sea determinista byte a byte;
`src/app` aplica además la canonicalización semántica completa
(`catalog::CanonicalizePetEntitlements`, con subsunción) antes de cada
`Save()`.

---

### DEC-125 — Catálogo schema "NVCATLG1" v3: precio + visibilidad de Shop como DATO
**Status:** DECIDIDO · Block 07. Extiende DEC-107 (schema v2 / propiedad
de dev).

**Contexto.** El brief §10 es explícito: el precio de compra y la
elegibilidad para el Shop público deben ser DATO, nunca una rama
`if (pet == "nidir")` en el runtime/UI.

**Decisión.** `NVCATLG1` v2 → v3: por entrada se agregan `priceClicks`
(`u64`) y `publiclyPurchasable` (`u8`). El `.nvcat` es un artefacto de
build (se recompila desde su manifest en el mismo commit), así que —
igual que en v1→v2 — NO hay ruta de migración: `tools/
compile_pet_catalog.py` sube a `schema_version: 3` y una versión
distinta se rechaza. Una entrada `publiclyPurchasable` con
`priceClicks == 0` la rechazan tanto el compilador como el loader C++
(precio cero no soportado — brief §26).

**Precios PROVISIONALES de QA/economía** (no balanceo final,
documentados como tales): Bunny 120, Nidir 300. **Frin queda
`publiclyPurchasable: false` en las DOS variantes** — su obtención es
onboarding + shop oculto de starters, trabajo futuro que NO se
implementa ni se insinúa (brief §11). El `.nvprev` liviano de Block
06.2 no cambia: el Shop reusa las mismas previews (sin abrir ningún
`.nvpack` para navegar).

---

### DEC-126 — Compra del Shop: política pura + transacción atómica de wallet
**Status:** DECIDIDO · Block 07.

**Decisión.**
- **`catalog::EvaluatePurchase`** (puro, sin GUI — brief §14): evalúa
  una compra contra el catálogo + balance + autorizaciones y devuelve
  el estado RESULTANTE sin mutar nada. Categorías: `kSuccess` /
  `kAlreadyOwned` / `kInsufficientBalance` / `kNotPurchasable`
  (no público, o precio 0) / `kInvalidTarget` (petId no está en el
  catálogo). En cualquier fallo `newBalance` == balance de entrada y
  `newEntitlements` == autorizaciones de entrada — nunca una mutación
  parcial. La resta solo corre tras verificar `balance >= precio`, así
  que **no hay underflow posible**.
- **`SpikeApp::HandlePurchaseRequest`**: si es `kSuccess`, muta
  `clickBalance` **y** `ownedEntitlements` en el MISMO `AppState`, sin
  escrituras intermedias, y llama a `FlushPersistedState()` — un solo
  `SerializeAppState` + un solo `rename` atómico persisten los dos
  juntos. Un crash no puede dejar "gasté el balance pero no tengo el
  pet". El per-click normal SIGUE usando el debounce de ~2s (perder
  ~2s de clicks es trivial); la persistencia inmediata es la única
  excepción, y es solo llamar al flush que ya existía (brief §13).
- **Confirmación inline** (`ShopView::confirming_`, estado local de la
  vista): "Get <pet>" abre la pregunta; el foco arranca en "Cancel";
  Esc o "Cancel" la cierran sin tocar nada; "Confirm" emite la compra.
  Un solo click perdido nunca gasta (brief §12). No es un modal
  gigante — cabe en la columna del hero.

---

### DEC-127 — Shop como sección separada del Product UI; navegación por pestañas de texto
**Status:** DECIDIDO · Block 07. Extiende DEC-112/DEC-117 (arquitectura
event-driven + composición hero + gallery de la Collection).

**Contexto.** El brief §6/§7/§16 pide que el Shop sea una sección
DISTINTA de la Collection, "meet another Nimvlet, not an online store
template" — sin sidebar, card wall, dashboard, carrito, búsqueda,
filtros, categorías, banners ni countdowns.

**Decisión.**
- `productui::ProductSection { kCollection, kShop }`, dueño en
  `ProductWindow`. Una fila de pestañas de texto compacta
  "Collection · Shop" reemplaza al viejo título/subtítulo de sección de
  Block 06 (`SectionNav` puro + `SectionHeaderView` SDL, compartidos
  por las dos secciones — la navegación se ve idéntica). Tocar una
  pestaña cambia de sección EN LA MISMA ventana; el runtime del pet no
  se toca. Mouse + teclado; los ids `nav:collection`/`nav:shop`
  encabezan el anillo de foco. Al reabrir el Product UI se vuelve a
  Collection (sin recordar la última sección — sin razón de bajo costo
  para hacerlo, brief §17).
- El Shop reusa la composición hero + gallery, el acento por pet, el
  hero stage y las previews `.nvprev`. Estados del hero (de
  `ShopModel`): asequible → botón "Get <pet>" en el acento del pet;
  saldo insuficiente → precio + línea contenida "Need N more clicks",
  sin acción; poseído → "In your collection", sin precio ni botón.
- Layout puro y testeable (`ShopLayout` + `ShopLayoutTest`), igual que
  `CollectionLayout`.
- **Copy editorial más larga** (brief §19): las descripciones de
  Bunny/Nidir/Frin pasan de una frase a un par de frases (aprobadas por
  el owner, EN/ES, data-driven en `PetEditorial`). La vista las
  envuelve con `TextCache::DrawTextWrapped` (word-wrap greedy) en la
  columna del hero; el layout reserva alto para hasta 3 líneas. La
  restricción "el nombre propio nunca aparece dentro del copy" de
  DEC-122 se levanta: la copy aprobada nombra al pet en su segunda
  frase.
- **Nota arquitectónica, NO implementada** (brief §20): un futuro hero
  podría usar un fondo escénico ilustrado por Nimvlet. No se generan ni
  se envían imágenes en Block 07, no se agrega un sistema de escenas.
  El `PetAccent` + hero stage actuales ya son un "seam" de datos de
  presentación por pet; agregar un backdrop estático opcional más
  adelante no se vuelve más difícil. Sin red en runtime; los backdrops
  futuros serían assets locales optimizados.

---

### DEC-128 — Corrección Block 07: migración legacy de Frin, objetivo de compra data-driven, y resolución de activo sin otorgar
**Status:** DECIDIDO · Block 07 (pasada de corrección). **Ajusta**
DEC-123 (semántica de `PetEntitlement`), DEC-124 (migración de
propiedad) y DEC-126 (política de compra). NO rediseña el Shop ni la
Collection.

**Contexto.** La primera versión de Block 07 tomó tres atajos
arquitectónicos:
1. La migración de un `ownedPetIds` "frin" de Block 06 producía
   `{frin, ""}`, con la semántica "todo Frin, toda variante presente y
   futura". El contrato pedía expandirlo a las DOS variantes
   HISTÓRICAS (`{frin, "male"} + {frin, "female"}`) — Block 06 exponía
   Macho + Hembra, no estableció propiedad de variantes futuras.
2. `{frin, ""}` como "pet entero" podía colapsar propiedad
   variant-específica en una autorización abierta.
3. `EvaluatePurchase` operaba sobre un `petId` suelto, asumiendo que
   alcanza siempre.
4. `EnsureActiveEntitlementOwned` OTORGABA propiedad si el pet activo
   no estaba autorizado — un bypass de economía (`active = nidir,
   owned = {bunny}` se volvía `owned = bunny + nidir` al cargar).

**Decisión.**

- **`PetEntitlement::Covers` es coincidencia EXACTA** en `{petId,
  variantId}`. `{frin, ""}` NO cubre `{frin, "male"}`. NO existe una
  autorización de "todas las variantes de un Nimvlet capaz de
  variantes"; la propiedad de Frin es siempre por variante, explícita.
  `CanonicalizePetEntitlements` pierde el paso de subsunción (ya no hay
  nada que subsumir).

- **`catalog::ExpandHistoricalWholePetEntitlements(ents, catalog)`** —
  el serializer parsea un `ownedPetIds` legacy a `{petId, ""}`
  PROVISIONAL (no tiene catálogo — ver docs/PERSISTENCE.md §3);
  `SpikeApp::Init()` corre esta función tras cargar el catálogo. Para
  un petId con entradas de variante en el catálogo, `{p, ""}` se
  reemplaza por las autorizaciones EXPLÍCITAS de cada variante; un pet
  sin variantes deja su `{p, ""}` igual. Idempotente: un save v4 limpio
  no cambia. **El AppState actual nunca contiene `{frin, ""}`.** Una
  tercera variante de Frin agregada DESPUÉS de la migración NO queda
  cubierta por el conjunto migrado.

- **`SeedEntitlementsFromCatalog`** otorga la autorización EXPLÍCITA de
  cada entrada `initiallyOwned` (`{frin, "male"}` y `{frin, "female"}`,
  no `{frin, ""}`).

- **`EvaluatePurchase(catalog, PetIdentity target, ...)`** — el
  objetivo es una IDENTIDAD de catálogo completa, resuelta contra una
  entrada EXACTA (`PetCatalog::Find`). Lo que se otorga es la identidad
  de esa entrada. `{frin, ""}` (comprar "todo Frin") -> `kInvalidTarget`
  (no hay tal entrada). Un catálogo SINTÉTICO donde `frin/male` es
  público hace que la MISMA política lo compre y otorgue `{frin,
  "male"}`, sin ninguna rama `if (pet == "frin")` — el seam para el
  shop oculto de starters (Block 09) existe sin código futuro. El Shop
  real de Block 07 sigue sin listar Frin; `ShopItem::entitlementTarget`
  (que la vista emite como `PurchaseRequest`) es esa identidad, no una
  suposición sobre petId.

- **`catalog::ResolveOwnedActiveIdentity(owned, catalog, wanted,
  &fellBack)`** reemplaza a `EnsureActiveEntitlementOwned`. NUNCA
  otorga: si `wanted` está autorizado se devuelve; si no, cae al
  default del catálogo si está autorizado, o a la primera entrada
  autorizada, o (nada autorizado — solo antes de la siembra) a `wanted`
  sin marcar fallback. `SpikeApp::Init()` la usa tras la
  expansión/siembra; repara `activePetId/Variant` y marca dirty en el
  fallback (salvo en una selección solo-DEV). `TrySwitchActivePet` gana
  un gate de propiedad (`OwnsIdentity`) y ya NO toca la propiedad — un
  switch de runtime jamás otorga; establecer propiedad es cosa de
  siembra / migración / compra. La siembra se re-ejecuta si el conjunto
  de autorizaciones quedó vacío por corrupción (un estado post-siembra
  no puede tener cero — no hay forma de "vender").

**Verificado contra el binario real:** una instalación nueva siembra
`{bunny,""} {frin,female} {frin,male}` (sin `{frin,""}`); un save v3 con
`ownedPetIds=[bunny,frin]` se reescribe a v4 con esas tres
autorizaciones y `frin/female` activo se conserva; un save v4 corrupto
con `active=nidir, owned={bunny}` reabre en bunny con `owned` intacto
(`{bunny,""}` solo — nidir NO se otorgó).

---

### DEC-129 — La expansión de propiedad legacy de Frin es una tabla histórica CONGELADA, no una enumeración del catálogo actual
**Status:** DECIDIDO · Block 07 (corrección final). **Ajusta** DEC-128
(`ExpandHistoricalWholePetEntitlements`). NO reabre la semántica de
`PetEntitlement`, `EvaluatePurchase`, el Shop, el switch de runtime ni
la activación de la Collection.

**Contexto.** DEC-128 dejó `ExpandHistoricalWholePetEntitlements(ents,
catalog)` **derivando** el conjunto de variantes de "poseer frin" al
**enumerar el catálogo ACTUAL** en el momento de migrar. Eso filtra
propiedad futura: si un bloque posterior agrega `frin/spirit` al
catálogo y el usuario recién entonces actualiza desde un save v2/v3, la
migración le otorgaría Spirit — que **nunca** formó parte de lo que
"poseer frin" significaba bajo el modelo de propiedad "por pet lógico"
de los schemas v1..v3. La propiedad legacy significa exactamente el
contenido disponible bajo el modelo viejo en aquel momento histórico:
para el Frin de Block 06, `frin/male` + `frin/female`, nada más.

**Decisión.**

- **Mapeo histórico CONGELADO.** `ExpandHistoricalWholePetEntitlements(std::vector<PetEntitlement>&)`
  ya **no recibe catálogo**. Una tabla en `src/catalog/CollectionModel.cpp`
  (anon namespace, `HistoricalLegacyVariants(petId)`) fija:

  ```
  "frin"                 ->  {frin, "male"}  {frin, "female"}
  (cualquier otro petId) ->  se deja como {p, ""}  (era sin variantes)
  ```

  Independiente de cuántas variantes de Frin tenga el catálogo ACTUAL.
  Si un bloque futuro incorpora otro Nimvlet capaz de variantes que
  alguna vez estuvo bajo el modelo por-pet-lógico, se agrega una fila
  con la lista EXACTA de aquel momento — nunca se deriva del catálogo.
  El catálogo actual PUEDE usarse para validar/diagnosticar ids
  históricos, nunca para descubrir el conjunto que la propiedad legacy
  otorga.

- **La expansión solo corre sobre un estado GENUINAMENTE legacy.**
  `DeserializeAppState` gana un out-param `outOnDiskSchemaVersion` que
  reporta la versión que traía el archivo EN DISCO, ANTES de normalizar
  `AppState::schemaVersion` a la actual. `AppStateStore::Load` lo
  propaga (inicializado a `kCurrentSchemaVersion`; solo un parseo
  exitoso de un save viejo lo baja — sin save / ilegible / corrupto
  queda en la actual). `SpikeApp::Init()` corre la reconciliación
  **solo si `loadedOnDiskSchema < kCurrentSchemaVersion`**. Un v4
  editado a mano con un `{frin, ""}` suelto NO se expande: con
  `Covers` de coincidencia exacta ese par no cubre ninguna identidad
  real, así que tampoco fabrica propiedad ni activación.

- **Sin ramas de Frin nuevas.** El mapeo congelado es dato de
  compatibilidad histórica, no lógica de producto: la política de
  compra, el `ShopModel`, `TrySwitchActivePet`, `BuildCollectionModel`
  y el matching de autorizaciones no ganan ningún caso especial de
  Frin ni de migración.

**Verificado (tests).** `EntitlementMigration/FutureVariantAlreadyInCatalogBeforeMigrationStaysUnowned`:
catálogo SINTÉTICO con `frin/male, frin/female, frin/spirit` **ya
presente**, se migra un save v2 y un v3 reales con `ownedPetIds=["frin"]`
por el mismo camino que la app (`DeserializeAppState` + gate por versión
en disco) -> `owns frin/male == true`, `owns frin/female == true`,
`owns frin/spirit == false`, `owns frin/"" == false`, y `frin/spirit`
no es activable. `EntitlementMigration/CurrentV4BareWholeFrinIsNotExpanded`:
un v4 con `{frin, ""}` no se expande ni manufactura activación.
También: seed v1, Frin legacy v2/v3, propiedad legacy sin variantes,
idempotencia, y un v4 limpio que pasa por el camino de carga sin
ensancharse.

---

### DEC-130 — Settings: sección del Product UI sobre la ÚNICA ruta canónica de preferencias, compartida con el menú rápido
**Status:** DECIDIDO · Block 08. **Extiende** DEC-127 (navegación por
pestañas de texto del Product UI) y la §8 de Block 06 (controles de
usuario del menú rápido). NO cambia el schema de AppState, NO toca la
economía/propiedad de Block 07, NO altera la estructura del menú rápido.

**Contexto.** Block 06 expuso tamaño / opacidad / lock / idioma SOLO por
el `NSStatusItem`. Block 08 agrega una tercera sección al Product UI
(`Collection · Shop · Settings`) para configurarlas también desde ahí.
El riesgo es un segundo sistema de preferencias con sus propias reglas
de persistencia/runtime (dos `HandleSize(...)`).

**Decisión.**

- **Una sola ruta de mutación/aplicación.** Las cuatro preferencias se
  mutan EXCLUSIVAMENTE por `SpikeApp::ApplySizeChoice` /
  `ApplyOpacityChoice` / `ApplyLockPosition` / `ApplyUiLanguage`. Cada
  una: escribe UN campo crudo de `appState_` (con la normalización que
  ya existía — `core::PetSizeChoiceId` / `NormalizeOpacityPercent` /
  `LanguageId`), `persistenceScheduler_.MarkDirty` (mismo debounce de
  ~2s de siempre — Block 08 NO agrega escrituras inmediatas), aplica el
  efecto de runtime (`ApplyPetWindowMetrics` / `SDL_SetWindowOpacity` /
  el gate de drag / `productWindow_.SetLanguage`), `PushShellState` (el
  menú nativo refleja el nuevo valor) y `PushPreferencesToProductWindow`
  (Settings refleja el nuevo valor). El menú rápido
  (`HandleShellAction`) y Settings (`ApplyPreferenceChange`, desde un
  `productui::SettingsChange` que sube por `ProductWindowEvent`) llaman
  a esos mismos cuatro Apply*. No hay una segunda ruta.

- **Sincronización bidireccional, una fuente de verdad.** `SettingsView`
  nunca muta sus propias preferencias: emite un `SettingsChange` y
  espera que src/app le re-empuje el estado final vía `SetPreferences`
  (igual que el Shop re-empuja el modelo tras una compra). Un cambio
  desde el menú rápido mientras Settings está abierto llega por el mismo
  `PushPreferencesToProductWindow`; un cambio desde Settings actualiza
  el menú por el mismo `PushShellState`.

- **`core::Preferences`** (puro) — las cuatro preferencias como un valor
  tipado y normalizado, la moneda común entre las dos superficies.
  `PreferencesFromStored(...)` reconstruye desde los campos crudos de
  AppState (0 de opacidad -> 100, id de tamaño desconocido -> medium,
  etc.): un archivo editado a mano con valores imposibles nunca produce
  un `Preferences` imposible. `StepSize` / `StepOpacityPercent` /
  `OtherLanguage` ciclan las opciones de un control segmentado (← →
  clamp; Enter/Espacio wrap).

- **Sin bump de schema.** AppState sigue en v4: `sizeChoice`,
  `opacityPercent`, `lockPosition`, `language` ya existían y ya
  persistían. Settings es UI nueva sobre estado viejo.

- **Presentación.** `productui::BuildSettingsLayout` (puro, en
  `nimvlets_productui_core`) — cabecera compartida (`SectionNav` gana
  `kSettings`) + dos grupos ("Companion": Size / Opacity / Lock
  position; "Language": el selector) con controles segmentados. Quiet,
  cálido, compacto: sin cards, sin acento por pet, sin previews (no se
  carga ningún `.nvpack`). El foco de teclado vive en la FILA
  (`row:size` ...); Tab/Shift+Tab/↑↓ recorren nav + filas, ← →
  cambian el valor de la fila enfocada, Enter/Espacio avanzan cíclico.
  El hit-test de mouse SÍ resuelve el segmento exacto.

- **Menú rápido intacto (brief §21/§22).** No se agrega un item
  "Settings…" al `NSStatusItem`; Settings se alcanza solo por la
  navegación del Product UI. La estructura y los checkmarks del menú no
  cambian.

**Verificado.** `tests/SettingsLayoutTest.cpp` (estado seleccionado por
preferencia, EN/ES, hit-test, cabe sin scroll, sin solapes, y
`SettingsAndQuickMenuAgreeOnSelection` — para un mismo estado, el
segmento seleccionado en Settings y el item chequeado en el submenú
equivalente del menú rápido nombran el mismo valor);
`tests/PreferencesTest.cpp` (normalización + ciclado);
`tests/PersistenceIntegrationTest.cpp`
(`PreferenceChangesSurviveReloadWithoutTouchingEconomy`). QA nativo
Retina en EN/ES + estado de foco de teclado.

### DEC-131 — Lifecycle de primer arranque: schema v5, `OnboardingLifecycle`, y la migración de usuarios existentes
**Status:** DECIDIDO · Block 09A. **Extiende** DEC-124 (schema v4) y
DEC-109/DEC-116 (migración hacia adelante de AppState). NO toca la
migración histórica de Frin (DEC-129 sigue vigente).

**Contexto.** El roadmap requiere onboarding de primer arranque. Con
exactamente un Nimvlet poseído, un estado puede ser un usuario NUEVO
que recién eligió su starter, o un usuario VIEJO al que le queda uno —
no se puede inferir de la propiedad. Y "sin archivo de estado" (usuario
genuinamente nuevo) tiene que ser distinguible de "estado viejo
migrado".

**Decisión.**

- **AppState schema v5** agrega un solo byte, `onboardingLifecycle`
  (`kPending` 0 / `kLegacyComplete` 1 / `kCompleted` 2). Default de un
  `AppState{}` = `kPending` (== "ningún save" == usuario potencialmente
  nuevo).
- **Migración v1/v2/v3/v4 → v5**: `DeserializeAppState` fija
  `kLegacyComplete` para CUALQUIER archivo que precede a v5 —
  preservando balance, propiedad, preferencias, posición y pet activo
  EXACTOS. Un usuario existente NUNCA se manda a selección de starter,
  NUNCA se le resetea la propiedad, NUNCA se le pone el balance en 0
  (brief §3.A/§4). Un byte de lifecycle fuera de {0,1,2} en un v5 →
  `kLegacyComplete` (NO DESTRUCTIVO — no se rechaza el archivo entero);
  un v5 truncado antes del byte → se rechaza (no se adivina).
- **`AppState::kFirstExplicitEntitlementSchema` (== 4)** es el nuevo
  umbral SEMÁNTICO fijo de la reconciliación de propiedad legacy de
  Block 07 (`ExpandHistoricalWholePetEntitlements`), en vez de
  `kCurrentSchemaVersion`: subir el schema por el onboarding (v5) NO
  debe empezar a "migrar" la propiedad de un v4.
- **`AppStateStore::Load` gana `outSaveFileExisted`**: `true` si había
  archivo (aunque no parsee), `false` solo si genuinamente no existía.
  src/app distingue un usuario nuevo (sin archivo → onboarding, cuando
  esté armado) de una recuperación de un archivo corrupto (existía → se
  trata como usuario existente, NUNCA se onboardea — brief §27).
- **Normalización dev/legacy → `kLegacyComplete`**: cuando corre la
  siembra de propiedad por catálogo (el camino dev/legacy, no
  onboarding), src/app pone el lifecycle en `kLegacyComplete` si estaba
  en `kPending`. NO es "inferir completitud de la propiedad" (brief §3):
  `ownershipSeeded` es un flag EXPLÍCITO de "hubo init".

**Verificado.** `tests/AppStateSerializerTest.cpp`
(`OnboardingLifecycleMigration`, `V5RoundTrips`,
`TruncatedV5LifecycleByteRejected`), `tests/AppStateStoreTest.cpp`
(`SaveFileExistedFlag`), `tests/EntitlementMigrationTest.cpp`
(`LegacySavesAreOnboardingComplete` — v1..v4 → kLegacyComplete con
todo lo demás preservado; `AppState{}` → kPending; el gate de
reconciliación de Frin ahora es `< kFirstExplicitEntitlementSchema`, así
que los tests v4 siguen sin expandirse). Smoke: un save v4 real carga,
migra a v5=legacy-complete, y su propiedad/balance sobreviven.

---

### DEC-132 — Onboarding: metadata de starter data-driven, gate de contenido de producción, política pura, secreto de 44 s event-driven, y harness solo-DEV
**Status:** DECIDIDO · Block 09A. **Depende de** DEC-131 (lifecycle).
NO habilita el onboarding de producción (falta contenido de
Artu/Rato/Rin Rin — se activa en Block 09B).

**Contexto.** Hay que construir la ARQUITECTURA real de
state/policy/migration/timing del onboarding SIN inventar contenido de
starter que no existe, y SIN romper el arranque normal actual.

**Decisión.**

- **`catalog::StarterRole` (catálogo schema "NVCATLG1" v4)** — DATO por
  entrada: `none` / `normal` (la tríada Artu/Rato/Rin Rin) / `secret`
  (Frin). NUNCA `if (pet == "artu")` en el runtime/UI (brief §7). Sin
  metadata especulativa de economía: un starter no es
  `publicly_purchasable` por serlo; el shop oculto es Block 10.
- **Gate de contenido de producción** (brief §8/§30/§31), en tres
  capas: (1) `tools/compile_pet_catalog.py` deja compilar
  `production_onboarding_ready: true` SOLO si el manifest declara ≥ 3
  starters `normal` Y el contenido de cada uno (pack + `.nvprev`) existe
  en disco; si no, la compilación FALLA. (2) `PetCatalogLoader` rechaza
  un `.nvcat` con el flag en true y < 3 starters normales. (3)
  `catalog::EvaluateOnboardingReadiness` — `armed` sii el flag + conteo
  ≥ `kRequiredNormalStarterCount`. **La metadata del secreto sola nunca
  arma nada.** El catálogo de dev actual: flag `false`, cero
  `starter_role` → nunca armado → **arranque normal idéntico al de
  antes de Block 09A** (brief §31).
- **`catalog::OnboardingPolicy`** (puro, sin SDL/I/O): `BuildOnboardingOffer`
  agrupa por rol (secreto colapsado bajo `{frin,""}` + variantes);
  `EvaluateOnboardingSelection` → `OnboardingGrant` con reglas exactas
  (normal ofrecido → grant; secreto sin reveal → rechazo; `{frin,""}`
  con reveal → "needs variant"; variante del secreto con reveal → grant
  EXACTO de esa variante; ya completo → `kAlreadyCompleted` con CERO
  mutación). `newBalance` = 0 SIEMPRE (brief §16). Un target inválido →
  cero mutación (brief §28).
- **Grant EXACTO de Frin** (brief §5/§13): elegir Frin Macho otorga
  `{frin,"male"}` y nada más; la otra variante queda sin otorgar
  (Block 10). NO se reutiliza `ExpandHistoricalWholePetEntitlements`:
  el onboarding es una política de grant NUEVA.
- **Transacción de completitud** (`HandleOnboardingSelection`, brief
  §14/§15): balance 0 + grant EXACTO + activo + `kCompleted` +
  `ownershipSeeded`, todo en el MISMO `AppState`, UNA escritura atómica
  (mismo contrato temp+rename que una compra — DEC-126). Idempotente:
  `!onboardingActive_` sale temprano y `alreadyCompleted` da cero
  mutación. Un crash no puede dejar "completado sin starter" ni "starter
  sin completar".
- **Secreto de los 44 s** (brief §10/§11/§12): DWELL DE SESIÓN sobre la
  pantalla activa, reloj MONOTÓNICO, NUNCA acumulado en AppState (una
  sesión nueva arranca un dwell fresco). `catalog::SecretRevealDeadlineMs`
  se integra al mismo cálculo `SDL_WaitEventTimeout` / next-deadline que
  `ambientDeadlineMs_` & co. — sin timer thread, sin polling. Al
  deadline: transición UNA vez + un redibujo, se limpia el deadline.
  `SecretRevealedAfterDwell(dwellMs)` = `dwellMs >= 44000` (frontera
  exacta), testeable con tiempo inyectado (sin `sleep`).
- **Onboarding NO es una sección** (brief §19): es un GATE. `ProductWindow`
  gana un modo `onboarding_` que se come todo el input, oculta la nav de
  secciones, y no se puede saltear (cerrar la ventana la re-enfoca; Esc
  solo cancela una confirmación). Composición cálida/quieta/espaciosa
  (`productui::OnboardingLayout` puro + `OnboardingView` SDL): encabezado
  + fila de candidatos, confirmación inline con el foco en "Cancel"
  (brief §23), sub-elección contenida de variante para Frin (brief §22).
  Usa el bundle `.nvprev` liviano — NUNCA abre un `.nvpack` para elegir
  (brief §26).
- **Harness solo-DEV** (brief §9): `NIMVLETS_DEV_ONBOARDING` carga
  `assets/dev/onboarding_dev_catalog.nvcat` (descriptores SINTÉTICOS
  `artu_dev`/`rato_dev`/`rinrin_dev` que prestan packs existentes — NO
  son Artu/Rato/Rin Rin, nunca se envían, el `(dev)` en el nombre lo
  hace inconfundible) y fuerza el gate mientras el lifecycle sea
  `kPending`. `NIMVLETS_DEV_ONBOARDING_REVEAL[_MS]` / `_STAGE` /
  `_CHOOSE` para QA sin dormir 44 s.

**Verificado.** `tests/OnboardingPolicyTest.cpp` (§28 selección/grant/
idempotencia/balance-0, §29 fronteras del deadline con tiempo inyectado,
§30 gate de contenido incl. "el secreto solo no cuenta" y "el catálogo
de dev actual no arma"); `tests/OnboardingLayoutTest.cpp` (etapas,
secreto oculto/revelado al final, EN/ES, hit-test, foco en Cancel,
cabe sin scroll con 3 y 4 tarjetas, sin nav de secciones);
`tests/PetCatalogLoaderTest.cpp` + `tools/test_asset_pipeline.py`
(catálogo v4: `starter_role`, `production_onboarding_ready` + su gate).
Smokes: arranque normal sin cambios, flujo DEV de starter normal,
reveal a 722 ms, selección de Frin macho y hembra, completitud +
reinicio (lifecycle=completed, balance 0, solo la variante elegida
poseída), ciclo de vida del Product UI.

### DEC-133 — El gate de onboarding de producción exige contenido que COINCIDE con la identidad del starter, no solo que exista; y separa el harness DEV con un byte propio
**Status:** DECIDIDO · Block 09A (pasada de endurecimiento). **Depende de**
DEC-132. **Refina** el gate de contenido de DEC-132 sin reabrir la
arquitectura de onboarding/persistencia/timing. NO habilita el
onboarding de producción (sigue sin contenido de Artu/Rato/Rin Rin).

**Contexto.** El gate de DEC-132 probaba que los archivos de starter
EXISTEN (conteo de `starter_role: normal`, `pack_path` presente,
`.nvprev` hermano presente). Necesario pero no suficiente: un
`pet_id: "artu"` que apunta al `.nvpack`/`.nvprev` de Bunny compilaba
`production_onboarding_ready: true` sin problema, porque los archivos de
Bunny existen. "El archivo existe" no puede significar "el contenido del
starter está listo para producción". Además el conteo era por FILAS, así
que filas duplicadas o variantes de un mismo Nimvlet podían inflar la
tríada. Y el catálogo sintético del harness DEV declaraba
`production_onboarding_ready: true` con alias a propósito — exactamente
la misconfiguración que el gate de producción debe rechazar.

**Decisión.**

- **Señal de identidad/procedencia (ya en los formatos en disco).** El
  `.nvpack` ("NVPACK2") lleva embebidos `id` y `variantGroup`; el
  `.nvprev` ("NVPREV1") lleva `pet_id`, `variant_id` y `source_pack`
  (basename del pack de origen). No se inventa ningún campo nuevo en
  esos formatos, ni firmas criptográficas, ni un registro de assets.
- **Gate de producción (compilador, prueba primaria) —
  `tools/compile_pet_catalog.py` `_validate_starter_content`.** Con
  `production_onboarding_ready: true`, para cada starter normal: su
  `.nvpack` y su `.nvprev` existen, PARSEAN (lectores reales
  `read_pet_pack` / el nuevo `read_pet_preview`), y su identidad
  EMBEBIDA pertenece a ese starter — `pack.id == pet_id` y
  `pack.variantGroup` vacío; `prev.pet_id`/`prev.variant_id` == los de
  la entrada y `prev.source_pack == basename(pack_path)`. Cualquier
  desajuste → la compilación FALLA ruidosamente.
- **Identidades lógicas distintas.** La readiness cuenta `pet_id`
  distintos entre los `starter_role: normal`, no filas. Un
  `starter_role: normal` con `variant_id` no vacío se rechaza (un
  starter normal es un Nimvlet lógico entero). El secreto (Frin) sigue
  sin contar.
- **`devSyntheticOnboarding` — catálogo "NVCATLG1" v5.** Un byte a nivel
  de catálogo, tras `productionOnboardingReady`, MUTUAMENTE EXCLUYENTE
  con él (el compilador y el loader rechazan ambos en `true`). Es el
  opt-in EXPLÍCITO del harness solo-DEV: se sigue exigiendo la tríada de
  identidades lógicas distintas + que cada pack/preview exista y parsee,
  pero NO la coincidencia de identidad (los alias `artu_dev` → pack de
  Bunny son el punto). El catálogo sintético dejó de declarar
  `production_onboarding_ready`.
- **`src/app` (`ResolveOnboarding`).** La rama DEV ahora exige
  `catalog_.DevSyntheticOnboarding()` además de `NIMVLETS_DEV_ONBOARDING`
  + `kPending`; si el catálogo cargado no es sintético, loguea y NO
  fuerza el gate. Las dos ramas quedan simétricas y disjuntas: un alias
  nunca alcanza producción (`ProductionOnboardingReady()` siempre
  `false` en el sintético), y un catálogo real bajo el env var no se
  fuerza a un onboarding cuyo contenido no coincide.
- **Defensa del loader (barata, runtime) — `PetCatalogLoader`.** Lee el
  byte v5, expone `PetCatalog::DevSyntheticOnboarding()`, rechaza ambos
  flags en `true`, rechaza un `starterRole` normal con `variantId`, y
  rechaza `productionOnboardingReady`/`devSyntheticOnboarding` con < 3
  identidades lógicas distintas de starter normal. **NO** reabre los
  `.nvpack` de decenas de MB para re-verificar identidad embebida: el
  `.nvcat` no lleva esa metadata y "el runtime no debe cargar todos los
  packs al arranque". La coincidencia de identidad pack/preview es una
  garantía de TIEMPO DE COMPILACIÓN; el loader re-checa solo lo
  estructural. `catalog::CountNormalStarters` también dedupe por `petId`.

**Verificado.** `tools/test_asset_pipeline.py` `PetCatalogCompileTest`
(+14: acepta 3 starters reales y coincidentes; rechaza pack faltante /
preview faltante / identidad de pack alias / identidad de preview alias /
preview derivada de otro pack / pack malformado / preview malformada /
solo 2 identidades lógicas / el secreto no cuenta / filas-variante no
inflan / ambos flags a la vez; `dev_synthetic_onboarding` permite alias
pero exige la tríada y no arma producción; el `.nvcat` de dev versionado
es sintético, el de producción tiene onboarding deshabilitado).
`tests/PetCatalogLoaderTest.cpp` (+3: byte v5 round-trip + exclusión
mutua + tríada de identidades lógicas para devSynthetic; normal con
variante rechazado; el secreto no cuenta). `tests/OnboardingPolicyTest.cpp`
(+1: `CountNormalStarters` cuenta identidades lógicas distintas).
418 tests C++, 162 Python. Smokes: arranque normal idéntico (catálogo
v5, "production onboarding not armed", siembra, bunny, balance 0); flujo
DEV (catálogo sintético → gate → reveal → CHOOSE frin/female →
"onboarding COMPLETE … lifecycle=completed") + reinicio (no re-onboarda)
+ re-run bajo el env var ("already completed"); `NIMVLETS_DEV_ONBOARDING`
con un catálogo NO sintético → "not a dev-synthetic onboarding catalog;
not forcing the gate" → arranque normal.

---

### DEC-134 — Pasada de corrección de QA del owner sobre Block 09A: composición del reveal de Frin, y la pestaña "Settings" inalcanzable desde Collection/Shop
**Status:** DECIDIDO · Block 09A (pasada de corrección de QA). **Depende
de** DEC-130 (ruta canónica de preferencias / sección Settings),
DEC-132/DEC-133 (onboarding). NO toca la arquitectura de
persistencia/policy/timing del onboarding (DEC-131/132/133 intactos): es
una corrección de presentación + un bug de ruteo de navegación.

**Contexto.** El owner probó a mano Blocks 07/08/09A y aceptó casi todo
(pets, animaciones, click-through, acumulación de clics, compra de Nidir
a 300, menú rápido nativo con Size/Opacity/Hide, idiomas, pantalla
inicial de onboarding, timing real de 44 s, selección Male/Female de
Frin). Dos rechazos:

1. **El reveal de Frin cambiaba la composición aprobada.** La pantalla
   inicial con exactamente tres tarjetas normales (Artu/Rato/Rin Rin) —
   su tamaño, espaciado, quietud y espacio en blanco — estaba aprobada.
   El reveal de los 44 s metía a Frin como CUARTA tarjeta en la misma
   fila: los tres originales se achicaban y se re-centraban, Frin parecía
   un starter normal más, y se perdía el espacio negativo que hacía
   atractiva la pantalla.
2. **La sección Settings del Product UI "aparecía vacía / no
   funcionaba"**, sobre todo tras el flujo DEV de onboarding.

**Decisión.**

- **Composición del reveal de Frin (`productui::OnboardingLayout`).** La
  primera fila se dibuja SIEMPRE idéntica — sus 3 tarjetas conservan
  EXACTAMENTE su caja/arte/nombre/especie, revelado el secreto o no (ya
  no se pasa Frin al helper de fila uniforme, así que su ancho no depende
  del conteo). Cuando el secreto se revela, Frin aparece en una SEGUNDA
  fila debajo, horizontalmente centrada, en el espacio en blanco
  inferior, con una tarjeta más compacta (`kRevealCardW` 180 /
  `kRevealArt` 96 — armoniosa, no idéntica) vía el nuevo
  `LayoutRevealedSecretCard`. Sin banner, sin "secret unlocked", sin
  cuenta regresiva, sin flash, sin animación: sigue siendo un recálculo
  de layout puro + UN redibujo (`OnboardingView::RevealSecret` sin
  cambios). Frin entra SIEMPRE al final de `candidates`/`focusOrder`
  (orden [normal 0..2, Frin]); `FocusList::SetItems` preserva el id
  enfocado, así el reveal no reordena ni arrebata el foco. El `HitTest`
  itera `candidates`, con lo que el hit-test de mouse cae automáticamente
  en la nueva caja BAJA de Frin. `contentHeight` con el secreto revelado
  usa `kRevealBottomPad` en vez de `kMargin`: la pantalla revelada entra
  en 800×560 sin scroll (EN y ES).
- **La pestaña "Settings" inalcanzable — bug de ruteo, no de
  arquitectura.** Block 08 agregó la sección `kSettings` y su pestaña
  `nav:settings` (que la cabecera compartida `SectionNav` dibuja en las
  TRES secciones), y enseñó a rutear las tres SOLO a `SettingsView`.
  `CollectionView::ActivateWidget` y `ShopView::ActivateWidget` seguían
  con el par `nav:collection`/`nav:shop` de Block 07: un click (o Enter)
  en "Settings" desde Collection o Shop se descartaba en silencio. Como
  el arranque del Product UI siempre cae en Collection, Settings era
  INALCANZABLE con mouse o teclado. Ninguna captura de QA de Block 08 lo
  vio porque todas forzaban la sección con `NIMVLETS_DEV_SECTION` →
  `ShowSectionForQA`, que saltea la pestaña. **Fix:** una única tabla,
  `productui::NavTargetSection(focusId, &outSection)` en `SectionNav`,
  que las tres vistas usan para rutear sus pestañas — así la tabla de
  secciones y sus consumidores no pueden volver a divergir. No se creó
  una segunda ruta de preferencias: la sección Settings ya funcionaba
  (DEC-130), solo no se podía llegar. La transición onboarding→normal ya
  era válida (`HandleOnboardingSelection` cierra la ventana; el reopen la
  reconstruye con `SetLanguage`/`SetPreferences`/`SetModels`), así que no
  hizo falta tocarla.
- **Harness DEV.** `NIMVLETS_DEV_ONBOARDING_CHOOSE` corre ahora ANTES del
  bloque `NIMVLETS_DEV_OPEN_COLLECTION`, para poder fotografiar el
  Product UI NORMAL post-onboarding (flujo del brief "reopen Product UI →
  Settings"). Nuevo `NIMVLETS_DEV_UI_NAV_SMOKE=1`: abre el Product UI y
  sintetiza un click de mouse REAL sobre cada pestaña desde cada sección
  (`ProductWindow::ClickNavTabForQA` → `HandleEvent` →
  `View::OnMouseDown` → `ActivateWidget` → `NavTargetSection`), logueando
  PASS/FAIL por salto — smoke no interactivo de alcanzabilidad, sin
  permisos del SO.

**Verificado.** `tests/SectionNavTest.cpp` (+4: `NavTargetSection` mapea
las TRES pestañas — la de Settings era la que faltaba —, rechaza ids
no-nav sin tocar el out-param, ida y vuelta contra las pestañas que
dibuja `BuildSectionHeaderLayout`, `NavFocusIdFor`).
`tests/OnboardingLayoutTest.cpp` (+5 neto: la primera fila queda EXACTA
byte-a-byte antes/después del reveal; Frin debajo y centrado con tarjeta
más chica; hit-test de Frin en su posición baja; el reveal no duplica
Frin; entra sin scroll EN+ES; y `FirstRowFits…RegardlessOfReveal`
reemplaza a `CardRowFitsHorizontallyForThreeAndFour`). 427 tests C++
(era 418), 162 Python. Debug + Release + universal2 (lipo: x86_64 arm64),
`nimvlets_macos_text_check` PASS, `nimvlets_macos_clickthrough_check`
PASS (sin cambios de plataforma). Smokes: `NIMVLETS_DEV_UI_NAV_SMOKE`
6/6 saltos alcanzables (y 2/6 con el bug reintroducido a mano —
Collection→Settings y Shop→Settings FAIL); capturas Retina del reveal
(EN/ES) con la primera fila sin mover y Frin abajo; Settings
post-onboarding renderiza completa (EN/ES); `NIMVLETS_DEV_PREFS` por la
ruta canónica `Apply*` se refleja en Settings y en el menú; Shop del
catálogo normal intacto (Nidir a 300 comprable); Shop del catálogo
sintético-DEV vacío = comportamiento esperado del harness (sin entradas
públicamente comprables; NO se agregan productos falsos). Deuda diferida
sin tocar: rediseño del Shop (dirección "browse primero"), la posición
visual de Nidir al subir tras un click, y la política always-on-top del
pet sobre el Product UI.

---

### DEC-135 — Shop BROWSE-FIRST: la sección abre en una estantería de personajes; el hero-first de entrada de DEC-127 queda superseded
**Status:** DECIDIDO · Block 09C. **Supersede** el ESTADO DE ENTRADA
hero-first de DEC-127 (el Shop abría con el primer pet ya expandido como
hero). El resto de DEC-127 sigue vigente: el Shop es una sección
separada, sin plantilla de tienda, reusa el acento por pet / hero stage
/ previews `.nvprev`, y su contenido es dato del catálogo. NO toca la
transacción de compra (DEC-126/DEC-128), las autorizaciones (DEC-123),
`catalog::ShopModel`, ni la exclusión de Frin. Es un rediseño de
**Product UI puro**.

**Contexto.** El owner probó el Shop de Block 07 y rechazó su jerarquía
visual: al entrar, el hero seleccionado (el primer pet) dominaba de
inmediato y la sección se sentía "como la Collection con controles de
compra pegados". El owner quiere que el Shop se sienta primero como "un
lugar chico donde miro personajes que podría conseguir", y que un
personaje se vuelva hero grande **solo después de elegirlo**. Referencia
de interacción (no de arte/marca): BROWSE → HOVER revela info liviana →
SELECT → HERO + detalle + compra.

**Decisión.**

- **Estado de presentación explícito y mínimo.** `productui::ShopLayout`
  gana `enum ShopPresentation { kBrowse, kSelected }`; `ShopView`
  distingue TRES cosas que antes se confundían: `hoverId_` (tarjeta bajo
  el mouse/foco — revela, no selecciona, no compra), `selectedPetId_`
  (`""` ⇒ BROWSE sin hero; un `petId` ⇒ ese personaje es el hero) y
  `confirming_` (sub-estado de SELECTED — la confirmación inline de
  Block 07, sin cambios). `hovered != selected != confirming`. No se
  agregó un modelo de estados abstracto: es la representación más chica
  que encaja.
- **BROWSE (entrada normal).** `BuildShopLayout` con `selectedPetId ==
  ""` produce una rejilla centrada de `ShopTile` (arte grande + nombre),
  columnas por cantidad (1..4 en una fila; 5/6 → 2 filas de ≤3; 7/8 →
  2 filas de 4), diseñada para el roster V1 futuro sin hardcodear
  Bunny/Nidir ni asumir dos entradas. El alto de la línea revelada se
  reserva SIEMPRE, así el hover no reordena. Encabezado quieto
  `kShopBrowseHeading` ("Nimvlets you can meet" / "…que puedes
  conocer"). Shop vacío (catálogo DEV) → mensaje quieto localizado
  `kShopEmpty`, sin mencionar el catálogo sintético.
- **HOVER / foco de teclado.** Revela bajo el nombre: `kOwned` → "In
  your collection" (acento); `kAffordable` → el precio (`kTextMuted`);
  `kInsufficientBalance` → el precio (`kTextFaint`) — distinción
  asequible/insuficiente "quiet", sin rojo, sin "no te alcanza". Sin
  tooltips OS, sin animación continua; `OnMouseMove` marca `dirty_` solo
  cuando el objetivo de hover cambia. El foco de teclado sobre una
  tarjeta da la MISMA información.
- **SELECT → HERO.** Un clic / Enter en una tarjeta SELECCIONA (nunca
  compra, nunca muta wallet/propiedad — `SelectHero` solo asigna
  `selectedPetId_`). El personaje se promueve a hero grande
  (`kHeroArt` 216 pt, ~3,6× una tarjeta del rail) con especie /
  descripción / precio / acción, y la estantería completa baja a un
  **rail** compacto bajo un divisor, sobre `kGalleryShelf`, con la
  tarjeta abierta marcada por un subrayado de acento. El rail lista
  TODOS los pets del Shop (no colapsa a una sola tarjeta con el catálogo
  real de 2). El foco se queda en esa misma tarjeta — la selección
  **nunca** salta el foco a la confirmación (brief §12). Un
  `selectedPetId` ausente del Shop (Frin, id desconocido, save editado)
  cae a BROWSE — nunca un hero fantasma.
- **Compra — Block 07 intacto.** Desde el hero: `Get <name>` → confirmación
  inline (`purchase:cancel` / `purchase:confirm`, foco arranca en
  Cancel) → `Confirmar` emite un `PurchaseRequest` con
  `entitlementTarget`. `SpikeApp::HandlePurchaseRequest` sin cambios:
  `EvaluatePurchase` → mutación atómica balance+propiedad → flush
  inmediato → refresco de Shop y Collection. Una tarjeta NUNCA gasta
  clics; un clic perdido nunca compra. `kInsufficientBalance` y
  `kOwned` sin CTA.
- **Sin selección persistida.** Cerrar/reabrir el Product UI, o volver a
  entrar a la sección (`ShopView::OnEnterSection`), devuelve el Shop a
  BROWSE. AppState no cambia (sin bump de schema).
- **QA.** `NIMVLETS_DEV_SHOP_PET=<petId>` se reinterpreta como "abrir el
  Shop con ese personaje SELECCIONADO" (hero); sin la variable, BROWSE
  normal. Nuevo `NIMVLETS_DEV_SHOP_FOCUS=<petId>` (foco de teclado sobre
  una tarjeta). `NIMVLETS_DEV_SHOP_HOVER` / `_CONFIRM` preservados.

**Verificado.** `tests/ShopLayoutTest.cpp` reescrito (8 → 23 tests):
BROWSE por defecto sin hero; rejilla 1/2/3/4/6/8 sin solapes, dentro del
contenido, orden de foco determinista = orden de catálogo; compone sin
scroll a 800×560 para el catálogo real y 1..4; resize (mín 600×460,
760×520, 1100×700); hover/foco revela el texto correcto por estado
(EN/ES); hover NO selecciona; selección promueve el hero y NO muta;
elegir otro reemplaza; selección privada/desconocida → BROWSE;
asequible/insuficiente/poseído del hero (Block 07); confirmación inline
(EN/ES, hit-test, orden Cancel<Confirm, "get" fuera); `confirming`
ignorado salvo SELECTED+asequible; Shop vacío localizado; rail de 8 sin
solapes a varios tamaños. `ShopModelTest` y `PurchasePolicyTest` de
Block 07 sin cambios y verdes. 442 tests C++ (era 427), 162 Python.
Debug + Release + universal2 (lipo: x86_64 arm64),
`nimvlets_macos_text_check` PASS, `nimvlets_macos_clickthrough_check`
PASS (sin cambios de plataforma). Smokes en vivo: BROWSE EN/ES, hover
revela "300 clicks", foco de teclado revela lo mismo, Nidir seleccionado
(asequible / insuficiente / confirmación / poseído), compra
`NIMVLETS_DEV_BUY=nidir` (balance 400→100, persiste al reabrir, la
Collection marca Nidir como poseído), `NIMVLETS_DEV_UI_NAV_SMOKE` 6/6,
8 ciclos open/close limpios, idle del Shop 0,0 % CPU. Congelado sin
tocar: Nidir (arte/animación/escala/geometría/runtime), la deuda
`lie_to_sit` de Frin, Collection, Settings, onboarding.

---

### DEC-136 — La Collection muestra SOLO los Nimvlets poseídos (owner QA, Block 09C)
**Status:** DECIDIDO · Block 09C (pasada de corrección de QA del owner).
**Supersede** el tercer estado "bloqueado" de la Collection que venía de
DEC-108 / DEC-112 (el álbum enumeraba TODO el roster y pintaba los no
poseídos con "Not in your collection"). NO cambia la arquitectura:
sigue `BuildCollectionModel` → `CollectionLayout` → `CollectionView`, y
el enum `OwnershipStatus` conserva sus tres valores. NO toca el Shop
browse-first (DEC-135), la compra (DEC-126/128), Settings ni onboarding.

**Contexto.** El owner aprobó el Shop browse-first y luego rechazó que
la Collection siguiera mostrando Nimvlets que no tiene — un Nidir no
poseído aparecía en la gallery inferior con "Not in your collection".
El modelo mental correcto: **Collection = lo que ya tengo; Shop = lo que
puedo mirar / comprar.** Lo públicamente comprable pero no poseído
pertenece al Shop, no a la Collection.

**Decisión.**
- **`catalog::BuildCollectionModel` descarta los ítems `kLocked`** (un
  Nimvlet sin NINGUNA variante poseída) tras calcular los estados. El
  pet activo se conserva SIEMPRE (es `kActive`, nunca `kLocked`, aunque
  un save corrupto lo tuviera sin poseer — `ResolveOwnedActiveIdentity`
  ya lo repara en el arranque). Un Frin con al menos una variante
  poseída queda `kOwnedInactive` y SÍ aparece, con la otra variante
  marcada no poseída y sin ruta de compra (semántica de DEC-128
  intacta). `CollectionModel::items` pasa a ser "una fila por pet lógico
  POSEÍDO".
- **`CanActivate`** no cambia: un pet no poseído ya no está en el modelo
  → `Find` devuelve nullptr → no activable (mismo resultado que el
  viejo chequeo `status == kLocked`). `HandleActivateRequest` intacto.
- **`CollectionLayout` — gallery vacía (un solo Nimvlet poseído,
  brief §4.A):** sin divisor, sin segundo plano; una sola línea quieta
  hacia el Shop (`StringKey::kCollectionOnlyActive` — "Meet more Nimvlets
  in the Shop." / "Conoce más Nimvlets en la Tienda."). Nada de
  placeholders. La resolución del hero ya caía al pet activo cuando
  `selectedPetId` no está en el modelo, así que un id no poseído
  seleccionado a mano nunca produce un hero "bloqueado".
- **No se rediseña la Collection:** hero + gallery, editorial, acento
  por pet, flujo "Use <name>", persistencia y refresco Shop↔Collection
  quedan igual. Es una corrección de filtrado en el modelo, en la capa
  correcta (la fuente de la gallery ya es owned-only, no se ocultan
  ítems tarde en el render).

**Verificado.** `tests/CollectionModelTest.cpp` (+2 neto: modelo
owned-only excluye no poseídos; Frin sin variante poseída excluido; un
pet recién poseído aparece; un solo poseído → un solo ítem).
`tests/CollectionLayoutTest.cpp` (+2 neto: la gallery excluye no
poseídos; un solo poseído → gallery vacía, sin divisor/segundo plano,
línea EN/ES, cabe en 800×560; el helper `FullyOwnedModel` reemplaza a
`DevModel` donde un test necesitaba ≥2 entradas de gallery; el viejo
`HeroLockedHasNoAction` pasa a `UnownedSelectionFallsBackToActivePet`).
`tests/EntitlementMigrationTest.cpp` (1 test: un `{frin, ""}` v4 suelto
ya ni aparece en el modelo — prueba más fuerte de "no se fabrica
propiedad"). 446 tests C++ (era 442), 162 Python. Debug + Release +
universal2 (lipo: x86_64 arm64), `nimvlets_macos_text_check` PASS,
`nimvlets_macos_clickthrough_check` PASS. Smokes en vivo: Collection
default (dev seed) muestra Bunny + Frin, SIN Nidir; tras
`NIMVLETS_DEV_BUY=nidir` Nidir aparece; Frin como hero conserva el
selector Male/Female; EN/ES; un solo Nimvlet poseído (vía el harness
DEV de onboarding) muestra la línea quieta sin divisor. Congelado sin
tocar: Nidir, deuda `lie_to_sit` de Frin, Shop browse-first, Settings,
onboarding.

---

### DEC-137 — Shop OCULTO DE STARTERS: submodo contextual de la sección Shop, gate lifecycle == kCompleted, no-divulgación del secreto, compra de VARIANTE EXACTA
**Status:** DECIDIDO · Block 10. **Depende de** DEC-123 (`PetEntitlement`,
coincidencia exacta), DEC-126 (aplicación atómica de compra), DEC-128
(objetivo de compra data-driven; `ExpandHistorical…` congelado),
DEC-131/DEC-132 (`OnboardingLifecycle` / política de starter),
DEC-135 (Shop browse-first). **NO** habilita el onboarding de
producción, **NO** agrega contenido de Artu/Rato/Rin Rin, **NO** toca
el catálogo de producción ni la transacción del Shop público (Frin
sigue ausente del Shop público), **NO** fija precios V1 de starters.

**Contexto.** Un usuario que eligió UNA variante de Frin en el
onboarding tiene que poder, eventualmente, comprar la OTRA con clics —
y la arquitectura debe soportar además starters NORMALES no poseídos —
SIN exponer Frin a quien nunca lo descubrió, sin una cuarta pestaña de
navegación, y sin rediseñar entitlements ni la transacción de compra.
El onboarding de producción sigue deshabilitado (falta el contenido de
09B), así que el harness sintético-DEV (`onboarding_dev_catalog.nvcat`)
es el escenario vivo primario.

**Decisión.**

- **Interpretación de datos del catálogo — SIN campo de schema nuevo.**
  `priceClicks` = el precio de compra de ESA identidad EXACTA;
  `publiclyPurchasable` = si esa identidad puede aparecer / comprarse en
  el Shop PÚBLICO; `starterRole` = si la identidad pertenece a la
  política de starter. El Starter Shop oculto usa `lifecycle ==
  kCompleted` + `starterRole != kNone` + `priceClicks > 0` + propiedad +
  reglas del secreto — SIN volver `publiclyPurchasable` true. Se
  inspeccionaron todos los consumidores: `BuildShopModel` y
  `EvaluatePurchase` (Shop público) siguen mirando SOLO
  `publiclyPurchasable`; el loader C++ solo rechaza `publiclyPurchasable
  && priceClicks == 0` (una entrada NO pública con precio es válida). Es
  una combinación NUEVA y coherente, no un cambio de semántica.

- **`catalog::StarterShopModel` + `IsStarterShopEligible` (puro, sin
  SDL).** `BuildStarterShopModel(catalog, lifecycleCompleted, owned,
  balance)` produce SOLO ofertas legítimas — cada una con la
  `PetIdentity` EXACTA, `displayName`, `role`, `priceClicks`, `status`
  (`kAffordable`/`kInsufficientBalance` + `clicksShort`), y
  `entitlementTarget`. `lifecycleCompleted` es un `bool` ya resuelto por
  `src/app` (`== persistence::OnboardingLifecycle::kCompleted` EXACTO —
  ver abajo) para que `src/catalog` NO dependa de `src/persistence`.

- **Algoritmo de elegibilidad (todas las condiciones).**
  (1) `lifecycleCompleted`; (2) hay una entrada de catálogo con esa
  identidad EXACTA y `starterRole != kNone`; (3) `priceClicks > 0`;
  (4) la identidad EXACTA NO está poseída (`Covers` exacto — poseer
  `{frin, "female"}` NO cubre `{frin, "male"}`); (5) starter SECRETO:
  además, el owner ya posee ALGUNA autorización del MISMO petId lógico
  (`OwnsAnyVariantOfPet`); (7) identidad estructuralmente válida. Un
  starter NORMAL no poseído solo necesita 1..4 + 7. **Genérico por rol +
  petId — NUNCA `if (petId == "frin")`.**

- **Regla de NO-DIVULGACIÓN del secreto (crítica, brief §3).** Una
  oferta de un starter SECRETO es elegible SOLO si el owner ya posee al
  menos una autorización EXACTA del MISMO petId lógico secreto. Así
  `owns {frin,female}` → puede ver `{frin,male}`; `owns {artu,""}` →
  Frin NUNCA aparece. **No se persiste ningún flag de "Frin fue
  revelado a los 44 s" y no se agrega ninguno** — la elegibilidad se
  deriva de la propiedad, no de un historial inventado.

- **Gate lifecycle == kCompleted EXACTO (brief §5).** NO
  `OnboardingConsideredComplete` (que es `!= kPending`): un usuario
  `kLegacyComplete` (migrado / dev / sembrado por catálogo) NUNCA ve el
  Starter Shop — evita exponer retroactivamente una economía de starters
  nueva a quien no pasó por el contrato real de elección.
  `SpikeApp::OnboardingLifecycleCompleted()`.

- **`catalog::EvaluateStarterPurchase` — canal de compra DISTINTO.** NO
  se reusa `EvaluatePurchase` ignorando `publiclyPurchasable` (eso
  volvería comprable cualquier identidad privada del catálogo — brief
  §15). Política pura propia que re-verifica INDEPENDIENTEMENTE lifecycle
  == kCompleted, rol de starter, precio > 0, regla de variante hermana
  del secreto, no-poseída, y saldo. El modelo/UI NO es un límite de
  seguridad. Resultados: `kSuccess` / `kAlreadyOwned` /
  `kInsufficientBalance` / `kLifecycleNotCompleted` / `kInvalidTarget` /
  `kNotEligible`. En cualquier fallo el estado resultante == el de
  entrada, canonicalizado; sin underflow.

- **VARIANTE EXACTA (brief §13).** El objetivo es una `PetIdentity`
  completa. Comprar la oferta de Frin otorga EXACTAMENTE `{frin,"male"}`
  o `{frin,"female"}` — NUNCA `{frin,""}` ni ambas. NO se reutiliza
  nada de la migración histórica de "Frin entero".

- **Aplicación atómica COMPARTIDA (brief §16).** `SpikeApp::
  ApplyPurchasedState(newBalance, newEntitlements)` — factorizado desde
  `HandlePurchaseRequest` — muta `clickBalance` + `ownedEntitlements` en
  el MISMO `AppState`, marca dirty, y flushea de inmediato (un solo
  `SerializeAppState` + un solo `rename` atómico — DEC-126). El Shop
  público y el Starter Shop tienen políticas de elegibilidad separadas
  pero el MISMO camino de aplicación de estado. **Sin bump de schema
  (AppState sigue en v5).**

- **NO auto-activa (brief §17).** Una compra de starter otorga
  propiedad; NO reemplaza el pet del escritorio. Tras la compra: balance
  y Collection se refrescan al instante, la nueva variante es usable
  desde la Collection (flujo "Use <name>" existente), el pet activo
  queda igual.

- **UX de acceso — submodo, NO una cuarta pestaña (brief §10/§11).**
  `SectionNav` sigue siendo EXACTAMENTE `Collection · Shop · Settings`.
  Cuando hay ≥ 1 oferta, el Shop público dibuja UNA línea quieta
  "Starter choices…" / "Opciones iniciales…" cerca del pie
  (`ShopLayout::starterAffordanceVisible`, `focusId "starter:enter"`) —
  sin banner, sin badge, sin toast, sin popup. Con 0 ofertas la
  afordancia NO existe. Activarla entra a un submodo que la sección
  Shop POSEE (`ProductWindow::starterShopSubmode_`): la cabecera
  compartida sigue marcando "Shop", el input se rutea a
  `StarterShopView` en vez de `ShopView`, y hay un back affordance
  quieto "← Shop" / "← Tienda" (`focusId "starter:back"`). Cambiar de
  sección o reabrir la ventana limpia el submodo.

- **Reutilización de layout / pintado (brief §12).** El submodo REUSA la
  geometría del Shop browse-first — se extrajeron `ComputeBrowseGrid` /
  `LayoutBrowseGrid` / `LayoutShopRail` / `LayoutShopHero` a helpers
  públicos en `ShopLayout.h` (una sola copia) — y su pintado — se
  extrajo `DrawShopTile` / `DrawShopHero` / `FillShopStagePrimitive` a
  `productui/ShopPaint.{h,cpp}`, usado por `ShopView` y
  `StarterShopView`. `productui::StarterShopLayout` (puro) opera en
  IDENTIDADES EXACTAS (`focusId "starteritem:<petId>/<variantId>"`, dos
  "frin" conviven) y compone el nombre `"Frin · Male"` con las claves
  `kMale`/`kFemale` (Frin no se traduce). El hero / las tarjetas
  resuelven la `.nvprev` por `(petId, variantId)` EXACTOS.

- **Teclado / Esc (brief §20).** Submodo browse: Tab/Shift+Tab, flechas,
  Enter/Espacio selecciona, Esc → Shop público. Seleccionado: "get"
  separado, Esc → browse. Confirmando: Cancel enfocado primero, Esc
  cancela la confirmación, Confirm separado, NUNCA se sale del submodo
  con una confirmación abierta. Esc en el submodo NUNCA cierra la
  ventana — siempre da un paso atrás.

- **Estado vacío (brief §19).** Comprar la última oferta deja el submodo
  abierto con una línea quieta "No more starter choices." / "No quedan
  opciones iniciales." + "← Shop". Sin confetti / toast / logro. Al
  volver al Shop público la afordancia ya no existe.

- **DEV harness (brief §22/§23).** `assets/dev/onboarding_dev_catalog_manifest.json`
  gana `price_clicks` de QA en las 5 entradas de starter
  (`frin/male` = `frin/female` = 150, `artu_dev` = 80, `rato_dev` = 100,
  `rinrin_dev` = 120) — `publicly_purchasable` sigue en false, así el
  Shop PÚBLICO del catálogo sintético sigue vacío. Son precios SÓLO DE
  QA / PROVISIONALES, NO el balanceo V1 de la economía de starters. Los
  precios públicos Bunny=120 / Nidir=300 NO cambian. El catálogo de
  producción (`pet_catalog.nvcat`) NO se toca — sus entradas siguen
  `starterRole: kNone` + precio 0, así el Starter Shop está INERTE en
  producción hasta que un bloque futuro marque las entradas de Frin.
  Nuevos hooks: `NIMVLETS_DEV_GRANT_CLICKS`, `NIMVLETS_DEV_STARTER_BUY`,
  `NIMVLETS_DEV_STARTER_SHOP` / `_OFFER` / `_HOVER` / `_FOCUS` /
  `_CONFIRM`.

**Verificado.** `tests/StarterShopModelTest.cpp` (14: gate de lifecycle
pending/legacy/completed, propiedad exacta excluida, normal priced
elegible, secreto elegible solo con variante hermana, secreto sin
hermana oculto, el secreto no filtra a un usuario de starter normal,
precio 0 sin oferta, no-starter nunca ofrecido, identidad de variante
EXACTA, asequible/insuficiente + clicksShort, orden determinista de
catálogo, identidad desconocida). `tests/StarterPurchasePolicyTest.cpp`
(13: variante secreta faltante OK, starter normal OK, secreto sin
hermana rechazado, lifecycle != completed rechazado, no-starter
rechazado, precio 0 rechazado, ya poseída rechazada, saldo insuficiente
rechazado, identidad inválida rechazada, fallo = cero mutación, éxito
otorga EXACTO a balance 0 sin underflow, dedup canónico + idempotencia,
modelo y política de acuerdo). `tests/StarterShopLayoutTest.cpp` (12:
round-trip de focusId, browse con back + ofertas en el focus order,
etiqueta de variante EXACTA "Frin · Male"/"Frin · Macho", hit-test de
browse, selección promueve el hero de variante EXACTA + `.nvprev`
correcta, insuficiente, selección desconocida → browse, confirmación
EN/ES, confirming ignorado salvo asequible, estado vacío EN/ES,
resize sin solapes, determinista). `tests/ShopLayoutTest.cpp` (+2:
sin afordancia por defecto — focus order y hit-test intactos; afordancia
cuando se pide — al final del focus order, hittest-able, sin scroll,
EN/ES). `tests/LocalizationTest.cpp` (+1: las 4 claves nuevas EN/ES).
`ShopModelTest` / `PurchasePolicyTest` / `CollectionModelTest` de Block
07/09C sin cambios y verdes. 488 tests C++ (era 446), 162 Python.
Debug + Release + universal2 (lipo: x86_64 arm64),
`nimvlets_macos_text_check` PASS, `nimvlets_macos_clickthrough_check`
PASS (sin cambios de plataforma). Smokes en vivo (Retina, appdata
aislado): usuario legacy → Shop público SIN afordancia; DEV elige
frin/female → afordancia "Starter choices…", submodo con oferta
frin/male + 3 dev normales; frin/male seleccionado → hero (lobo blanco,
"White wolf", "150 clicks", "Get Frin · Male"); confirmación "Spend 150
clicks to add Frin · Male…"; `NIMVLETS_DEV_STARTER_BUY=frin/male` →
balance 500→350, ambas variantes EXACTAS persistidas, activo sin
cambiar, la oferta desaparece; comprar las 4 → "No more starter
choices."; Collection tras la compra muestra Frin con Male/Female ambas
poseídas y activables; ES completo ("Tienda", "← Tienda", "Opciones
iniciales", "Frin · Macho", "Obtener Frin · Macho"); Nidir público a
300 sigue comprable; `NIMVLETS_DEV_UI_NAV_SMOKE` 6/6. Congelado sin
tocar: onboarding de producción (deshabilitado), branch
`block/09b-starter-content-production-onboarding`, Nidir, la deuda
`lie_to_sit` de Frin, contenido de animación, el catálogo de
producción.

---

### DEC-138 — Block 10, corrección de QA del owner: wallet CANÓNICO en la cabecera compartida, y el Shop oculto de starters se vuelve VERDADERAMENTE oculto (hotspot invisible, sin afordancia visible)
**Status:** DECIDIDO · Block 10 (pasada de corrección de QA del owner).
**Depende de** DEC-127/DEC-130 (cabecera compartida / ruta canónica de
preferencias), DEC-137 (Shop oculto de starters). **Supersede SOLO** el
mecanismo de acceso VISIBLE de DEC-137 (la afordancia "Starter choices…"
del Shop público). NO toca la elegibilidad, el gate de lifecycle == kCompleted,
la regla de no-divulgación del secreto, la compra de variante EXACTA, la
aplicación atómica de estado, ni el submodo en sí (incluido su "← Shop").

**Contexto.** El owner probó Block 10 a fondo y aprobó casi todo. Dos
rechazos:

1. **Bug real de sincronización del wallet.** Con el mismo estado
   corriendo, la cabecera de Collection y de Shop mostraba "500 clicks"
   y la de **Settings** mostraba "0 clicks". El balance persistido/runtime
   NO era 0 — solo Settings dibujaba mal. Causa raíz: `SettingsView::Render`
   (Block 08) llamaba `DrawSectionHeader(..., /*clickBalance=*/0, ...)`
   hard-codeado — Settings nunca recibió el balance. Collection y Shop
   guardaban cada uno su propia copia de `clickBalance_` (empujada por
   `SetModel(model, clickBalance)`); Settings no tenía ni el campo. Eran,
   de hecho, autoridades de wallet por-sección.

2. **El Shop oculto de starters no era lo bastante oculto.** El owner
   rechazó la línea VISIBLE "Starter choices…" / "Opciones iniciales…"
   dentro del Shop público. Quiere que sea un easter-egg de verdad: un
   HOTSPOT INVISIBLE en la esquina inferior derecha.

**Decisión.**

- **Wallet CANÓNICO, autoridad ÚNICA en `ProductWindow`.** `ProductWindow`
  gana `std::uint64_t clickBalance_`, lo fija `SetModels`, y `DrawFrame()`
  lo pasa a `Render()` de la sección visible — las CUATRO. El balance
  YA NO viaja con el modelo: `CollectionView::SetModel` /
  `ShopView::SetModel` / `StarterShopView::SetModel` pierden el parámetro;
  cada `Render(...)` gana un `std::uint64_t clickBalance` trailing (el
  compilador obliga a pasarlo — una sección nueva no puede "olvidarse").
  El texto se formatea UNA vez en la capa PURA: `BuildSectionHeaderLayout`
  gana `clickBalance` y produce `SectionHeaderLayout::clicksText` (vía
  `FormatClickCount`); `DrawSectionHeader` solo dibuja ese string (perdió
  su parámetro de balance Y el de idioma — todo el texto ya viene
  localizado en `header`). Cada `*LayoutInput` (Collection / Shop /
  Settings / StarterShop) gana `clickBalance` y lo reenvía a
  `BuildSectionHeaderLayout`. Mismo espíritu que DEC-130 para las
  preferencias: una fuente de verdad, todas las vistas consumen la
  MISMA. Sin bump de schema (AppState intacto).

- **Afordancia visible ELIMINADA.** Se borra `ShopLayout::starterAffordance*`,
  `ShopLayoutInput::starterAffordanceVisible`, `AppendStarterAffordance`,
  el focusId `"starter:enter"` (fuera del focusOrder y del HitTest),
  `ShopView::SetStarterAffordanceVisible` y `DrawStarterAffordance`. La
  clave de localización `kStarterChoicesAffordance` se elimina (no queda
  copy de producto muerta). `kStarterChoicesHeading` / `kStarterShopBack`
  / `kStarterShopEmpty` se conservan — se usan DENTRO del submodo.

- **HOTSPOT INVISIBLE, esquina inf-der.** `ShopLayout` gana
  `bool starterHotspotArmed` + `UiRect starterHotspotRect` (48×48 pt,
  anclado a `{viewportW-48, viewportH-48}` en coords de VIEWPORT, sin
  scroll — es "chrome secreto", no contenido) + `HitStarterHotspot(x, y)`
  (consulta SEPARADA de `HitTest`, que NUNCA devuelve nada para el
  hotspot). NO se dibuja, NO está en `focusOrder`, sin hover, sin
  cursor, sin foco, sin Tab. `ArmStarterHotspot` lo recalcula en cada
  `BuildShopLayout` → el resize lo reubica. `ShopView::SetStarterHotspotArmed`
  (lo arma `ProductWindow::SetModels` sii `!starterShopModel.Empty()`).
  `ShopView::OnMouseDown`: SOLO cuando `HitTest` devolvió "" (zona
  muerta, nunca sobre un control visible) se consulta
  `HitStarterHotspot`; si cae, `enterStarterSubmode`. Solo click
  primario. Sin ofertas legítimas → `starterHotspotArmed == false` →
  no-op total (un usuario `kLegacyComplete` / `kPending`, o uno sin
  hermana del secreto, no descubre nada).

- **La elegibilidad sigue siendo autoridad del MODELO.** El hotspot es
  solo una ENTRADA oculta al `StarterShopModel` ya filtrado — nunca
  cambia qué ofertas hay. Un usuario que eligió `artu_dev` cuyo modelo
  legítimo tiene solo `rato_dev` / `rinrin_dev`: el hotspot abre ESE
  submodo; Frin sigue ausente.

- **Dentro del submodo, todo igual.** Teclado / Esc / selección / Get /
  Cancel / Confirm / "← Shop" sin cambios (el gesto de descubrimiento es
  mouse-only; una vez adentro, el teclado funciona como siempre).

- **Hooks DEV.** `NIMVLETS_DEV_STARTER_SHOP=1` sigue abriendo el submodo
  DIRECTAMENTE (no cuenta como descubribilidad de producción). Nuevo
  `NIMVLETS_DEV_STARTER_HOTSPOT=1`: sintetiza un click primario REAL en
  la esquina inf-der y lo procesa por el MISMO camino que un click del
  owner (`ProductWindow::ClickStarterHotspotForQA` → `HandleEvent` →
  `ShopView::OnMouseDown` → `HitStarterHotspot`); loguea si abrió o fue
  no-op — prueba el hotspot Y su gate sin un click humano.

**Verificado.** `tests/SectionNavTest.cpp` (+1: `BuildSectionHeaderLayout`
formatea el balance canónico a `clicksText`, EN/ES, singular/plural,
agrupación de dígitos — para las 3 secciones). `tests/SettingsLayoutTest.cpp`
(+1, REGRESIÓN de la falla exacta: `BuildSettingsLayout` con
`in.clickBalance = 500` → `header.clicksText == "500 clicks"`; default 0
documentado). `tests/ShopLayoutTest.cpp` (reemplaza los 2 tests de
afordancia por 3 de hotspot: desarmado por defecto = focus order / hit-test
intactos + `HitStarterHotspot` false en la esquina; armado = rect en el
viewport pegado a la esquina inf-der, punto dentro cae, adyacente fuera
no, NUNCA en focusOrder ni HitTest, hit-test de tarjetas intacto, Shop
público vacío + armado sigue funcionando; el resize lo reubica). +
`ShopLayoutTest`/`SettingsLayoutTest` chequean que la cabecera del Shop /
Settings muestra el balance canónico. `tests/LocalizationTest.cpp`
(`kStarterChoicesAffordance` removida; las 3 claves del submodo se
mantienen). 491 tests C++ (era 488), 162 Python. Debug + Release +
universal2 (lipo: x86_64 arm64), `nimvlets_macos_text_check` PASS,
`nimvlets_macos_clickthrough_check` PASS. Smokes en vivo (Retina, appdata
aislado): **Settings muestra "500 clicks"** igual que Shop / Collection
(antes "0 clicks"); tras comprar frin/male, Settings muestra "350 clicks";
Shop público de un usuario legacy = SIN ninguna pista visible;
`NIMVLETS_DEV_STARTER_HOTSPOT` → OPENED para un usuario Frin-hembra,
NO-OP para uno legacy; el hotspot de un usuario que eligió `artu_dev`
abre `rato_dev` / `rinrin_dev` SIN Frin; ES completo ("Tienda", "←
Tienda", "Opciones iniciales", "500 clics"); Nidir público a 300 sigue
comprable; `NIMVLETS_DEV_UI_NAV_SMOKE` 6/6. Congelado sin tocar: Nidir,
la deuda `lie_to_sit` de Frin, onboarding, política de compra /
elegibilidad, la Collection owned-only, el catálogo de producción, el
schema de AppState.

### DEC-139 — Modo de conteo de clics GLOBAL, opt-in: autorizado e implementado; modo pedido vs. efectivo; prevención de doble conteo; Input Monitoring (no Accessibility); AppState v6

**Fecha:** 2026-09-01 · **Bloque:** 11A · **Estado:** vigente ·
**Supersede:** la prohibición de AGENTS.md §14 y de
`docs/PRIVACY_SECURITY.md` §B ("no implementar hasta que un bloque lo
autorice explícitamente"), **solo** esa prohibición.

**Contexto.** Desde Block 01, contar clics fuera del Nimvlet era una
feature futura **prohibida**: AGENTS.md §14 la vetaba junto con el
permiso que necesitaría, y DEC-086 registra que en Block 05 se descartó
explícitamente un monitor global de `NSEvent` —técnicamente viable para
el problema de click-through— por esta misma regla. El brief de Block
11A la autoriza. **Ninguna restricción de privacidad se relaja**; varias
se vuelven más específicas y quedan fijadas por tests.

**Producto.** Una preferencia de **Settings** —y solo de Settings—:
`Click counting [ Nimvlet only ] [ Anywhere ]`, default **Nimvlet
only**, para usuarios nuevos y migrados por igual. El menú rápido **no**
la gana: es la primera preferencia que vive solo en Settings, y esa
asimetría es deliberada (el menú se queda chico; Settings crece).
`tests/QuickMenuModelTest.cpp` fija la regresión.

**Modo PEDIDO vs. modo EFECTIVO.** Se persiste lo que el owner eligió;
lo que está pasando se DERIVA
(`core::ResolveEffectiveClickCounting(requested, monitorActive)`):
efectivo es Global sii se pidió `Anywhere` **y** el monitor corre de
verdad. Si no puede activarse —permiso denegado/revocado, backend
ausente, fallo de arranque— cae con seguridad a **Local**: los clics
sobre el pet siguen contando. Nunca se descarta un clic en silencio ni
se finge que el modo global está activo.

*Decisión de UX pedida explícitamente por el brief §5:* la preferencia
persistida **NO se auto-degrada**. Al confirmar con "Continue" se
escribe `anywhere` aunque el permiso quede pendiente, y Settings muestra
el estado real. Razón: en macOS `CGRequestListenEventAccess()` casi
nunca concede en el momento (el diálogo solo ofrece abrir Ajustes del
Sistema). Revertir la preferencia haría que el owner concediera el
permiso, volviera, y encontrara el control de vuelta en "Nimvlet only" —
más ambiguo, no menos. Así, el control refleja lo que el owner eligió y
la línea de estado refleja lo que ocurre.

**Prevención de doble conteo — el punto central.** En modo global, un
clic sobre el pet lo ven las dos rutas y valdría +2. La regla vive en
una política PURA (`core::CountedClickShouldIncrement`), no en `if
(global)` repartidos: (Local, pet)→sí; (Local, global)→no *(defensivo:
un evento reenviado tarde no se cuela)*; (Global, pet)→**no**; (Global,
global)→sí. `SpikeApp::HandleCountedClick(source, nowMs)` es el ÚNICO
punto de mutación del wallet para las dos fuentes. No hay segundo
wallet, ni contadores por fuente, ni `source` persistido. En modo global
el clic sobre el pet **sigue** disparando su animación de personalidad:
solo cambia de dónde viene la moneda.

**Semántica de arrastre, guiada por privacidad.** En modo global una
pulsación primaria cuenta **una vez, aunque se vuelva un arrastre**.
Distinguir clic de arrastre exigiría mirar el movimiento del puntero —
coordenadas. Se prefiere una semántica ligeramente distinta y honesta
antes que un seguimiento de puntero que el producto promete no hacer. En
modo local la semántica histórica no cambia (un arrastre no cuenta).

**macOS.** `CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap,
**kCGEventTapOptionListenOnly**, CGEventMaskBit(**kCGEventLeftMouseDown**))`,
con permiso **Input Monitoring** (`CGPreflightListenEventAccess` /
`CGRequestListenEventAccess`) — **NO Accessibility** (`AXIsProcessTrusted`
sigue prohibido en todo `src/`), **NO** Screen Recording, **NO** root.
El callback lee el TIPO del evento y nada más.

*Hilo dedicado, y por qué.* Se leyó primero la fuente pineada de SDL
3.4.12 (AGENTS.md §4): `Cocoa_PumpEventsUntilDate` usa
`NSDefaultRunLoopMode`, así que el run loop principal **sí** atiende
sources… mientras espera. Durante un render, la carga de un `.nvpack` o
un redibujo del Product UI, no. Un `CGEventTap` tiene timeout duro y el
sistema lo **deshabilita** si el callback no responde
(`kCGEventTapDisabledByTimeout`) — colgarlo del run loop principal
apagaría el conteo justo cuando el owner interactúa. El hilo existe solo
mientras el modo está activo, bloquea en `CFRunLoopRun()` (cero polling,
cero wakeups periódicos), y se para con una `CFRunLoopSource` **señalada**
—latched— que elimina la carrera "Stop() antes de CFRunLoopRun()" sin
necesitar un despertar de guardia. `Stop()` hace `join`.

*Entrega.* Un `SDL_PushEvent` de un `SDL_EVENT_USER` propio **sin código
ni datos** — el mismo seam que el menú rápido usa desde Block 06.
`SDL_PushEvent` es thread-safe y (leído de la fuente) dispara el wakeup
del `SDL_WaitEventTimeout` principal.

**Sin prompt al arrancar.** Elegir "Anywhere" muestra primero una
explicación de primera parte; **solo "Continue"** puede llamar al pedido
nativo. En cada arranque se hace *preflight* (que nunca muestra
diálogo): si ya está concedido el monitor arranca en silencio, si no se
cae a local y Settings lo dice. Tras un pedido pendiente/denegado se
nombra el lugar del OS y se ofrece **"Check again"** —
**deliberadamente NO** un deep link `x-apple.systempreferences:` hacia
Input Monitoring, que no es API documentada y ha cambiado entre
versiones.

**Capacidad, no plataforma.** Settings consume
`platform::GlobalClickUiState` (capacidad/permiso/actividad ya
resueltos) y el **nombre del permiso como DATO** sustituido en
`{permission}`. `src/productui`, `src/app` y `src/core` no contienen
ningún `#ifdef __APPLE__` / `#ifdef _WIN32`.

**Windows / Linux: honestidad, no paridad fingida.** Los adapters
reportan `kUnavailable` y Settings dibuja "Anywhere" apagado con "Not
available on this system". El diseño Win32 (`WH_MOUSE_LL`, nunca
`WH_KEYBOARD_LL`) y el X11 (`XI_RawButtonPress`) quedan **investigados y
documentados, no escritos**: brief §16/§17 + AGENTS.md §4 — un hook
global de input no se demuestra compilando, y este bloque no tuvo
Windows ni Linux para correrlo. Wayland no puede, por diseño del
protocolo, sin capturar pantalla / `/dev/input` / un portal que
*desvía* el puntero.

**AppState v6.** Un solo campo nuevo: `clickCountingMode`, string
(`""`/`"nimvlet_only"`/`"anywhere"`), con la disciplina de
`sizeChoice`/`language`. Migración v1..v5 → **`kNimvletOnly`**.
**La frontera histórica de propiedad NO se movió:** el gate sigue siendo
`< kFirstExplicitEntitlementSchema` (**== 4**), un umbral semántico
fijo, jamás `kCurrentSchemaVersion` — el error que el brief §12 pedía
descartar. Fijado por regresión: v4→v6 y v5→v6 no ganan variantes de
Frin, no reinterpretan propiedad, y la constante sigue valiendo 4.

**El guard de privacidad se afila, no se afloja.** El test de Python que
prohibía `CGEventTapCreate` de plano ahora fija que: el tap y las APIs
de permiso viven en **exactamente un archivo**; el tap es listen-only y
nunca `kCGEventTapOptionDefault`; la máscara es exactamente
`kCGEventLeftMouseDown` (falla si aparece cualquier otro evento); el
callback no lee coordenadas/flags/timestamp/proceso destino; el pedido
de permiso ocurre en un solo call site; el callback de reenvío no tiene
parámetros de datos; y `AppState` no persiste nada más que el modo.
Además sigue prohibiendo, en todo `src/`, Accessibility, captura de
pantalla, HID crudo, síntesis de eventos, `WH_KEYBOARD_LL`, constantes
de teclado, enumeración de apps y red. El guard mide **código**, no
comentarios — para que los adapters puedan explicar en prosa justamente
lo que no usan.

**Verificado.** 535 tests C++ (eran 491) y 166 Python (eran 162), Debug
+ Release + universal2 (lipo: x86_64 arm64, tests corriendo en el
binario fat), `nimvlets_macos_text_check` y
`nimvlets_macos_clickthrough_check` PASS. En vivo en macOS 26.6 (arm64),
app-data aislado: arranque local sin monitor ni prompt; 5 eventos
globales en modo local → **0 contados**; "Anywhere" con permiso
concedido → monitor **ACTIVE**; **doble conteo**: 4 clics del pet + 6
eventos globales → balance 3 → **9** (los del pet sumaron 0); reinicio
con `anywhere` persistido → preflight y arranque **sin prompt**; volver
a local → *"monitor stopped"* y los eventos globales dejan de contar;
archivo en disco **v6**; shutdown limpio siempre. Release en reposo:
**0.0 % de CPU en 12 muestras** y RSS indistinguible, con el monitor ON
y OFF. Regresiones sin cambios: `NIMVLETS_DEV_UI_NAV_SMOKE` 6/6,
onboarding DEV (`lifecycle=completed`), hotspot invisible del Starter
Shop, compra del Shop público (Nidir a 300), Collection owned-only,
EN/ES completo.

**Hueco honesto — CERRADO.** Al escribir esta entrada no se había
verificado que una pulsación **física** real del escritorio llegara al
tap y sumara 1: sintetizar un clic exigiría `CGEventPost` / permiso de
*post event* o Accessibility, prohibidos acá, así que solo podía
cerrarlo una persona. **El owner corrió la checklist completa a mano en
macOS y dio PASS** — ver DEC-140, `docs/GLOBAL_CLICK_MODE.md` §16.1 y
`docs/PLATFORM_SPIKE.md` §13.3.

**Congelado sin tocar:** Nidir (control dorado), la deuda `lie_to_sit`
de Frin, el contenido de Bunny/Nidir/Frin, el timing de hover, el
scheduling ambient, la precedencia de animaciones, onboarding de
producción, la política de compra/elegibilidad, el Starter Shop oculto,
la Collection owned-only, el catálogo de producción, y Block 09B.

### DEC-140 — QA física del owner en macOS: el conteo global queda VERIFICADO; el wallet se refresca en vivo por una sola ruta; `Collection…` restaura la ventana minimizada; la redacción amplia de Input Monitoring se explica en primera persona

**Fecha:** 2026-09-01 · **Bloque:** 11A (pase final de corrección de QA) ·
**Estado:** vigente · **Complementa:** DEC-139 (no lo supersede: ninguna
decisión de arquitectura, privacidad o permiso cambia).

**Contexto.** DEC-139 dejó un hueco explícito: nadie había clickeado
físicamente. El owner corrió la checklist entera sobre hardware real y
encontró tres cosas — una confirmación y dos defectos, ninguno en el
monitor mismo.

**1. El camino físico del `CGEventTap` de macOS: PASS.** Modo local:
clics físicos afuera **+0**, clic directo sobre el Nimvlet **+1**.
"Anywhere": 5 clics primarios físicos afuera → **+5** exactos; 1 clic
físico sobre el Nimvlet → **+1** exacto, con su animación intacta;
arrastre físico → **+1**; clic derecho **+0**; scroll **+0**; teclado
**+0**. Cambiar a "Nimvlet only" corta el conteo de afuera en el acto y
conserva el del pet; el reinicio deja preferencia y permiso coherentes.
`docs/PLATFORM_SPIKE.md` §13 pasa de NOT TESTED a **PASS** en esa fila.
**El monitor global no se rediseñó ni se tocó** en este pase: misma
semántica de primary-mouse-down, sin teclado, sin coordenadas, sin
historial, sin Accessibility.

**2. El wallet no se refrescaba en vivo con Settings visible.** Con el
Product UI en **Settings** y conteo local, un clic sobre el Nimvlet
subía `AppState::clickBalance` pero la cabecera seguía mostrando el
número viejo hasta cambiar de sección; con "Anywhere" parecía funcionar.

*Causa real:* no era conteo, ni persistencia, ni formateo, sino
**invalidación**. `ProductWindow::SetModels` asignaba el balance mostrado
y, de paso, ensuciaba Collection / Shop porque cada `*View::SetModel`
marca su vista sucia. Settings **no recibe ningún modelo**, así que nada
la ensuciaba y `RenderIfNeeded()` —que dibuja solo con `pendingExpose_` o
con la vista **activa** sucia— no hacía nada. La "liveness" de "Anywhere"
era igual de accidental: clickear en otra app le quita el foco a nuestra
ventana, macOS repinta su chrome, llega un `SDL_EVENT_WINDOW_EXPOSED`, y
el frame se redibuja tomando de rebote el balance nuevo.

*Corrección canónica:* el cambio de valor **es** la invalidación. El
`uint64_t` suelto pasó a ser `productui::WalletDisplay` (puro), y
`ProductWindow::SetClickBalance` —el único escritor, al que `SetModels`
delega— marca `pendingExpose_` **si y solo si** el número cambió. El
balance vive en la cabecera compartida, así que la invalidación es
independiente de la sección, por construcción. No se agregó ningún wallet
por sección, ninguna variable de balance en Settings, ningún polling, ni
se tocó `SpikeApp::HandleCountedClick`: local y global siguen convergiendo
en el MISMO punto de mutación y el MISMO debounce de persistencia. Un
clic contado = un frame, solo si el número cambió; con la ventana
minimizada no se dibuja nada.

**3. La ventana minimizada no volvía desde `Collection…`.** `FocusWindow()`
hacía `BringApplicationToForeground` + `SDL_ShowWindow` + `SDL_RaiseWindow`,
y **ninguna de las dos llamadas de SDL toca una ventana minimizada**:
`SDL_ShowWindow` corta porque una ventana minimizada no es
`SDL_WINDOW_HIDDEN`, y `Cocoa_RaiseWindow` se salta su cuerpo mientras
`[nswindow isMiniaturized]` (leído en la fuente de la SDL pineada,
AGENTS.md §4).

*Corrección:* **cross-platform, con SDL, sin nada nativo nuevo** —
`SDL_RestoreWindow` antes de mostrar/subir, decidido por la política pura
`productui::ResolveWindowPresentStep(exists, minimized)`: cerrada → crear,
visible → traer al frente (contrato preexistente intacto), minimizada →
**restaurar LA MISMA ventana**. Nunca se crea una segunda ventana ni se
resetea pet activo / wallet / sección / Shop / preferencias / onboarding;
si el owner minimizó en Settings o en el Shop, vuelve a esa sección. El
restore se pide **una vez por minimización** (latch rearmado por los
eventos reales `WINDOW_MINIMIZED` / `WINDOW_RESTORED`), porque un solo
`Collection…` pasa por `FocusWindow()` dos veces y la segunda podría caer
en la rama `zoom:` de `Cocoa_RestoreWindow` y desmaximizar la ventana.

**4. La redacción amplia de macOS, dicha por nosotros primero.** El
diálogo real dice algo del estilo *"would like to receive keystrokes from
any application"* y Ajustes del Sistema describe Input Monitoring en
términos de teclado. **Eso no autoriza nada**: la máscara sigue siendo
`CGEventMaskBit(kCGEventLeftMouseDown)` y sólo eso, listen-only, sin
teclado, sin Accessibility, sin Screen Recording — re-auditado, y el guard
de Python ahora además exige **un solo** `CGEventMaskBit(` en el archivo.
Lo que cambió es la **copy**: la explicación previa al permiso anticipa
que el sistema puede describirlo de forma amplia, aclara que esa redacción
cubre la categoría entera del permiso y no lo que Nimvlets hace, y repite
el alcance real. Se dice de forma genérica ("tu sistema" + `{permission}`),
sin ramas por plataforma, sin pretender control sobre el texto de Apple y
sin insinuar que podamos pedir una categoría más chica. El recordatorio
corto se repite en los dos estados en los que la entrada del permiso está
viva en Ajustes del Sistema. Sin modal nuevo y sin nada forzado en cada
arranque.

**5. Atribución del permiso en DEV: "Antigravity IDE".** En desarrollo
macOS listó al IDE del owner en vez de a Nimvlets. Investigado: el binario
es un Mach-O suelto, ad-hoc/linker-signed, **sin bundle**
(`Info.plist=not bound`, sin `MACOSX_BUNDLE` en el CMake), así que TCC
atribuye el permiso al **proceso responsable** — la app que lanzó la
terminal. Consistente con la topología actual, **no** un bug de identidad
(no hay identidad que pueda estar mal). No se cambió nada del build para
maquillar la captura, no se tocó TCC, ni `tccutil`, ni `sudo`.
**La identidad del permiso en RELEASE queda NOT TESTED** y es requisito
de QA de un bloque futuro con un `Nimvlets.app` real y firmado.

**Alcance de Settings.** El grupo `Companion / Interaction / Language`
queda como está: este pase solo corrigió el wallet en vivo y la copy del
permiso. El rediseño visual grande de Settings sigue siendo trabajo
separado.

**Congelado sin tocar:** el monitor global y su ciclo de vida, el permiso
(preflight / request / un solo call site / sin prompt al arrancar), modo
pedido vs. efectivo con fallback seguro a local, la regla de no doble
conteo, AppState v6 y la frontera congelada de migración de Frin, el
runtime del pet, el contenido de Bunny/Nidir/Frin, onboarding, el Starter
Shop oculto, la política de compra, y el catálogo de producción.

### DEC-141 — Block 11B: Settings es la superficie de configuración COMPLETA; visibilidad transitoria por ruta canónica; "Reset position" (colocación segura, sin coordenadas inventadas); el ítem del menú pasa a "Open Nimvlets…"

**Fecha:** 2026-09-02 · **Bloque:** 11B · **Estado:** vigente ·
**Depende de** DEC-130 (ruta canónica de preferencias / sección Settings),
DEC-138/DEC-140 (cabecera compartida / wallet canónico / restauración de
la ventana). **NO** toca la arquitectura de Global Click de Block 11A
(DEC-139/DEC-140 intactos), ni persistencia (sin bump de schema — sigue
en v6), ni el runtime del pet, ni el contenido. **NO** es la pasada
visual futura del universo Nimvlets.

**Contexto.** Block 11A dejó Settings con `Companion / Interaction /
Language` y el wallet en vivo, pero la superficie funcional estaba
incompleta: no había forma de mostrar/ocultar el pet desde Settings, ni
de recuperar una ventana que quedó fuera de pantalla tras un cambio de
monitores. Este bloque completa esa superficie **actual** (no la
rediseña).

**1. Settings es la configuración COMPLETA; el menú rápido es un
subconjunto de conveniencia.** Regla de producto permanente (ahora en
`AGENTS.md` §2): el menú de la barra se queda deliberadamente chico y
rápido (pet, Show/Hide, Open Nimvlets…, Size, Opacity, Lock, Language,
Quit); Settings puede —y debe— tener funcionalidad que el menú no tiene.
`Interaction` (Block 11A) fue el primer caso; Block 11B suma Visibility y
"Reset position" **solo** en Settings. `tests/QuickMenuModelTest.cpp`
(`QuickMenuStaysASmallSubsetOfSettings`) fija que ninguna etiqueta del
menú menciona esos controles y que el conjunto de submenús sigue siendo
`Size / Opacity / Language`.

**2. Visibility en Companion, TRANSITORIA, por la ruta canónica.** Una
fila `[ Shown ] [ Hidden ]` como primera de Companion. Cambia la
visibilidad real del pet en el acto (`SDL_HideWindow`/`SDL_ShowWindow`);
el Product UI sigue visible con el pet oculto; no cambia pet activo,
propiedad ni balance. **No se persiste** (contrato preexistente de
Block 06 §10: esconder ≠ salir, el pet arranca visible en cada
lanzamiento — sin campo nuevo en AppState, sin bump de schema). Hay UN
solo punto de mutación, `SpikeApp::ApplyPetVisibility(bool hidden)`,
análogo a los `Apply*` de DEC-130: tanto el toggle Show/Hide del menú
rápido como la fila de Settings entran por ahí, y ese punto re-empuja el
estado a Settings (`PushCompanionStateToProductWindow`) — las dos
superficies derivan del **mismo** bool (`petHidden_`), así que no pueden
divergir. `SettingsView` emite un `productui::SettingsCommand`
(Shown/Hidden/Reset) — un canal aparte de `SettingsChange`, hermano de
`GlobalClickAction` — porque NO es una preferencia persistida y no debe
tocar `core::Preferences` / `core::PreferenceField`.

**3. "Reset position": colocación SEGURA canónica, sin coordenadas
inventadas.** Fila `Position [ Reset position ]` en Companion, una ACCIÓN
(no un toggle persistido). Semántica de `SpikeApp::ResetPetPositionToSafeDefault()`:
1. determina el display que **contiene la ventana del Product UI**
   (`SDL_GetWindowFromID(productWindow_.WindowId())` → `SDL_GetDisplayForWindow`
   → `SDL_GetDisplayBounds`);
2. calcula el destino con la pieza **pura** `core::SafePetPlacement(display,
   petW, petH)` — **centrado** (idéntico a `SDL_WINDOWPOS_CENTERED`, el
   default de arranque cuando no hay posición guardada), **acotado** para
   que el rectángulo entero quede dentro del display, y anclando el borde
   sup-izq si el pet es más grande que el display. `petW/petH` =
   `EffectiveCanvasWidth()/Height()` (respeta la escala de tamaño). No
   hay ninguna constante de escritorio hard-codeada;
3. `SDL_SetWindowPosition(window_, …)` sobre la ventana del pet
   (chequeando el valor de retorno real, igual que la restauración de
   `Init()`);
4. persiste por la **MISMA** ruta que el fin de un drag
   (`appState_.lastWindowPosition` + `persistenceScheduler_.MarkDirty`),
   y re-resuelve la dirección una vez (`UpdateDirectionFromWindowPosition`).
La acción es **quieta**: sin diálogo, sin toast, sin animación, sin
mostrar coordenadas ni IDs de monitor, sin permisos nuevos. **Lock
Position NO la bloquea** (`core::PetDragAllowed` solo gatea el *inicio de
un drag*; un reset explícito del owner es otra cosa), y funciona con el
pet **oculto** (la ventana existe).

**4. Wayland: honestidad, no un hack.** `xdg-shell` no permite que un
cliente pida una posición absoluta para una `xdg_toplevel` (ver
`docs/LINUX_PLATFORM.md` §3.3/§6). Se añade `platform::AbsoluteWindowPositioningSupported()`
al seam compartido `TransparentWindowSupport.h` — macOS/Windows `true`,
Linux delega en la tabla pura ya unit-testeada
`LinuxBackendSupportsPositionRestore()` (X11 `true`, Wayland `false`). En
un backend sin capacidad, Settings **dibuja "Reset position" apagado**
(fuera del hit-test y del anillo de foco — el mismo patrón que el
segmento "Anywhere" sin capacidad de Block 11A) con una línea corta y sin
alarma ("Position can't be reset on this system."), y
`ResetPetPositionToSafeDefault()` es un no-op con log si igual se lo
invoca. Todo lo demás de Settings sigue funcional. Las limitaciones
documentadas de Wayland **no cambian**.

**5. `Open Nimvlets…` — corrección de verdad de producto.** El ítem del
menú rápido decía `Collection…`, pero la ventana ya contiene
`Collection · Shop · Settings` desde Blocks 07/08, y Block 11A (DEC-140)
fijó que invocarlo con la ventana ya abierta restaura **LA MISMA**
ventana y su **sección actual** — no "Collection". El string visible pasa
a **"Open Nimvlets…" / "Abrir Nimvlets…"**. El refactor interno era
acotado y mecánico, así que también se renombró `ShellAction::kOpenCollection
→ kOpenProductUi` y `StringKey::kCollectionMenuItem → kOpenNimvletsMenuItem`
(~7 sitios de código + tests). La **semántica** de abrir/enfocar/restaurar
NO cambia: sigue exactamente el contrato de DEC-140 (cerrada → crear en
Collection; visible → traer al frente; minimizada → restaurar la misma
ventana y sección; nunca una segunda ventana, nunca un reset de estado).

**Verificación.** macOS (Release, arm64): capturas de framebuffer del
grupo Companion en EN y ES (Visibility, Position, y "Reset position" con
foco de teclado), y el smoke DEV `NIMVLETS_DEV_RESET_POSITION=1` mueve la
ventana del pet al destino seguro de su display y marca
`lastWindowPosition` dirty por la ruta de siempre. Los tres smokes en
vivo de Block 11A (`NIMVLETS_DEV_WALLET_LIVE_SMOKE`,
`NIMVLETS_DEV_RESTORE_SMOKE`, `NIMVLETS_DEV_UI_NAV_SMOKE`) siguen PASS.
**Windows/Linux: NOT TESTED** — solo build/CI, como el resto del Product
UI (ver `docs/PLATFORM_SPIKE.md` §12). El owner puede verificar la fila
Visibility, "Reset position" (incluido estando Lock ON y el pet oculto) y
la etiqueta `Open Nimvlets…` en vivo en macOS.

**Congelado sin tocar:** Global Click de Block 11A entero (AppState v6,
modo pedido vs. efectivo, Input Monitoring, no doble conteo, sin prompt
al arrancar); la restauración de la ventana minimizada y el wallet
canónico en vivo (DEC-138/DEC-140); Collection owned-only (DEC-136); Shop
browse-first (DEC-135); el Starter Shop oculto (DEC-137/DEC-138);
onboarding (DEC-131..DEC-134); la política de compra; el catálogo de
producción; el contenido de Nidir/Frin.
