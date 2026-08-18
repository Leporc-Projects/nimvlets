# Nimvlets — Performance Budgets

These are **engineering budgets**, not commercial promises. They're
targets to design and measure against, not marketing claims about the
finished product — actual numbers only become a promise once measured
on final, signed/notarized Release builds with real content.

## Budgets

| Metric | Target | Aspiration | Notes |
|---|---|---|---|
| RAM, resident, normal operation | < 100 MB | < 64 MB where reasonable | Measured on Release, after the process stabilizes (not the first-second spike). |
| CPU, idle with animation running | < 1% average | — | Measured over a real window (tens of seconds), not a 1-second sample. Reference hardware = whatever machine measured it; see the Block 01 report for the actual machine. |
| CPU, hidden/inactive (future) | ~0% except for real events/timers | — | Not applicable yet — Block 01 has no "hidden" state. |
| GPU | Trivial | — | A handful of filled circles at low frame rate; not meaningfully measured in Block 01. |
| Network, base | 0 bytes | — | No network activity is initiated by the app at runtime. |
| Install size, v1 | < 150 MB | smaller | Not applicable to Block 01 (no installer/packaging yet — see NON-SCOPE). |
| Startup, future | ≤ 1 s | — | On reasonable hardware; approximate, not instrumented precisely in Block 01. |

## Methodology rules (binding, not optional)

- **Release only.** Debug builds (unoptimized, with extra runtime
  checks) are never used to conclude anything about these budgets.
- **No single-sample conclusions.** A metric is measured over a real
  time window with multiple samples, not one snapshot.
- **No sudo / no special instrumentation.** Measurements use ordinary,
  unprivileged OS tools (`ps`, `top`, process listing) so the method is
  reproducible by anyone building this repo, not just on an
  instrumented dev machine.
- **Honest gaps.** If a metric can't be measured reliably without
  privileged tools or extra instrumentation this block doesn't have,
  it's marked `NOT MEASURED` with a reason — never guessed.

## Block 01's actual measurements

See the Block 01 report, §7 "Performance," for this block's concrete
numbers (RSS, idle CPU, startup, binary size) and the exact method used
to obtain each one, plus any caveats (e.g. measured on Apple Silicon
only — no Intel Mac or Windows machine was available for a real
hardware run in this block; see `docs/PLATFORM_SPIKE.md`).

## Block 02's actual measurements

Same method as Block 01 (`ps -o rss,%cpu,time`, 3-second intervals over
a 27–30s window, Release build, native arm64, no `sudo`). See
`docs/ANIMATION_RUNTIME.md` §6 for the full write-up of *why* these
numbers changed (the deadline-driven scheduler replacing Block 01's
fixed ~12fps tick — DEC-021).

| Scenario | CPU (average, steady state) | RSS |
|---|---|---|
| Block 01, Bunny fixture, fixed ~12fps tick (baseline) | ≈2% | ≈72 MB |
| Block 02, static idle (production default: no passive action due, no interaction) | **≈0.0%** | ≈73 MB |
| Block 02, passive action forced every ~2s (`NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS=2` — an artificial stress scenario, ~150x more frequent than the real ~300s default) | <1% (peak 1.0%, mostly 0.0–0.5%) | ≈74 MB |

The "CPU, idle with animation running" budget row above (< 1% average)
is now met with considerable margin at true static idle (≈0.0%, not
just under budget), and even the artificial worst-case passive-action
stress scenario stays under the 1% target. Click-reaction CPU was not
measured as a separate scenario in this block (it shares the exact same
render/hit-mask-update code path as passive actions — see
`docs/ANIMATION_RUNTIME.md` §3 — so the passive-action stress number
above is a reasonable proxy, not a claim of an identically-measured
data point). RSS remains well within the < 100 MB budget across all
three scenarios.

## Mediciones reales de Block 03

La persistencia (`src/persistence`) agrega una escritura a disco con
debounce y, según `docs/PERSISTENCE.md` §9, una espera máxima del
event loop acotada a 1 segundo (corrigiendo un bug de latencia de
shutdown que encontraron los propios tests de ese bloque — ver
`docs/DECISION_LOG.md` DEC-028). Re-medido con el mismo método que
Block 02, Release, arm64 nativo, persistencia activa
(`NIMVLETS_DEV_APPDATA_DIR` apuntado a un directorio temporal
aislado):

| Escenario | CPU (promedio, steady state) | RSS |
|---|---|---|
| Block 03, idle estático, persistencia activa, sin escrituras pendientes | **≈0.0%** | ≈73–74 MB |

Sin cambios respecto a Block 02 dentro del ruido de medición — el cap
de espera de 1 segundo y el scheduler de persistencia (en idle, no
dirty) agregan un despertar que no hace ningún trabajo la abrumadora
mayoría de las veces, no un busy-wait ni un tick de render periódico.
Una única escritura con debounce (unas pocas decenas de bytes,
renombrada atómicamente) no es suficientemente grande como para
registrarse en una muestra de `ps` a intervalos de 3 segundos; no se
hizo una medición dedicada de CPU/latencia por escritura en este
bloque, ya que ninguno de sus requisitos la pedía y el mecanismo (una
pequeña escritura bufferizada + un rename) no tiene ningún camino
realista para ser un costo medible a esta escala.

## Mediciones reales de Block 04

Block 04 (`src/catalog`, ver `docs/CATALOG.md`) no agrega ningún
polling ni toca el cálculo de `waitMs` del event loop — el requisito
específico de este bloque (§5 del brief: "confirmar que el switching
repetido no deja crecimiento obvio de recursos") se midió comparando
el RSS en steady-state tras una ráfaga chica de switches contra una
ráfaga grande, vía el mecanismo solo-DEV
`NIMVLETS_DEV_SWITCH_TEST_COUNT` (ver `docs/CATALOG.md` §7), Release,
arm64 nativo:

| Escenario | RSS (steady state, tras completar la ráfaga) |
|---|---|
| 5 switches automatizados (todos al mismo pet — solo Bunny es real) | ≈76.3–76.4 MB |
| 500 switches automatizados (mismo pet, 100x más) | ≈76.4–76.5 MB |

Diferencia de bien menos de 1 MB entre 5 y 500 switches — dentro del
ruido normal del allocator, no una tendencia de crecimiento. Cada
switch suelta las texturas del pet anterior antes de adjuntar las del
nuevo (ver `docs/CATALOG.md` §6), así que este resultado es consistente
con lo esperado: nada se acumula por switch. CPU se mantuvo en 0.0%
en ambos escenarios una vez completada la ráfaga (el trabajo real de
cada switch — leer un archivo pequeño, crear/destruir una textura — es
demasiado breve para registrarse en una muestra de `ps`).

## Linux (Block 04.1)

Sin números reales todavía. `.github/workflows/ci.yml`'s `linux-x64`
job captura una muestra de `ps -o pid,rss,%cpu,%mem` durante su smoke
X11 (ver `docs/LINUX_PLATFORM.md` §10) — pero como este bloque no pudo
ejecutar (`push`/`publish` prohibidos para esta sesión, ver AGENTS.md
§15) ese workflow, no existe ninguna medición real de Linux para
reportar acá todavía; los primeros números existirán recién después de
que ese job corra post-integración. Como con cualquier medición de
CI-VM (ver "Methodology rules" arriba), esos números **nunca**
establecerán un presupuesto final por sí solos — solo confirman ausencia
de regresiones obvias (leaks, busy-wait) en ese entorno específico.
Sin cambios de arquitectura esperados en el CPU/RSS de Linux: mismo
event loop deadline-driven que macOS/Windows, sin ningún polling nuevo
en X11 (mismo mecanismo event-driven que macOS) ni en Wayland
(`usingPollDrivenClickThrough_` es `false` ahí específicamente para
evitar un loop inútil — ver `docs/LINUX_PLATFORM.md` §4).
