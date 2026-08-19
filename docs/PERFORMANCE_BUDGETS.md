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

## Mediciones reales de Block 04.2, primera pasada (SUPERSEDED)

<details><summary>Números originales, ya no vigentes — ver la sección
siguiente para las mediciones correctas de la segunda pasada</summary>

Medido contra el binario Release real (arm64 nativo, `ps -o rss,%cpu`,
muestras cada 3s), vía `NIMVLETS_DEV_SWITCH_TEST_COUNT=2` (Bunny ->
Nidir).

**Bunny, sin switch (regresión, sin cambios de Block 04.2):** RSS
≈74.0 MB, CPU 0.0% en steady state — idéntico al baseline de bloques
anteriores.

**Nidir cargado (idle loop real, 25 frames, canvas nativo 513×525):**

| fps del idle loop | CPU (steady state) | RSS (steady state) |
|---|---|---|
| 12 (primer intento) | ≈11–12% | ≈199 MB |
| 6 (valor elegido en la primera pasada) | ≈4–5.5% | ≈259 MB |

Esta medición se hizo con el `idle` de Nidir modelado incorrectamente
como `PlaybackKind::kLoop` continuo (ver DEC-044) — **no es una
medición válida de idle estático final** y quedó explícitamente
invalidada por el propio owner al pedir la corrección de semántica.
Se conserva acá solo como referencia histórica de por qué la
corrección era necesaria.

</details>

## Mediciones reales de Block 04.2, segunda pasada (semántica corregida + canvas lógico + downscale)

Medido contra el binario Release real (arm64 nativo, `ps -o rss,%cpu`,
muestras cada 1.5–3s sobre ventanas de 15–30s), tras aplicar
DEC-044 (base estática + idle periódico one-shot), DEC-045 (canvas
lógico 156×160) y DEC-046 (downscale de runtime a 320px máx. por
lado) — ver `docs/NIDIR_CONTENT.md` §§5.1, 7, 8. Escenarios medidos
por separado, cada uno como proceso propio, señal `SIGTERM` limpia al
final:

| Escenario | Cómo se disparó | CPU | RSS (steady state) |
|---|---|---|---|
| **(A) Base estática** (sin passive action pendiente, sin click) | `NIMVLETS_DEV_SWITCH_TEST_COUNT=2` (Bunny → Nidir), sin overrides adicionales | **0.0%** (5 muestras, 15s) | **≈127.2–127.6 MB** |
| **(B) Idle periódico activo** (`passiveActions[0]`, `kOneShot`, disparado cada 15s vía `NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS=15`) | ídem + intervalo forzado (~20x más frecuente que el default de 300s, para poder observar el pico dentro de una ventana corta) | **pico ≈2.2–2.4%** durante la reproducción (~3s), **0.0%** el resto del ciclo | **≈127.5–127.7 MB, plano** en los 4 ciclos observados |
| **(C) Click activo** (`clickReaction` placeholder, `kOneShot`, ~100ms) | ráfaga de 500 clicks sintéticos vía `NIMVLETS_DEV_CLICK_TEST_COUNT=500` | **no medible de forma confiable** — ver nota abajo | **≈127.2–127.7 MB, sin crecimiento tras 500 disparos** |
| Bunny, sin switch (regresión) | proceso separado, sin overrides | 0.0% | ≈74.1–74.2 MB — idéntico al baseline histórico |

**Nota sobre (C):** el placeholder de click (dos frames, ~100ms) es
demasiado corto para que un muestreo de `ps` (que ya de por sí
promedia sobre intervalos de segundos) capture su CPU real de forma
aislada; el mecanismo DEV solo puede disparar clicks sintéticos, no
sostener la reproducción en un loop repetido sin interacción real. Lo
que sí se confirmó de forma sólida: una ráfaga de 500 clicks
sintéticos consecutivos no deja crecimiento de RSS (mismo patrón que
las 500 acciones pasivas — ver abajo), y el costo por frame de
`clickReaction` comparte exactamente el mismo camino de código de
render/hit-mask que `passiveActions[0]` (ver
`docs/ANIMATION_RUNTIME.md` §3) — así que el costo instantáneo real es
arquitectónicamente comparable al pico medido en (B), aunque no exista
un número medido de forma independiente para (C). Esto se documenta
como una limitación honesta, no como un número inventado.

**Comparación con la primera pasada:** el RSS de Nidir bajó de ≈259 MB
a **≈127 MB** (el downscale de runtime a 320px máx. por lado, DEC-046,
reduce cada frame a ~37% de sus pixeles nativos) y el CPU en reposo
real bajó de ≈4–5.5% *continuo* a **0.0% en reposo, con picos breves
de ≈2.3% solo durante los ~3s que dura una reproducción esporádica**
— la comparación correcta ya no es "Nidir siempre cuesta X% de CPU"
sino "Nidir cuesta 0% la gran mayoría del tiempo, igual que Bunny,
salvo por ventanas cortas y poco frecuentes de reproducción".

**Ausencia de crecimiento de recursos confirmada** con dos mecanismos
distintos contra el binario real: 500 switches Bunny/Nidir alternados
(`tests/PetSwitchingTest.cpp` + smoke real), 40 cambios de dirección
(`NIMVLETS_DEV_DIRECTION_TEST_COUNT=40`), y 500 clicks sintéticos
consecutivos (`NIMVLETS_DEV_CLICK_TEST_COUNT=500`, este bloque,
segunda pasada) — ninguno mostró RSS creciendo más allá del ruido
normal del allocator (variación de menos de 1 MB entre muestras).

**Presupuesto:** con la corrección de semántica, Nidir en reposo ahora
cumple el objetivo de "CPU, idle con animación corriendo" (< 1%
promedio) con el mismo margen que Bunny — el idle real ya no es
continuo, así que la comparación "idle loop siempre corriendo" de la
primera pasada ya no aplica. El pico breve durante la reproducción
esporádica del idle periódico (~2.3%, ~3s cada 5 minutos por defecto)
no se declara sujeto a este mismo presupuesto de "reposo", ya que por
definición no es reposo — se documenta como el costo real y aceptado
de reproducir una animación de 25 frames a canvas 156×160, sin
optimizar más allá de lo que DEC-045/DEC-046 ya hicieron (AGENTS.md:
"Optimize only where justified by evidence").

## Mediciones reales de Block 04.2, tercera pasada (click-fire real importado)

Mismo método (Release, arm64 nativo, `ps -o rss,%cpu`, procesos
separados por escenario, `SIGTERM` limpio al final), tras importar el
click-fire real de 25 frames (DEC-048) y corregir el bug de cobertura
de texturas (DEC-049) — ver `docs/NIDIR_CONTENT.md` §6.

| Escenario | Cómo se disparó | CPU | RSS (steady state) |
|---|---|---|---|
| **(A) Base estática** | `NIMVLETS_DEV_SWITCH_TEST_COUNT=2`, sin overrides adicionales, 15s de muestreo | **0.0%** | **≈156.0–156.5 MB** |
| **(B) Idle periódico activo** | ídem + `NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS=15`, 30s de muestreo | pico **≈2.3%** durante la reproducción (~3s), 0.0% el resto | ≈156.0–156.5 MB, plano en los ciclos observados (sin cambio vs. la segunda pasada — el click-fire no afecta este escenario) |
| **(C) Click-fire activo** | `NIMVLETS_DEV_CLICK_TEST_COUNT=1`, muestreo cada 0.5s desde el arranque | pico inicial ≈14.7% (dominado por arranque: creación de ventana + adjuntar ~102 texturas del pack completo, no por el click en sí), luego **≈1.8–4.3%** durante los ~3s de reproducción real, cae a 0.0% al terminar | ≈155.9–156.4 MB, plano tras el arranque |
| Ráfaga de 500 clicks sintéticos consecutivos | `NIMVLETS_DEV_CLICK_TEST_COUNT=500` | 0.0% tras el arranque (coalescidos, ver nota) | ≈156.4 MB, sin crecimiento — confirma que disparar `TriggerClick()` repetidamente no acumula recursos (no se crea ninguna textura nueva por click; `AttachAllTextures()` ya adjuntó todo una sola vez al cargar el pet) |
| Bunny, sin switch (regresión) | proceso separado, sin overrides | 0.0% | ≈73.6 MB — idéntico al baseline histórico |

**Nota sobre el escenario (C):** a diferencia de la segunda pasada
(placeholder de ~100ms, imposible de medir de forma aislada con
`ps`), el click-fire real dura ~3s — suficiente para un muestreo
significativo por primera vez. El pico observado (~1.8–4.3%, excluido
el primer valor dominado por arranque) es del mismo orden que el pico
de (B) (~2.3%), consistente con lo esperado: ambas animaciones
comparten el mismo camino de código de render/hit-mask por frame
(`docs/ANIMATION_RUNTIME.md` §3). "Ráfaga de 500 clicks" no ejercita
500 reproducciones reales secuenciales (todas se disparan antes de que
arranque el loop principal con el mismo timestamp, así que se
coalescen en la última) — mide "disparar `TriggerClick()`
repetidamente no acumula recursos", no "reproducir la animación 500
veces seguidas no acumula recursos"; esto último no es sintetizable
sin entrada de mouse real en este entorno, pero es un bajo riesgo
arquitectónico: cada reproducción reutiliza los mismos punteros de
textura ya adjuntados una vez al cargar el pet, sin ninguna asignación
nueva por click.

**RSS estático subió de ~127MB (segunda pasada) a ~156MB (tercera
pasada)** — 50 frames reales de click-fire (25 por dirección) ahora
correctamente resididos en memoria (antes del fix de DEC-049 estos
frames técnicamente ocupaban memoria pero se renderizaban rotos/
invisibles en dirección no canónica — el fix es estrictamente
necesario, el costo de RSS es la consecuencia correcta de tener
contenido real y funcional, no una regresión evitable). Investigado
si el incremento es evitable (block brief: "check for avoidable
residency... duplicated texture/mask data, oversized surfaces"):

- **Frames sobredimensionados:** NO — los frames de click-fire (nativo
  624×612) están acotados al mismo límite genérico de 320px por lado
  que idle (`runtime_max_frame_dimension`, DEC-046), confirmado
  (320×314 tras el downscale).
- **Datos duplicados:** NO — a diferencia del placeholder anterior
  (que reutilizaba los bytes de `idle/frame_000`), el click-fire real
  es contenido genuinamente distinto por frame; el formato del pack
  nunca compartía bytes entre animaciones de todos modos (cada
  entrada de frame se compila con su propia copia), así que esto no
  introduce ninguna duplicación nueva.
- **Ambas direcciones residentes simultáneamente, aunque solo una se
  muestra a la vez:** SÍ — esto es real y evitable en principio (ver
  DEC-050), estimado en ~15MB de ahorro potencial para Nidir si se
  implementara carga/descarga de texturas por dirección bajo demanda.
  Deliberadamente NO implementado en esta pasada: la complejidad/riesgo
  (manejar una animación en reproducción activa durante un cambio de
  dirección, entre otros casos borde) es desproporcionada frente al
  ahorro para una pasada de corrección puntual — documentado como
  oportunidad real para un bloque futuro, no como omisión sin
  examinar.

Con esto, el RSS estático de Nidir (~156MB) sigue por encima del
objetivo de "< 100MB" de la tabla de presupuestos — igual que ya
ocurría en la segunda pasada (~127MB) — documentado como limitación
real conocida, no oculta ni forzada a bajar artificialmente
("Do not pick a resize merely to improve metrics").

## Mediciones reales de Block 04.3 (canvas de trabajo compartido)

Re-medido con el mismo método (Release, arm64 nativo, `ps -o rss,%cpu`,
procesos separados por escenario) tras la política de canvas de
trabajo compartido/anclado por contenido (DEC-051) y la dirección
automática por mitad de pantalla (DEC-052) — ver
`docs/NIDIR_CONTENT.md` §12/§13. El canvas lógico de Nidir cambió de
156×160 a 160×157 (área prácticamente idéntica: 24,960 vs. 25,120
puntos²), así que no se esperaba ningún cambio de recursos real, y
eso fue justo lo que se confirmó:

| Escenario | CPU | RSS (steady state) |
|---|---|---|
| Base estática | 0.0% | ≈156.1–156.4 MB (sin cambio vs. la tercera pasada de Block 04.2, ~156.0–156.5 MB) |
| Idle periódico activo (forzado c/15s vía override DEV) | pico ≈2.2–2.4% durante la reproducción (~3s) en la mayoría de las corridas; una corrida aislada mostró un pico de ~6.9% no reproducido en una repetición inmediata — tratado como ruido de medición del entorno, no una regresión real (ver "Methodology rules": ninguna conclusión de una sola muestra) | ≈156.0–156.7 MB, plano |
| Click-fire activo | pico inicial ≈19.8% dominado por arranque (creación de ventana + adjuntar texturas), luego ≈1.8–3.9% durante los ~3s de reproducción real, cae a 0.0% al terminar | ≈156.4–156.5 MB, plano |
| Bunny (regresión) | 0.0% | ≈73.6–73.7 MB — idéntico al baseline histórico |

Sin cambios de presupuesto respecto a la tercera pasada de Block 04.2
— el canvas de trabajo compartido reorganiza CÓMO se distribuyen los
pixeles dentro del mismo límite de `runtime_max_frame_dimension`
(320px), no cuántos bytes totales terminan en el pack compilado ni en
RSS; el tamaño del pack compilado se mantuvo en el mismo orden de
magnitud (~41.0MB, un cambio de +0.16% frente a los ~40.9MB de la
tercera pasada de Block 04.2, dentro del ruido de redondeo de
downscale por frame). La política de dirección automática
(`UpdateDirectionFromWindowPosition()`) no agrega ningún polling —
se dispara solo ante eventos reales (`SDL_EVENT_WINDOW_MOVED`) o una
vez en `Init()`/tras un switch, así que no se esperaba ni se midió
ningún impacto de CPU en reposo, confirmado por el 0.0% de la fila
"Base estática" de arriba.
