# Nimvlets — Persistencia local de estado (Block 03)

Esto describe la pequeña capa de persistencia, local-only, construida
en Block 03 sobre el runtime de contenido/animación de Block 02. No es
un bloque de UI — nada aquí agrega una pantalla de Shop, Collection,
onboarding, o selección de pet. Ver `docs/ANIMATION_RUNTIME.md` para el
runtime en el que esto se enchufa, y `docs/DECISION_LOG.md` DEC-025 a
DEC-028 para por qué se tomó cada decisión de abajo.

## 1. Alcance

Persistido, con significado real en el runtime hoy:

- **Click balance** (`AppState::clickBalance`, `uint64`) — la única
  moneda (AGENTS.md §2). Se incrementa en cada click válido y, desde
  **Block 07**, se **decrementa** en una compra del Shop (nunca por
  debajo de 0 — la política de compra verifica `balance >= precio`
  antes de restar; ver `docs/PRODUCT_UI.md` §18).
- **Active pet id** (`AppState::activePetId`) — se mantiene
  sincronizado con la verdad: cualquier pet que se haya cargado
  realmente (`content::PetDefinition::id`). Este bloque no implementa
  *selección* de pet: siempre carga exactamente un pack, así que el
  campo siempre refleja ese único pet. Existe para que una futura UI de
  selección tenga dónde leer/escribir sin un cambio de schema.
- **Active variant id** (`AppState::activeVariantId`) — se conserva a
  través de load/save; nada en este bloque escribe un valor no vacío
  (todavía no existe selección de variante — ver
  `content::PetDefinition::variantGroup`, también schema-only).
- **Last window position** (`AppState::lastWindowPosition`) — se
  actualiza cada vez que termina un drag; se usa para reabrir la
  ventana donde el usuario la dejó (ver §7).

**Agregado en Block 06 (schema v2), Block 06.1 (schema v3) y Block 07
(schema v4) — ver §3:**

- **Propiedad** — Block 06/06.1 la modelaban como `AppState::ownedPetIds`
  (conjunto de `petId`). **Block 07 (schema v4) la reemplaza por
  `AppState::ownedEntitlements`**: pares `{petId, variantId}` capaces de
  expresar "posee solo Frin macho" (`persistence::OwnedEntitlement` —
  datos planos, sin dependencia de `src/catalog`; `src/app` puentea a
  `catalog::PetEntitlement`, cuya coincidencia es EXACTA — no hay
  autorización de "todas las variantes de un Nimvlet", DEC-128).
  `ownershipSeeded` (sin cambios) distingue "nunca se inicializó" de
  "posee cero"; `src/app` re-siembra si el conjunto quedó vacío por
  corrupción. La semilla otorga la autorización EXPLÍCITA de cada
  entrada `initiallyOwned` (Frin siembra sus dos variantes, no un
  `{frin, ""}`) solo en el primer arranque. Ver `docs/CATALOG.md`
  §11–§12 y `docs/PRODUCT_UI.md` §19.
- **Preferencias del menú rápido** (`AppState::lockPosition`,
  `sizeChoice`, `opacityPercent`) — controles de usuario expuestos por
  el menú de la barra (`docs/PRODUCT_UI.md` §8). `core::DisplayControls`
  (puro) los traduce a comportamiento genérico de runtime.
- **Idioma del Product UI** (`AppState::language`, schema v3 — Block
  06.1). `""` = el owner nunca eligió; en ese caso `SpikeApp` resuelve
  el inicial del locale del OS (en/es) SIN persistirlo. Una elección
  explícita desde el menú `Language ▸` sí se persiste y gana desde ese
  momento. Ver `docs/PRODUCT_UI.md` §14 y DEC-115/DEC-116.
- **NO** persistido: la visibilidad del pet ("Hide Nimvlet"). Al
  relanzar la app el pet siempre arranca visible (esconder ≠ salir,
  brief §17).

No persistido, y no planeado: **historial** de compras (solo el estado
final — balance + autorizaciones), precios (viven en el catálogo, no en
el estado del usuario), estado de onboarding/selección de starter,
procedencia/analítica de cualquier tipo.

Genérico por construcción: `activePetId`/`activeVariantId` son simples
strings y `ownedEntitlements` son pares de strings, no enums de Nimvlets
conocidos — agregar un nuevo pet id o variante más adelante nunca
requiere tocar `src/persistence`.

## 2. Política de ubicación de almacenamiento

Producción: `SDL_GetPrefPath("Leporc Projects", "Nimvlets")` — el
resolutor multiplataforma propio de SDL para el directorio de app-data
por usuario. Crea el directorio ella misma si hace falta y retorna:

- macOS: `~/Library/Application Support/Leporc Projects/Nimvlets/`
- Windows: `%APPDATA%\Leporc Projects\Nimvlets\`
- Linux (verificado en Block 04.1, no reimplementado — ver
  `docs/LINUX_PLATFORM.md` §7): `~/.local/share/Leporc Projects/Nimvlets/`
  (basado en `$XDG_DATA_HOME`, el mecanismo estándar de Linux para
  datos de usuario por app), tanto en X11 como en Wayland — la
  resolución de `SDL_GetPrefPath()` no depende del backend de video.

No hizo falta código específico de plataforma para esto (a diferencia
de la transparencia/click-through de ventana) — SDL ya lo abstrae por
completo, así que `src/platform/*` queda intacto en este bloque (y en
Block 04.1: el adapter Linux nuevo tampoco toca nada de persistencia).

**Override solo-DEV:** `NIMVLETS_DEV_APPDATA_DIR`, verificado antes de
llamar a `SDL_GetPrefPath()`. Si se setea a una ruta no vacía, esa ruta
se usa en su lugar (se crea si no existe) — el comportamiento de
producción (el caso sin setear) queda igual. Esto es lo que permite que
la QA manual y los smoke tests no interactivos propios de este bloque
ejerciten el camino real de save/load contra un directorio temporal
aislado, nunca la ubicación real por usuario — refleja exactamente el
patrón de `NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS` de Block 02 (ver
`docs/ANIMATION_RUNTIME.md` §8).

```bash
NIMVLETS_DEV_APPDATA_DIR=/tmp/nimvlets_dev_state ./build/macos-debug/src/app/nimvlets_spike
```

Un archivo dentro de ese directorio: `state.nvstate` (más un archivo de
staging transitorio `state.nvstate.tmp` que solo existe a mitad de
escritura — ver §4).

Nunca se hardcodea ni se comitea una ruta absoluta específica de una
máquina — el resultado de `SDL_GetPrefPath()` es un valor de runtime, y
el override DEV es una variable de entorno, nunca una ruta literal en
el código fuente.

## 3. Formato de archivo ("NVSTATE1")

Productor/consumidor: `persistence::SerializeAppState` /
`DeserializeAppState` (`src/persistence/AppStateSerializer.cpp`). Puro
— sin I/O de archivos — así que es directamente testeable con buffers
de bytes en memoria (`tests/AppStateSerializerTest.cpp`), la misma
separación que `content::PetPackLoader` estableció en Block 02. Todos
los enteros little-endian (cada plataforma que este proyecto soporta —
x86_64, arm64 — es little-endian; no se implementa byte-swapping,
igual que todo otro formato en disco de este repositorio).

```
magic             : 8 bytes, "NVSTATE1"   (el magic NO cambia entre schemas)
schemaVersion     : uint32                (4 desde Block 07)
-- cuerpo compartido v1+ (Block 03):
clickBalance      : uint64
activePetId       : string   (uint32 byte-length + UTF-8 bytes)
activeVariantId   : string
hasWindowPosition : uint8   (0/1)
lastWindowX       : int32   (solo tiene sentido si hasWindowPosition)
lastWindowY       : int32
-- bloque v2+ (Block 06):
ownershipSeeded   : uint8   (0/1)
-- propiedad: el layout de la lista cambia en v4
[schemaVersion <= 3]  ownedPetIdCount : uint32 ; ownedPetIds : string[]   (ordenados, sin duplicados)
[schemaVersion >= 4]  ownedEntitlementCount : uint32 ; (petId:string, variantId:string)[]   (canónico: ordenado por (petId,variantId), sin duplicados, sin petId vacío; "" en variantId = Nimvlet SIN variantes)
lockPosition      : uint8   (0/1)
sizeChoice        : string  ("small"/"medium"/"large"; "" => "medium")
opacityPercent    : uint32  (0 => sin preferencia => 100)
-- añadido de v3 (Block 06.1), sin cambios en v4:
language          : string  ("en"/"es"; "" => nunca elegido => se resuelve del locale del OS, sin persistir)
```

**Determinista:** serializar el mismo `AppState` dos veces produce una
salida idéntica byte a byte — sin timestamps, sin padding, sin orden de
iteración de map/set en ningún lugar del formato
(`SerializationIsDeterministic` en `tests/AppStateSerializerTest.cpp`).

**Versionado + migración hacia adelante (Block 06 DEC-109, Block 06.1
DEC-116, Block 07 DEC-124/DEC-128):** `DeserializeAppState` lee
cualquier versión en `[1, kCurrentSchemaVersion]` — hoy **1, 2, 3 y
4**. Un archivo más viejo se lee con su layout y los campos de
versiones posteriores quedan en su default; `outState.schemaVersion` se
fija a la versión actual, así que el próximo `Save()` lo reescribe al
formato actual.

**Migración de propiedad v2/v3 -> v4, en dos etapas** (DEC-128,
afinada por DEC-129):

1. **Parseo provisional (serializer).** `DeserializeAppState` lee la
   lista `ownedPetIds` (petIds sueltos) y produce un `{petId, ""}` por
   cada uno. El serializer NO tiene catálogo, así que no puede saber
   qué petIds tienen variantes — `{frin, ""}` acá es PROVISIONAL, no
   "todo Frin". El mismo parseo reporta por un out-param
   (`outOnDiskSchemaVersion`) la versión que traía el archivo EN DISCO,
   antes de normalizar `schemaVersion` a la actual; `AppStateStore::Load`
   lo propaga (queda en `kCurrentSchemaVersion` si no hay save o está
   corrupto).
2. **Reconciliación por tabla histórica CONGELADA (`src/app`).**
   `SpikeApp::Init()` corre `catalog::ExpandHistoricalWholePetEntitlements(ents)`
   **solo si la versión en disco es < la actual** (un estado
   genuinamente v1/v2/v3; nunca sobre un v4). La función **ya no recibe
   catálogo**: una tabla fija (`HistoricalLegacyVariants`) mapea
   `"frin" -> {frin, "male"} {frin, "female"}` — las variantes que Block
   06 realmente exponía — y deja cualquier otro `{p, ""}` igual (era un
   pet sin variantes). Idempotente. **El AppState persistido nunca queda
   con `{frin, ""}`.** Una variante de Frin agregada al catálogo DESPUÉS
   del schema v3 (p. ej. un hipotético `frin/spirit`) **no** queda
   cubierta por el conjunto migrado — aunque ya exista en el catálogo
   cuando ocurre la migración (DEC-129): la propiedad legacy significa
   exactamente el contenido del modelo viejo, no "toda variante futura".
   Un v4 editado a mano con un `{frin, ""}` suelto tampoco se expande
   (no pasa el gate de versión), y con `Covers` exacto ese par no cubre
   ninguna identidad real.

El resto es "los campos nuevos arrancan en su default". El click
balance, la posición de ventana, el idioma, la propiedad, el
pet/variante activo y las preferencias de tamaño/opacidad/lock
**sobreviven** cada actualización. Una versión más nueva desconocida
(v5+) o basura sigue tratándose como datos corruptos (ver §5), nunca se
adivina. `NormalizeOwnedEntitlements` (orden + dedup, sobre una copia)
mantiene la salida determinista byte a byte;
`catalog::CanonicalizePetEntitlements` (orden + dedup, sin subsunción —
DEC-128) es la forma canónica semántica que `src/app` aplica antes de
cada `Save()`.

## 4. Comportamiento de escritura atómica

`persistence::AppStateStore::Save()` (`src/persistence/AppStateStore.cpp`):

1. Serializa el `AppState` completo.
2. Lo escribe a `state.nvstate.tmp` en el mismo directorio. Si esto
   falla en cualquier punto (no se puede abrir, no se puede escribir el
   contenido completo), el `state.nvstate` real nunca se toca —
   `Save()` retorna `false` con un `outError` específico y se detiene
   aquí.
3. Solo si el paso 2 tuvo éxito completo: `std::filesystem::rename(tempPath,
   statePath)`. Un rename dentro del mismo directorio es atómico en
   los filesystems que este proyecto soporta — el archivo real tiene el
   contenido viejo completo o el contenido nuevo completo, nunca una
   escritura parcial, incluso si el proceso crashea, se corta la
   energía, o el disco se llena a mitad de la escritura.

Esta es la misma razón por la que un fallo en el paso 2 deja el save
*previo* válido completamente intacto: el archivo real nunca se abre
para escritura directamente, solo se reemplaza en un único paso atómico
al final. `FailedWritePreservesPriorValidSave` en
`tests/AppStateStoreTest.cpp` demuestra esto directamente (ver §8 para
cómo se simula un fallo de escritura de forma portátil en los tests).

Un `Save()` fallido se reporta (`outError`, logueado por `SpikeApp` vía
`SDL_Log`) y nunca crashea la app — ver §6 para qué pasa con el cambio
pendiente después de eso.

## 5. Recuperación ante corrupción

`AppStateStore::Load()` nunca lanza excepción y siempre retorna un
`AppState` usable:

| Situación | Resultado |
|---|---|
| Todavía no existe `state.nvstate` (primera ejecución) | `AppState{}` (defaults seguros) |
| El archivo existe pero no se puede abrir/leer | `AppState{}` |
| El archivo parsea pero tiene magic inválido / está truncado / schema version no soportada | `AppState{}` |
| El archivo parsea y el schema version coincide | el estado parseado |

Cada caso de "defaults seguros" setea un string opcional `outWarning`
(p. ej. *"existing app-state save could not be used (...); using
defaults"*) para que quien llama pueda loguear *por qué*, sin forzar a
cada call site a manejar una ruta de error separada — un save corrupto
o ilegible nunca queda indistinguible en silencio de "todavía no hay
save" en el log, aunque ambos produzcan el mismo resultado seguro en
memoria. `LoadRecoversFromCorruptFile` en `tests/AppStateStoreTest.cpp`
escribe bytes de basura directamente (evitando `Save()`) para simular
corrupción real en disco, no solo un input sintético para el parser.

## 6. Política de debounce / escritura

Los clicks pueden llegar muchas veces por segundo; escribir a disco una
vez por click sería un desperdicio sin sentido.
`persistence::PersistenceScheduler`
(`src/persistence/PersistenceScheduler.h`, puro, sin I/O de archivos,
testeable con timestamps fabricados exactamente igual que
`core::FrameScheduler`):

- El **primer** cambio después de un estado limpio/flusheado arma un
  deadline de **2000ms** (`PersistenceScheduler::kDefaultDebounceMs`)
  en el futuro — suficientemente corto para que un crash poco después
  del último cambio pierda a lo sumo ~2 segundos de progreso,
  suficientemente largo para que una ráfaga realista de clicks rápidos
  colapse en una escritura.
- Cualquier cambio posterior **antes** de que ese deadline dispare no
  hace nada a efectos de scheduling: actualiza el `AppState` en memoria
  pero **no** arma un nuevo deadline ni empuja el existente. Esto es lo
  que hace que "10 clicks rápidos" cuesten exactamente una escritura a
  disco, no diez, y también lo que evita que la actividad continua deje
  la persistencia sin escribir indefinidamente (un debounce de tipo
  "deslizante"/reset-en-cada-actividad podría, en principio, no
  disparar nunca bajo clicks sin parar; una ventana fija desde el
  *primer* cambio pendiente no puede).
- Un flush **fallido** deja el estado dirty (el cambio pendiente no se
  descarta en silencio) pero reprograma el reintento otro `debounceMs`
  más adelante, en vez de reintentar en el despertar inmediatamente
  siguiente del event loop — acotando la frecuencia de reintento a como
  máximo una vez por intervalo de debounce incluso bajo un fallo
  persistente (p. ej. un directorio eliminado). Sin backoff
  exponencial, sin límite de reintentos — no hace falta a esta escala,
  y no se pidió.
- **El shutdown limpio siempre flushea** lo que quede dirty,
  incondicionalmente, sin importar el deadline de debounce
  (`SpikeApp::Shutdown()` llama a `FlushPersistedState()` antes de
  desmontar cualquier otra cosa).
- **Una compra del Shop (Block 07) flushea de INMEDIATO**, no por el
  debounce (DEC-126). Racional: perder ~2s de *clicks* ante un crash es
  trivial y el debounce vale la pena; una compra cambia *propiedad* —
  un crash entre gastar el balance y el flush perdería la compra (o,
  peor, dejaría el balance intacto sin el pet). `HandlePurchaseRequest`
  muta `clickBalance` **y** `ownedEntitlements` en el mismo `AppState` y
  llama al `FlushPersistedState()` que ya existe — un solo
  `SerializeAppState` + un solo `rename` atómico persisten los dos
  JUNTOS. El per-click NO cambia: sigue con `MarkDirty` + debounce, no
  se convierte en una escritura a disco por click (brief §13).

`PersistenceScheduler::NextFlushDeadlineMs()` se integra en el cálculo
de deadline de `SDL_WaitEventTimeout` que ya existe en
`SpikeApp::Run()`, exactamente igual que ya lo hacen los deadlines de
animación/acción pasiva (ver `docs/ANIMATION_RUNTIME.md` §6) — sin loop
de polling nuevo, sin thread de timer; el event loop simplemente
también despierta cuando hay un flush pendiente por vencer.

## 7. Integración en el runtime

| Evento | Efecto |
|---|---|
| Arranque | Resuelve el directorio de app-data (§2); carga el estado existente o defaults; **si el save venía de un schema en disco < el actual, reconcilia la propiedad legacy** con una tabla histórica CONGELADA (`ExpandHistoricalWholePetEntitlements`, sin catálogo — DEC-129) y siembra si hace falta; resuelve el pet activo contra el catálogo Y contra la PROPIEDAD (`catalog::ResolveOwnedActiveIdentity` — un activo no autorizado cae a uno que sí lo está, **sin otorgar nada**, DEC-128); sincroniza `activePetId`/`activeVariantId` con lo que realmente se cargó (marca dirty si cambió); si había una posición de ventana guardada, abre ahí en vez de centrada. |
| Click | `clickCount_` (diagnóstico solo de sesión) y `appState_.clickBalance` (persistido) se incrementan ambos; se marca el scheduler como dirty. Deliberadamente dos contadores separados — ver `docs/DECISION_LOG.md` DEC-026. |
| **Compra del Shop (Block 07)** | `EvaluatePurchase` (puro); si es `kSuccess`, `appState_.clickBalance` -= precio y `appState_.ownedEntitlements` += la autorización, en el mismo struct; se marca dirty y se **flushea de inmediato** (un solo write atómico — DEC-126). Sin `kSuccess` no se toca nada. |
| Fin de drag | `appState_.lastWindowPosition` se setea a la posición final de la ventana; se marca el scheduler como dirty. |
| Despertar del event loop, deadline de flush alcanzado | `FlushPersistedState()` — no hace nada salvo que esté realmente dirty. |
| Shutdown limpio | `FlushPersistedState()` incondicionalmente (ignora el deadline; sigue sin hacer nada si no hay nada dirty). |

**Sin validación de límites de pantalla/monitor.** Una posición de
ventana guardada se restaura exactamente como se guardó, aunque la
configuración de pantalla haya cambiado desde entonces (se desconectó
un monitor, cambió la resolución). No se intenta en este bloque — ver
las limitaciones del informe de Block 03.

**Linux/Wayland (Block 04.1) no puede aplicar la posición guardada en
absoluto** — no es una limitación de validación de límites, es que el
protocolo `xdg-shell` no tiene ningún mecanismo para que un cliente
pida una posición absoluta para una toplevel normal (ver
`docs/LINUX_PLATFORM.md` §3.3/§6). La coordenada se guarda y se
preserva igual que en cualquier otra plataforma — `SpikeApp::Init()`
solo deja de poder *aplicarla* ahí, y lo loguea explícitamente en vez
de fallar en silencio.

## 8. Testeabilidad

`src/persistence` no tiene ninguna dependencia de SDL, así que cada
pieza es directamente testeable sin display:

- `tests/AppStateSerializerTest.cpp` — tests puros de round-trip/
  determinismo/corrupción contra buffers de bytes en memoria (10
  casos).
- `tests/AppStateStoreTest.cpp` — directorios temporales reales y
  aislados (creados frescos por test, eliminados después — nunca la
  ubicación real por usuario), cubriendo defaults, round-trip,
  atomicidad (sin archivo `.tmp` sobrante), recuperación ante
  corrupción, y manejo de fallos de escritura. Los fallos de escritura
  se simulan pre-creando un *directorio* exactamente en la ruta que
  `Save()` usaría para su archivo temporal — abrir un directorio para
  escritura como si fuera un archivo regular falla de forma uniforme
  en cada plataforma que este proyecto soporta, evitando trucos
  frágiles y dependientes de la plataforma basados en permisos de
  `chmod` (POSIX y Windows difieren lo suficiente en eso como para
  volverlo inestable en CI).
- `tests/PersistenceSchedulerTest.cpp` — comportamiento de debounce/
  coalescencia/reintento con timestamps fabricados (8 casos).
- `tests/PersistenceIntegrationTest.cpp` — la misma secuencia de
  click/drag/flush/shutdown que `SpikeApp` realmente ejecuta, conectada
  con `AppStateStore` + `PersistenceScheduler` reales (de directorio
  temporal), el mismo patrón que `tests/ClickAccountingTest.cpp`
  estableció en Block 02 para la clasificación de click/drag.

Los cuatro corren a través de la misma invocación de `ctest` que
cualquier otro test de este repositorio; ninguno requiere un display ni
el directorio real de app-data.

## 9. Un bug de capacidad de respuesta ante shutdown, encontrado y corregido por los propios tests de este bloque

Mientras se construían los smoke tests no interactivos que requiere
este bloque (la restricción de "sin QA manual" de §6), salió a la luz
un problema real y preexistente de latencia:
`SDL_WaitEventTimeout` no se interrumpe por sí misma ante un
`SIGINT`/`SIGTERM` entregado en esta plataforma — el event loop solo
vuelve a chequear `ShutdownRequested()` cuando su *propia* espera
efectivamente retorna (un evento real, o que venza el timeout
solicitado). Una vez que un tramo de idle verdaderamente estático no
tiene nada más programado por minutos (quedando solo el deadline de
acción pasiva de ~300s), una señal de terminación podía tardar hasta
ese tanto en notarse — presente técnicamente desde Block 01/02, solo
que nunca ejercitado por un test que dejara a la app asentarse del todo
antes de enviarle la señal.

Corregido acotando la espera máxima del event loop a 1000ms
(`kMaxWaitMs` en `src/app/SpikeApp.cpp`), sin importar cuán lejos esté
el próximo deadline real. Esto acota la latencia de shutdown a
aproximadamente un segundo sin reintroducir un tick de render: un
despertar que no encuentra nada que hacer (`ShutdownRequested()` en
false, ningún deadline realmente alcanzado) no hace ningún trabajo de
redraw/hit-mask/disco antes de volver a dormir — confirmado
re-midiendo el CPU en idle estático después del fix (sigue en ≈0.0%,
ver `docs/PERFORMANCE_BUDGETS.md`). Ver `docs/DECISION_LOG.md`
DEC-028.

## 10. Intencionalmente no implementado

- **Sin historial de transacciones.** Solo se persiste el estado FINAL
  del wallet: `clickBalance` + `ownedEntitlements`. No hay log de
  compras, timestamps, ni precios pagados (los precios viven en el
  catálogo, no en el estado del usuario).
- **Sin estado de onboarding / selección de starter.** La arquitectura
  de autorizaciones (`ownedEntitlements` con variantes) lo soporta,
  pero Block 07 no lo escribe (Block 09).
- **Sin validación de límites de pantalla/monitor para la posición de
  ventana** (ver §7).
- **Sin encriptación, sin sincronización en la nube, sin cuenta.**
  Almacenamiento puramente local, sin autenticación, de un único
  archivo — ver AGENTS.md §5 y `docs/PRIVACY_SECURITY.md`.

*(Histórico: hasta Block 06 esta sección decía "sin migración de
schema" y "sin economía/compra". Ambas cosas cambiaron — la migración
hacia adelante existe desde Block 06 / DEC-109 y se generalizó en Block
07 / DEC-124; el Shop y el gasto de clicks existen desde Block 07.)*
