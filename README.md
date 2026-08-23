# Nimvlets

Lightweight, native, cross-platform desktop companion. A small
transparent window shows one creature on your desktop; drag it around,
click it to earn clicks (the only currency), spend clicks to unlock more
creatures permanently.

Este repositorio está en **Block 05 — Behavior Runtime + Frin Vertical
Slice + Baseline Cleanup**, construido sobre Block 04.3 (corrección
post-QA de Nidir/Bunny), Block 04.2 (assets reales + pipeline
direccional), Block 04.1 (Linux como plataforma de escritorio, ver
[`docs/LINUX_PLATFORM.md`](docs/LINUX_PLATFORM.md)), Block 04
(catálogo de pets + switching en runtime, ver
[`docs/CATALOG.md`](docs/CATALOG.md)), Block 03 (persistencia local,
ver [`docs/PERSISTENCE.md`](docs/PERSISTENCE.md)), Block 02 (content +
animation foundation, ver
[`docs/ANIMATION_RUNTIME.md`](docs/ANIMATION_RUNTIME.md)), y Block 01
(foundation + platform feasibility spike). Block 05 generaliza el
runtime de contenido de un modelo fijo idle/click/passive a un **grafo
de comportamiento por-estado** (`content::BehaviorState`), agrega el
segundo Nimvlet con estados reales — **Frin** (macho/hembra, lobo
blanco/crema, un único Nimvlet lógico con dos variantes visuales —
transición sentado/acostado, ver
[`docs/FRIN_CONTENT.md`](docs/FRIN_CONTENT.md)) — junto con Bunny y
Nidir, corrige el comportamiento real de hover (ahora exige dwell
continuo de 0.5s sobre pixeles visibles, desacoplado del timer
ambient), fija el intervalo ambient de Bunny/Nidir en 12s y el
rest-delay de Frin sentado en 10s, y agrega una escala visual por-pet
genérica y data-driven (`content::PetDefinition::visualScale`). Esto explícitamente *no* es
todavía el producto terminado — ver
[`docs/PLATFORM_SPIKE.md`](docs/PLATFORM_SPIKE.md) para lo que está
verificado y lo que no, y `AGENTS.md` para los contratos de
ingeniería permanentes.

## Requirements

- CMake ≥ 3.25
- A C++20 compiler (Apple Clang / Clang / MSVC / GCC — see below)
- macOS: Xcode Command Line Tools (`xcode-select --install`)
- Windows: Visual Studio 2022 with the "Desktop development with C++"
  workload
- Linux: Ninja (`ninja-build`) + the X11/Wayland development packages
  listed in [`docs/LINUX_PLATFORM.md`](docs/LINUX_PLATFORM.md) §2
- Python 3 (for `tools/stats_loc.py` and the asset pipeline — no
  packages needed)
- No manual SDL3 install required — see below.

## Build

SDL3 (pinned to `release-3.4.12`) is fetched automatically via CMake
`FetchContent` on first configure; there's nothing to install by hand
beyond each platform's own compiler/toolchain (and, on Linux, the
X11/Wayland dev packages above).

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

# Linux x86_64 (X11 and Wayland, both detected at runtime — see
# docs/LINUX_PLATFORM.md)
cmake --preset linux-debug        # or linux-release
cmake --build --preset linux-debug
```

Build directories live under `build/<preset-name>/`, never inside the
source tree.

## Run the foundation spike

```bash
# run from the repo root — the catalog below is loaded via a
# relative path that resolves from the current working directory
./build/macos-debug/src/app/nimvlets_spike

# owner manual QA: launch a specific catalog entry without touching your
# real persisted pet selection (Block 05) -- "petId" or "petId/variantId"
NIMVLETS_DEV_SELECT_PET=bunny ./build/macos-debug/src/app/nimvlets_spike
NIMVLETS_DEV_SELECT_PET=nidir ./build/macos-debug/src/app/nimvlets_spike
NIMVLETS_DEV_SELECT_PET=frin/male ./build/macos-debug/src/app/nimvlets_spike
NIMVLETS_DEV_SELECT_PET=frin/female ./build/macos-debug/src/app/nimvlets_spike

# QA convenience: ambient action every ~5s instead of the real per-state
# default (12s for Bunny/Nidir, 10s for Frin's seated rest delay --
# production behavior is unchanged — see docs/ANIMATION_RUNTIME.md)
NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS=5 ./build/macos-debug/src/app/nimvlets_spike

# conveniencia de QA: persiste a un directorio aislado en vez de la
# ubicación real de app-data por usuario (ver docs/PERSISTENCE.md §2)
NIMVLETS_DEV_APPDATA_DIR=/tmp/nimvlets_dev_state ./build/macos-debug/src/app/nimvlets_spike

# conveniencia de QA: ejecuta N switches de pet automáticos y no
# interactivos justo después de arrancar, para smoke-testear el
# switching (no es comportamiento de producto — ver docs/CATALOG.md §7)
NIMVLETS_DEV_SWITCH_TEST_COUNT=5 ./build/macos-debug/src/app/nimvlets_spike

# conveniencia de QA: alterna N veces entre Direction::kRight/kLeft
# justo después de arrancar. En producción, la dirección ya se resuelve
# sola: mitad derecha de la pantalla -> right, mitad izquierda -> left
# -- esta variable sigue sirviendo para forzar alternancias rápidas sin
# mover la ventana de verdad.
NIMVLETS_DEV_DIRECTION_TEST_COUNT=5 ./build/macos-debug/src/app/nimvlets_spike
```

Combiná `NIMVLETS_DEV_SELECT_PET` con `NIMVLETS_DEV_APPDATA_DIR` para
probar cualquier pet sin arriesgar tu estado persistido real — ver
"Owner manual QA" más abajo para la lista completa de comandos.

This opens a small, borderless, always-on-top, transparent window
showing whichever pet the catalog resolves as active (see
[`docs/CATALOG.md`](docs/CATALOG.md)) — by default **Bunny** (id
`bunny`, renamed from `bunny_dev` in Block 05 — see
`docs/DECISION_LOG.md`; 134×176 logical canvas, `visualScale=1.0`,
real production art since Block 04.3 — see
[`docs/BUNNY_CONTENT.md`](docs/BUNNY_CONTENT.md)). **Nidir** (176×173
native canvas × `visualScale=1.10` = 194×190 on screen, Block 05 — see
[`docs/NIDIR_CONTENT.md`](docs/NIDIR_CONTENT.md)) and **Frin**
male/female (125×176 / 138×176, `visualScale=1.0`, the first Nimvlet
with real seated/lying state transitions — see
[`docs/FRIN_CONTENT.md`](docs/FRIN_CONTENT.md)) round out the catalog.
All four are reachable via `NIMVLETS_DEV_SELECT_PET`/
`NIMVLETS_DEV_SWITCH_TEST_COUNT` above (no UI selector yet). The
catalog and the active pet's pack are required, not optional — if
either can't be loaded (e.g. not run from the repo root), the app logs
a specific error and exits rather than falling back to any
placeholder; if a *previously saved* pet selection can't be resolved
or loaded (e.g. an old `bunny_dev` save from before the Block 05
rename), it falls back to the catalog's default and repairs the saved
selection instead of crashing. Haz click en la región visible para
incrementar el click balance (persistido localmente — ver
[`docs/PERSISTENCE.md`](docs/PERSISTENCE.md)) y reproducir una
reacción de click corta; cada ~12s (Bunny y Nidir) también reproduce
una acción ambient por su cuenta, y mantener el cursor quieto sobre el
pet (sin click) durante 0.5s continuos dispara la misma acción, con su
propio dwell desacoplado del timer ambient. Frin tiene su propio ritmo:
tras ~10s de reposo genuino sentado se acuesta (`sit_to_lie` ->
`lying`), y acostado no tiene timer ambient — un click lo levanta.
Arrástrala para mover la ventana (la nueva posición también se
persiste); hacer click en el área transparente pasa a lo que esté
debajo. Cerrar y reabrir la ventana la reabre donde la dejaste, con tu
click balance intacto.

On Linux/X11 (always) and on macOS with an accelerated renderer driver
(Metal/OpenGL/GPU), click-through hit-testing is handed to SDL's own
`SDL_SetWindowShape()` mechanism (event-driven, no polling) — see
`docs/PLATFORM_SPIKE.md` §5.1 (macOS) and `docs/LINUX_PLATFORM.md` §3.2
(X11's XShape extension) for why.

macOS defaults to SDL's *software* renderer instead (see
`docs/DECISION_LOG.md` DEC-083), where installing a window shape makes
SDL's renderer paint the shape bitmap over our content — so there
Nimvlets drives per-pixel click-through itself, and owns the native
state outright so SDL's Cocoa backend cannot overwrite it on every
mouse-moved event. Both root causes are measured, not inferred; see
`docs/PLATFORM_SPIKE.md` §11 and DEC-086. The cursor is sampled **only
while it is inside the window's rectangle** — with the cursor anywhere
else on screen the app does not wake up for click-through at all. No
new OS permission is involved (no Accessibility, no Input Monitoring,
no global input hook).

Windows uses the same Nimvlets-driven mechanism on its own terms, where
the native shape path isn't verified safe regardless of renderer.
Linux/Wayland has neither mechanism available with the pinned SDL3
(see `docs/LINUX_PLATFORM.md` §6): a click on a transparent pixel there
is safely ignored by Nimvlets, but — unlike the other cases above — it
does not reach whatever's underneath; this is a real Wayland protocol
limitation, not a bug.

The window is intentionally borderless and non-focusable (that's the
product requirement, not a bug), so it has no close button. Quit it from
the terminal you launched it from with **Ctrl+C**, or from elsewhere
with:

```bash
pkill -TERM -f nimvlets_spike
```

## Owner manual QA

```bash
# Bunny (default)
./build/macos-debug/src/app/nimvlets_spike

# Nidir
NIMVLETS_DEV_SELECT_PET=nidir ./build/macos-debug/src/app/nimvlets_spike

# Frin (macho)
NIMVLETS_DEV_SELECT_PET=frin/male ./build/macos-debug/src/app/nimvlets_spike

# Frin (hembra)
NIMVLETS_DEV_SELECT_PET=frin/female ./build/macos-debug/src/app/nimvlets_spike
```

Cada uno de estos, sin `NIMVLETS_DEV_APPDATA_DIR`, sigue leyendo/
escribiendo tu estado persistido REAL (`activePetId` se actualiza al
switchear vía UI/click normalmente, nunca por esta variable sola — ver
el comentario de `kDevSelectPetEnvVar` en `src/app/SpikeApp.cpp`) — si
preferís no tocarlo en absoluto durante la QA, agregá
`NIMVLETS_DEV_APPDATA_DIR=/tmp/nimvlets_qa` a cualquiera de los
comandos de arriba.

## Test

```bash
ctest --preset macos-debug --output-on-failure
```

Tests are pure `src/core`/`src/content`/`src/catalog`/`src/persistence`
logic — no SDL, no display required, so they run the same in CI as on
a dev machine.

```bash
python3 tools/test_asset_pipeline.py
```

Pure-Python `unittest` coverage for the asset pipeline (mirroring,
frame-sequence validation, content-anchored canvas normalization,
area-average downscale) — run by hand, not via CTest (no C++
dependency).

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
src/content      pure, data-driven behavior-graph model + animation controller + pack loader (no SDL)
src/catalog      pure pet identity + catalog + active-selection/switching logic (no SDL)
src/persistence  pure local state model + serializer + atomic-write store + debounce scheduler (no SDL)
src/graphics     SDL rendering: turns a content::FrameDefinition into a texture
src/platform     native macOS (AppKit) / Windows (Win32) / Linux (X11+Wayland) window glue,
                 plus LinuxBackendPolicy — pure X11-vs-Wayland capability logic, built on every OS
src/app          the spike executable: event loop + wiring
tests/           CTest-driven unit tests for src/core, src/content, src/catalog, and src/persistence
tools/           dev tooling (stats_loc.py, prep_dev_sprite.py, compile_pet_pack.py, compile_pet_catalog.py,
                 generate_bunny_pack.py, generate_nidir_pack.py, generate_frin_pack.py,
                 validate_frame_sequence.py, test_asset_pipeline.py)
assets/dev/      compiled runtime packs (*.nvpack) + compiled catalog (*.nvcat) -- see assets/dev/README.md
assets/source/nimvlets/  real Nimvlet source art (frames, spritesheets, DESCRIPTION.txt — see
                 assets/source/nimvlets/README.md and docs/NIDIR_CONTENT.md/BUNNY_CONTENT.md/FRIN_CONTENT.md)
cmake/           CMake helper modules (warnings, SDL3 fetch)
docs/            product + engineering contracts (see AGENTS.md §18)
```

See [`AGENTS.md`](AGENTS.md) for the full engineering contract
(architecture, privacy/security rules, dependency rules, Git workflow).
