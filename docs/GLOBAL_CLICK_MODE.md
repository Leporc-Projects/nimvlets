# Nimvlets — Modo de conteo de clics global (Block 11A)

Este documento describe la feature **opt-in** que deja contar
pulsaciones del botón primario en cualquier parte del sistema, no solo
sobre el Nimvlet. Es la feature que `AGENTS.md` §14 y
`docs/PRIVACY_SECURITY.md` §B mantuvieron **explícitamente prohibida**
desde Block 01 hasta que un brief la autorizara. El brief de Block 11A
la autoriza; **todas las restricciones de privacidad siguen vigentes** y
este bloque las hace más específicas, no más laxas. Ver DEC-139.

## 1. Qué es, en una línea

Una preferencia de **Settings** —y solo de Settings— que cambia *dónde*
tiene que ocurrir un clic para que sume al wallet:

```
Click counting        [ Nimvlet only ]   [ Anywhere ]
```

Default de producto y de migración: **Nimvlet only**. Un usuario
existente sigue exactamente igual tras actualizar, y la app **nunca pide
un permiso de input por el solo hecho de arrancar**.

## 2. Qué se observa, y qué NO

Lo único que el monitor nativo reporta es:

> ocurrió una presión del botón primario

Y lo único que la app hace con eso es **sumar 1 al balance de clics**.

La firma del callback lo hace cumplir estructuralmente, no por
convención (`src/platform/GlobalClickMonitor.h`):

```cpp
using GlobalPrimaryClickCallback = void (*)(void* userData);
```

No hay dónde poner una coordenada. **No se puede filtrar lo que no se
puede transportar.**

NUNCA se observa, lee, transmite ni persiste:

| No se observa | |
|---|---|
| teclado | pixeles de pantalla / capturas |
| contenido o títulos de ventanas | aplicación activa o enfocada |
| nombres de procesos/apps | URLs, texto, portapapeles |
| coordenadas del puntero | timestamps de clics |
| trayectorias | historial de clics |
| estadísticas por app | contenido de la rueda de scroll |

El botón derecho y el medio se **ignoran** — no "se observan y
descartan": ni siquiera están en la máscara del tap (§5).

Sin telemetría, sin red, sin nube, sin cuenta, sin permiso de Screen
Recording, sin Accessibility.

## 3. Semántica exacta de conteo

El modo global cuenta **pulsaciones del botón PRIMARIO al bajar**
(`mouse down`). Una pulsación física = +1.

| Gesto | Suma |
|---|---|
| un clic primario normal | **+1** |
| un doble clic | **+2** (son dos pulsaciones) |
| empezar un arrastre | **+1** |
| clic derecho | 0 |
| clic medio | 0 |
| scroll | 0 |
| teclado | 0 |

**Que un arrastre cuente una sola vez es INTENCIONAL, y la razón es de
privacidad.** Distinguir "clic" de "arrastre" exige mirar el movimiento
del puntero entre el `down` y el `up` — es decir, coordenadas. Nimvlets
se niega a inspeccionar o guardar trayectorias del puntero solo para
recrear en modo global la clasificación de gestos que sí hace en local
(donde el dato ya es suyo: son eventos de su propia ventana). Se
prefiere una semántica ligeramente distinta y honesta antes que un
seguimiento de puntero que el producto promete no hacer.

En modo local la semántica histórica no cambia: un arrastre del pet **no
cuenta**, un clic sí (`core::DragClassifier`).

## 4. Modo PEDIDO vs modo EFECTIVO

Son dos cosas distintas, y separarlas es lo que evita el peor estado
posible ("los clics dejaron de contar y nadie me avisó").

```
PEDIDO   (persistido)   NimvletOnly | Anywhere
EFECTIVO (derivado)     Local       | Global
```

`core::ResolveEffectiveClickCounting(requested, monitorActive)`:
efectivo es **Global** si y solo si el owner pidió `Anywhere` **Y** el
monitor está **realmente corriendo**. `monitorActive` es un hecho
consultado al adapter nativo (`IsActive()`), nunca una deducción de la
preferencia.

Si `Anywhere` no puede activarse —permiso denegado, permiso revocado,
backend ausente, fallo de arranque— el modo efectivo cae a **Local**:
los clics sobre el Nimvlet siguen contando. **Nunca se descarta un clic
en silencio, y nunca se finge que el modo global está activo.**

### La preferencia persistida NO se auto-degrada

Decisión de producto (el brief pedía elegir y documentar): cuando el
owner confirma `Anywhere` con "Continue", **se persiste `anywhere`
aunque el permiso quede pendiente**, y Settings muestra el estado real.

Por qué, y no lo contrario: en macOS `CGRequestListenEventAccess()`
casi nunca concede en el momento — el diálogo solo ofrece abrir Ajustes
del Sistema, y el permiso recién queda efectivo cuando el usuario lo
activa ahí (a veces tras reiniciar la app). Si se revirtiera la
preferencia al no conceder al instante, el owner iría a Ajustes del
Sistema, concedería el permiso, volvería… y encontraría el control de
vuelta en "Nimvlet only". Eso es más ambiguo, no menos. Con la decisión
actual: el control refleja **lo que el owner eligió**, y la línea de
estado refleja **lo que está pasando**.

Volver a "Nimvlet only" es siempre inmediato y nunca necesita permiso.

## 5. Prevención de doble conteo — el punto central del bloque

En modo global efectivo, un clic **sobre el Nimvlet** lo ven las dos
rutas: el monitor global (porque ocurrió en el sistema) y la ventana del
pet (porque ocurrió encima de ella). Sin una regla explícita, una
pulsación física valdría **+2**.

La regla vive en una política pura, no repartida en `if (global)`
(`src/core/ClickCounting.h`):

| modo efectivo | fuente | ¿suma? | |
|---|---|---|---|
| Local | clic del pet | **sí** | comportamiento histórico |
| Local | evento global | no | defensivo: un evento reenviado tarde, tras `Stop()`, no se cuela |
| Global | clic del pet | **no** | ← la regla del bloque |
| Global | evento global | **sí** | única fuente de moneda |

En modo global el clic sobre el pet **sigue haciendo todo lo demás**:
clasificación de gesto, animación de personalidad, hover, arrastre. Lo
único que cambia es de dónde viene la moneda (§9).

`SpikeApp::HandleCountedClick(source, nowMs)` es el **único** punto de
mutación del wallet para las dos fuentes: consulta la política y, solo
si esa fuente cuenta, hace `++clickBalance` + `MarkDirty` (el mismo
debounce de siempre) + refresco del wallet canónico del Product UI. No
hay un segundo wallet, ni contadores por fuente, ni `source` persistido.

### 5.1 La redacción AMPLIA del OS vs. nuestra máscara de eventos

En la QA física del owner (Block 11A), macOS mostró el diálogo del
permiso con una redacción del estilo *"would like to receive keystrokes
from any application"*, y Ajustes del Sistema describe **Monitorización
de entrada** en términos de teclado. Es razonable que alarme: Nimvlets
promete exactamente lo contrario.

Lo que hay que entender —y lo que la copy ahora dice— es que **esa
redacción es de la categoría de TCC entera, no una descripción de esta
app**. Apple define una sola categoría para "leer input a bajo nivel"; no
existe una variante "solo mouse" que podamos pedir en su lugar, y no
tenemos ninguna forma de cambiar el texto de Apple. Tampoco lo
insinuamos.

**Lo que sí acota el alcance es nuestro código**, y es verificable línea
por línea:

| Afirmación | Dónde se ve |
|---|---|
| Un solo tipo de evento en la máscara | `CGEventMaskBit(kCGEventLeftMouseDown)` — un único `CGEventMaskBit(` en todo `src/` |
| Listen-only, nunca modifica el input | `kCGEventTapOptionListenOnly` |
| Ningún evento de teclado observado | `kCGEventKeyDown` / `kCGEventKeyUp` / `kCGEventFlagsChanged` no aparecen en `src/` (fuera de comentarios) |
| El callback no lee nada del evento salvo su tipo | sin `CGEventGetLocation` / `…IntegerValueField` / `…Flags` / `…Timestamp` |
| Nunca Accessibility ni Screen Recording | `AXIsProcessTrusted` / `AXUIElement` / `CGRequestScreenCaptureAccess` prohibidos en todo `src/` |

Los cinco los fija `PrivacyInvariantTest` en `tools/test_asset_pipeline.py`
contra la fuente real (§14), incluido el chequeo, agregado en esta
corrección, de que hay **exactamente un** `CGEventMaskBit(` — o sea, ni
una máscara OR-eada ni un segundo evento colado.

Además de la explicación previa al permiso, el recordatorio de alcance
("Nimvlets listens only for primary mouse presses, whatever the system
permission is called.") se repite en los dos estados en los que el owner
tiene la entrada del permiso delante en Ajustes del Sistema: **falta el
permiso** y **Active**. No hay ningún modal nuevo, ni nada que se
muestre en cada arranque.

### 5.2 Atribución del permiso en DESARROLLO — "Antigravity IDE"

En el run de DESARROLLO del owner, la lista de Monitorización de entrada
mostró **"Antigravity IDE"** en vez de "Nimvlets". Investigado, y es
**consistente con la topología actual de build/ejecución**, no un bug de
identidad:

- Lo que se ejecuta es un **Mach-O suelto**
  (`build/macos-debug/src/app/nimvlets_spike`), no un `.app`.
- Está firmado **ad-hoc / linker-signed** (`Signature=adhoc`,
  `flags=0x20002(adhoc,linker-signed)`), con `Info.plist=not bound`.
- **No existe ningún bundle**: el CMake no usa `MACOSX_BUNDLE`, no hay
  `Info.plist`, no hay bundle id. No hay, literalmente, una identidad de
  app que TCC pueda mostrar.
- TCC atribuye entonces el permiso al **proceso responsable**: la app
  que lanzó el ejecutable — la terminal del IDE del owner.

**Consecuencia, dicha sin adornos: la identidad del permiso en RELEASE
NO está verificada.** Es un requisito de QA de un bloque futuro, cuando
exista un `Nimvlets.app` real, firmado, con su propio bundle id: hay que
repetir el flujo completo del permiso desde ese bundle y confirmar que
macOS identifica a **Nimvlets** en el diálogo y en Ajustes del Sistema.
Este bloque no arma infraestructura de firma/empaquetado para cambiar
esa captura de DEV (fuera de alcance), no toca bases de datos de TCC y
no usa `tccutil` ni `sudo`.

## 6. macOS — la implementación real

`src/platform/macos/GlobalClickMonitor.mm`.

| | |
|---|---|
| **Permiso** | **Input Monitoring** (TCC) |
| **NO** | Accessibility, Screen Recording, admin/root |
| Preflight | `CGPreflightListenEventAccess()` — nunca muestra diálogo |
| Pedido | `CGRequestListenEventAccess()` — **un solo call site** |
| Mecanismo | `CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionListenOnly, …)` |
| Máscara | `CGEventMaskBit(kCGEventLeftMouseDown)` — **un solo evento** |

**Listen-only, sin excepción.** Con `kCGEventTapOptionListenOnly` el
valor de retorno del callback ni se consulta; el callback devuelve el
evento intacto igualmente. Nimvlets **nunca** modifica, suprime, retrasa
ni consume el clic del usuario.

**El callback lee el TIPO del evento y nada más.** No llama
`CGEventGetLocation`, ni `CGEventGetIntegerValueField`, ni
`CGEventGetFlags`, ni `CGEventGetTimestamp`, ni consulta el proceso
destino. Su cuerpo entero es "si es un left-mouse-down, avisá" (+
re-habilitar el tap si el sistema lo deshabilitó).

### Por qué un hilo dedicado

Se investigó primero si el tap podía colgarse del run loop principal.
Leyendo la **fuente pineada de SDL 3.4.12** (AGENTS.md §4 — nunca por
analogía): `Cocoa_PumpEventsUntilDate` usa
`[NSApp nextEventMatchingMask:untilDate:inMode:NSDefaultRunLoopMode]`,
así que el run loop principal **sí** atiende sources… **mientras
espera**. Durante `RenderFrame()`, `ApplyCurrentHitMask()`, la carga de
un `.nvpack` al cambiar de pet, o un redibujo del Product UI, no atiende
nada. Un `CGEventTap` tiene **timeout duro**: si su callback no responde
a tiempo, el sistema lo **deshabilita**
(`kCGEventTapDisabledByTimeout`). Colgarlo del run loop principal haría
que el conteo global se apagara solo justo cuando el owner está
interactuando.

El hilo dedicado:

- existe **solo** mientras el modo global está activo;
- bloquea en `CFRunLoopRun()` — **event-driven, cero polling, cero
  wakeups periódicos**;
- no hace nada más que reenviar el evento;
- se para con una `CFRunLoopSource` versión 0 **señalada** desde el hilo
  principal. Una señal queda *latched*, así que elimina la carrera de
  "`Stop()` llegó antes de que `CFRunLoopRun()` empezara" **sin** un
  wakeup periódico de guardia;
- `Stop()` hace `join`, así que al volver está **garantizado** que no
  puede llegar ningún callback más.

### Entrega al hilo principal

El callback hace un único `SDL_PushEvent` de un `SDL_EVENT_USER` propio
**sin código ni datos** (no hay nada que llevar). SDL documenta
`SDL_PushEvent` como seguro desde cualquier hilo, y —leído de la fuente
pineada— `SDL_PeepEvents(SDL_ADDEVENT)` dispara `SDL_SendWakeupEvent()`
→ `Cocoa_SendWakeupEvent` → `[NSApp postEvent:atStart:YES]`, que
despierta el `SDL_WaitEventTimeout` del loop principal. Es el **mismo
seam** que el menú rápido nativo ya usaba desde Block 06: sin callbacks
entre hilos, sin estado global compartido, sin timer.

La mutación canónica del balance ocurre después, en el hilo principal.

## 7. UX del permiso (macOS)

Elegir "Anywhere" **no** pide el permiso de inmediato. La política pura
`platform::EvaluateGlobalClickRequest(status)` decide:

- **`kApplyDirectly`** — no hace falta permiso, o ya está concedido: se
  aplica y listo (no se explica un diálogo que no va a aparecer).
- **`kNeedsExplanation`** — se muestra primero la explicación de
  primera parte, con dos botones. **Solo "Continue" puede llamar al
  pedido nativo.**
- **`kUnavailable`** — la plataforma no puede: el pedido se ignora.

La explicación dice qué se necesita y, sobre todo, qué **no** se observa:

> To count clicks outside Nimvlets, your system needs Input Monitoring.
> Your system may describe that permission broadly — even as keyboard or
> keystroke access; that wording covers the whole permission, not what
> Nimvlets does. Nimvlets only counts primary mouse presses — never
> keys, pointer positions, apps, or screen content.
>
> [ Not now ]  [ Continue ]

La segunda frase se agregó tras la QA física del owner — ver §5.1.

`Esc` equivale a "Not now" (descarta la explicación antes de poder
cerrar la ventana).

**Nunca se pide el permiso**: al arrancar, desde onboarding, desde el
Shop, desde la Collection, desde el menú rápido, ni porque un save diga
`anywhere`. En cada arranque se hace **preflight** y, si ya está
concedido, el monitor arranca en silencio; si no, se cae a local y
Settings lo dice.

### Tras un pedido denegado / pendiente

Settings muestra el estado y **cómo resolverlo**, con un reintento
manual:

> Input Monitoring permission needed
> Turn Nimvlets on under Input Monitoring in System Settings, then check
> again.
> [ Check again ]

**Deliberadamente NO se usa un deep link** tipo
`x-apple.systempreferences:` hacia el panel de Input Monitoring: no es
API documentada y ha cambiado entre versiones de macOS. Nombrar el lugar
+ un botón "Check again" que re-consulta el permiso e intenta arrancar
de nuevo es más robusto y no depende de comportamiento no soportado.

## 8. Estados que Settings muestra

Derivados por `platform::ResolveGlobalClickUiState(requested, status)` —
capacidad, permiso y actividad ya resueltos a estado **genérico**, para
que `src/productui` no tenga ninguna rama por plataforma (§10).

| Situación | Segmento "Anywhere" | Línea de estado | Acción |
|---|---|---|---|
| Modo local, plataforma capaz | elegible | *(ninguna)* | — |
| Anywhere, monitor corriendo | elegible | **Active** + alcance solo-mouse + nota de arrastre | — |
| Anywhere, falta permiso | elegible | **{permiso} permission needed** + cómo concederlo + alcance solo-mouse | Check again |
| Anywhere, permiso ok pero no arrancó | elegible | **Could not start** | Check again |
| Plataforma sin capacidad | **apagado** (se dibuja, no se puede elegir) | **Not available on this system** | — |

En modo local **no se dice nada** del permiso: estado que el owner no
pidió es ruido, no información.

El nombre del permiso ("Input Monitoring") lo aporta el **adapter** vía
`GlobalClickStatus::permissionName` y se sustituye en `{permission}` —
por eso la copy puede nombrar el permiso real sin que el Product UI
conozca la plataforma.

## 9. Lo que el modo global NO cambia

- Las animaciones del pet. Un clic directo sobre el Nimvlet dispara su
  reacción de personalidad **igual** en los dos modos.
- La precedencia DRAG > CLICK > HOVER/AMBIENT > BASE/STATIC.
- El contenido de Bunny / Nidir / Frin, el timing de hover, el
  scheduling ambient, los pesos de animación.
- El arrastre: se sigue arrastrando igual, y el camino local de arrastre
  nunca suma un clic.
- El wallet: es uno solo, continuo. Cambiar de modo no pierde ni
  duplica nada — solo cambia **quién** lo alimenta.

## 10. Arquitectura

```
src/core/ClickCounting.h        política PURA: modo pedido, modo efectivo,
                                 y la matriz de 4 casos de doble conteo
src/platform/GlobalClickTypes.h  capacidad / permiso / estado de UI (PURO:
                                 sin AppKit, sin windows.h, sin X11)
src/platform/GlobalClickMonitor.h  la interfaz del monitor (sin SDL)
src/platform/<os>/GlobalClickMonitor.{mm,cpp}  adapters reales
src/app/SpikeApp                 ciclo de vida del monitor + el ÚNICO punto
                                 de mutación del wallet
src/productui/SettingsLayout     el grupo "Interaction", desde estado GENÉRICO
tests/FakeGlobalClickMonitor.h   doble puro (solo tests)
```

**Frontera de privacidad propia.** Deliberadamente **no** se pliega
dentro de `TransparentWindowSupport`, `SystemShell` ni el input normal
de SDL (brief §6): nada del camino estándar de interacción necesita
esto, y nada de esto se enciende salvo que el owner lo pida.

**Nada fuera de `src/platform/*`** incluye headers de event tap,
AppKit/permisos, `windows.h`/hooks Win32, ni APIs de input global de
X11/XInput2. **Ningún `#ifdef __APPLE__` / `#ifdef _WIN32`** en
`src/app`, `src/core` ni `src/productui`. Los dos hechos están fijados
por tests (§13).

## 11. Persistencia — AppState v6

Se agrega **un** campo: `AppState::clickCountingMode`, un string
(`""` / `"nimvlet_only"` / `"anywhere"`), con la misma disciplina que
`sizeChoice` y `language` — `core::ParseClickCountingMode` normaliza
cualquier valor desconocido a `kNimvletOnly`, el default privado.

**Migración v1..v5 → v6:** el campo llega vacío ⇒ **Nimvlet only**.
Ningún usuario existente queda con conteo global habilitado por
actualizar, y la app no pide ningún permiso por haber subido de schema.

**La frontera histórica de propiedad sigue congelada.** El riesgo que el
brief pedía descartar explícitamente es escribir la migración como
`if (oldSchema < currentSchema) expandir Frin` — con
`kCurrentSchemaVersion == 6`, eso haría que un save v4 o v5 (propiedad ya
explícita) volviera a pasar por la expansión histórica y ganara
variantes que su dueño nunca compró. El gate real es
`< AppState::kFirstExplicitEntitlementSchema` (**== 4**), un umbral
**semántico fijo** que este bump no mueve (DEC-129). Ver
`tests/EntitlementMigrationTest.cpp`, que fija v4→v6 y v5→v6 como
regresión explícita, y que la constante sigue valiendo 4.

**Lo ÚNICO que esta feature persiste es la preferencia.** Ni
coordenadas, ni timestamps, ni historial, ni contadores por fuente, ni
el estado del permiso (que se consulta en cada arranque, no se guarda).

## 12. Matriz de soporte por plataforma

| Plataforma | Capacidad | Permiso | Estado |
|---|---|---|---|
| **macOS** | `kSupportedNeedsPermission` | Input Monitoring | **Implementado.** Permiso, creación del tap, arranque/parada del monitor y persistencia **verificados en vivo** en macOS 26.6 (arm64). El conteo de una pulsación física real del escritorio requiere QA manual del owner — ver §14. |
| **Windows** | `kUnavailable` | — | **Investigado, NO implementado.** Ver §12.1. |
| **Linux / X11** | `kUnavailable` | — | **Investigado, NO implementado.** Ver §12.2. |
| **Linux / Wayland** | `kUnavailable` | — | **No es posible** con medios legítimos. Ver §12.2. |

En Windows y Linux, Settings dibuja el segmento "Anywhere" **apagado** y
la línea "Not available on this system". El modo "Nimvlet only" funciona
exactamente igual que siempre.

### 12.1 Windows — por qué el adapter reporta `kUnavailable`

El camino moderno soportado sería un hook de bajo nivel de **mouse
únicamente**: `SetWindowsHookEx(WH_MOUSE_LL, …)`, filtrando
`WM_LBUTTONDOWN`, devolviendo siempre `CallNextHookEx` (nunca suprimir),
reenviando al hilo principal, desinstalando con `UnhookWindowsHookEx`.
Nunca `WH_KEYBOARD_LL`. Sin admin. Esa forma encaja perfectamente detrás
de esta misma interfaz.

**No se escribió** porque AGENTS.md §4 prohíbe afirmar una conducta de
plataforma que no se corrió en ese OS, y el brief §16 es explícito: *"do
NOT write speculative unvalidated Win32 code merely to claim support"*.
Un hook global de input es justo el tipo de código cuya corrección **no
se demuestra compilando**: que no trague ni retrase clics, que el hilo
que lo instala tenga la bomba de mensajes que `WH_MOUSE_LL` exige, el
`LowLevelHooksTimeout`, y la desinstalación limpia al apagar son todos
hechos de **runtime**. Este bloque no tuvo máquina Windows ni QA
interactiva de Windows. **NOT RUNTIME VERIFIED / validación diferida.**

### 12.2 Linux — X11 y Wayland

**X11.** `XI_RawButtonPress` de XInput2 sobre la ventana raíz sí daría,
en teoría, una notificación pasiva del botón primario sin grabs, sin
suprimir input y sin root. Pero este proyecto **no usa Xlib
directamente en ningún lado**: XInput2 está activado *dentro de la SDL
pineada* (`SDL_X11_XINPUT`, ver `cmake/FetchSDL3.cmake`), no en nuestro
código, y `src/platform/linux/` no enlaza `libX11` ni `libXi`.
Implementarlo exigiría dependencias de desarrollo nuevas (+ paquetes
nuevos en CI), una conexión X propia en paralelo a la que SDL ya
administra, y un bucle de eventos aparte — exactamente lo que AGENTS.md
§10 pide no agregar sin razón concreta y §4 prohíbe declarar verificado
sin correrlo. `docs/LINUX_PLATFORM.md` §13 ya registra que Linux nunca
tuvo QA interactiva. **Diseñado, no fingido.**

**Wayland.** No hay camino legítimo, y no es una limitación de este
proyecto sino del **diseño del protocolo**: un cliente Wayland ordinario
solo recibe input cuando el compositor le da foco al puntero sobre su
propia superficie. Las únicas rutas para "ver clics en cualquier lado"
serían capturar la pantalla (prohibido de forma permanente, AGENTS.md
§5), leer `/dev/input` (root o grupo `input`, prohibido), o un portal de
**captura** de entrada, cuya semántica *desvía/captura* el puntero en
vez de observarlo pasivamente — rompería el uso normal del escritorio y
no es lo que esta feature hace. **Modo global no disponible en
Wayland.** Limitación honesta > paridad fingida.

## 13. Tests

**Política pura** (`tests/ClickCountingPolicyTest.cpp`): ids
persistidos y fallback a local ante cualquier valor desconocido; modo
efectivo (exige pedido **y** monitor vivo); la matriz de 4 casos, con la
invariante de que en cualquier modo cuenta **exactamente una** fuente;
los tres escenarios del brief §20; **una pulsación física sobre el pet
nunca vale +2**; semántica de arrastre por modo; cambiar de modo no
pierde ni duplica; `core::Preferences` con default local.

**Costura de plataforma** (`tests/GlobalClickMonitorTest.cpp` +
`tests/FakeGlobalClickMonitor.h`): todos los estados de capacidad /
permiso → estado de UI; cuándo hace falta explicar antes de pedir; ciclo
de vida start/stop/idempotencia; un evento reenviado por pulsación, sin
coalescer; tras `Stop()` el callback ya no puede invocarse; fallo de
arranque y caída en runtime; el flujo real de macOS (pedir no concede) y
la recuperación por "Check again"; e integración monitor → reenvío →
wallet canónico.

**Persistencia** (`tests/AppStateSerializerTest.cpp`): round-trip v6
determinista byte a byte; `""` vs `"nimvlet_only"` como estados
distintos en disco; v1..v5 → local; v6 truncado rechazado.

**Frontera de migración** (`tests/EntitlementMigrationTest.cpp`): v4→v6
no gana variantes de Frin; v5→v6 no reinterpreta propiedad; un v1/v2/v3
genuino **sí** sigue expandiendo; balance / lifecycle / preferencias /
propiedad intactos; y la constante sigue congelada en 4.

**Settings** (`tests/SettingsLayoutTest.cpp`): el grupo Interaction y su
fila; sin aviso en modo local; segmento apagado + estado en una
plataforma sin capacidad (y el hit-test nunca lo devuelve); explicación
con sus dos botones, el permiso nombrado **y la frase que anticipa la
redacción amplia del OS** (§5.1); estado "falta permiso" con "Check
again" y el recordatorio de alcance; "Active" con el recordatorio de
alcance **antes** de la nota de arrastre; que el párrafo más largo entra
entero en las líneas reservadas —en EN y en ES— sin cortarse con "…";
orden de foco con los botones justo después de su fila; EN/ES completo;
resize; y que el estado por defecto sigue entrando sin scroll en
800×560.

**Wallet en vivo** (`tests/ProductWindowStateTest.cpp`): la regresión
exacta de la QA del owner — con Settings visible, un clic contado deja
el balance canónico Y un repintado pendiente sin cambiar de sección; la
paridad local/global por el mismo camino; que ninguna sección se
comporta distinto; que un clic **no** contado no toca nada (ni wallet,
ni persistencia, ni repintado); un repintado por clic contado, ni uno
más; y el contrato de presentación de la ventana (cerrada / visible /
minimizada). Ver docs/PRODUCT_UI.md §22.

**Menú rápido** (`tests/QuickMenuModelTest.cpp`): el menú **no** gana
"Click counting" — regresión explícita de la decisión de producto.

**Auditoría de fuente** (`tools/test_asset_pipeline.py`,
`PrivacyInvariantTest`): ver §14.

## 14. Auditoría de privacidad, fijada en tests

El guard de Python que existía desde Block 05 prohibía `CGEventTapCreate`
de plano. Block 11A lo autoriza — y el guard **no se relaja: se vuelve
más específico**. Ahora fija que:

- las APIs de event tap y de permiso de Input Monitoring aparecen en
  **exactamente un archivo**, `src/platform/macos/GlobalClickMonitor.mm`,
  y en ningún otro (el guard mide **código**, no comentarios: los
  adapters de Windows/Linux nombran en prosa justamente lo que no usan);
- el tap es `kCGEventTapOptionListenOnly` y nunca
  `kCGEventTapOptionDefault`;
- la máscara es exactamente `CGEventMaskBit(kCGEventLeftMouseDown)` —
  el guard falla si aparece cualquier otro tipo de evento — y hay
  **exactamente un** `CGEventMaskBit(` en el archivo, así que tampoco se
  puede colar un segundo evento OR-eado ni `kCGEventMaskForAllEvents`
  (chequeo agregado en la corrección de QA del owner, §5.1);
- el callback no lee coordenadas, flags, timestamp, proceso destino ni
  número de botón;
- el pedido de permiso ocurre en **un solo call site**;
- el callback de reenvío no gana **ningún** parámetro de datos;
- `AppState` persiste el modo y **nada más** de esta feature.

Y sigue prohibido en **todo** `src/`: Accessibility
(`AXIsProcessTrusted`, `AXUIElement`), captura de pantalla, HID crudo,
síntesis de eventos (`CGEventPost`, `CGRequestPostEventAccess`),
`WH_KEYBOARD_LL`, constantes de teclado, enumeración de apps
(`NSWorkspace`, `CGWindowListCopyWindowInfo`, `GetForegroundWindow`), y
red.

## 15. Rendimiento

Ver `docs/PERFORMANCE_BUDGETS.md` para las cifras completas. En resumen:

- **Modo global OFF** (el default): no se instala ningún monitor, no hay
  hilo extra, no hay polling ni wakeups. El costo de runtime es
  **idéntico** al de antes de este bloque.
- **Modo global ON, en reposo**: monitoreo **event-driven** nativo. El
  hilo dedicado bloquea en `CFRunLoopRun()` sin timeout ni wakeups
  periódicos. Medido en Release: **0.0 % de CPU** en las 12 muestras, y
  **sin delta medible de RSS** frente a OFF.
- El callback nativo no provoca ningún redibujo salvo el del wallet tras
  un clic real, y **no agrega ningún término** al cálculo de `waitMs`
  del event loop.
- Un clic contado invalida **un solo frame** de la sección visible, y
  solo si el número cambió (`productui::WalletDisplay`). Con el Product
  UI cerrado no invalida nada; con la ventana **minimizada** no se
  dibuja ni se presenta nada — lo pendiente queda pendiente y se pinta
  una vez al restaurarla. Sin eso, "Anywhere" habría pintado un frame
  por cada clic del sistema contra una ventana en el Dock (corrección de
  QA del owner — docs/PRODUCT_UI.md §22).

## 16. QA manual del owner (macOS)

Lo que este bloque **sí** verificó en vivo, en esta máquina (macOS 26.6,
arm64), con un directorio de app-data aislado:

- arranque en modo local: **ningún** monitor instalado, ningún prompt;
- 5 eventos globales reenviados en modo local → **0 contados** (balance
  intacto);
- pedir "Anywhere" con el permiso ya concedido → el monitor arranca y
  queda **ACTIVE**;
- **el test de doble conteo**: con el modo global activo, 4 clics del pet
  + 6 eventos globales → balance 3 → **9** (los 4 del pet sumaron **0**);
- reinicio con `anywhere` persistido → **preflight**, arranque en
  silencio, **sin prompt**, balance preservado;
- volver a "Nimvlet only" → *"global click monitor stopped"*, y desde ahí
  los eventos globales dejan de contar y los del pet vuelven a contar;
- el archivo en disco es **v6** y termina en `anywhere` / `nimvlet_only`;
- shutdown limpio en todos los casos.

**Lo que NO se pudo verificar sin un humano** — una pulsación **física**
real del botón primario en el escritorio llegando al tap. Sintetizar un
clic requeriría `CGEventPost` / permiso de *post event* (o
Accessibility), que este producto tiene prohibido. Pasos para el owner:

1. Arrancar un build de dev con app-data aislado.
2. Abrir el Product UI desde el menú de la barra → **Settings**.
3. En **Interaction → Click counting**, elegir **Anywhere**.
4. Leer la explicación de privacidad.
5. **Continue**.
6. Observar el diálogo de macOS / abrir Ajustes del Sistema.
7. Activar **Nimvlets** en **Privacidad y seguridad ▸ Monitorización de
   entrada**. (Si macOS pide reiniciar la app, hacerlo: al volver, el
   preflight la enciende sola, sin volver a preguntar.)
8. Volver a Settings; si dice "falta permiso", pulsar **Check again**.
9. Confirmar que el estado pasa a **Active**.
10. Hacer varios clics **fuera** de Nimvlets.
11. Confirmar que el wallet de la cabecera sube **uno por clic**, en vivo.
12. Hacer **un** clic directamente **sobre el Nimvlet**.
13. Confirmar que el wallet sube **exactamente 1** y que la animación de
    click **igual se reproduce**.
14. **Arrastrar** el Nimvlet una vez.
15. Confirmar que la pulsación contó **una** sola vez.
16. Volver a **Nimvlet only**.
17. Confirmar que los clics en cualquier otro lado **dejan de contar de
    inmediato**.
18. Confirmar que los clics sobre el pet **siguen contando**.
19. Reiniciar la app y confirmar que preferencia y permiso quedan
    coherentes.

No hace falta —y no se debe— automatizar Ajustes del Sistema, tocar
bases de datos de TCC, ni usar `sudo`.

### 16.1 Resultado: PASS físico del owner (macOS)

El owner corrió el guion completo de arriba a mano, con clics físicos
reales. **Resultado: PASS.** Verificado con sus propias manos:

- modo local: clics físicos fuera → **+0**; clic directo sobre el
  Nimvlet → **+1**;
- "Anywhere" activo: **5** clics primarios físicos fuera de Nimvlets →
  exactamente **+5**; **1** clic físico sobre el Nimvlet → exactamente
  **+1**, con la animación de click intacta;
- arrastre físico → exactamente **+1**; clic derecho → **+0**; scroll →
  **+0**; teclado → **+0**;
- volver a "Nimvlet only" → los clics de afuera dejan de contar en el
  acto, los del pet siguen contando;
- reinicio: preferencia y permiso coherentes.

O sea: el camino real del `CGEventTap` de macOS está **verificado por el
owner**, no inferido. Las dos correcciones que salieron de esa sesión
(refresco en vivo del wallet, y recuperación de la ventana minimizada)
están en docs/PRODUCT_UI.md §22 y en DEC-140.

### 16.2 Smokes automatizados en vivo (DEV)

Dos hooks solo-DEV cubren, contra la ventana real, lo que un test puro
de `tests/` no puede probar. Ver README.md:

```bash
# Un clic contado refresca el wallet YA, en las tres secciones, y la
# otra fuente no suma (no doble conteo). Con el permiso concedido,
# NIMVLETS_DEV_CLICK_COUNTING=anywhere corre la mitad GLOBAL.
NIMVLETS_DEV_APPDATA_DIR=/tmp/nv_qa NIMVLETS_DEV_HIDE_PET=1 \
  NIMVLETS_DEV_WALLET_LIVE_SMOKE=1 ./build/macos-debug/src/app/nimvlets_spike
```

## 17. Fuera de alcance (no implementado, a propósito)

Historial de clics, estadísticas, conteo por app, coordenadas, conteo de
teclado, conteo de clic derecho/medio, conteo de scroll,
launch-at-login, hotkeys globales, logros, audio, telemetría, red,
cuentas, sync en la nube.
