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

## Mediciones reales de Block 04.3, corrección post-QA (doble-present, +5% tamaño, Bunny real)

Re-medido tras: el fix de doble-present/redraw de confirmación
(DEC-054), el candidato de tamaño global +5% (DEC-055), y la migración
de Bunny a assets reales (DEC-056). Mismo método (Release, arm64
nativo, `ps -o rss,%cpu`), lanzamientos DIRECTOS al pet en cuestión
(vía un `state.nvstate` fabricado) para evitar que un switch previo
contamine la medición del pet en reposo (ver la nota de RSS-tras-switch
más abajo):

| Pet | Escenario | CPU | RSS (steady state) |
|---|---|---|---|
| Nidir | Base estática | 0.0% | ≈155.7 MB |
| Nidir | Idle periódico activo | pico ≈2.6% | ≈155.7 MB, plano |
| Nidir | Click-fire activo | pico ≈2.2–3.5% (excluido el primer sample, dominado por arranque) | ≈155.8 MB, plano |
| Bunny (real) | Base estática | 0.0% | ≈138.0 MB |
| Bunny (real) | Idle periódico activo | pico ≈2.5% | ≈137.5–138.0 MB, plano |
| Bunny (real) | Click activo | pico ≈2.1–2.9% (excluido el primer sample) | ≈138.0 MB, plano |

**Sin cambio material para Nidir** respecto a la medición anterior de
esta misma sección (~156MB estático, picos ~2-4% durante reproducción)
-- ni el fix de doble-present (solo agrega presents, sin costo de
memoria) ni el candidato +5% (el canvas de trabajo y el límite de
`runtime_max_frame_dimension` no cambian, solo el tamaño LÓGICO en
puntos) tenían por qué moverlo, y no lo hicieron.

**Bunny real (~138MB estático) reemplaza al fixture sintético anterior
(~73-74MB)** -- un aumento esperado y proporcional: el fixture
sintético tenía 7 frames derivados de una sola imagen; el contenido
real trae 50 frames genuinamente distintos (25 idle + 25 click, cada
uno con su propia dirección espejada) en dos animaciones completas,
mucho más comparable en volumen al de Nidir que al fixture anterior.

**Hallazgo honesto sobre RSS post-switch:** un lanzamiento DIRECTO a
Nidir mide ≈155.7MB, pero Nidir medido DESPUÉS de haber cargado a
Bunny al menos una vez en la misma corrida (p. ej. vía
`NIMVLETS_DEV_SWITCH_TEST_COUNT=2`, Bunny -> Nidir) mide ≈188-192MB —
una diferencia real de ~32-36MB que NO aparece en un lanzamiento
directo. Investigado antes de descartarlo como ruido: una ráfaga de
500 switches automatizados (`NIMVLETS_DEV_SWITCH_TEST_COUNT=500`,
terminando en Nidir) se estabiliza en ≈191.9MB -- prácticamente
idéntico a la medición de solo 2 switches (≈188.4MB), NO escalando
con la cantidad de switches. Esto descarta un leak real por switch
(que sí escalaría con la cantidad de repeticiones) y es consistente
con un comportamiento benigno y bien conocido de los allocators de
memoria (macOS `libmalloc` incluido): páginas de heap liberadas tras
soltar el pack más grande de Bunny (31.7MB, ~100 texturas reales)
pueden quedar retenidas/reservadas por el allocator en vez de
devolverse inmediatamente al sistema operativo, apareciendo como RSS
"elevado" aunque la memoria esté genuinamente libre y sea reutilizable
por asignaciones futuras del mismo proceso. Documentado honestamente
como la explicación mejor sustentada por la evidencia disponible (un
offset constante, no creciente, tras el primer switch con contenido
grande) -- no confirmado con una herramienta de profiling de memoria
dedicada, que este entorno no tiene disponible.

## Mediciones reales de Block 05 (Frin, 15s ambient, escala visual, +2 acciones ambient por pet)

Mismo método (Release, arm64 nativo, `ps -o rss,%cpu,time`, lanzamientos
DIRECTOS vía `NIMVLETS_DEV_SELECT_PET` + `NIMVLETS_DEV_APPDATA_DIR`
aislado para evitar el efecto post-switch de arriba), 4 pets en
paralelo, muestreados cada ~10-20s hasta estabilizar:

| Pet | RSS estático estable | CPU estático estable | Pack compilado (bytes en disco, proxy del lado CPU) |
|---|---|---|---|
| Bunny | ≈213 MB | 0.0% | 43.0 MB |
| Nidir | ≈195–196 MB | 0.0% | 55.3 MB |
| Frin (macho) | ≈209–210 MB | 0.0% | 59.5 MB |
| Frin (hembra) | ≈219 MB | 0.0% | 65.9 MB |

**CPU sigue en 0.0% exacto en reposo estático** en los 4 (el `TIME`
acumulado de `ps` no crece entre muestras separadas por ~10-20s reales)
— el mecanismo event-driven de `docs/ANIMATION_RUNTIME.md` §6 sigue
intacto; ninguno de los cambios de este bloque (grafo de estados,
cooldown de hover independiente, redraw de confirmación generalizado)
introdujo ningún polling nuevo.

**RSS crece de forma real y sustancial frente a Block 04.3** (Bunny
138MB -> 213MB, Nidir 156MB -> 196MB) — dominado por contenido nuevo
genuino (Bunny/Nidir ya tenían su segunda acción ambient desde la
corrección post-QA de Block 04.3, pero ESTE bloque no le agregó
contenido nuevo a ninguno de los dos; la comparación exacta contra esa
medición específica no se puede reconstruir con precisión sin volver a
compilar esa versión exacta del pack) y por Frin siendo, junto con
Nidir, de los packs más grandes hasta ahora (4 animaciones × 25 frames
× 2 direcciones cada una). **Los 4 pets exceden claramente el budget
de <100MB** de la tabla de arriba — un hecho ya conocido de antes de
este bloque (ver el brief), no una regresión introducida acá.

**Causa raíz, cuantificada esta vez:** `content::FrameDefinition::pixels`
(el RGBA8 crudo, lado CPU) permanece residente en memoria de forma
PERMANENTE mientras un pet está activo — no solo durante la carga —
porque `core::AlphaMask::FromAlphaChannel()` lo necesita en cada cambio
de frame para reconstruir el hit-mask de click-through
(`SpikeApp::ApplyCurrentHitMask()`). Al mismo tiempo,
`graphics::AttachFrameTexture()` sube ESOS MISMOS pixeles a una
`SDL_Texture` (lado GPU) para cada frame, y `AttachAllTextures()`
adjunta TODAS las colecciones de TODOS los estados/acciones/direcciones
de un pet por adelantado, nunca bajo demanda. El tamaño del pack
compilado en disco (columna de la derecha, arriba) es un proxy directo
y medido del volumen de datos CPU-side resident — la tabla completa de
Nidir (§8 de `docs/NIDIR_CONTENT.md`) ya documentaba la mitad de esto
("mantener AMBAS direcciones de TODAS las animaciones residentes...
ahorraría del orden de 15MB") sin implementarlo; este bloque confirma,
con las 4 mediciones de arriba, que el patrón es el mismo para los 4
pets y que la duplicación CPU+GPU (no solo la de dirección) es
probablemente el término dominante — el tamaño de pack de Bunny
(43.0MB) más su textura GPU equivalente (misma cantidad de pixeles,
formato RGBA32) más el overhead base del proceso SDL (~15-20MB medido
en corridas previas) da un total del orden de magnitud correcto para
explicar ≈213MB sin necesitar ningún otro factor.

**Optimización concreta, NO implementada en este bloque** (por qué:
implementarla bien exige decidir un mecanismo de invalidación de caché
para el hit-mask y/o carga perezosa de texturas por dirección/estado,
un cambio de arquitectura real con superficie de riesgo (una animación
en reproducción cuando cambia la dirección activa, o cuando el estado
activo cambia y las texturas de un estado recién-inactivo deberían
liberarse) desproporcionada frente al resto del alcance de este
bloque, exactamente el caso que el brief permite documentar en vez de
implementar):

1. Precalcular y cachear el `core::AlphaMask` de cada frame UNA VEZ al
   cargar el pack (en vez de reconstruirlo desde `pixels` en cada
   cambio de frame) y liberar `FrameDefinition::pixels` inmediatamente
   después de adjuntar la textura GPU + precalcular esa máscara — el
   hit-mask es much más chico que el RGBA crudo (un booleano por celda
   del canvas lógico, no 4 bytes por pixel nativo), así que esto
   eliminaría la copia CPU-side casi por completo sin perder ninguna
   funcionalidad.
2. ~~Cargar bajo demanda (y liberar) las texturas de la dirección/estado
   NO activos~~ — **IMPLEMENTADO en la pasada de estabilización de
   Block 05, ver DEC-081 y la tabla de abajo.** El resultado fue más
   fuerte que lo propuesto acá: en vez de carga perezosa por
   dirección/estado, el runtime pasó a UNA sola textura reutilizable
   por pet (`graphics::ActiveFrameTexture`), actualizada en el lugar.

Cualquiera de las dos, implementada con cuidado, reduciría RSS de
forma sustancial sin sacrificar corrección visual — pero ninguna es
segura de implementar apurado dentro de este bloque, así que quedan
documentadas como el próximo paso concreto en vez de intentadas a
medias.

## Mediciones reales de Block 05, tercera pasada de corrección post-QA (transforma canónica por-estado)

**Tamaño de pack compilado (dato preciso y reproducible, git-comparable
byte a byte -- no sujeto al ruido de un muestreo de RSS):**

| Pet | Antes de esta pasada | Después | Delta |
|---|---|---|---|
| Bunny | 47 283 395 bytes | 47 283 395 bytes | 0 (mismas dimensiones compiladas; bytes SÍ cambian -- confirmado con `cmp` -- por el resize de una sola pasada, ver `docs/BUNNY_CONTENT.md` §13, pero NVPACK2 almacena pixeles crudos sin comprimir, así que el tamaño total depende solo de las dimensiones de frame, no de sus valores) |
| Nidir | 61 097 185 bytes | 61 097 185 bytes | 0 (mismo razonamiento -- dimensiones sin cambio) |
| Frin (macho) | 59 542 532 bytes | 55 625 732 bytes | **-3 916 800 bytes (-6.6%)** -- canvas de trabajo compartido 689×968 -> 543×815 (ya no infla `lying`/`lie_to_sit` artificialmente, ver DEC-075) |
| Frin (hembra) | 65 287 174 bytes | 63 459 334 bytes | **-1 827 840 bytes (-2.8%)** -- canvas 534×683 -> 496×653 |

Reducción real y medida para Frin -- consecuencia directa de la
corrección geométrica, no un objetivo de optimización de esta pasada.
`visual_scale` (nuevo, más alto: 1.25 Nidir / 1.30 Frin) NO afecta este
tamaño -- es puramente un multiplicador de render-time, nunca toca los
bytes compilados (ver `docs/ANIMATION_RUNTIME.md` §11).

**RSS -- observación liviana, NO una re-medición con el mismo rigor que
la tabla de arriba:** un chequeo rápido (Release, arm64, 4 pets en
paralelo con `NIMVLETS_DEV_APPDATA_DIR` aislado, muestreado a los
~15s/35s/50s) mostró RSS bajando de forma monótona en los 4 (p. ej.
Bunny 150 -> 114 -> 76 MB) en vez de estabilizarse en una meseta clara
dentro de esa ventana -- consistente con macOS purgando páginas
inactivas del proceso con el tiempo, no necesariamente comparable
punto a punto con la meseta ~200s+ que documentó la tabla de arriba.
Ningún pet mostró CPU sostenido en reposo (%CPU volvía a 0.0 entre
redraws) -- el mecanismo event-driven sigue intacto. No se re-hizo la
medición completa con el protocolo original (muestreo hasta
estabilizar de verdad) -- ver limitaciones del informe de este bloque.


## Mediciones reales: una textura reutilizable vs. una por frame (Block 05, pasada de estabilización)

A/B con los DOS caminos compilados en el mismo binario (selector
temporal `NIMVLETS_DEV_RENDERER`, retirado al adoptar). Release, arm64
nativo, `NIMVLETS_DEV_APPDATA_DIR` aislado, RSS leído con `ps -o rss=`
a los ~12s de arranque estable, sin interacción:

| Pet | una textura por frame | una textura reutilizable | reducción |
|---|---|---|---|
| Bunny | 217.9 MB | 166.3 MB | **-23.7%** (-50.4 MB) |
| Nidir | 258.1 MB | 194.5 MB | **-24.6%** (-62.1 MB) |
| Frin (macho) | 252.8 MB | 183.4 MB | **-27.4%** (-67.7 MB) |
| Frin (hembra) | 267.1 MB | 197.7 MB | **-26.0%** (-67.8 MB) |

Esto retira el término GPU de la duplicación CPU+GPU que la sección
anterior identificó como dominante. **La copia CPU-side sigue viva** —
`FrameDefinition::pixels` permanece residente porque
`core::AlphaMask::FromAlphaChannel()` lo necesita en cada cambio de
frame para el hit-mask de click-through, así que la optimización #1 de
la lista de arriba (precalcular/cachear la máscara y liberar `pixels`)
sigue pendiente y sigue siendo la siguiente ganancia grande.

**Invariante de recursos verificado** contra el binario real: 50
redraws (5 clicks + 6 cambios de dirección + animaciones) crean UNA
textura; 4 switches de pet crean 2 en total (creación perezosa — un pet
que nunca se dibuja nunca reserva textura). Ningún crecimiento por
ciclo de animación/dirección.

Los 4 pets siguen excediendo el budget de <100MB — sin cambio de
conclusión respecto de la sección anterior, solo ~50-68 MB más cerca.

## Mediciones reales: click-through con muestreo condicional (Block 05, pasada de estabilización)

Release, arm64 nativo, `NIMVLETS_DEV_APPDATA_DIR` aislado, sin
interacción humana durante la ventana de medición.

**Metodología** (importa, porque el arranque de este binario NO es
despreciable — cargar y decodificar un pack de 47-78 MB domina los
primeros segundos): se muestrea el CPU acumulado del proceso
(`ps -o time=`, resolución 10 ms) en T1 y en T2 y se toma el DELTA, así
el arranque se cancela en vez de contaminar la cifra de reposo. Nunca
se usa `%CPU` de `ps` (es un promedio decreciente sobre toda la vida
del proceso, no la tasa instantánea).

| Escenario | Ventana | CPU en régimen | RSS |
|---|---|---|---|
| Nidir, reposo, cursor FUERA de la ventana (sin muestreo) | 120 s | **0.06%** | 111 MB |
| Nidir, reposo, cursor DENTRO (muestreo armado) | 120 s | 0.03% | 97 MB |
| Nidir, animando (ambient cada 1 s) | 30 s | **3.00%** | 135 MB |
| Bunny, reposo | 40 s | 0.03% | 162 MB |
| Bunny, animando (self-loop, ambient cada 1 s) | 40 s | **1.80%** | 119 MB |
| Frin macho, reposo SENTADO | 40 s | 0.03% | 206 MB |
| Frin macho, reposo ACOSTADO (estado terminal, sin timer) | 40 s | 0.03% | 74 MB |

### Lo que estas cifras SÍ dicen

**En reposo el costo de click-through es cero medible.** Las filas de
reposo están todas en 0.03-0.06%, que es el piso de esta medición (1-2
ticks de 10 ms sobre 40-120 s). No se distinguen entre sí, y no se
distinguen del costo base del loop (un despertar por segundo, acotado
por `kMaxWaitMs` para que un SIGINT se note dentro de ~1 s).

**Frin acostado es genuinamente gratis.** `lying` no define
`ambient_actions`, así que nunca hay timer armado: el pet se queda
quieto indefinidamente sin despertar el loop por comportamiento. Eso
también explica por qué no hay una fila de "Frin animando" — con el
intervalo ambient acelerado, Frin se acuesta una vez y se queda ahí; su
animación no cicla como la de Bunny/Nidir.

### Cuánto costaba el muestreo permanente (cota determinista)

La comparación "cursor dentro vs. fuera" de arriba NO resuelve el costo
del muestreo: las dos filas están en el piso, y el cursor físico se
movió durante la ventana larga (no es controlable desde acá). Así que
se midió aparte, con un programa determinista que hace **exactamente**
lo que `SpikeApp::PollHover()` hace por muestra
(`SDL_GetGlobalMouseState` + `SDL_GetWindowPosition` + un lookup en un
hit-mask del mismo tamaño), sin depender de dónde esté el cursor:

```
4964 muestras en 100 s (49.6 Hz)   CPU: 0.09 s -> 0.27 s  =  0.18 s / 90 s
```

**≈0.20% de CPU por un muestreo continuo a ~50 Hz.**

Esa es la cifra que el diseño anterior pagaba el **100% del tiempo**,
porque encuestaba siempre. Con la política de
`core::EvaluateClickThrough()` (Block 05, DEC-086) ese costo se paga
solo mientras el cursor está DENTRO del rectángulo de la ventana del
pet — una región chica y una fracción chica del tiempo — y es
exactamente cero en cualquier otra parte de la pantalla. No es una
optimización de CPU pico; es eliminar un loop de despertares
permanente, que es lo que AGENTS.md §2 pide ("event-driven scheduling.
Do not run a permanent 60/144 FPS game loop when nothing is changing on
screen").

### Cadencia que queda

`hoverScheduler_` a 60 Hz (medido ~50 Hz real, por el redondeo de
`SDL_Delay`), armado **solo** con el cursor dentro del rectángulo de la
ventana. Es el mínimo inherente al mecanismo: mientras el click-through
está ACTIVO la ventana no recibe eventos de mouse, así que detectar el
regreso del cursor no puede ser event-driven — ver DEC-086 para por qué
esto no es una elección de este diseño sino una propiedad de
`NSWindow.ignoresMouseEvents` (la propia ruta de shape de SDL tiene la
misma forma).

### Honestidad sobre el RSS

Las cifras de RSS de la tabla varían bastante entre corridas del MISMO
escenario (Nidir 97-135 MB, Bunny 119-162 MB, Frin 74-206 MB) porque
dependen de cuántos frames decodificados se tocaron antes del muestreo
— un pack se carga entero pero sus páginas se residencian a medida que
se dibujan. **No leer estos números como un ranking entre pets.** El
término dominante sigue siendo el mismo que identificó la sección
anterior: `FrameDefinition::pixels` queda residente porque
`core::AlphaMask::FromAlphaChannel()` lo necesita en cada cambio de
frame. Precalcular/cachear la máscara y liberar `pixels` sigue siendo
la siguiente ganancia grande de memoria, y sigue pendiente — esta
pasada no la abordó (estaba fuera de alcance).

## Mediciones reales de Block 06 (Product UI + menú rápido)

macOS Release, Apple Silicon nativo, una sola máquina. Muestras chicas
(`top -l 4/5 -s 1`, ventanas de 1 s) — **NO son presupuestos finales**
(esta metodología no establece presupuestos desde VMs ni desde muestras
de 1 s — misma honestidad que el resto de este documento). Se sanea el
directorio de app-data (`NIMVLETS_DEV_APPDATA_DIR`) para no tocar el
estado real del owner.

| Escenario | Idle CPU | RSS |
|---|---|---|
| Pet-only en reposo (Bunny, tras ~12–16 s de asentarse) | **≈0.0%** | ≈120–166 MB |
| Product UI ABIERTO, en reposo (pet oculto, tras ~12–17 s) | **≈0–1%** (muestras de 1 s, ruido) | ≈103–112 MB |
| Pet-only en reposo TRAS 3 ciclos open/close de la Collection | **≈0.0%** | ≈165–166 MB |

### Lo que estas cifras SÍ dicen

- **No hay loop de render oculto del Product UI** (brief §19). Con la
  Collection abierta y sin interacción, el CPU es ≈0%:
  `ProductWindow::RenderIfNeeded()` — llamada una vez por vuelta del
  event loop del pet — es un no-op salvo que la vista esté `dirty_` o
  haya un `EXPOSED` pendiente. El cálculo de `waitMs` del event loop
  del pet NO se toca: no se agrega ningún término de deadline para la
  ventana de producto (ver DEC-112). Los eventos de SDL despiertan el
  loop de todos modos.
- **Cerrar la Collection no deja residuo.** Tras 3 ciclos open/close, el
  pet-only idle vuelve a ≈0% de CPU y el RSS queda en la misma banda
  que un arranque pet-only fresco (≈165 vs ≈164–166 MB) — sin
  acumulación. `ProductWindow::Close()` destruye el `SDL_Renderer`, sus
  texturas y los caches (`TextCache`/`PetPreviewCache`).
- Verificado además contra el binario con `NIMVLETS_DEV_COLLECTION_CYCLES=8`:
  8 pares open/close seguidos, pet activo tras todos, renderer del pet
  vivo, shutdown limpio.

### Costo transitorio conocido

Abrir la Collection carga el pack del pet **poseído-inactivo** (Frin,
~72 MB en disco) para extraer su frame de preview; el `PetDefinition`
se descarta de inmediato, solo queda una textura chica. Es un pico
transitorio pagado una vez por apertura de la Collection (o una vez por
ciclo en la prueba de ciclos — de ahí que cada ciclo tarde ~1.3 s). Con
muchos pets poseídos esto escalaría; un thumbnail precompilado o un
loader en background sería el fix — se registra como limitación
conocida (DEC-113), no se abordó en este bloque.

### Honestidad sobre el RSS

El RSS varía run a run entre ~103 y ~166 MB, dominado por asignaciones
del driver SDL/Metal y por `FrameDefinition::pixels` residente (el
término que la sección de Block 05 identificó y que sigue pendiente).
No es una regresión de Block 06: la ventana de producto en reposo mide
*menos* (≈103–112 MB, pet oculto) que el pet-only (el pet oculto no
tiene su textura de frame residente). Precalcular/liberar la máscara de
alpha sigue siendo la siguiente ganancia grande de memoria y sigue
fuera de alcance.

## Mediciones reales de Block 06.1 (hero + gallery, arte de locked, localización)

macOS Release, Apple Silicon nativo, una máquina, muestras chicas
(`top -l 5 -s 1`) — **NO presupuestos finales**, misma honestidad que
el resto del documento. Directorio de app-data aislado.

| Escenario | Idle CPU | RSS |
|---|---|---|
| Pet-only en reposo (Bunny), asentado | **≈0–1%** (ruido de 1 s) | ≈121 MB |
| Collection ABIERTA, en reposo (pet oculto) | **≈0.0%** | ≈206 MB |
| Pet-only en reposo tras 2 / 6 / 14 ciclos open/close de la Collection | **≈0.0%** | ≈332 / 279 / 227 MB |

### Lo que estas cifras SÍ dicen

- **No hay loop de render oculto** (invariante de DEC-112, sin cambios):
  Collection abierta y sin interacción → CPU ≈0%. La composición
  hero + gallery no agrega ningún deadline de render; el micro-lift de
  hover es un cambio de estado instantáneo (DEC-118), no un tween.
- **No hay leak.** El RSS tras 2/6/14 ciclos open/close **no crece con
  la cantidad de ciclos** — de hecho el run de 14 midió *menos* (227 MB)
  que el de 2 (332 MB). La dispersión es ruido del allocator/driver,
  no acumulación. `ProductWindow::Close()` sigue liberando el renderer,
  todas las texturas y los dos caches (verificado por
  `NIMVLETS_DEV_COLLECTION_CYCLES`; el pet y su renderer quedan vivos).

### El costo transitorio subió respecto de Block 06

Block 06 cargaba UN pack de preview (Frin, el único poseído-inactivo).
Block 06.1 muestra también el arte de los pets **locked** (más callado
— brief §12), así que al abrir la Collection se cargan **dos** packs
(Nidir ~61 MB + Frin ~72 MB) para extraer sus frames. Los
`PetDefinition` se descartan de inmediato (solo quedan dos texturas
chicas), pero el high-water mark de RSS de los buffers de decodificación
transitorios no vuelve del todo al allocator — de ahí que la banda
absoluta de RSS con la Collection abierta (~200–330 MB) esté por encima
de la de Block 06 (~103–166 MB). Es un costo pagado una vez por apertura,
liberado en `Close()`, y sigue siendo la misma limitación conocida
(DEC-113/DEC-117): un thumbnail precompilado o un loader en background
lo resolvería. El término dominante de fondo (`FrameDefinition::pixels`
residente por `core::AlphaMask`) sigue pendiente, igual que desde Block
05.

## Mediciones reales de Block 06.2 (previews livianas `.nvprev`, nitidez Retina)

macOS Release, Apple Silicon nativo, una máquina, muestras chicas,
directorio de app-data aislado. Block 06.2 **elimina** la carga de
packs de animación para las previews (DEC-119): al abrir la Collection
solo se leen los 4 artefactos `.nvprev` (~1,44 MB en total) y se suben
como texturas chicas.

| Escenario | Idle CPU | RSS |
|---|---|---|
| Pet-only en reposo (Bunny), asentado | **≈0.0%** | ≈166 MB |
| Collection ABIERTA, en reposo (pet oculto) | **≈0.0%** | **≈180 MB** *(06.1: ~206–330)* |
| `LoadBundle` (4 `.nvprev`) al abrir | — | +1,37 MB de textura |
| 20 ciclos open/close de la Collection | **≈0.0%** tras terminar | RSS asienta **≈128 MB** (por debajo del baseline pet-only) |
| Cambio de variante Frin Macho ⇆ Hembra | — | *fetch = lookup en un mapa, sub-ms* (+ redibujo completo ~8–10 ms) |

### Antes / después del cambio de variante Frin

Con packs completos (06.1), el `Acquire` de la variante destino era, en
frío/tibio: lectura de 72–76 MB **45–100 ms**, parseo (decodificar
todos los frames) 7–12 ms, en el hilo de render. Con `.nvprev` (06.2) la
preview de las 4 variantes ya está residente tras `LoadBundle`, así que
`Get(petId, variantId)` es un lookup en `std::unordered_map` sobre una
`SDL_Texture*` — el costo observable del "cambio" es solo el redibujo
event-driven de la Collection (~8–10 ms, dominado por rasterizar el
texto que cambió y los spans de las primitivas del hero stage).

### Lo que estas cifras SÍ dicen

- **El RSS con la Collection abierta bajó ~145 MB** (≈324 → ≈180). El
  incremento sobre el baseline pet-only (~166 MB) es la ventana +
  renderer + 1,37 MB de previews + cache de texto + framebuffers de GPU.
- **No hay leak.** 20 ciclos open/close: `LoadBundle` corre 20 veces
  (log), y el RSS asienta ~128 MB — *por debajo* del baseline pet-only,
  no acumula. `Close()` sigue liberando renderer + texturas + caches.
- **No hay loop de render oculto** (DEC-112 intacto): CPU ≈0% con la
  Collection abierta o cerrada; `RenderIfNeeded` sigue siendo no-op
  salvo `dirty_`/`EXPOSED`; ningún deadline nuevo en el `waitMs` del
  pet.
- El `"Use <pet>"` real todavía carga el pack de runtime completo
  (~76 MB para Frin hembra) de forma transaccional al pet activo — eso
  es correcto y esperado (selección de preview ≠ activar el pet).
- El término dominante de fondo (`FrameDefinition::pixels` del pet
  activo residente para `core::AlphaMask`) sigue pendiente desde Block
  05 — no lo toca este bloque.
