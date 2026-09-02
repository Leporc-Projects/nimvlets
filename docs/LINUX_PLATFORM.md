# Nimvlets — Soporte de plataforma Linux (Block 04.1)

Este documento describe cómo Linux se convirtió en un target de
escritorio de primera clase para Nimvlets en Block 04.1, sin cambiar
ningún feature de producto (block brief: "make Linux a first-class
desktop target ... without changing product features"). Complementa,
no reemplaza, a `docs/PLATFORM_SPIKE.md` (el registro de QA de Block
01, específico de macOS/Windows) y a `docs/DECISION_LOG.md` (DEC-033 en
adelante, para las decisiones de este bloque con su fecha/estado).

## 1. Alcance

- **Arquitectura soportada:** solo **x86_64**. No se agregó ni se
  probó ningún target `arm64`/`aarch64` de Linux en este bloque.
- **Backends de escritorio:** **X11** y **Wayland**, ambos detectados
  en runtime vía `SDL_GetCurrentVideoDriver()` -- nunca forzados (el
  brief es explícito: "Do not force X11 globally"). Ver §3.
- **Fuera de alcance, explícitamente:** empaquetado/distribución
  (`.deb`, `.rpm`, Flatpak, AppImage, etc.), cualquier arquitectura que
  no sea x86_64, y cualquier backend de escritorio Linux que no sea
  X11/Wayland (p. ej. un framebuffer directo).
- **Sin cambio de producto:** ningún feature nuevo, ninguna UI nueva.
  Este bloque es puramente habilitar la plataforma -- ver AGENTS.md
  §16 ("Don't build ahead of scope").

## 2. Build: presets, generador, dependencias

Dos presets nuevos en `CMakePresets.json` (mismo patrón que los
existentes de macOS/Windows: `configurePresets`/`buildPresets`/
`testPresets`, gateados por `condition.lhs = "${hostSystemName}"`,
`rhs = "Linux"`):

```bash
cmake --preset linux-debug      # o linux-release
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
```

**Generador: Ninja**, explícito (`"generator": "Ninja"`) -- el brief
pide preferir Ninja "unless the existing build gives a concrete reason
not to"; no se encontró ninguna razón para no hacerlo (los presets de
macOS no fijan un generador explícito, dejando el default de CMake; los
de Windows sí fijan uno explícito, `"Visual Studio 17 2022"`, por la
misma razón práctica: Linux necesita `ninja-build` instalado, igual que
CI instala explícitamente).

### Paquetes de desarrollo Ubuntu/Debian (los que usa CI)

```bash
sudo apt-get install -y --no-install-recommends \
  ninja-build pkg-config \
  libx11-dev libxext-dev libxi-dev libxrandr-dev \
  libwayland-dev libwayland-bin libxkbcommon-dev
```

(`build-essential`/`cmake` no se listan porque las imágenes de runner
`ubuntu-24.04` de GitHub Actions ya traen gcc/g++/cmake preinstalados
-- ver `.github/workflows/ci.yml`; instalarlos ahí también sería
redundante, no incorrecto, si corrés esto en una máquina Linux propia
que no los tenga, instalalos igual.)

Esta lista es deliberadamente más chica que "todas las features" que
`docs/README-linux.md` de la propia fuente de SDL documenta (audio
ALSA/PulseAudio/PipeWire, joystick, haptics, IBus, DBus, etc.) -- el
brief pide "keep dependencies minimal", y `SpikeApp::Init()` solo
llama `SDL_Init(SDL_INIT_VIDEO)`, así que nada de eso hace falta. Lo
que sí se necesita, y por qué, está documentado en
`cmake/FetchSDL3.cmake`:

| Paquete | Para qué |
|---|---|
| `libx11-dev` | X11 core (`SDL_X11`) |
| `libxext-dev` | Extensión XShape -- el mecanismo real de click-through en X11 (ver §4) |
| `libxi-dev` | XInput2 (`SDL_X11_XINPUT`) -- ruta moderna de entrada de mouse/teclado |
| `libxrandr-dev` | Consulta de escala/DPI por monitor con precisión (`SDL_X11_XRANDR`) |
| `libwayland-dev` | Cliente Wayland (`SDL_WAYLAND`) -- headers + `.pc` |
| `libwayland-bin` | Provee el binario `wayland-scanner` que el build de SDL necesita |
| `libxkbcommon-dev` | Manejo de teclado/keymap, usado tanto por X11 como por Wayland |

**No instalado a propósito:** `libdecor-0-dev` (nuestra ventana siempre
se crea con `SDL_WINDOW_BORDERLESS` -- ver `src/app/SpikeApp.cpp` --
así que la ruta de decoración cliente-side de Wayland,
`WAYLAND_SHELL_SURFACE_TYPE_LIBDECOR` en la fuente de SDL, nunca se
selecciona en runtime; `SDL_WAYLAND_LIBDECOR` se apaga explícitamente
en `cmake/FetchSDL3.cmake`), `wayland-protocols` (SDL 3.4.12 vendorea
su propia copia de los XML de protocolo bajo `wayland-protocols/`
dentro de su propio árbol fuente -- no depende del paquete del
sistema), y `libgl1-mesa-dev`/`libegl1-mesa-dev` (OpenGL es un feature
"soft" en el CMake de SDL: si los headers no están, se desactiva sin
fallar el configure -- y Xvfb no tiene GPU real de todos modos, así
que el renderer de software es lo que efectivamente corre en el smoke
de CI).

Cada sub-feature X11 que SDL deja en ON por defecto (`SDL_X11_XCURSOR`,
`SDL_X11_XDBE`, `SDL_X11_XFIXES`, `SDL_X11_XSCRNSAVER`,
`SDL_X11_XSYNC`, `SDL_X11_XTEST`) se apaga explícitamente en
`cmake/FetchSDL3.cmake` para Linux: ninguna la usa este proyecto, y
dejarla en ON con el paquete de desarrollo correspondiente ausente
**falla el configure con FATAL_ERROR** en la fuente pineada
(`SDL_missing_dependency()`, `cmake/macros.cmake`) -- no un simple
warning que se salta en silencio, como sí ocurre en otras plataformas.
Apagarlas de antemano evita tanto ese riesgo como paquetes apt de más.

**No se cambió la versión pineada de SDL** (`release-3.4.12`, ver
`cmake/FetchSDL3.cmake`) -- no hizo falta ningún fix/feature de una
versión más nueva para nada de este bloque.

## 3. Investigación: ¿alcanza SDL solo, o hace falta código nativo?

El brief pide explícitamente, antes de escribir cualquier código nativo
X11/Wayland: "inspect the pinned SDL3 implementation and document why
SDL alone is insufficient." Esto se hizo leyendo directamente el árbol
fuente de SDL 3.4.12 que trae `cmake/FetchSDL3.cmake` (bajo
`build/*/​_deps/sdl3-src/`, el mismo método ya establecido en Block 01
para el hallazgo de macOS -- ver `docs/PLATFORM_SPIKE.md` §5.1),
**nunca** asumiendo comportamiento de reportes de terceros. Hallazgos,
uno por uno:

### 3.1 Transparencia, always-on-top, not-focusable: SDL alcanza solo

`SDL_WINDOW_TRANSPARENT` ya lo maneja el propio backend de creación de
ventana tanto en X11 (`src/video/x11/SDL_x11window.c`) como en Wayland
(`SetSurfaceOpaqueRegion` en `src/video/wayland/SDL_waylandwindow.c`)
sin ningún llamado nativo adicional -- a diferencia de macOS, que sí
necesitó `NSWindow.opaque`/`backgroundColor`/`hasShadow` explícitos
(ver `src/platform/macos/TransparentWindowSupport.mm`).
`SDL_WINDOW_ALWAYS_ON_TOP` lo aplica `X11_CreateWindow` directamente en
X11. `SDL_WINDOW_NOT_FOCUSABLE` lo traduce X11 a WM hints ICCCM
estándar (`wmhints->input = False`). Conclusión: **`ConfigureCompanionWindow()`
en Linux no necesita ningún llamado Xlib/Wayland nativo para estos tres
requisitos** -- ver `src/platform/linux/TransparentWindowSupport.cpp`.

### 3.2 Click-through / shape: X11 sí, Wayland no, con la SDL pineada

`SDL_SetWindowShape()` despacha a través de un puntero de función por
driver (`device->UpdateWindowShape`, `src/video/SDL_sysvideo.h`):

- **X11: SÍ está wireado.** `X11_UpdateWindowShape`
  (`src/video/x11/SDL_x11shape.c`) llama a
  `X11_XShapeCombineMask`/`XShapeCombineRegion` con `ShapeInput` -- la
  extensión XShape aplicada a la región de *entrada* del mouse, nunca a
  los pixeles renderizados. Misma familia exacta que
  `Cocoa_UpdateWindowShape` en macOS (§5.1 de
  `docs/PLATFORM_SPIKE.md`): render-safe, event-driven, sin polling.
  Confirmado por lectura directa del código, no asumido por analogía.
- **Wayland: NO está wireado, en absoluto.** No existe ningún
  `SDL_waylandshape.c` en el árbol fuente, y
  `src/video/wayland/SDL_waylandvideo.c` nunca asigna
  `device->UpdateWindowShape`. Llamar `SDL_SetWindowShape()` en Wayland
  con esta SDL pineada no tiene ningún efecto.

Esto es **la** pieza de evidencia central de este bloque: confirma que
el fallback poll-driven de Windows (`SetWindowClickThrough()`
alternando una propiedad nativa binaria de "ignorar todo el input")
tampoco tiene ningún análogo Wayland disponible con la API pública de
SDL 3.4.12, por dos hechos adicionales de la misma fuente:

1. `Wayland_GetGlobalMouseState()` (`src/video/wayland/SDL_waylandmouse.c`)
   solo retorna una posición real `if (mouse->focus)` -- mientras el
   cursor ya está sobre nuestra propia ventana. Esto en sí *no* es un
   problema (nuestra ventana no tiene input region restringida, así
   que el compositor entrega el evento de "pointer enter" en cuanto el
   cursor toca el rectángulo completo de la ventana, transparente o
   no) -- pero confirma que no hay forma de sondear la posición global
   *fuera* de nuestra ventana, que es justo lo que un mecanismo tipo
   `ignoresMouseEvents` necesitaría para saber cuándo "reactivarse".
2. `wl_surface_set_input_region()` (el mecanismo real de Wayland para
   restringir qué parte de una superficie recibe input) solo se usa
   internamente en `src/video/wayland/SDL_waylandwindow.c` para
   ventanas `SDL_WINDOW_TOOLTIP`, y SDL no expone el `wl_compositor`
   subyacente para que código externo cree su propia `wl_region`. La
   única forma de lograr esto sería hablar el protocolo Wayland en
   crudo (un `wl_registry` propio, en paralelo a la cola de eventos que
   SDL ya administra sobre el mismo `wl_display`) -- una pieza de
   complejidad real, sin hardware Linux disponible en este bloque para
   verificarla, y con riesgo genuino de interferir con el propio
   despacho de eventos de SDL. Se decidió explícitamente no
   construirla -- ver §12 (decisiones fuera del prompt) y AGENTS.md
   §16.

**Conclusión, aplicada en `src/platform/LinuxBackendPolicy.h`/`.cpp`**
(lógica pura, testeada -- ver §8): en X11, `NativeShapeHitTestIsRenderSafe()`
retorna `true` (mismo tier que macOS). En Wayland, retorna `false`, y
además `ClickThroughPollingIsMeaningful()` (una función nueva de este
bloque -- ver §4) retorna `false` también, porque ningún polling
lograría nada ahí.

### 3.3 Posición de ventana: X11 sí, Wayland no (por protocolo, no por SDL)

`Wayland_SetWindowPosition()` (`src/video/wayland/SDL_waylandwindow.c`)
retorna literalmente `SDL_SetError("wayland cannot position non-popup
windows")` para cualquier `xdg_toplevel` -- una limitación del
protocolo `xdg-shell` en sí (ningún cliente puede pedir una posición
absoluta de pantalla para una toplevel normal; es una decisión de
diseño deliberada del protocolo, por motivos de aislamiento entre
apps/compositor), no un bug de SDL ni de este proyecto.
`X11_SetWindowPosition()` sí llama `XMoveWindow` directamente y
funciona igual que en cualquier otra plataforma. Ver §6.

## 4. Arquitectura del adapter Linux

Dos piezas nuevas, siguiendo el seam ya establecido en
`src/platform/TransparentWindowSupport.h` (AGENTS.md §3: "src/app
never contains an #ifdef for this"):

- **`src/platform/LinuxBackendPolicy.h`/`.cpp`** -- lógica **pura**
  (sin SDL, sin X11, sin Wayland): un `enum class LinuxVideoBackend
  { kX11, kWayland, kOther }`, un parser de string
  (`ParseLinuxVideoBackend`, mapea el string exacto que
  `SDL_GetCurrentVideoDriver()` retorna), y tres tablas de política
  (`LinuxBackendSupportsNativeShapeHitTest`,
  `LinuxBackendClickThroughPollingIsMeaningful`,
  `LinuxBackendSupportsPositionRestore`), cada una citada contra la
  evidencia de §3. Vive **fuera** de `src/platform/linux/`
  deliberadamente -- ese directorio solo se compila en Linux (ver
  `CMakeLists.txt` raíz), pero esta lógica no necesita ningún header de
  plataforma, así que se compila y testea en **cualquier host**,
  incluido este dev machine macOS (`src/platform/CMakeLists.txt`
  agrega `nimvlets_platform_policy` incondicionalmente, igual que
  `nimvlets_core`) -- ver §8.
- **`src/platform/linux/TransparentWindowSupport.cpp`** -- la
  implementación real del seam compartido: detecta el backend activo
  vía `SDL_GetCurrentVideoDriver()` una vez por llamado (barato,
  retorna un puntero a un string estático de SDL) y delega toda
  decisión a `LinuxBackendPolicy`. `ConfigureCompanionWindow()` no
  necesita ningún llamado nativo (§3.1) -- solo loguea el backend
  resuelto y, en Wayland, la limitación de always-on-top.
  `SetWindowClickThrough()` está implementada de forma honesta pero es
  código muerto en el wiring actual (ver el punto siguiente): nunca
  finge haber aplicado click-through, solo retorna `false` con un log
  explicativo una única vez.

### `ClickThroughPollingIsMeaningful()`: una extensión nueva del seam compartido

Antes de este bloque, `src/app/SpikeApp.cpp` decidía entre el path
nativo de shape y el fallback de polling con un solo booleano
(`usingNativeShapeHitTest_` = `platform::NativeShapeHitTestIsRenderSafe()`).
Eso alcanzaba con dos plataformas porque "no hay shape nativo" y "el
polling sí funcionaría" siempre coincidían (Windows). Linux/Wayland
rompe esa coincidencia: **ni el shape nativo ni el polling logran
nada** ahí (§3.2). Correr igual el scheduler de ~60Hz de
`hoverScheduler_`, sabiendo de antemano que `SetWindowClickThrough()`
nunca podría cambiar nada, sería exactamente el "polling loop"
permanente que el brief §8 y AGENTS.md §2 ("event-driven scheduling")
prohíben.

Por eso se agregó `platform::ClickThroughPollingIsMeaningful()` al
header compartido (con una implementación trivial, correcta y
documentada en macOS/Windows también -- ver §12), y un nuevo miembro
`usingPollDrivenClickThrough_` en `SpikeApp` (`!usingNativeShapeHitTest_
&& platform::ClickThroughPollingIsMeaningful()`) que reemplaza los dos
sitios donde `SpikeApp::Run()` decidía "pollear o no" en base al
booleano viejo. Resultado neto:

| Plataforma/backend | Shape nativo | Polling útil | Mecanismo real |
|---|---|---|---|
| macOS | sí | (irrelevante) | `SDL_SetWindowShape`, event-driven |
| Windows | no | sí | polling ~60Hz + `WS_EX_TRANSPARENT` (sin cambios) |
| Linux/X11 | sí | (irrelevante) | `SDL_SetWindowShape` (XShape), event-driven |
| Linux/Wayland | no | **no** | ninguno a nivel OS -- ver §6 |

`src/app/SpikeApp.cpp` sigue sin ningún `#ifdef` de plataforma: sigue
reaccionando únicamente a estas dos consultas de capacidad, exactamente
como antes de este bloque.

**Block 11B** agregó una tercera consulta con la misma forma:
`platform::AbsoluteWindowPositioningSupported()` (macOS/Windows `true`;
Linux delega en `LinuxBackendSupportsPositionRestore()`, la tabla pura ya
citada en §3.3). Settings la usa para deshabilitar "Reset position" en
Wayland (§6) -- sin `#ifdef`, sin una tabla nueva de `LinuxBackendPolicy`
(reusa `SupportsPositionRestore`).

## 5. Comportamiento X11

Con esta SDL pineada, X11 alcanza **paridad completa** con macOS en
todos los requisitos del brief §2: ventana transparente/borderless
real (SDL nativo), always-on-top real (`X11_SetWindowAlwaysOnTop`,
wireado), not-focusable real (WM hints ICCCM), click-through/hit-test
per-frame real y event-driven (`SDL_SetWindowShape` vía XShape,
render-safe -- confirmado por lectura de fuente, §3.2), y restauración
de posición real (`XMoveWindow`, §3.3). No se escribió ningún código
nativo Xlib adicional más allá de lo que `NativeShapeHitTestIsRenderSafe()`
y `ClickThroughPollingIsMeaningful()` ya necesitan consultar.

## 6. Comportamiento Wayland

Wayland recibe soporte de build y de código completo (SDL_WAYLAND=ON,
la misma ventana transparente/borderless, el mismo pipeline de
animación/persistencia/catálogo -- nada de eso es específico de
backend), pero con dos limitaciones **reales, del protocolo en sí, no
de este código** -- documentadas acá en vez de escondidas u
ocultadas con un hack:

- **Sin always-on-top.** El protocolo `xdg-shell` no tiene ningún hint
  de apilamiento (`stacking`) que un cliente pueda pedir para una
  toplevel normal -- confirmado porque `src/video/wayland/SDL_waylandwindow.c`
  nunca asigna `device->SetWindowAlwaysOnTop` en absoluto (a
  diferencia de X11, que sí lo wirea). Lograr esto en Wayland
  requeriría un protocolo específico del compositor (p. ej.
  `wlr-layer-shell`, solo disponible en compositores basados en
  wlroots -- no GNOME/Mutter, no KDE/KWin necesariamente) -- exactamente
  el tipo de "compositor-specific hack" que el brief prohíbe
  implementar solo para forzar esto.
- **Sin restauración de posición absoluta.** Ver §3.3.
  `appState_.lastWindowPosition` **se sigue guardando y preservando
  igual** (mismo formato NVSTATE1, sin ninguna variante Linux-only --
  ver §7) -- lo único que cambia es que, en Wayland, aplicarla al
  arrancar no tiene ningún efecto observable. `SpikeApp::Init()` ahora
  chequea el valor de retorno real de `SDL_SetWindowPosition()` (antes
  de este bloque no se chequeaba en absoluto) y loguea explícitamente
  cuándo no pudo aplicar una posición guardada, citando
  `SDL_GetError()` -- nunca falla, nunca crashea, y nunca finge
  silenciosamente que funcionó.

  **Block 11B -- "Reset position" de Settings.** La acción de recuperación
  de Settings (`docs/PRODUCT_UI.md` §20.7) también depende de colocar una
  toplevel en absoluto, así que Wayland tampoco puede. En vez de fingir,
  Settings la **deshabilita**: el botón `[ Reset position ]` se dibuja
  apagado -- fuera del hit-test y del anillo de foco, el mismo patrón que
  el segmento "Anywhere" sin capacidad del conteo global -- con la línea
  corta y sin alarma *"Position can't be reset on this system."* La
  decisión sale de `platform::AbsoluteWindowPositioningSupported()`, una
  extensión nueva del seam compartido `TransparentWindowSupport.h` que en
  Linux delega en la tabla pura y ya unit-testeada
  `LinuxBackendSupportsPositionRestore()` (X11 `true` / Wayland `false`,
  citada contra `Wayland_SetWindowPosition` en §3.3). El resto de Settings
  (Visibility, Size, Opacity, Lock, Language, Click counting) sigue
  funcional en Wayland. Ver DEC-141.
- **Sin click-through real a la app de abajo.** Ver §3.2 para la
  evidencia completa. En la práctica: un click sobre un pixel
  transparente dentro del rectángulo de la ventana **no llega** a lo
  que esté visualmente debajo (Wayland, a diferencia de X11/macOS, no
  tiene forma de restringir la input region de nuestra superficie con
  la API pública de esta SDL) -- pero tampoco se registra como click
  válido de Nimvlets: el chequeo `IsPointInteractive()` que ya existía
  en `SpikeApp::HandleEvent()` (contra la máscara alfa real del frame
  activo) sigue rechazando cualquier press fuera de la región opaca,
  así que el click simplemente se absorbe en silencio en vez de hacer
  algo incorrecto. Ningún crash, ninguna mentira -- un click sobre un
  área transparente en Wayland no hace nada visible, ni para Nimvlets
  ni para la app de abajo.

Ninguna de las tres es un bug de este código -- son límites reales del
window system, documentados como tales por instrucción explícita del
brief ("Do not claim parity where the window system itself imposes a
limitation").

## 7. Persistencia en Linux

Sin cambios de código en `src/persistence` -- se **verificó**, no se
reimplementó. `SDL_GetPrefPath("Leporc Projects", "Nimvlets")` en
Linux, según la propia documentación de SDL
(`include/SDL3/SDL_filesystem.h`), resuelve a algo con la forma
`~/.local/share/Leporc Projects/Nimvlets/` (basado en `$XDG_DATA_HOME`,
el mecanismo estándar de Linux para datos de usuario por app -- no una
ruta hardcodeada por este proyecto). El override solo-DEV
`NIMVLETS_DEV_APPDATA_DIR` sigue funcionando igual (es una variable de
entorno más el mismo `std::filesystem::create_directories`, ninguno de
los dos es específico de plataforma -- ver `src/app/SpikeApp.cpp`).
Click balance y pet/variante activos persisten igual que en
macOS/Windows; la posición de ventana persiste igual (§6 para la
restricción de *aplicarla* en Wayland). **Sin formato Linux-only**: el
mismo binario NVSTATE1 (`docs/PERSISTENCE.md` §3), little-endian, que
ya asume x86_64/arm64 -- Linux x86_64 encaja en esa asunción existente
sin ningún cambio.

## 8. Tests

`tests/LinuxBackendPolicyTest.cpp` (7 casos, corre en **cualquier**
host vía `nimvlets_platform_policy` -- ver §4): parseo del string de
driver (X11, Wayland, case-sensitive/exacto, desconocido, `nullptr`) y
las tres tablas de política (shape nativo, polling útil, restauración
de posición), cada caso confirmando que **solo X11** retorna `true`.
Deliberadamente **sin** ninguna aserción que dependa de un compositor
real corriendo (eso vive en el smoke de CI bajo Xvfb/Weston -- ver §9,
brief §7: "Keep compositor-dependent assertions out of pure unit
tests"). Corridos junto al resto de la suite (`ctest --preset
macos-debug`): 121/121 en este dev machine macOS, incluyendo estos 7
nuevos sobre el baseline de 114 de Block 04.

## 9. CI: matriz y smoke de Linux

`.github/workflows/ci.yml` agrega `linux-x64` (`ubuntu-24.04`, pineado
igual que `windows-2022` lo está para Windows -- una imagen exacta, no
`ubuntu-latest`), llevando la matriz a 4 jobs: `macos-arm64`,
`macos-universal2`, `linux-x64`, `windows-x64`. Los tres jobs
existentes no se tocaron.

El job `linux-x64` instala las dependencias mínimas de §2, configura +
compila + corre el CTest completo (bloqueante), y agrega dos pasos de
smoke GUI no interactivo, con tratamiento deliberadamente distinto:

- **X11 (Xvfb): bloqueante.** El brief lo pide sin condicionales
  ("non-interactive smoke under Xvfb, explicitly use
  SDL_VIDEODRIVER=x11"), y Xvfb+X11+SDL es un patrón de CI extremadamente
  estándar en todo el ecosistema -- riesgo bajo incluso sin poder
  correrlo en este bloque (ver la brecha de abajo).
- **Wayland (Weston headless): intento real, no bloqueante**
  (`continue-on-error: true`). Levanta `weston --backend=headless-backend.so`
  (el backend headless oficial de Weston, pensado para exactamente este
  uso -- CI/testing sin GPU/logind reales) y corre `nimvlets_spike`
  contra él con `SDL_VIDEODRIVER=wayland` explícito.

### Brecha de validación Wayland en CI (honesta, por instrucción explícita del brief)

Este bloque se ejecutó enteramente en un host macOS, sin ninguna
máquina Linux real disponible y sin poder ejecutar (`push`/`publish`
están prohibidos para esta sesión -- ver AGENTS.md §15) el workflow de
GitHub Actions para observar si realmente corre. **No se puede afirmar
que el smoke de Weston headless funcione en el runner real** -- hay
fricciones conocidas en el ecosistema más amplio con
`XDG_RUNTIME_DIR`/D-Bus/logind en imágenes CI mínimas que no se
pudieron descartar sin poder correr esto. Por instrucción explícita del
brief ("if reliable headless Wayland runtime validation is not
achievable, do not fake PASS; report the exact gap while keeping
Wayland build/code support intact"), el paso se dejó como intento real
y honesto, no bloqueante, con su resultado (`PASS`/`INCONCLUSIVE/FAIL`)
impreso explícitamente en el log en vez de forzado a verde. Lo que **sí**
está completo y verificado en este bloque, independientemente de si ese
paso específico pasa: el build de Wayland (`SDL_WAYLAND=ON`), la rama
de código Wayland de `src/platform/linux/TransparentWindowSupport.cpp`,
y toda la lógica de política testeada en `LinuxBackendPolicyTest.cpp`
(§8). El gap real y pendiente es exactamente uno: **nadie ha visto
correr Nimvlets sobre un compositor Wayland**, ni en este bloque ni
antes -- igual que Windows sigue sin verificación GUI real desde Block
01 (`docs/PLATFORM_SPIKE.md` §8).

## 10. Performance / recursos

El brief §8 pide reportar CPU/RSS en idle si el smoke de Linux lo
permite, con la advertencia explícita de no fijar presupuestos finales
desde una VM de CI. El paso de smoke X11 de `.github/workflows/ci.yml`
captura una muestra (`ps -o pid,rss,%cpu,%mem,comm`) automáticamente en
cada corrida futura -- pero como el workflow **no se ejecutó** en esta
sesión (sin `push`, ver §9), **no hay ningún número real que reportar
todavía**; los primeros números de Linux existirán recién después de
que este workflow corra post-integración. No se agregó ningún polling
ni redraw permanente nuevo para Linux (§4: `usingPollDrivenClickThrough_`
es `false` en Wayland específicamente para evitarlo); en X11, el
mecanismo es el mismo event-driven de macOS, sin ningún tick nuevo.

## 11. Privacidad

Igual que en macOS/Windows -- ver `docs/PRIVACY_SECURITY.md` §F. Ni
X11 ni Wayland requieren ningún permiso elevado para nada de lo que
este bloque implementa: XShape/XInput2/XRandr son extensiones X11
ordinarias sin diálogo de permiso alguno (a diferencia de macOS,
Screen Recording nunca se solicita ni hace falta acá); Wayland es, si
acaso, *más* restrictivo por diseño (de ahí las limitaciones de §6).
Sin captura de pantalla, sin hooks de input global, sin root/admin
(Xvfb y Weston corren sin privilegios especiales en CI), sin red.

## 12. Decisiones tomadas fuera del prompt

- **No se implementó `wl_surface_set_input_region` en crudo.** Habría
  sido la única forma de lograr click-through per-pixel real en
  Wayland (§3.2 explica por qué la SDL pineada no lo expone), pero
  hubiera significado hablar el protocolo Wayland directamente
  (`wl_registry`/`wl_compositor` propios) en paralelo a la cola de
  eventos que SDL ya administra sobre el mismo `wl_display` -- riesgo
  real de interferencia, sin ninguna máquina Linux para verificarlo, y
  el tipo de complejidad que AGENTS.md §16 pide evitar sin un pedido
  explícito. Se documentó el gap (§6) en vez de construir algo no
  verificable.
- **`ClickThroughPollingIsMeaningful()` nueva en el seam compartido**
  (§4) -- no estaba en el brief tal cual, pero es la consecuencia
  directa y necesaria de que Linux introdujera un tercer platform
  donde "sin shape nativo" y "el polling serviría" dejan de coincidir;
  sin esto, Wayland habría heredado un polling loop permanente e inútil,
  violando el brief §8 explícitamente.
- **Job de Wayland en CI no bloqueante** (§9) -- el brief lo permite
  ("do not fake PASS; report the exact gap"), pero la forma concreta
  (`continue-on-error: true` + un mensaje explícito de resultado en el
  log) es una elección de este bloque, no una instrucción literal.
- **Sub-features X11 minimizadas** (Xcursor/Xdbe/Xfixes/Xscrnsaver/Xsync/Xtest
  apagadas, XInput2/XRandr mantenidas) -- ver §2 para la justificación
  completa de cada una.

## 13. QA pendiente / lo que este bloque NO verificó

Por instrucción explícita del brief ("no manual owner QA in this
block", "Do not claim local Linux runtime validation if none
occurred"):

- **Ningún click/drag/hover real fue observado en Linux**, en ningún
  backend. Todo lo de §5/§6 es una conclusión de lectura de fuente +
  diseño, no una observación.
- **El smoke de CI no se ejecutó en esta sesión** (§9/§10) -- los
  pasos existen y están escritos con el mismo cuidado que el resto de
  este bloque, pero su resultado real es desconocido hasta que alguien
  integre y el workflow corra.
- **Sin hardware Linux real de ningún tipo** (ni x86_64 físico ni VM)
  se usó en ningún momento de este bloque -- coherente con
  `docs/PLATFORM_SPIKE.md` §8, que documenta la misma honestidad para
  Windows desde Block 01.

La validación real de Linux es, por diseño de este bloque, el job de CI
de §9 una vez integrado -- no esta sesión.

## 14. Conteo de clics global (Block 11A): NO disponible en Linux

Ver `docs/GLOBAL_CLICK_MODE.md` §12.2 para el detalle. Resumen, con la
misma disciplina de §3 (investigar antes de escribir código nativo, y no
declarar verificado lo que no se corrió):

**X11.** `XI_RawButtonPress` de XInput2 sobre la ventana raíz daría, en
teoría, una notificación pasiva del botón primario sin grabs, sin
suprimir input y sin root — encajaría detrás de la interfaz
`platform::GlobalClickMonitor` sin cambiarla. **No se implementó.** Este
proyecto no usa Xlib directamente en ningún lado: XInput2 está activado
*dentro de la SDL pineada* (`SDL_X11_XINPUT`, ver
`cmake/FetchSDL3.cmake`), no en nuestro código, y
`src/platform/linux/` no enlaza `libX11` ni `libXi`. Hacerlo exigiría
dependencias de desarrollo nuevas (+ paquetes nuevos en CI), una
conexión X propia en paralelo a la que SDL ya administra (o hurgar en su
interna), y un bucle de eventos aparte — exactamente lo que AGENTS.md
§10 pide no agregar sin razón concreta, y §4 prohíbe declarar verificado
sin correrlo. §13 de este documento ya registra que Linux nunca tuvo QA
interactiva. Diseñado, no fingido.

**Wayland.** No hay camino legítimo, y no por una limitación de este
proyecto sino por **diseño del protocolo**: un cliente Wayland ordinario
solo recibe input cuando el compositor le da foco al puntero sobre su
propia superficie. Las únicas rutas para "ver clics en cualquier lado"
serían capturar la pantalla (prohibido de forma permanente, AGENTS.md
§5), leer `/dev/input` (root o grupo `input`, prohibido), o un portal de
**captura** de entrada — cuya semántica *desvía/captura* el puntero en
vez de observarlo pasivamente, rompería el uso normal del escritorio y
no es lo que esta feature hace.

**Consecuencia de producto, en los dos backends:** el adapter
(`src/platform/linux/GlobalClickMonitor.cpp`, sin cabeceras X11 ni
protocolo Wayland) reporta `kUnavailable`; Settings dibuja el segmento
"Anywhere" apagado con la línea "Not available on this system"; y el
modo "Nimvlet only" funciona exactamente igual que siempre. Se suma a la
lista de limitaciones honestas de §6, no se disimula.
