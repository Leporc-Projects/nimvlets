# Nimvlets — Platform Spike (Block 01)

This document is the QA **plan** (§2, written against the block brief
before running anything), the **results** (§3 onward), and — because
this block went through two rounds of interactive macOS QA with the
repository owner — the record of a real bug hunt (§5) that changed the
shipped click-through mechanism partway through. Nothing below is
upgraded to PASS without real evidence; where a claim rests on an
earlier, since-superseded belief, that's said explicitly rather than
quietly overwritten.

**Scope note (Block 02):** this document is Block 01's QA record and is
left as-is below — it remains an accurate account of what was tested
and found *then*. The underlying window/transparency/click-through
mechanism it describes (§5) is unchanged in Block 02. One number is
superseded: row 13's "render tick running the whole time" no longer
describes the running app — Block 02 replaced the fixed render tick
with a deadline-driven scheduler that lets static idle stop rendering
entirely; see `docs/ANIMATION_RUNTIME.md` §6 and
`docs/PERFORMANCE_BUDGETS.md` for the current numbers.

**Scope note (Block 04.1):** this document remains macOS/Windows-only
below — Linux's own QA plan, results (source-inspection-based, since no
Linux machine was available this block), and open gaps live entirely in
the new `docs/LINUX_PLATFORM.md` instead of being folded in here. Ítem
por ítem, Linux/X11 alcanza el mismo nivel que la fila de macOS de
abajo (mismo mecanismo `SDL_SetWindowShape`, confirmado por lectura de
la fuente pineada — ver `docs/LINUX_PLATFORM.md` §3.2/§5); Linux/Wayland
tiene limitaciones reales del protocolo (`xdg-shell`) documentadas ahí
mismo, no acá.

**Nota de alcance (Block 05, pasada de estabilización):** el mecanismo
de click-through de macOS que §5.1 describe (`SDL_SetWindowShape`,
event-driven, sin polling) **ya no es el que se envía**. La causa no es
que §5.1 estuviera mal — sigue siendo cierto para el driver acelerado
— sino que DEC-083 cambió el driver por default de macOS a "software"
por razones de corrección VISUAL, y ahí la ruta de shape corrompe el
render. El mecanismo actual, su causa raíz medida y las alternativas
descartadas están en §11, más abajo; §5.1 y §5.5 se conservan
íntegras como el registro de cómo se llegó hasta acá.

## 1. What was evaluated

`SDL_SetWindowShape()` was evaluated first, as the block brief
requires. The initial verdict (based on community reports, not the SDL
source itself) was to reject it — see §5.1. Interactive QA later
disproved the fallback that decision led to, and re-reading the actual
pinned SDL 3.4.12 source reversed the verdict for macOS specifically:
**`SDL_SetWindowShape()` is what this block ships.** §5 is the full
story; `docs/DECISION_LOG.md` DEC-006 (superseded) and DEC-017
(current) carry the same arc as dated decision records.

## 2. QA plan (macOS)

For each item: build Release, run `nimvlets_spike`, and check —

1. transparent window renders
2. borderless (no title bar/decoration)
3. small bounding box
4. real per-pixel alpha (not just a translucent rectangle)
5. click-through on fully transparent pixels
6. click detected on the visible (opaque) region
7. drag moves the window
8. a drag is never miscounted as a click, and vice versa
9. no more focus-stealing than the platform makes unavoidable
10. always-on-top over a normal window
11. high-DPI backing on a Retina display
12. clean shutdown, no hung/zombie process
13. render/event loop does not busy-wait
14. no invasive permission is requested anywhere in the process

## 3. Results — macOS (final)

Machine: Apple M5, macOS 26.6.1 (Darwin 25.6.0), Retina display (2x).
Build: Release, `cmake --preset macos-release`. Verified with **two**
visuals — the analytic placeholder (`core::BlobSilhouette`) and the
**Bunny QA fixture** (a real, non-analytic asset with its own alpha
channel — see §6) — both driven through the exact same code path.

| # | Item | Verdict | Evidence |
|---|---|---|---|
| 1 | Transparent window | **PASS** | Pixel-inspected a real captured frame (`screencapture -l<windowID>`, Screen Recording permission granted mid-block — see §5.4): all four corners read exact `(0,0,0,0)` RGBA. |
| 2 | Borderless | **PASS** | The captured window image is accounted for entirely by the shape + transparent background pixels; no title-bar chrome pixels anywhere in the 320×320 capture. |
| 3 | Small bounding box | **PASS** | `CGWindowListCopyWindowInfo`: `kCGWindowBounds = {Width=160, Height=160, ...}` (independent WindowServer read) *and* the captured frame is exactly 320×320 physical pixels (160×160 logical × 2x). |
| 4 | Real alpha | **PASS** | Pixel-inspected: background exactly alpha=0, interior alpha≈253–255 (Bunny) / 255 (placeholder), sharp antialiased transition at the silhouette edge — not a flat translucent rectangle. |
| 5 | Click-through on transparent pixels | **PASS** | Root-caused and fixed after being reported FAIL twice by the repository owner — see §5. Final manual confirmation (owner, with Bunny): clicking a transparent point near Bunny reaches the application underneath; Nimvlets' own log shows no click/drag registered for it (correct silence). |
| 6 | Click on visible region | **PASS** | Owner-confirmed with Bunny: `click #1` logged for a deliberate click on the visible sprite. Also exercised dozens of times across both QA rounds (`click #1`…`#13` in the final session's log) with zero misclassifications. |
| 7 | Drag moves the window | **PASS** | Owner-confirmed with Bunny: dragging visibly moved the window; log shows `drag ended (correctly not counted as a click)`. |
| 8 | Click vs. drag never confused | **PASS** | `core::DragClassifier` unit tests (8/8) plus dozens of real interactive presses/drags across both QA rounds, zero misclassifications observed in any log. |
| 9 | Focus behavior | **PASS** | Real bug found and fixed (see §5.3): launching the spike used to make it the frontmost/active app. Objectively confirmed fixed — frontmost app was `Claude` both immediately before *and* immediately after launching the spike. |
| 10 | Always-on-top | **PASS** | `kCGWindowLayer = 3` (`NSFloatingWindowLevel`), strictly above ordinary app windows at layer 0 — read directly from the WindowServer. |
| 11 | High-DPI backing | **PASS** | Real bug found and fixed (see §5.2): the placeholder used to render at half size in the window's top-left quadrant on this 2x display. Fixed and pixel-confirmed: `SDL_GetWindowSize`→160×160, `SDL_GetWindowSizeInPixels`→320×320, and a captured frame's opaque-pixel bounding box now matches the full window, not a quarter of it. |
| 12 | Clean shutdown | **PASS** | `SIGINT`/`SIGTERM` → `Shutdown()` exercised many times across this block (including at the very end of both QA rounds): log always shows `clean shutdown, N click(s) recorded`, process leaves the process table, zero crash reports in `~/Library/Logs/DiagnosticReports`. |
| 13 | No busy-wait | **PASS** | Idle-animated Release run: ≈1.2–2% average CPU over 27s sampling windows (placeholder vs. Bunny — see §7) with the render tick running the whole time. A busy-wait loop would show ≈100% of one core. |
| 14 | No invasive permission requested | **PASS** | Grep-verified: no Accessibility, Input Monitoring, admin/root, or global-hook API anywhere in `src/`. Screen Recording was granted **to the agent's automation shell**, on the owner's explicit instruction, purely to inspect the spike's own window pixels for this QA — not requested by `nimvlets_spike` itself, which still requests nothing beyond `SDL_INIT_VIDEO`. See §5.4 and `docs/PRIVACY_SECURITY.md`. |

## 4. How macOS QA actually happened, in two rounds

Block 01 was first closed with several macOS items marked PARTIAL/NOT
TESTED, on the reasoning that the agent session had no way to capture
screen pixels or synthesize input without either an unavailable
permission or "invasive automation" the block brief warns against. The
repository owner then asked for that gap to be closed for real:

1. **Screen Recording was requested from and granted by the owner**
   (not assumed, not routed around) specifically so the agent could
   inspect the *spike's own window* — never the owner's broader screen
   — via `screencapture -l<windowID>` (captures one window's pixel
   buffer directly; never a full-screen or arbitrary-region capture,
   which would risk picking up unrelated content — an actual near-miss
   on that exact risk is recorded in §5.4).
2. **Interactive clicks/drags were performed by the owner**, not
   synthesized, on the agent's explicit numbered instructions each
   round — never `CGEventPost` or accessibility-driven automation.

That combination — real screen pixels plus a real human's mouse —
is what turned §3 from PARTIAL/NOT TESTED into PASS, and is also what
surfaced the three real bugs in §5.

## 5. Bugs found during interactive QA (all fixed, all owner-confirmed)

### 5.1 Click-through: root cause and fix

**Symptom (reported by the owner, twice):** clicking a transparent
point near the placeholder shape did not reach the application
underneath — the click seemed to vanish (Nimvlets' own log showed
nothing for it either, ruling out "our window silently ate it and did
something with it").

**First fix attempt (insufficient):** the original mechanism —
`NSWindow.ignoresMouseEvents` toggled from a poll of
`SDL_GetGlobalMouseState()` tied to the 12fps render tick — was
suspected of simple lag (a click landing before the next poll caught
up), so the poll was moved to its own ~60Hz schedule
(`hoverScheduler_`), independent of rendering. The owner re-tested:
**still failed**, and asked for the pipeline to be instrumented rather
than guessed at further, and for polling frequency to not be raised
again until a real cause was demonstrated.

**Instrumentation (see §5.5) and root cause:** six pipeline stages were
logged on transition only (global cursor → window position → local
coordinate → hit-test result → requested click-through state →
*actual* `NSWindow.ignoresMouseEvents` read back immediately after
setting it). Across two owner-supervised test rounds, the instrumented
pipeline never showed a single mismatch between "requested" and
"actual" — the toggle itself always worked exactly as commanded. The
real cause was found by reading the pinned **SDL 3.4.12 Cocoa backend
source directly** (not third-party reports):
`src/video/cocoa/SDL_cocoawindow.m`'s `-mouseMoved:` handler calls
`updateIgnoreMouseState:` on *every real mouse-moved NSEvent* whenever
the window has `SDL_WINDOW_TRANSPARENT`, and that function
**unconditionally resets `ignoresMouseEvents` to `NO`** unless an
`SDL_SetWindowShape()` surface is set:

```objc
// src/video/cocoa/SDL_cocoawindow.m
- (void)updateIgnoreMouseState:(NSEvent *)theEvent
{
    SDL_Surface *shape = ...SDL_PROP_WINDOW_SHAPE_POINTER...;
    BOOL ignoresMouseEvents = NO;
    if (shape) { /* ... compute from shape alpha ... */ }
    _data.nswindow.ignoresMouseEvents = ignoresMouseEvents;   // always runs
}
```

Since this block was manually managing `ignoresMouseEvents` *without*
ever calling `SDL_SetWindowShape`, SDL itself was silently resetting
our own assignment back to `NO` on essentially every mouse movement —
including the movement immediately preceding a click. By the time a
click landed, the window was usually back to accepting events, so it
consumed the click at the OS level (our own app then correctly
declined to count it as a valid gesture, since it wasn't on the visible
region — hence the observed silence on both sides).

**Fix:** re-read `SDL_SetWindowShape`'s actual macOS implementation
(`src/video/cocoa/SDL_cocoashape.m`) instead of relying on the earlier
community-report-based rejection (§5.1's own predecessor, §1). On
macOS specifically, that implementation **only ever touches
`ignoresMouseEvents`** — it does not composite, clip, or otherwise
touch rendered pixels, contrary to what the earlier evaluation assumed
(that assumption held for the reported use case, which was about
Windows' `UpdateLayeredWindow`-based behavior — see
`docs/DECISION_LOG.md` DEC-017 for the full comparison). Once a shape
is set, SDL's own `updateIgnoreMouseState:` computes `ignoresMouseEvents`
*from that shape's alpha at the current cursor position* on every real
mouse-moved event instead of resetting to `NO` — a correct,
event-driven mechanism requiring zero polling from the app.
`src/app/SpikeApp.cpp` now builds an `SDL_Surface` from a
`core::AlphaMask` (rasterized from whichever visual is active — Bunny's
real alpha channel or the placeholder's analytic shape) and calls
`SDL_SetWindowShape()` once at startup;
`platform::NativeShapeHitTestIsRenderSafe()` gates this per platform
(`true` on macOS, `false`/unverified on Windows — see §8). The
poll-driven mechanism (`PollHover()`/`UpdateClickThrough()`) is kept
as the Windows fallback, unchanged in its own logic, just no longer
exercised on macOS.

**Result:** owner-confirmed PASS in the final QA round — see §3, item 5.

### 5.2 High-DPI render scale bug

**Symptom (found by the agent, pixel-inspecting a captured frame, not
reported by the owner):** on this 2x Retina display, the placeholder
shape rendered at half its intended size, confined to the window's
top-left quadrant, instead of filling the window.

**Root cause:** `SDL_SetRenderLogicalPresentation()` was never called.
Its default, `SDL_LOGICAL_PRESENTATION_DISABLED`, maps render
coordinates 1:1 to physical backbuffer pixels — so
`core::BlobSilhouette`'s deliberately-DPI-independent logical 160×160
coordinates only covered a 160×160 physical corner of the real 320×320
backbuffer.

**Fix:** `SDL_SetRenderLogicalPresentation(renderer, 160, 160,
SDL_LOGICAL_PRESENTATION_LETTERBOX)` right after creating the renderer.
Pixel-confirmed fixed both for the placeholder and, later, for Bunny
(§6): the opaque-pixel bounding box of a captured frame now spans the
full window.

### 5.3 App-activation focus steal

**Symptom (found by the agent, via objective frontmost-app
inspection, not visually reported by the owner):** launching the spike
made it the frontmost/active application, even though
`SDL_WINDOW_NOT_FOCUSABLE` already correctly prevented the *window*
from becoming key.

**Root cause:** SDL's Cocoa backend calls `[NSApp
activateIgnoringOtherApps:YES]` during startup by default —
`SDL_WINDOW_NOT_FOCUSABLE` only affects window-level key status, not
app-level activation.

**Fix:** `SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP, "1")` before
`SDL_Init()` — an official SDL hint documented for exactly this
("A variable controlling whether to force the application to become
the foreground process when launched on macOS"). Confirmed fixed:
frontmost app was `Claude` (the owner's active app) both immediately
before and immediately after launching the spike, measured via
`NSWorkspace.frontmostApplication`.

### 5.4 A near-miss on capturing unrelated content

While diagnosing §5.1, an early attempt used `screencapture -R<region>`
(a screen-region capture at the coordinates `CGWindowListCopyWindowInfo`
reported for the spike window) instead of a window-scoped capture. The
result showed unrelated content — not Nimvlets, not the background
app used for click-through testing — because the window had since
moved (owner-dragged) and a full/region screen capture reflects
whatever is really composited at that screen location, which had
become a different, unrelated real window. **The captured image was
deleted immediately, unexamined beyond confirming the mismatch, and
never referenced again.** Every capture after that point in this block
used `screencapture -l<windowID>` (captures one window's own pixel
buffer directly, regardless of what else is on screen at that
location) — which is both more reliable for this purpose and
structurally incapable of picking up unrelated content, rather than
relying on discipline alone.

### 5.5 Click-through instrumentation

Referenced throughout `src/app/SpikeApp.cpp`'s comments. Built
specifically to diagnose §5.1, logging six pipeline stages **only on
transition** (a value is printed only when it differs from the
last-logged value for that stage — never once per poll regardless of
change, which at 60Hz would flood the log):

1. global cursor position (`SDL_GetGlobalMouseState`)
2. window position (`SDL_GetWindowPosition`)
3. computed local coordinate (1 − 2)
4. hit-test result (`IsPointInteractive()`)
5. requested click-through state
6. `NSWindow.ignoresMouseEvents`, **read back** immediately after
   setting it (not assumed to equal what was requested)

Across both QA rounds this never showed a single stage-5/stage-6
mismatch — proving the poll-driven toggle itself was never the bug
(see §5.1). Compiled out entirely in Release builds (`#ifndef NDEBUG`,
both the logging calls *and* the backing fields — not just silenced)
so a normal run's log stays clean; available in Debug builds for
whoever eventually brings up the Windows fallback (§8) on real
hardware.

## 6. Bunny: the Block 01 closure QA fixture

The analytic placeholder (two overlapping circles, `core::BlobSilhouette`)
is mathematically exact but can't validate hit-testing against **real**
alpha data — antialiased edges, non-convex silhouette, a texture
instead of a flat fill. For final closure, the repository owner
supplied a real illustrated asset ("Bunny") as a **temporary QA
fixture only** — explicitly not the start of the real content system
(`docs/PET_CONTENT_SPEC.md` still describes zero implementation in
Block 01).

- **Pipeline:** source PNG (1254×1254, real alpha) → resized to 320×320
  (`sips`, preserves alpha) → `tools/prep_dev_sprite.py` (dependency-free
  Python, reuses this block's own PNG-decoding logic) → `assets/dev/bunny.rgba`,
  a trivial uncompressed format (4-byte magic + width + height + raw
  RGBA8) the C++ side reads with no PNG decoder and no `SDL_image`
  dependency (`graphics::DevSprite`).
- **Rendering:** `SDL_CreateTextureFromSurface` + `SDL_RenderTexture`
  into the same 160×160 logical destination rect the placeholder uses —
  exercises the exact `SDL_SetRenderLogicalPresentation` fix from §5.2
  against a second, independent asset. Pixel-confirmed correct (full
  window filled, sharp, no DPI regression).
- **Hit-testing:** `graphics::DevSprite::BuildAlphaMask()` rasterizes
  the *loaded image's own alpha channel* into a `core::AlphaMask` at
  the window's logical resolution — not the analytic shape. This same
  mask feeds both `SDL_SetWindowShape()` (§5.1's fix) and the
  MOUSE_BUTTON_DOWN defense-in-depth check, so what's clickable is
  derived from the same real pixel data on both the "let the click in"
  and "let the click through" sides.
- **Alpha threshold — `DevSprite::kHitTestAlphaThreshold = 128`
  (50%):** chosen after inspecting this specific asset's real alpha
  histogram, not picked arbitrarily:
  - background pixels: exactly alpha = 0 (60.6% of the image)
  - interior ("visibly part of the bunny") pixels: clustered tightly
    at alpha ≈ 253–254 (98% of all non-zero-alpha pixels are ≥128)
  - a thin antialiased edge band in between (the only pixels a
    threshold choice actually affects)

  128 is the standard antialiased-edge midpoint and, per the
  histogram, cleanly separates "background" from "visible shape" for
  this asset — see `docs/DECISION_LOG.md` DEC-018 for the full
  analysis with numbers.
- **Fallback, not replacement:** if `assets/dev/bunny.rgba` fails to
  load (e.g. the process isn't launched from the repo root, where the
  relative path resolves from), `SpikeApp` logs that and falls back to
  the analytic placeholder unchanged — the placeholder is not deleted
  and is still what `tests/SilhouetteTest.cpp` exercises directly,
  independent of which visual the running app happens to show.

**Manual QA result (owner-confirmed, final round):** click on Bunny →
registered (`click #1`); click on a transparent point near Bunny,
inside the window → reached the application underneath (silence in
Nimvlets' own log); drag on Bunny → moved the window, logged as
`drag ended (correctly not counted as a click)`, not a click.

## 7. Performance sampling (Release, native arm64)

Method: `ps -o rss,%cpu,time`, 3-second intervals, no `sudo`, no
special instrumentation — see the Block 01 Final Closure Report for
the full sample tables.

| Visual | RSS (stabilized) | CPU, idle-animated (steady state) |
|---|---|---|
| Analytic placeholder (original Block 01 close) | ≈70 MB | ≈1.2% average |
| Bunny QA fixture (this closure pass) | ≈72 MB | ≈2% average (after excluding the first few seconds' one-time texture/shape setup cost) |

The increase with Bunny is expected and modest: rendering a real
320×320 texture with linear filtering is more work than filling two
flat-colored circles. Both remain far below "busy-wait" territory
(≈100% of one core) and within `docs/PERFORMANCE_BUDGETS.md`'s
targets.

## 8. Windows

No Windows machine is available in this development environment
(Darwin only) — nothing below claims otherwise. Unchanged by this
block's macOS-focused QA rounds.

| Item | Status |
|---|---|
| Configure/Build | **NOT RUN locally.** Presets and `src/platform/windows/TransparentWindowSupport.cpp` exist and compile against verified SDL3 property names. `.github/workflows/ci.yml`'s `windows-x64` job exists but has not executed (nothing is pushed in this block). |
| Tests | **NOT RUN**, same reason. `tests/` has no SDL/platform dependency. |
| GUI runtime | **NOT TESTED.** No display, no Windows machine in this block. |
| Click-through mechanism | `platform::NativeShapeHitTestIsRenderSafe()` returns `false` for Windows (conservative default — not verified). It keeps using the poll-driven `SetWindowClickThrough()` fallback, **unchanged** by this block's SDL_SetWindowShape fix, which is macOS-specific (§5.1). Whether Windows' own `SDL_SetWindowShape` backend is *also* render-safe (contrary to the community reports DEC-017 cites) is an open question for real hardware, not assumed either way. |

### Pending before Windows can be trusted

1. Push this branch (owner's decision, outside this block) and confirm
   the `windows-x64` CI job is green.
2. On real Windows 10/11 x64 hardware: build and run
   `nimvlets_spike.exe`, repeat this block's manual QA (§3's items,
   with Bunny).
3. Specifically watch `WS_EX_TRANSPARENT` toggling — the one native
   code path in this entire block that has never executed anywhere,
   not even in CI (no display there).
4. Optionally: investigate whether Windows' `SDL_SetWindowShape`
   backend is also render-safe, the way §5.1 found macOS's to be — if
   so, Windows could drop its poll-driven fallback too. Not attempted
   here; no way to verify without Windows hardware.

## 9. Intel Mac / Universal2

Built and verified on the Apple Silicon dev machine used for this
block — see the Block 01 Final Closure Report for the full `lipo -info`
output and binary sizes. `cmake --preset macos-universal2-release`
configures and builds cleanly (including all fixes from §5); both
`nimvlets_spike` and `nimvlets_tests` report `arm64 x86_64` via
`lipo -info`; the host-arch slice runs and passes all unit tests.

**A cross-compiled universal2 build is not a substitute for a real
smoke test on Intel hardware** — none was available in this block.
Real, open item: the transparent window / click-through / drag
behavior on Intel has never been observed.

## 10. SDL3 recommendation

**KEEP SDL3 + native adapters.** Reaffirmed with stronger evidence than
the block's first close had.

The click-through investigation (§5.1) is itself the best evidence for
this: the *first* verdict — reject `SDL_SetWindowShape`, hand-roll
polling instead — was wrong, but SDL3 wasn't the reason it was wrong;
an incomplete evaluation (community reports instead of the pinned
source) was. Once the actual macOS backend source was read, the
correct, minimal, event-driven mechanism was already sitting in SDL3's
public API, needing only a `core::AlphaMask`-to-`SDL_Surface` adapter
(≈15 lines) to use correctly. That's exactly what "native adapters"
means in this recommendation: SDL3 as the cross-platform base, with
platform-specific behavior isolated behind `src/platform/*` (here,
`NativeShapeHitTestIsRenderSafe()`) rather than reason to abandon the
framework.

Two more real bugs were found and fixed in this block (§5.2 DPI scale,
§5.3 focus steal) — both were **our own missing calls**
(`SDL_SetRenderLogicalPresentation`, `SDL_SetHint`), not SDL defects;
both have official, documented, one-line fixes. Zero SDL3 API
mismatches were found between documentation and the pinned 3.4.12
source across this entire block.

Reconsider only if a block hits an incompatibility SDL3 can't be
worked around for with a small adapter — not encountered here, even
after two full rounds of real interactive QA and a genuine bug hunt.

---

## 11. macOS click-through, estado actual (Block 05, pasada de estabilización)

Esta sección reemplaza a §5.1 como descripción de **lo que se envía
hoy** en macOS. §5.1 no se corrige ni se borra: era correcta para el
driver acelerado, que era el default cuando se escribió.

### 11.1 Por qué la ruta de shape ya no se puede usar

`SDL_RenderPresent()` llama a `SDL_RenderApplyWindowShape()`
(`src/render/SDL_render.c:5463` en la fuente pineada 3.4.12) para toda
ventana transparente. Esa función crea una textura desde la superficie
de forma y le pide un blend mode CUSTOM
(`SDL_ComposeCustomBlendMode(ZERO, SRC_ALPHA, ADD, ZERO, SRC_ALPHA, ADD)`).
El renderer de software no implementa `SupportsBlendMode`, así que
`IsSupportedBlendMode()` (`SDL_render.c:1409`) rechaza todo modo custom
y la llamada falla; SDL ignora el fallo por diseño, la textura de forma
se queda en `SDL_BLENDMODE_BLEND`, y **el bitmap blanco de la forma se
dibuja ENCIMA del contenido**. Esa es la "silueta blanca opaca".

Repro mínimo (programa SDL3 standalone, flags de ventana de
producción, sin código de la app):

| driver | ¿acepta el blend mode de la forma? | pixel central tras instalar la forma |
|---|---|---|
| `software` | **NO** — "That operation is not supported" | `(255,255,255,255)` |
| `metal` | sí | `(0,0,0,0)` (sin cambio) |

`SDL_SetWindowShape(window, NULL)` restaura el render de inmediato, así
que la corrupción **no es permanente** — dura exactamente lo que dure
la forma instalada. DEC-085 afirmaba lo contrario; ver la corrección
anotada ahí.

### 11.2 Por qué el muestreo de cursor venía perdiendo

En SDL 3.4.12 hay exactamente dos escritores de
`NSWindow.ignoresMouseEvents` (`grep -rn ignoresMouseEvents src/`):

1. `Cocoa_UpdateWindowShape()` — `src/video/cocoa/SDL_cocoashape.m:50`,
   solo desde `SDL_SetWindowShape()`.
2. `-[Cocoa_WindowListener updateIgnoreMouseState:]` —
   `src/video/cocoa/SDL_cocoawindow.m:1073`, solo desde `-mouseMoved:`
   (línea 1893), bajo `(window->flags & SDL_WINDOW_TRANSPARENT)`.
   `-mouseDragged:`/`-rightMouseDragged:`/`-otherMouseDragged:`
   reenvían todos ahí.

El (2) lee la forma desde `SDL_PROP_WINDOW_SHAPE_POINTER` y, **sin
forma instalada, asigna `NO` incondicionalmente**. Medido con los flags
de producción: UN solo `NSEventTypeMouseMoved` entregado a nuestra
propia ventana da vuelta el valor de YES a NO.

Por eso §5.1 tenía razón en que la instrumentación "nunca mostró un
mismatch": el toggle SIEMPRE funcionaba en el instante de la
asignación. Lo que faltaba medir era el estado **después** de
actividad de mouse real — que es exactamente el diagnóstico que esta
pasada agregó (`ReadWindowClickThrough()`, lee sin escribir).

### 11.3 Mecanismo que se envía

`platform::MakeClickThroughAuthoritative()` intercepta
`-setIgnoresMouseEvents:` para nuestra ventana (override agregado a su
clase concreta vía el runtime de Objective-C; swizzle in-place solo si
esa clase ya implementaba el selector) y descarta toda escritura que no
venga del adaptador. Nimvlets escribe llamando a la IMP original
directamente.

Alternativas evaluadas y por qué no: ver DEC-086 (instalar la forma
igual — imposible, renderer y Cocoa leen la MISMA propiedad; parchear
SDL pineado — forkea una dependencia pineada a un tag exacto y hay que
revalidarla en cuatro plataformas; segunda ventana con forma solo para
hit-test — la ventana visual sigue siendo transparente, así que SDL le
pisaría `ignoresMouseEvents` igual; monitor global de NSEvent —
prohibido por AGENTS.md §5/§14).

**Permisos:** ninguno nuevo. Todo esto es configuración de nuestra
propia ventana dentro de nuestro propio proceso más consultas de
POSICIÓN de cursor (`SDL_GetGlobalMouseState`). Sin Accessibility, sin
Input Monitoring, sin Screen Recording, sin hook global de input.

### 11.4 Política de muestreo

`core::EvaluateClickThrough()` (pura, con tests): mientras el cursor
está FUERA del rectángulo de la ventana el estado de click-through es
inobservable — ningún click de ahí puede llegarnos — así que se elige
deliberadamente NO-click-through, lo que devuelve la entrega normal de
eventos y convierte el ingreso a la ventana en un EVENTO en vez de algo
que haya que encuestar. El muestreo periódico queda armado **solo
mientras el cursor está dentro del rectángulo**.

Límite honesto: mientras el click-through está ACTIVO la ventana no
recibe eventos de mouse, así que detectar el regreso del cursor exige
muestrear. Es inherente a `ignoresMouseEvents`, no una elección de este
diseño (la ruta de shape de SDL tiene la misma forma: reevalúa en
`-mouseMoved:`). Un salto instantáneo del cursor a un pixel
transparente seguido de un click inmediato, sin ningún evento de
movimiento intermedio, puede perderse por hasta un intervalo de
muestreo.

### 11.5 Verificación nativa reproducible

`src/platform/macos/ClickThroughOwnershipCheck.mm` — ejecutable
opcional (no entra jamás a la corrida normal de CTest; la CI de las
cuatro plataformas queda igual):

```bash
cmake --preset macos-debug -DNIMVLETS_ENABLE_GUI_CHECKS=ON
cmake --build --preset macos-debug --target nimvlets_macos_clickthrough_check
./build/macos-debug/src/platform/macos/nimvlets_macos_clickthrough_check
```

Ejercita el adaptador REAL con `NSEvent`s reales entregados a nuestra
propia ventana (el camino exacto de AppKit que dispara el código de SDL
de §11.2 — in-process, sin permisos): la forma nunca se instala, el
render no se corrompe, 25 escrituras externas de SDL se interceptan y
el estado aguanta, y 4 ciclos transparente -> opaco -> transparente
salen correctos.

**Lo que esta verificación NO prueba** (y por lo tanto sigue siendo QA
interactiva del owner): que un click humano real sobre un pixel
transparente llegue a la app de abajo. Mover la ventana por debajo de
un cursor quieto genera CERO eventos de mouse (medido: `motion=0
enter=0 leave=0` en ambos estados), así que este harness no puede
simular movimiento real de cursor.

## 12. Product UI + menú rápido (Block 06)

### 12.1 Estado por plataforma

| Item | macOS | Windows | Linux |
|---|---|---|---|
| Ventana de producto (Collection) se abre/cierra, renderiza | **PASS** (visto, capturas de QA) | NOT TESTED (compila) | NOT TESTED (compila) |
| Texto del sistema (`platform::RasterizeText`) | **PASS** (Core Text; `nimvlets_macos_text_check` headless + capturas) | N/A — stub devuelve `false` | N/A — stub devuelve `false` |
| Menú rápido nativo (`NSStatusItem`) | **PARTIAL** — se instala (log "quick menu installed"), estructura cubierta por `QuickMenuModelTest`; el despliegue visual del `NSMenu` no se capturó por harness (ver 12.3) | N/A — adapter no-op | N/A — adapter no-op |
| Live pet switch desde la Collection | **PASS** — `NIMVLETS_DEV_ACTIVATE=frin/female` cambia el pet del escritorio y la Collection lo refleja, sin reiniciar | NOT TESTED | NOT TESTED |
| Open/close lifecycle (pet sobrevive) | **PASS** — `NIMVLETS_DEV_COLLECTION_CYCLES=8`, pet activo tras todos, shutdown limpio | NOT TESTED | NOT TESTED |
| Ciclo de vida event-driven (sin loop de render oculto) | **PASS** — CPU ≈0% con la Collection abierta en reposo (`docs/PERFORMANCE_BUDGETS.md`) | — | — |

Windows/Linux: los tres jobs de CI (configure/build/test) siguen verdes.
Las costuras (`TextRasterizer`, `SystemShell`,
`BringApplicationToForeground`) tienen stubs honestos, no fingidos
(brief §24). Una implementación real (DirectWrite / fontconfig+FreeType;
bandeja / StatusNotifierItem) es trabajo futuro, hardware mediante.

### 12.2 QA manual del owner (macOS) — checklist

Desde la raíz del repo, Release recomendado. Usar
`NIMVLETS_DEV_APPDATA_DIR` para no tocar el estado real.

```bash
./build/macos-release/src/app/nimvlets_spike
```

1. **Menú de la barra**: aparece el icono monocromo de Nimvlets arriba
   a la derecha. Abrirlo: header con el nombre del pet, "Hide Nimvlet",
   "Collection…", "Size ▸" (Small/Medium/Large, uno marcado),
   "Opacity ▸" (100/85/70/55 %), "Lock Position" (checkable),
   "Quit Nimvlets".
2. **Collection…**: abre una ventana normal ~760×540. Grid: Bunny
   ("On desktop"), Nidir ("Not in your collection", sin arte), Frin
   ("Male · Female"). Balance arriba a la derecha.
3. **Detalle**: click en Frin → panel expandido con arte grande, chips
   Male/Female, botón "Use Frin". Cambiar de chip actualiza la preview.
4. **Switch en vivo**: "Use Frin" → el pet del escritorio pasa a ser
   Frin, sin reiniciar; la Collection ahora muestra Bunny "Use" y Frin
   "On desktop".
5. **Cerrar la Collection** (botón rojo): la app sigue viva, el pet
   sigue en el escritorio, el menú sigue disponible. Reabrir Collection
   funciona.
6. **Hide Nimvlet / Show Nimvlet**: oculta/restaura solo el pet; la app
   no termina.
7. **Lock Position**: arrastrar el pet no lo mueve; clickearlo sí
   cuenta (el balance sube, visible al reabrir la Collection); hover y
   click-through siguen.
8. **Size / Opacity**: cambian el pet en vivo y persisten (cerrar y
   reabrir).
9. **Quit Nimvlets**: la app entera termina, limpio.
10. **Teclado en la Collection**: Tab recorre las entradas y (con
    detalle abierto) los chips + el botón; Enter/Space activa; Esc
    cierra el detalle o la ventana; anillo de foco terracota visible.

### 12.3 Límite del harness de captura

El entorno de agente de este bloque no pudo inyectar clicks/teclas
sintéticas de forma confiable a la ventana SDL (`CGEventPost` requiere
permiso de Accessibility para el proceso que los emite, no concedido) —
por eso las capturas del panel de detalle y del switch en vivo se
generaron con los mecanismos `NIMVLETS_DEV_OPEN_COLLECTION=<petId>` /
`NIMVLETS_DEV_ACTIVATE` (que ejercitan la MISMA ruta de código que un
click real: `HitTest` → `OpenDetail` / `CanActivate` →
`TrySwitchActivePet`), y el `NSMenu` desplegado no se capturó. La ruta
de input real está cubierta por tests puros
(`CollectionLayout::HitTest`, `FocusList`) y es idéntica en estructura
a la de la ventana del pet, que sí funciona con input real.

### 12.4 Adenda Block 06.1 (hero + gallery, localización)

Sin cambios de plataforma: 06.1 es un pase visual + de idioma sobre la
misma arquitectura macOS-única. El estado de 12.1 sigue vigente
(Windows/Linux: compilan, stubs honestos).

Añadidos al checklist de QA manual del owner (12.2):

11. **Composición hero + gallery**: al abrir, el pet activo es el
    HERO (arte grande, forma de acento sutil, nombre grande, especie,
    estado, botón); los demás en la gallery. Click en un pet de la
    gallery → pasa a hero.
12. **Selector de variante de Frin**: con Frin como hero, "Male ·
    Female" tipográfico con subrayado del acento bajo la seleccionada;
    cambiar actualiza la preview y el botón al instante.
13. **Locked (Nidir)**: su arte se ve, más callado; sin botón de
    compra ni precio.
14. **Acento por pet**: la forma detrás del hero y la línea de foco
    tienen el tono del pet (Bunny apricot, Nidir violeta, Frin azul);
    el resto de la UI no cambia de color.
15. **`Language ▸`** en el menú: English / Español, uno marcado.
    Elegir el otro → el menú entero y la Collection abierta cambian de
    idioma **al instante**, sin reiniciar. "clicks" → "clics", nunca
    "monedas"; "Bunny"/"Nidir"/"Frin" y "Nimvlets" no cambian.
    Persiste: cerrar y reabrir la app mantiene el idioma elegido.
16. **`Size ▸ Large`** ahora es 1.15 (antes 1.30): más chico que en
    Block 06; una preferencia "large" guardada se ve al nuevo tamaño
    sin más.

El límite del harness de captura (12.3) sigue igual — las capturas del
hero por estado y del idioma se generaron con
`NIMVLETS_DEV_OPEN_COLLECTION=<petId[/variant]>` +
`NIMVLETS_DEV_LANGUAGE=<en|es>` (misma ruta de código que un click /
una elección de menú reales); el `NSMenu` en español no se capturó por
harness — verificación manual del owner.

### 12.5 Adenda Block 06.2 (previews livianas, nitidez Retina, hero stage)

Sin cambios de plataforma: 06.2 sigue siendo macOS-única, misma
arquitectura; Windows/Linux compilan con stubs honestos. Novedades del
adaptador macOS que el owner debería mirar en su máquina:

17. **Cambio de variante Frin instantáneo**: con Frin como hero,
    alternar Macho ⇆ Hembra ya NO tiene la pausa perceptible de 06.1
    (antes abría un `.nvpack` de ~76 MB por cambio; ahora es un lookup
    sobre una textura ya residente — ver DEC-119 y
    `docs/PERFORMANCE_BUDGETS.md`).
18. **Texto nítido en Retina**: el texto centrado del botón y de la
    gallery ("Use Frin", nombres, estados) ya no se ve blando —
    `GlyphBlitOrigin` acota el bitmap a píxel entero del dispositivo
    (DEC-120). El check nativo `nimvlets_macos_text_check` verifica
    además la relación de píxeles 1x vs 2x.
19. **Hero stage con acento**: halo asimétrico teñido con el color del
    pet alrededor del arte + la Collection en dos planos (gallery sobre
    un neutro cálido más profundo). El botón "Use <pet>" ahora tiene el
    tinte del pet, no es casi-negro.

`platform::RasterizeText` no cambió — el arreglo Retina es de colocación
en `productui`, no del rasterizador. Las 8 capturas de QA de 06.2 se
generaron con `NIMVLETS_DEV_PRODUCT_SHOT=<path>` (vuelca el framebuffer
del renderer de la Collection vía `SDL_RenderReadPixels` + `SDL_SaveBMP`,
a densidad nativa — cómputo local, sin ninguna captura de pantalla del
SO, AGENTS.md §5).

### 12.6 Adenda Block 07 (Shop + wallet)

**Sin cambios de plataforma.** Block 07 es enteramente `src/productui`
+ `src/catalog` + `src/persistence` + `src/app`: agrega el Shop como
sección del Product UI, la navegación `Collection · Shop`, el wallet
gastable y las autorizaciones capaces de variantes. `src/platform/*`
queda intacto; Windows/Linux compilan con los mismos stubs honestos.

Los dos checks nativos de macOS se corrieron y **siguen PASS**, sin
regresión de Block 06:

- `nimvlets_macos_text_check` — todos los sub-checks PASS, incluida la
  relación de píxeles 1x vs 2x (el path Retina de DEC-120). El
  word-wrap nuevo (`TextCache::DrawTextWrapped`) usa `DrawText` /
  `MeasureText` sin tocar el rasterizador.
- `nimvlets_macos_clickthrough_check` — `ALL CHECKS PASSED (0
  failures)`; la ventana del pet y su per-pixel hit-test no cambian
  (`core::EvaluateClickThrough` intacto).

Checklist del owner (macOS), además del de 06.1/06.2:

20. **Navegación Collection ⇆ Shop**: la fila `Collection · Shop`
    responde con mouse y con teclado (Tab llega a las pestañas, Enter
    cambia). El pet en el escritorio no parpadea ni se recarga al
    cambiar de sección.
21. **Compra**: en el Shop, "Get <pet>" abre la pregunta inline; el
    foco arranca en "Cancelar"; `Esc` la cierra sin gastar; "Confirmar"
    baja el balance y el pet pasa a "In your collection" en las dos
    secciones al instante. Reiniciar Nimvlets: el balance reducido y la
    propiedad nueva siguen ahí.
22. **Frin**: NO aparece en el Shop (ni como comprable ni con precio).
23. **EN/ES**: cambiar de idioma re-etiqueta la nav, el precio, "Get" /
    "Obtener", la pregunta de confirmación y "In your collection" al
    instante, sin reiniciar.

Las 8 capturas de QA de Block 07 (5 EN + 2 ES + 1 Collection ES) se
generaron con `NIMVLETS_DEV_PRODUCT_SHOT` + los hooks
`NIMVLETS_DEV_SECTION` / `NIMVLETS_DEV_SHOP_PET` / `NIMVLETS_DEV_SHOP_CONFIRM`
/ `NIMVLETS_DEV_BUY` contra un directorio de app-data aislado — mismo
mecanismo de cómputo local que 06.2, sin captura de pantalla del SO.

**Pasada de corrección de Block 07 (DEC-128).** Migración legacy de
Frin a variantes explícitas, objetivo de compra data-driven
(`EvaluatePurchase` toma una `PetIdentity`), y resolución del pet
activo sin bypass de economía. Es enteramente lógica: **ningún dibujo
del Product UI cambió** — las 8 capturas de Block 07 siguen siendo
válidas — y `src/platform/*` no se tocó. Los dos checks nativos de
macOS se re-corrieron: `nimvlets_macos_text_check` PASS,
`nimvlets_macos_clickthrough_check` `ALL CHECKS PASSED (0 failures)`.
Verificado además contra el binario real: instalación nueva siembra
`{bunny,""} {frin,female} {frin,male}` (sin `{frin,""}`); un save v3
con `ownedPetIds=[bunny,frin]` se reescribe a v4 con esas tres
autorizaciones; un save v4 corrupto con `active=nidir, owned={bunny}`
reabre en bunny con `owned` intacto (nidir NO se otorgó).

## 13. Modo de conteo de clics global, OPT-IN (Block 11A)

Diseño completo en `docs/GLOBAL_CLICK_MODE.md`. Acá, el estado de
plataforma con la disciplina PASS / NOT TESTED de AGENTS.md §4.

### 13.1 Estado por plataforma

| Item | macOS | Windows | Linux/X11 | Linux/Wayland |
|---|---|---|---|---|
| Capacidad reportada | `kSupportedNeedsPermission` | `kUnavailable` | `kUnavailable` | `kUnavailable` |
| Permiso | **Input Monitoring** | — | — | — |
| Preflight sin diálogo (`CGPreflightListenEventAccess`) | **PASS** | n/a | n/a | n/a |
| Creación del event tap listen-only | **PASS** | NOT IMPLEMENTED | NOT IMPLEMENTED | NOT POSSIBLE |
| Arranque/parada del monitor, shutdown limpio | **PASS** | n/a | n/a | n/a |
| Arranque sin prompt con `anywhere` persistido | **PASS** | n/a | n/a | n/a |
| Prevención de doble conteo (app-level) | **PASS** | n/a | n/a | n/a |
| Persistencia v6 del modo | **PASS** | (compila) | (compila) | (compila) |
| **Pulsación FÍSICA del escritorio contada** | **NOT TESTED** — requiere un humano; ver §13.3 | n/a | n/a | n/a |
| Settings muestra "Not available on this system" | n/a | **NOT TESTED** (no se corrió) | **NOT TESTED** | **NOT TESTED** |

### 13.2 Lo verificado en vivo (macOS 26.6, arm64, Debug + Release)

Con `NIMVLETS_DEV_APPDATA_DIR` aislado y el pet oculto:

- arranque en modo local: **ningún** monitor instalado, ningún prompt de
  TCC;
- 5 eventos globales reenviados en modo local → **0 contados**;
- pedir "Anywhere" con el permiso ya concedido → *"global click monitor
  ACTIVE"*;
- **doble conteo:** con el modo global activo, 4 clics del pet + 6
  eventos globales → balance 3 → **9** (los del pet sumaron **0**);
- reinicio con `anywhere` persistido → preflight, arranque silencioso,
  **sin prompt**, balance preservado;
- volver a "Nimvlet only" → *"global click monitor stopped"*; los
  eventos globales dejan de contar, los del pet vuelven;
- el archivo en disco es **v6** y termina en `anywhere` /
  `nimvlet_only`;
- shutdown limpio en todos los casos (el monitor se para y hace `join`
  ANTES del flush de estado).

Regresiones de lo anterior, sin cambios: `NIMVLETS_DEV_UI_NAV_SMOKE`
6/6; onboarding DEV completa y fija `lifecycle=completed`; el hotspot
invisible del Starter Shop sigue abriendo con ofertas legítimas y siendo
no-op sin ellas; compra del Shop público (Nidir a 300) intacta;
`nimvlets_macos_text_check` y `nimvlets_macos_clickthrough_check` PASS.

### 13.3 El hueco honesto

**No se verificó que una pulsación FÍSICA real del botón primario en el
escritorio llegue al tap y sume 1.** Sintetizar un clic requeriría
`CGEventPost` / permiso de *post event* (o Accessibility) — APIs que
este producto tiene prohibidas y que el guard de privacidad rechaza. Lo
que sí quedó probado es todo lo que rodea a ese paso: el permiso, la
creación del tap, el hilo, el reenvío al hilo principal, la política de
conteo, la persistencia y el apagado. El paso restante es QA manual del
owner — la checklist está en `docs/GLOBAL_CLICK_MODE.md` §16.

**Windows y Linux: NOT RUNTIME VERIFIED.** No se escribió código Win32
ni X11 para esta feature (brief §16/§17): los adapters reportan
`kUnavailable` honestamente y el diseño investigado queda documentado en
`docs/GLOBAL_CLICK_MODE.md` §12. Que compilen en CI no es una
afirmación de comportamiento.
