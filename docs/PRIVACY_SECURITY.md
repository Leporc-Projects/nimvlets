# Nimvlets — Privacy & Security

## A. Standard interaction flow (implemented, Block 01)

The spike (and every future block's standard path) only ever reads
input aimed at Nimvlets itself:

- SDL window events (mouse button/motion, window close) delivered
  because the cursor is over our own window.
- The current global cursor **position** — `SDL_GetGlobalMouseState()`
  on all platforms (backed by `NSEvent.mouseLocation` polling on macOS,
  `GetCursorPos` on Windows) — polled once per idle-animation tick
  (~every 83 ms) purely to decide click-through state. This is a
  position *query*, not an event *hook*: it does not require
  Accessibility, Input Monitoring, or any other elevated permission,
  and it never sees keyboard input, other windows' content, or which
  application is focused.

No permissions beyond ordinary windowing are requested by this flow. No
Accessibility. No Input Monitoring. No Screen Recording. No admin/root.

**Actualización (Block 05, pasada de estabilización — DEC-086).** El
mecanismo de click-through de macOS cambió; el perfil de privacidad
**no**. Dos precisiones sobre el texto de arriba:

- La cadencia "~cada 83 ms" ya no describe el comportamiento real. El
  muestreo de posición de cursor ahora se **arma solo mientras el
  cursor está dentro del rectángulo de nuestra ventana** (ver
  `src/core/ClickThroughPolicy.h`): con el cursor en cualquier otro
  lugar de la pantalla no se consulta la posición del cursor **en
  absoluto**. Es estrictamente menos consulta que antes, no más.
- Nimvlets ahora intercepta `-setIgnoresMouseEvents:` **de su propia
  ventana**, dentro de **su propio proceso**, para que el backend Cocoa
  de SDL no pise su decisión per-pixel. Eso es configuración de una
  ventana propia vía el runtime de Objective-C — **no** es un hook de
  input, no observa eventos de nadie más, no lee contenido de otras
  ventanas ni qué aplicación está enfocada, y no requiere ningún
  permiso de TCC.

Sigue sin pedirse: Accessibility, Input Monitoring, Screen Recording,
admin/root. Sigue sin instalarse ningún monitor global de `NSEvent`
(que era una alternativa técnicamente viable para este problema y se
descartó explícitamente por esta razón — ver DEC-086 y AGENTS.md
§5/§14).

## B. Global click mode — IMPLEMENTED in Block 11A (opt-in)

> **Estado.** Esta sección describía, desde Block 01, una feature
> **futura y prohibida**. El brief de Block 11A la autorizó y la
> implementó. Los requisitos de abajo eran condiciones *previas* al
> envío; ahora son **contratos permanentes cumplidos**, y se verifican
> contra la fuente real en `tools/test_asset_pipeline.py`
> (`PrivacyInvariantTest`). Ver **§H** para el estado real de lo
> implementado, y `docs/GLOBAL_CLICK_MODE.md` para el diseño completo.

Una feature separada y opt-in deja al usuario contar clics en cualquier
parte del sistema, no solo sobre el Nimvlet. Requisitos que debían
cumplirse **antes** de poder enviarla — y que se cumplen:

- The exact OS permission it needs, and exactly what it observes, must
  be explained to the user before it's requested.
- **Mouse-only.** Never keyboard.
- **No screen capture.** Ever.
- **No content inspection** (no reading what's on screen, no reading
  window titles/text).
- **No app-name tracking** (never records which application was
  focused/clicked).
- **No coordinate or click-history storage.** The only functional
  output is incrementing a counter; raw event data is never persisted.
- Implemented as a separate, clearly bounded module — not folded into
  the standard interaction path.

**Histórico (Block 01–10).** Durante diez bloques esto no estuvo
implementado y ningún permiso — Accessibility, Input Monitoring, ni
ningún otro — se pidió en ninguna parte del código; la garantía
grep-eable era que `src/` no contenía ninguna referencia a `CGEventTap`,
`AXIsProcessTrusted`, `SetWindowsHookEx` ni ninguna otra API de hook
global.

**Desde Block 11A** esa garantía se vuelve más específica en vez de
desaparecer: `CGEventTapCreate` y las APIs de permiso de Input
Monitoring existen en **exactamente un archivo**
(`src/platform/macos/GlobalClickMonitor.mm`) y en ningún otro;
`AXIsProcessTrusted`, `AXUIElement`, `SetWindowsHookEx`,
`WH_KEYBOARD_LL`, `CGEventPost` y toda API de captura de pantalla o de
enumeración de apps **siguen sin aparecer en ningún lugar de `src/`**.
Ver §H.

## C. Explicit non-goals (every block, unless one is explicitly revised)

Nimvlets does not, and this block does not add anything that:

- captures the screen or any window's contents;
- logs keyboard input, globally or otherwise;
- logs mouse clicks globally (see §B/§H — desde Block 11A existe un
  modo OPT-IN, apagado por defecto, que **cuenta** —no registra—
  pulsaciones del botón primario: sin coordenadas, sin timestamps, sin
  historial, sin app, sin contenido; su única salida funcional es
  incrementar el contador);
- enumerates other running applications for behavioral purposes;
- opens network sockets or makes network requests (Block 01: zero
  network activity, confirmed in the Block 01 report);
- sends telemetry of any kind;
- downloads assets at runtime;
- requires an account or a subscription;
- writes data anywhere beyond: build output, the process's own working
  directory (what SDL/AppKit/Win32 themselves touch to run a window),
  and — since Block 03 — exactly one local, per-user app-data file
  (see §E). No other location is ever written to.

## D. Block 01 self-check

See the Block 01 report's "Privacy / Permissions" section for the
concrete yes/no confirmation (Accessibility / Input Monitoring / Screen
Recording / admin-root / global hooks / network — all "no") as actually
observed for this block's spike executable.

## E. Persistencia local de estado (Block 03)

Block 03 agrega exactamente un archivo local, sin autenticación, por
usuario — ver `docs/PERSISTENCE.md` para el diseño completo. Relevante
para este documento específicamente:

- **Sin cuenta, sin nube, sin red** — el archivo se escribe y lee solo
  mediante llamadas ordinarias de filesystem local (`std::filesystem`,
  `<fstream>`); nada en `src/persistence` abre un socket ni hace una
  llamada de red (verificable por grep: sin `socket(`, `curl`,
  `http(s)://`, `SDL_net`, ni similares en ningún lugar de `src/`).
- **Sin telemetría, sin analítica, sin seguimiento de procedencia** —
  el archivo contiene solo click balance, id de pet/variante activos, y
  última posición de ventana (la moneda de AGENTS.md §2 y el alcance
  propio de este bloque; ver `docs/PERSISTENCE.md` §1 para exactamente
  qué se guarda y qué no).
- **La ubicación es el directorio estándar del propio sistema operativo
  para app-data por usuario** (`SDL_GetPrefPath()`), no una ruta oculta
  ni inusual — el mismo tipo de ubicación que usa cualquier app de
  escritorio nativa convencional para preferencias/datos guardados
  locales.
- **Nunca se hardcodea ni se comitea una ruta absoluta específica de
  una máquina** — la ruta real es un valor de runtime que viene de
  `SDL_GetPrefPath()`; el override `NIMVLETS_DEV_APPDATA_DIR` usado
  para testing es una variable de entorno, nunca una ruta literal en el
  código fuente.

## F. Linux (Block 04.1)

Ver `docs/LINUX_PLATFORM.md` para el diseño completo. Relevante para
este documento específicamente: el soporte de Linux no requiere
**ninguno** de los permisos que este documento ya prohíbe en general
(§C) -- confirmado explícitamente para X11 y Wayland, no solo asumido
por herencia de las reglas de arriba:

- **Sin captura de pantalla.** Ninguna extensión X11 usada
  (XShape/XInput2/XRandr, ver `docs/LINUX_PLATFORM.md` §2) lee
  pixeles de otras ventanas ni de la pantalla; son extensiones
  ordinarias de hit-testing/input/consulta de modo de video, sin
  diálogo de permiso alguno en ningún escritorio Linux estándar.
- **Sin Accessibility/Input Monitoring equivalentes.** X11 no tiene
  ningún concepto de permiso análogo a los de macOS para lo que este
  bloque usa; Wayland es, si acaso, *más* restrictivo por diseño de
  protocolo (de ahí que ciertas cosas -- always-on-top, restauración
  de posición, click-through real -- ni siquiera sean posibles para
  ninguna app, con o sin permiso -- ver `docs/LINUX_PLATFORM.md` §6).
- **Sin root/admin.** Xvfb y Weston (usados en el smoke de CI, ver
  `docs/LINUX_PLATFORM.md` §9) corren sin privilegios elevados; nada
  en `src/platform/linux/` ni en `cmake/FetchSDL3.cmake` requiere
  `sudo` en tiempo de ejecución (solo la instalación de paquetes apt en
  CI, que es responsabilidad del runner, no de la app).
- **Sin red/telemetría en runtime**, igual que en toda otra
  plataforma -- grep-verificable en `src/platform/linux/` igual que en
  el resto de `src/` (§C de este documento).

## G. Product UI + menú rápido (Block 06)

Block 06 agrega una ventana de aplicación normal (Collection) y un
`NSStatusItem` con menú rápido en macOS. **No introduce ningún permiso
nuevo** (brief §20), grep-verificable igual que el resto de `src/`:

- **Sin captura de pantalla.** El producto nunca captura la pantalla,
  bajo ninguna circunstancia — el contrato permanente de §C. Las
  capturas de QA de este bloque son diagnóstico de DESARROLLO de
  nuestra propia ventana (AGENTS.md §5): nunca se comitean, nunca son
  comportamiento del producto, nunca automatizadas dentro de él.
- **Sin Accessibility / Input Monitoring / Screen Recording.** Un
  `NSStatusItem` y un `NSMenu` son UI de nuestra propia app. Core Text
  (`platform::RasterizeText`) dibuja en un `CGBitmapContext` de nuestra
  propia memoria. `-[NSApplication activateIgnoringOtherApps:]`
  (`platform::BringApplicationToForeground`, para que la ventana de
  producto reciba teclado) no dispara ningún diálogo de TCC. Ningún
  hook de input global (AGENTS.md §5/§14).
- **Sin conteo global de clicks.** El click balance sigue
  incrementándose SOLO por un click real sobre el pet. Este bloque lo
  MUESTRA dentro del Product UI pero no puede gastarlo (no hay Shop
  todavía) y no lo expone junto al pet cuando el Product UI está
  cerrado (brief §13).
- **Sin cuenta, sin red, sin telemetría, sin sync.** Igual que siempre.
  El nuevo estado persistido (propiedad + preferencias del menú) va al
  mismo único archivo local sin autenticación de §E.

## H. Conteo de clics global, OPT-IN (Block 11A)

Ver `docs/GLOBAL_CLICK_MODE.md` para el diseño completo. Lo relevante
para este documento:

**Qué observa.** Exclusivamente **pulsaciones del botón primario al
bajar**, en todo el sistema, y solo mientras el owner lo activó. La
única salida funcional es **+1 al contador de clics**.

**Qué NO observa — nunca, en ningún modo:**

| | |
|---|---|
| teclado | pixeles de pantalla / capturas |
| contenido o títulos de ventanas | aplicación activa o enfocada |
| nombres de procesos/apps | URLs, texto, portapapeles |
| coordenadas del puntero | timestamps de clics |
| trayectorias | historial de clics |
| estadísticas por app | contenido de la rueda de scroll |

El botón derecho, el medio y el scroll ni siquiera están en la máscara
del event tap: no hay nada que "descartar".

**La firma del callback es la garantía estructural**, no una promesa de
prosa:

```cpp
using GlobalPrimaryClickCallback = void (*)(void* userData);
```

No hay dónde poner una coordenada.

**Permiso.** En macOS: **Input Monitoring** (TCC). **NO** Accessibility
(`AXIsProcessTrusted` / `AXUIElement` siguen prohibidos en todo `src/`),
**NO** Screen Recording, **NO** admin/root. Se pide desde **un solo
call site**, solo tras un "Continue" explícito del owner sobre una
explicación de primera parte, y **jamás al arrancar** — ni siquiera
cuando el estado guardado dice `anywhere`: ahí se hace *preflight* (que
nunca muestra un diálogo) y, si no está concedido, se cae a conteo local
y Settings lo informa.

**Listen-only.** El clic del usuario nunca se modifica, suprime, retrasa
ni consume.

**Qué se persiste.** Solo la preferencia del owner
(`AppState::clickCountingMode`: `""` / `"nimvlet_only"` / `"anywhere"`,
schema v6). Nada más: ni coordenadas, ni timestamps, ni historial, ni
contadores por fuente, ni el estado del permiso. El default de producto
y de migración es **Nimvlet only** — ningún usuario existente queda con
conteo global habilitado por actualizar.

**Sin red, sin telemetría, sin nube, sin cuenta**, igual que siempre.

**Módulo separado.** Vive en su propia frontera
(`src/platform/GlobalClickMonitor.h` + un adapter por OS), deliberadamente
fuera de `TransparentWindowSupport` / `SystemShell` / el input normal de
SDL.

**Verificado por tests, no solo escrito.** `tools/test_asset_pipeline.py`
(`PrivacyInvariantTest`) chequea contra la fuente real que el event tap
y las APIs de permiso viven en un único archivo, que el tap es
listen-only, que su máscara es exactamente `kCGEventLeftMouseDown` **y
que hay un solo `CGEventMaskBit(`** —ni una máscara OR-eada ni un
segundo evento colado, ni `kCGEventMaskForAllEvents`—, que el callback
no lee coordenadas/flags/timestamp/proceso destino, que el pedido de
permiso ocurre una sola vez, que el callback de reenvío no tiene
parámetros de datos, y que `AppState` no persiste nada más que el modo.
El guard mide **código**, no comentarios.

### H.1 La redacción de macOS es AMPLIA; nuestra máscara no

En la QA física del owner, macOS pidió el permiso con una redacción del
estilo *"would like to receive keystrokes from any application"*, y
Ajustes del Sistema describe Monitorización de entrada en términos de
teclado. **Eso no amplía nada de lo de arriba.** Es el texto de la
categoría de TCC completa: Apple no ofrece una variante "solo mouse", no
podemos cambiar su redacción, y no insinuamos que podamos. Lo que acota
el alcance real es el código —una máscara de un solo evento, listen-only,
un callback sin payload—, y eso es lo que los guards fijan.

La explicación de primera parte que Nimvlets muestra **antes** de pedir
el permiso ahora dice justamente eso, para que el owner no se encuentre
la discrepancia sin contexto; y el recordatorio de alcance se repite en
los estados en los que la entrada del permiso está viva en Ajustes del
Sistema. Ver `docs/GLOBAL_CLICK_MODE.md` §5.1.

### H.2 Identidad de la app en el permiso — DEV vs RELEASE

En DESARROLLO el binario es un Mach-O suelto, firmado ad-hoc y **sin
ningún bundle** (`Info.plist=not bound`, sin `MACOSX_BUNDLE` en el
CMake), así que TCC atribuye el permiso al **proceso responsable** — la
app que lanzó la terminal desde la que se ejecutó. Por eso el owner vio
**"Antigravity IDE"** y no "Nimvlets" en la lista de Monitorización de
entrada. Es consistente con la topología actual, no un bug de identidad:
no hay identidad de app que pudiera estar mal.

**La identidad del permiso en RELEASE no está verificada, y no se
declara verificada.** Cuando exista un `Nimvlets.app` real y firmado,
hay que repetir el flujo completo desde ese bundle y confirmar que macOS
identifica a Nimvlets en el diálogo y en Ajustes del Sistema. Este
bloque no arma firma/empaquetado, no toca bases de datos de TCC, no usa
`tccutil` ni `sudo`. Ver `docs/GLOBAL_CLICK_MODE.md` §5.2 y
`docs/PLATFORM_SPIKE.md`.
