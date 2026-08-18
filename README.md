# Nimvlets

Lightweight, native, cross-platform desktop companion. A small
transparent window shows one creature on your desktop; drag it around,
click it to earn clicks (the only currency), spend clicks to unlock more
creatures permanently.

Este repositorio está actualmente en **Block 04.1 — Habilitación de
Linux como Plataforma de Escritorio**, construido sobre Block 04 —
Catálogo de Pets + Switching en Runtime (ver
[`docs/CATALOG.md`](docs/CATALOG.md)), Block 03 — Persistencia Local
de Estado (el click balance, el id del pet activo, y la última
posición de ventana sobreviven a un reinicio — ver
[`docs/PERSISTENCE.md`](docs/PERSISTENCE.md)), Block 02 — Content +
Animation Foundation (un pequeño runtime de contenido+animación,
data-driven — ver
[`docs/ANIMATION_RUNTIME.md`](docs/ANIMATION_RUNTIME.md)), y Block 01 —
Foundation + Platform Feasibility Spike (bootstrap disciplinado del
repo más una prueba de que el enfoque central de
windowing/transparencia/hit-testing/drag es viable). Block 04.1 suma
**Linux x86_64 (X11 y Wayland)** como target de escritorio de primera
clase, sin cambiar ningún feature de producto — ver
[`docs/LINUX_PLATFORM.md`](docs/LINUX_PLATFORM.md) para el diseño
completo, incluyendo las limitaciones reales de Wayland (sin
always-on-top, sin restauración de posición absoluta) que el protocolo
en sí impone. Esto explícitamente *no* es todavía el producto
terminado — ver [`docs/PLATFORM_SPIKE.md`](docs/PLATFORM_SPIKE.md)
para lo que está verificado y lo que no, y `AGENTS.md` para los
contratos de ingeniería permanentes que sigue este bloque y cada
bloque futuro.

## Requirements

- CMake ≥ 3.25
- A C++20 compiler (Apple Clang / Clang / MSVC / GCC — see below)
- macOS: Xcode Command Line Tools (`xcode-select --install`)
- Windows: Visual Studio 2022 with the "Desktop development with C++"
  workload
- Linux: Ninja (`ninja-build`) + the X11/Wayland development packages
  listed in [`docs/LINUX_PLATFORM.md`](docs/LINUX_PLATFORM.md) §2
- Python 3 (for `tools/stats_loc.py` only — no packages needed)
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

# QA convenience: passive action every ~5s instead of the real ~300s
# default (production behavior is unchanged — see docs/ANIMATION_RUNTIME.md §8)
NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS=5 ./build/macos-debug/src/app/nimvlets_spike

# conveniencia de QA: persiste a un directorio aislado en vez de la
# ubicación real de app-data por usuario (ver docs/PERSISTENCE.md §2)
NIMVLETS_DEV_APPDATA_DIR=/tmp/nimvlets_dev_state ./build/macos-debug/src/app/nimvlets_spike

# conveniencia de QA: ejecuta N switches de pet automáticos y no
# interactivos justo después de arrancar, para smoke-testear el
# switching (no es comportamiento de producto — ver docs/CATALOG.md §7)
NIMVLETS_DEV_SWITCH_TEST_COUNT=5 ./build/macos-debug/src/app/nimvlets_spike
```

This opens a small (160×160), borderless, always-on-top, transparent
window showing whichever pet the catalog resolves as active (see
[`docs/CATALOG.md`](docs/CATALOG.md)) — for now, always **Bunny**, a
deterministically-generated DEV animation pack
(`assets/dev/bunny_pack.nvpack`) derived from a real illustrated QA
fixture the repository owner supplied in Block 01, *not* final content
(see [`docs/PET_CONTENT_SPEC.md`](docs/PET_CONTENT_SPEC.md) and
[`docs/ANIMATION_RUNTIME.md`](docs/ANIMATION_RUNTIME.md)). The catalog
and the active pet's pack are required, not optional — if either can't
be loaded (e.g. not run from the repo root), the app logs a specific
error and exits rather than falling back to any placeholder; if a
*previously saved* pet selection can't be resolved or loaded, it falls
back to the catalog's default instead of crashing. Haz click en la
región visible para incrementar el click balance (persistido localmente — ver
[`docs/PERSISTENCE.md`](docs/PERSISTENCE.md)) y reproducir una reacción
de click corta; cada ~300s (o el override DEV de arriba) también
reproduce una acción pasiva corta por su cuenta. Arrástrala para mover
la ventana (la nueva posición también se persiste); hacer click en el
área transparente pasa a lo que esté debajo. Cerrar y reabrir la
ventana la reabre donde la dejaste, con tu click balance intacto.

On macOS and Linux/X11, click-through hit-testing is handed to SDL's
own `SDL_SetWindowShape()` mechanism (event-driven, no polling) — see
`docs/PLATFORM_SPIKE.md` §5.1 (macOS) and `docs/LINUX_PLATFORM.md` §3.2
(X11's XShape extension) for why. A poll-driven fallback exists for
Windows, where that mechanism isn't verified safe yet. Linux/Wayland
has neither mechanism available with the pinned SDL3 (see
`docs/LINUX_PLATFORM.md` §6): a click on a transparent pixel there is
safely ignored by Nimvlets, but — unlike macOS/Windows/Linux-X11 — it
does not reach whatever's underneath; this is a real Wayland protocol
limitation, not a bug.

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
src/catalog      pure pet identity + catalog + active-selection/switching logic (no SDL)
src/persistence  pure local state model + serializer + atomic-write store + debounce scheduler (no SDL)
src/graphics     SDL rendering: turns a content::FrameDefinition into a texture
src/platform     native macOS (AppKit) / Windows (Win32) / Linux (X11+Wayland) window glue,
                 plus LinuxBackendPolicy — pure X11-vs-Wayland capability logic, built on every OS
src/app          the spike executable: event loop + wiring
tests/           CTest-driven unit tests for src/core, src/content, src/catalog, and src/persistence
tools/           dev tooling (stats_loc.py, prep_dev_sprite.py, compile_pet_pack.py, compile_pet_catalog.py, generate_bunny_dev_pack.py)
assets/dev/      dev-only placeholder assets (see assets/dev/README.md)
cmake/           CMake helper modules (warnings, SDL3 fetch)
docs/            product + engineering contracts (see AGENTS.md §18)
```

See [`AGENTS.md`](AGENTS.md) for the full engineering contract
(architecture, privacy/security rules, dependency rules, Git workflow).
