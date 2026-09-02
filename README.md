# Nimvlets

Lightweight, native, cross-platform desktop companion. A small
transparent window shows one creature on your desktop; drag it around,
click it to earn clicks (the only currency), spend clicks to unlock more
creatures permanently.

Este repositorio está en **Block 11B — Completar Settings + controles de
recuperación** (Settings pasa a ser la superficie de configuración
**completa**; el menú rápido queda como un subconjunto de conveniencia,
deliberadamente chico — DEC-141). El grupo **Companion** de Settings gana
dos controles TRANSITORIOS, no persistidos: **Visibility**
`[ Shown ] [ Hidden ]`, que cambia la visibilidad real del pet por la
MISMA ruta canónica que el Show/Hide del menú (`SpikeApp::ApplyPetVisibility`
— sin bump de schema, esconder ≠ salir, el pet arranca visible en cada
lanzamiento), y **Position** `[ Reset position ]`, una acción quieta de
recuperación que devuelve una ventana fuera de pantalla a la colocación
SEGURA canónica (centrada + acotada, idéntica al default de arranque) del
display que contiene el Product UI, persistiendo por la MISMA ruta que el
fin de un drag — funciona con el pet oculto y con Lock Position ON, y en
Wayland se dibuja apagada en vez de fingir (`xdg-shell` no coloca
toplevels). El ítem del menú rápido `Collection…` pasa a **`Open
Nimvlets…`**: la ventana ya trae `Collection · Shop · Settings` y la
acción restaura la MISMA ventana y su sección (semántica de Block 11A
intacta). Ver [`docs/PRODUCT_UI.md`](docs/PRODUCT_UI.md) §20.7 y DEC-141.
Construido sobre **Block 11A — Modo de conteo de clics GLOBAL,
opt-in** (una preferencia de **Settings** —y solo de Settings— que deja
contar pulsaciones del botón primario en **cualquier** parte del
sistema, no solo sobre el Nimvlet: `Click counting [ Nimvlet only ]
[ Anywhere ]`, **apagada por defecto** para usuarios nuevos y migrados
por igual. Es la feature que AGENTS.md §14 mantuvo **explícitamente
prohibida** desde Block 01 hasta que un brief la autorizara; el brief de
Block 11A la autoriza y **ninguna restricción de privacidad se relaja**.
Es **mouse-only** y su única salida funcional es **+1 al contador**: la
firma del callback nativo no tiene dónde llevar una coordenada, un
timestamp, una app ni nada más. En macOS usa un event tap
**listen-only** de CoreGraphics con la máscara de **un solo evento**
(`kCGEventLeftMouseDown`) y el permiso **Input Monitoring** — **NO**
Accessibility, **NO** Screen Recording — pedido desde **un solo lugar**,
solo tras una explicación de primera parte y un "Continue" del owner,
**nunca al arrancar**. El modo **pedido** y el **efectivo** son cosas
distintas: si el permiso falta o se revoca, el conteo cae con seguridad
a local y Settings lo dice, en vez de dejar de contar en silencio. Y un
clic físico sobre el pet **nunca vale +2**: en modo global el monitor es
la única fuente de moneda, mientras la animación de personalidad sigue
igual. Windows y Linux reportan honestamente "no disponible" — el
diseño está investigado, no escrito. AppState sube a **v6** sin mover la
frontera histórica de propiedad (sigue congelada en 4). Ver
[`docs/GLOBAL_CLICK_MODE.md`](docs/GLOBAL_CLICK_MODE.md),
[`docs/PRIVACY_SECURITY.md`](docs/PRIVACY_SECURITY.md) §H,
[`docs/PRODUCT_UI.md`](docs/PRODUCT_UI.md) §22 y DEC-139), sobre
**Block 10 — Shop oculto de starters** (un
submodo CONTEXTUAL de la sección Shop — no una cuarta pestaña: un
usuario que completó el onboarding y eligió UNA variante de Frin puede
comprar la OTRA con clics; la regla de no-divulgación mantiene a Frin
oculto para quien nunca lo descubrió; compra de VARIANTE EXACTA por el
mismo camino atómico que el Shop público, sin auto-activar el pet; el
catálogo de producción no se toca — se ejercita por el harness
sintético-DEV `NIMVLETS_DEV_ONBOARDING`; ver
[`docs/ONBOARDING.md`](docs/ONBOARDING.md) §15/§16,
[`docs/PRODUCT_UI.md`](docs/PRODUCT_UI.md) §21 y DEC-137), sobre
**Block 09C — Shop Browse-First** (el Shop
abre en una estantería de personajes: la rejilla del arte es el
contenido primario; hover/foco revela precio o propiedad; recién al
seleccionar un personaje aparece el hero grande con la acción de compra
— la transacción de compra de Block 07 queda intacta; ver
[`docs/PRODUCT_UI.md`](docs/PRODUCT_UI.md) §16 y DEC-135. Una pasada de
QA del owner además dejó la **Collection con SOLO los Nimvlets
poseídos** — los comprables no poseídos viven en el Shop, no en la
Collection; DEC-136), sobre
**Block 07 — Shop + Wallet Economy** (el click counter se vuelve un
wallet real: una segunda sección del Product UI, el **Shop**, alcanzable
por una navegación de texto `Collection · Shop`; comprar consume clicks
y otorga propiedad permanente, con confirmación inline para no gastar
por accidente; la propiedad pasa a ser **autorizaciones capaces de
variantes** — un `petId` y opcionalmente una variante concreta;
descripciones editoriales un poco más largas;
ver [`docs/PRODUCT_UI.md`](docs/PRODUCT_UI.md) §16–§19), sobre
**Block 06.2 — Collection Identity + Instant Previews + Retina Text**
(hero stage con acento por pet, botón de acción tintado, previews
compiladas livianas `.nvprev` en vez del pack completo, texto nítido en
Retina, copy editorial EN/ES), sobre **Block 06.1 —
Collection Visual Polish + EN/ES Localization** (composición hero +
gallery, acento de identidad por pet, idioma inglés/español) y
**Block 06 — Product Shell + Collection + Native Quick Menu**,
construido sobre Block 05 (Behavior Runtime + Frin Vertical Slice),
Block 04.3 (corrección post-QA de Nidir/Bunny), Block 04.2 (assets
reales + pipeline
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
Nidir, corrige el comportamiento real de hover (dwell continuo,
desacoplado del timer ambient — fijado en 0.4s), fija el intervalo
ambient de Bunny/Nidir y el rest-delay de Frin sentado en 12s (unificados),
agrega una escala visual por-pet genérica y data-driven
(`content::PetDefinition::visualScale`), invierte la semántica de
dirección runtime de Frin (`Direction::kRight`/`kLeft` — pedido de
producto explícito; las carpetas de import siguen registrando la
orientación real que el owner exportó, nunca la semántica de runtime —
ver [`docs/FRIN_CONTENT.md`](docs/FRIN_CONTENT.md) §9.1), y establece
la **identidad semántica de pose**: si el contenido declara que la
punta de una animación *es* la pose base estable de un estado, el pack
compilado usa el **frame canónico exacto** de esa base — igualdad RGBA
pixel por pixel, sin tolerancias ni métricas de "suficientemente
parecido" (`first_frame_is_state_base` / `last_frame_is_state_base`,
ver [`docs/ANIMATION_RUNTIME.md`](docs/ANIMATION_RUNTIME.md) §19). Eso
reemplazó — y permitió borrar — seis capas sucesivas de compensación
geométrica que intentaban aproximar esas puntas midiéndolas; los PNG
fuente nunca se tocan, la sustitución es una referencia de tiempo de
compilación. El compilador aplica **una sola transforma rígida por
animación** (una escala uniforme + una traslación constante), nunca
cambia el sistema de coordenadas visual de un pet solo porque se activó
otra animación, y **nunca inventa movimiento aparente** del personaje
completo — un mecanismo previo que interpolaba la traslación por frame
se retiró tras QA ([§17](docs/ANIMATION_RUNTIME.md)). La variación de
silueta autorada (una cola que se abre, una cabeza que se levanta) es
arte y no se corrige. Cuando un export no cierra geométricamente, el
residual se mide y se reporta como deuda de contenido en vez de
disimularse. Cuando en cambio el desajuste es CONSTANTE en toda una
secuencia —un export que trae al personaje con otra relación de aspecto
o con otra exposición— el contenido puede declararlo y el compilador
deriva **una** corrección fija para ese clip entero (un par
`(scale_x, scale_y)`, o una ganancia RGB con alpha intacto): nunca por
frame, nunca interpolada, y derivada de la correspondencia de poses
estables, nunca de los frames intermedios, que cambian de silueta a
propósito — ver [`docs/ANIMATION_RUNTIME.md`](docs/ANIMATION_RUNTIME.md)
§20 y [`docs/FRIN_CONTENT.md`](docs/FRIN_CONTENT.md) §13.
Esto explícitamente *no* es
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
# default (12s for Bunny/Nidir/Frin's seated rest delay, all unified --
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

### Product UI (Block 06 / 06.1 / 06.2 / 07)

El menú de la barra de macOS (icono monocromo arriba a la derecha)
abre el Product UI: una ventana normal con dos secciones, **Collection**
y **Shop**, alcanzables por una fila de texto `Collection · Shop`
(mouse o teclado; el runtime del pet no se toca al cambiar).

La **Collection** es un álbum de **los Nimvlets que ya tienes** (Block
09C: los no poseídos viven en el Shop, no acá — DEC-136), con una
composición **hero + gallery** — el Nimvlet seleccionado es el
protagonista, con su arte grande sobre un *hero stage* teñido con su
acento de identidad, su especie, una descripción (un par de frases desde
Block 07), estado, selector tipográfico de variante (Frin) y un botón de
acción con el tinte del pet; los demás Nimvlets poseídos en una gallery
discreta sobre un segundo plano. Con un solo Nimvlet, la gallery se
reemplaza por una línea quieta hacia el Shop. Switch de pet en vivo.

El **Shop** (Block 07 + 09C — **browse-first**) abre en una **estantería
de personajes**: una rejilla del arte de cada Nimvlet públicamente
comprable, no un hero gigante. El pointer o el foco de teclado sobre una
tarjeta **revela** info liviana (precio, o "In your collection") — sin
seleccionar ni comprar. Recién al **seleccionar** un personaje (clic /
Enter) se promueve a un hero grande con especie, descripción, precio y
"Get <pet>", y la estantería baja a un rail compacto que sigue
permitiendo elegir otro. "Get <pet>" abre una confirmación inline
("¿Gastar N clics…?" · Cancelar / Confirmar) — un solo click perdido
nunca gasta. Al confirmar, el balance baja, la propiedad es permanente y
aparece al instante en las dos secciones; el pet recién comprado se
puede activar sin reiniciar. **Frin no aparece en el Shop normal.**
Cerrar/reabrir el Product UI devuelve el Shop a la estantería. Precios
provisionales de QA: Bunny 120, Nidir 300.

**Block 10 — Shop oculto de starters.** Para un usuario que completó el
onboarding (lifecycle `kCompleted` EXACTO), si hay ≥ 1 oferta el acceso
es un **HOTSPOT INVISIBLE** — un click primario en la esquina INFERIOR
DERECHA del Shop público (48×48 pt, sin texto / foco / hover / cursor;
easter-egg deliberado; corrección de QA del owner — DEC-138). Sin
ofertas legítimas el click de la esquina es no-op total. Entra a un
submodo que la sección Shop posee (la cabecera sigue marcando "Shop"; un
back affordance "← Shop"), con la MISMA jerarquía browse → hover → hero →
Get → Cancelar/Confirmar, pero en IDENTIDADES EXACTAS: un usuario que
eligió Frin hembra ve una oferta "Frin · Male". La regla de
no-divulgación mantiene a Frin oculto para quien nunca lo descubrió.
Comprar otorga la variante EXACTA (nunca ambas), por el mismo camino
atómico que el Shop público, y NO cambia el pet del escritorio. El
catálogo de producción no se toca — el Starter Shop se ejercita por el
harness sintético-DEV.

El **click balance** (visible SOLO acá) está en la cabecera compartida,
arriba a la derecha. El menú de la barra NO cambia — Show/Hide, Lock
Position, Size (Small/Medium/Large — Large = 1.15), Opacity
(100/85/70/55 %), **Language ▸ (English / Español)** y Quit; el Shop se
alcanza solo desde el Product UI. Cambiar de idioma actualiza todo al
instante, sin reiniciar. Cerrar el Product UI NO termina la app. Ver
[`docs/PRODUCT_UI.md`](docs/PRODUCT_UI.md).

El arte del hero/gallery sale de artefactos `.nvprev` livianos
(`assets/dev/<pack>.nvprev`, ~0,3–0,4 MB c/u) — **la Collection ya no
abre el `.nvpack` de animación completo** (~46–76 MB) para una preview
estática. Regenerar tras regenerar cualquier pack:
`python3 tools/compile_pet_previews.py`.

```bash
# QA / capturas: abrir la Collection al arrancar (sin clickear el menú).
# "1" solo la abre; "petId" o "petId/variantId" además elige el hero.
NIMVLETS_DEV_OPEN_COLLECTION=1            ./build/macos-debug/src/app/nimvlets_spike
NIMVLETS_DEV_OPEN_COLLECTION=frin/female  ./build/macos-debug/src/app/nimvlets_spike

# QA: fuerza el idioma de la sesión sin persistir ("en" / "es").
NIMVLETS_DEV_LANGUAGE=es NIMVLETS_DEV_OPEN_COLLECTION=1 ./build/macos-debug/src/app/nimvlets_spike

# QA: arrancar con el pet oculto (equivale a "Hide Nimvlet"), para
# capturar la Collection sin la ventana always-on-top del pet.
NIMVLETS_DEV_HIDE_PET=1 NIMVLETS_DEV_OPEN_COLLECTION=1 ./build/macos-debug/src/app/nimvlets_spike

# QA: disparar una activación desde la Collection sin un click real
# ("petId" o "petId/variantId") — misma ruta que el botón "Use".
NIMVLETS_DEV_ACTIVATE=frin/female ./build/macos-debug/src/app/nimvlets_spike

# QA: abrir y cerrar la ventana de Collection N veces (smoke de ciclo
# de vida — el runtime del pet lo sobrevive).
NIMVLETS_DEV_COLLECTION_CYCLES=8 ./build/macos-debug/src/app/nimvlets_spike

# QA (Block 06.2): poner el foco de TECLADO sobre un chip de variante
# del hero (captura de "keyboard-focused variant").
NIMVLETS_DEV_OPEN_COLLECTION=frin/male NIMVLETS_DEV_VARIANT_FOCUS=female \
  ./build/macos-debug/src/app/nimvlets_spike

# QA (Block 06.2): volcar el framebuffer de la Collection a un BMP a
# densidad nativa (SDL_RenderReadPixels — sin captura de pantalla del
# SO) y salir. Combinable con lo de arriba.
NIMVLETS_DEV_HIDE_PET=1 NIMVLETS_DEV_OPEN_COLLECTION=frin/male \
  NIMVLETS_DEV_PRODUCT_SHOT=/tmp/collection.bmp \
  ./build/macos-debug/src/app/nimvlets_spike

# --- Shop (Block 07 + 09C browse-first) / Settings (Block 08) --------
# NIMVLETS_DEV_SECTION=shop|settings|collection -> sección visible al abrir.
# NIMVLETS_DEV_SHOP_PET=<petId>         -> abre el Shop con ESE personaje
#   SELECCIONADO (hero + detalle). Sin la variable: modo BROWSE normal.
# NIMVLETS_DEV_SHOP_HOVER=<petId>       -> hover sobre una tarjeta (rejilla
#   de browse, o rail si hay selección): revela precio / propiedad.
# NIMVLETS_DEV_SHOP_FOCUS=<petId>       -> foco de teclado sobre una tarjeta.
# NIMVLETS_DEV_SHOP_CONFIRM=1           -> abre la confirmación inline
#   (necesita NIMVLETS_DEV_SHOP_PET: solo existe con un personaje elegido).
# NIMVLETS_DEV_PREFS=small,70,lock,es   -> aplica preferencias por la MISMA
#   ruta canónica que el menú rápido (SpikeApp::Apply*). Tokens:
#   small|medium|large, 100|85|70|55, lock|unlock, en|es, y (Block 11B,
#   visibilidad TRANSITORIA por SpikeApp::ApplyPetVisibility) shown|hidden.
#   Sirve para capturar un estado no-default de Settings y como smoke en
#   vivo de que esa ruta produce el AppState/runtime esperado.
# NIMVLETS_DEV_SETTINGS_FOCUS=row:opacity -> foco de teclado sobre una fila
#   de Settings (row:visibility|row:size|row:opacity|row:lock|row:position|
#   row:clickcounting|row:language).
# NIMVLETS_DEV_RESET_POSITION=1         -> Block 11B: invoca "Reset position"
#   por la MISMA ruta canónica (SpikeApp::ResetPetPositionToSafeDefault),
#   sin un click real. Mueve la ventana del pet al destino SEGURO del
#   display que contiene el Product UI y marca lastWindowPosition dirty.
#   Necesita NIMVLETS_DEV_OPEN_COLLECTION.
# NIMVLETS_DEV_UI_NAV_SMOKE=1           -> smoke NO interactivo de que las
#   tres pestañas (Collection · Shop · Settings) son ALCANZABLES con un
#   click desde cualquier sección (mismo camino que un click del owner:
#   HandleEvent -> View::OnMouseDown -> ActivateWidget -> NavTargetSection).
#   Loguea PASS/FAIL por salto y sale. Sin permisos del SO. Ver DEC-134.
# NIMVLETS_DEV_BUY=<petId>              -> confirma una compra (no
#   interactivo) ANTES de abrir el UI: EvaluatePurchase -> mutación
#   atómica de balance + propiedad -> flush inmediato. Combinar con
#   NIMVLETS_DEV_CLICK_TEST_COUNT=<n> para tener saldo y con
#   NIMVLETS_DEV_APPDATA_DIR para no tocar el estado real.

# Shop en modo BROWSE (la entrada normal), con una tarjeta bajo el hover:
NIMVLETS_DEV_APPDATA_DIR=/tmp/nv_qa NIMVLETS_DEV_CLICK_TEST_COUNT=500 \
  NIMVLETS_DEV_HIDE_PET=1 NIMVLETS_DEV_OPEN_COLLECTION=1 \
  NIMVLETS_DEV_SECTION=shop NIMVLETS_DEV_SHOP_HOVER=nidir \
  NIMVLETS_DEV_PRODUCT_SHOT=/tmp/shop_browse.bmp \
  ./build/macos-debug/src/app/nimvlets_spike

# Shop con Nidir SELECCIONADO en estado "asequible" (saldo 500, Nidir 300):
NIMVLETS_DEV_APPDATA_DIR=/tmp/nv_qa NIMVLETS_DEV_CLICK_TEST_COUNT=500 \
  NIMVLETS_DEV_HIDE_PET=1 NIMVLETS_DEV_OPEN_COLLECTION=1 \
  NIMVLETS_DEV_SECTION=shop NIMVLETS_DEV_SHOP_PET=nidir \
  NIMVLETS_DEV_PRODUCT_SHOT=/tmp/shop.bmp \
  ./build/macos-debug/src/app/nimvlets_spike

# Shop tras comprar Nidir (balance 200, "In your collection"):
NIMVLETS_DEV_APPDATA_DIR=/tmp/nv_qa2 NIMVLETS_DEV_CLICK_TEST_COUNT=500 \
  NIMVLETS_DEV_BUY=nidir NIMVLETS_DEV_HIDE_PET=1 NIMVLETS_DEV_OPEN_COLLECTION=1 \
  NIMVLETS_DEV_SECTION=shop NIMVLETS_DEV_SHOP_PET=nidir \
  NIMVLETS_DEV_PRODUCT_SHOT=/tmp/shop_owned.bmp \
  ./build/macos-debug/src/app/nimvlets_spike

# Settings en un estado no-default (Small / 70% / Lock On):
NIMVLETS_DEV_APPDATA_DIR=/tmp/nv_qa3 NIMVLETS_DEV_HIDE_PET=1 \
  NIMVLETS_DEV_OPEN_COLLECTION=1 NIMVLETS_DEV_SECTION=settings \
  NIMVLETS_DEV_PREFS=small,70,lock \
  NIMVLETS_DEV_PRODUCT_SHOT=/tmp/settings.bmp \
  ./build/macos-debug/src/app/nimvlets_spike

# --- Companion: visibilidad + "Reset position" (Block 11B) -----------
# A. Settings con el pet oculto (fila Visibility -> "Hidden"), ES, con el
#    botón "Reset position" enfocado. Confirma la sincronización
#    ApplyPetVisibility -> Settings y el anillo de foco de la fila nueva.
NIMVLETS_DEV_APPDATA_DIR=/tmp/nv_11b_a NIMVLETS_DEV_OPEN_COLLECTION=1 \
  NIMVLETS_DEV_SECTION=settings NIMVLETS_DEV_PREFS=hidden,es \
  NIMVLETS_DEV_SETTINGS_FOCUS=row:position \
  NIMVLETS_DEV_PRODUCT_SHOT=/tmp/settings_11b_es.bmp \
  ./build/macos-debug/src/app/nimvlets_spike
# D. "Reset position" en vivo: mueve la ventana del pet al destino seguro
#    de su display y persiste por la ruta del fin-de-drag (buscar el log
#    "Reset position -> pet moved to the safe default ...").
NIMVLETS_DEV_APPDATA_DIR=/tmp/nv_11b_d NIMVLETS_DEV_OPEN_COLLECTION=1 \
  NIMVLETS_DEV_SECTION=settings NIMVLETS_DEV_RESET_POSITION=1 \
  ./build/macos-debug/src/app/nimvlets_spike
# E. Reset con Lock Position ON: la posición cambia igual (Lock solo
#    gatea el inicio de un drag, no un reset explícito del owner).
NIMVLETS_DEV_APPDATA_DIR=/tmp/nv_11b_e NIMVLETS_DEV_OPEN_COLLECTION=1 \
  NIMVLETS_DEV_SECTION=settings NIMVLETS_DEV_PREFS=lock \
  NIMVLETS_DEV_RESET_POSITION=1 \
  ./build/macos-debug/src/app/nimvlets_spike
# F. hide -> reset -> show sigue siendo seguro (el pet oculto también
#    puede recibir un reset de posición):
NIMVLETS_DEV_APPDATA_DIR=/tmp/nv_11b_f NIMVLETS_DEV_OPEN_COLLECTION=1 \
  NIMVLETS_DEV_SECTION=settings NIMVLETS_DEV_PREFS=hidden \
  NIMVLETS_DEV_RESET_POSITION=1 \
  ./build/macos-debug/src/app/nimvlets_spike
# G. minimizar el Product UI y recuperarlo con "Open Nimvlets…" (misma
#    ventana, misma sección) — el smoke en vivo de Block 11A cubre las
#    tres secciones:
NIMVLETS_DEV_APPDATA_DIR=/tmp/nv_11b_g NIMVLETS_DEV_RESTORE_SMOKE=1 \
  ./build/macos-debug/src/app/nimvlets_spike

# --- Onboarding de primer arranque (Block 09A, SOLO-DEV) --------------
# El onboarding de PRODUCCIÓN está DESHABILITADO (falta contenido de
# Artu/Rato/Rin Rin — ver docs/ONBOARDING.md). Este harness ejercita la
# máquina de estados/UI real con descriptores SINTÉTICOS. Usar SIEMPRE
# un NIMVLETS_DEV_APPDATA_DIR aislado y fresco.
# NIMVLETS_DEV_ONBOARDING=1              -> carga el catálogo sintético
#   assets/dev/onboarding_dev_catalog.nvcat (compilado con
#   dev_synthetic_onboarding: true) y fuerza el gate si el lifecycle es
#   kPending Y ese catálogo se declaró sintético. artu_dev/rato_dev/
#   rinrin_dev NO son Artu/Rato/Rin Rin (packs/previews ALIAS) — nunca
#   se envían; ese byte es mutuamente excluyente con
#   production_onboarding_ready, así el alias nunca arma producción
#   (DEC-133).
# NIMVLETS_DEV_ONBOARDING_REVEAL_MS=<n>  -> deadline del secreto en <n> ms
#   en vez de 44000 (smoke sin dormir 44 s).
# NIMVLETS_DEV_ONBOARDING_REVEAL=1       -> revela el secreto al arranque.
# NIMVLETS_DEV_ONBOARDING_STAGE=browse|variant|confirm[:<focusId>]
#   -> fuerza una etapa (p. ej. confirm:cand:artu_dev, confirm:var:female).
# NIMVLETS_DEV_ONBOARDING_CHOOSE=<petId>[/<variant>]  -> confirma la
#   selección sin interacción (transacción de completitud atómica). Corre
#   ANTES del bloque NIMVLETS_DEV_OPEN_COLLECTION, así que combinarlo con
#   NIMVLETS_DEV_OPEN_COLLECTION + NIMVLETS_DEV_SECTION + NIMVLETS_DEV_PRODUCT_SHOT
#   fotografía el Product UI NORMAL de post-onboarding (DEC-134).

# Pantalla de selección de starter (EN), con el secreto revelado
# (los 3 normales sin moverse + Frin en una segunda fila centrada abajo):
NIMVLETS_DEV_APPDATA_DIR=/tmp/nv_ob1 NIMVLETS_DEV_ONBOARDING=1 \
  NIMVLETS_DEV_ONBOARDING_REVEAL=1 \
  NIMVLETS_DEV_PRODUCT_SHOT=/tmp/onboarding.bmp \
  ./build/macos-debug/src/app/nimvlets_spike

# Flujo completo: elegir Frin hembra, completar, y reiniciar:
NIMVLETS_DEV_APPDATA_DIR=/tmp/nv_ob2 NIMVLETS_DEV_ONBOARDING=1 \
  NIMVLETS_DEV_ONBOARDING_REVEAL=1 NIMVLETS_DEV_ONBOARDING_CHOOSE=frin/female \
  ./build/macos-debug/src/app/nimvlets_spike
NIMVLETS_DEV_APPDATA_DIR=/tmp/nv_ob2 ./build/macos-debug/src/app/nimvlets_spike  # reinicio: NO re-onboardea

# Settings del Product UI NORMAL tras completar el onboarding DEV (EN):
NIMVLETS_DEV_APPDATA_DIR=/tmp/nv_ob3 NIMVLETS_DEV_ONBOARDING=1 \
  NIMVLETS_DEV_ONBOARDING_CHOOSE=artu_dev \
  NIMVLETS_DEV_OPEN_COLLECTION=1 NIMVLETS_DEV_SECTION=settings \
  NIMVLETS_DEV_PRODUCT_SHOT=/tmp/post_onboarding_settings.bmp \
  ./build/macos-debug/src/app/nimvlets_spike

# --- Shop oculto de starters (Block 10, SOLO-DEV) --------------------
# El Starter Shop está INERTE en el catálogo de PRODUCCIÓN (sus entradas
# no tienen starterRole ni precio). Se ejercita SOLO por el harness
# sintético-DEV: NIMVLETS_DEV_ONBOARDING=1 en CADA arranque (el env var
# elige el catálogo sintético; sin él se carga el de producción). Sólo
# aparece si el lifecycle es kCompleted EXACTO (tras un CHOOSE real);
# un usuario legacy/dev-seed (kLegacyComplete) NUNCA lo ve.
# NIMVLETS_DEV_GRANT_CLICKS=<n>   -> suma n clics al wallet sin disparar
#   animaciones. Corre DESPUÉS de NIMVLETS_DEV_ONBOARDING_CHOOSE (que deja
#   el balance en 0) y ANTES de STARTER_BUY / OPEN_COLLECTION.
# NIMVLETS_DEV_STARTER_BUY=<petId>[/<variant>]  -> compra no interactiva
#   por el mismo camino que "Confirmar" (EvaluateStarterPurchase ->
#   ApplyPurchasedState atómico). Requiere lifecycle kCompleted.
# NIMVLETS_DEV_STARTER_SHOP=1     -> entra al submodo DIRECTAMENTE
#   (necesita NIMVLETS_DEV_SECTION=shop). En producción el acceso es un
#   HOTSPOT INVISIBLE en la esquina inf-der del Shop público (DEC-138).
# NIMVLETS_DEV_STARTER_HOTSPOT=1  -> sintetiza el click REAL de la esquina
#   inf-der (mismo camino que un click del owner). Loguea si abrió o fue
#   no-op (hotspot no armado = sin ofertas legítimas).
# --- Conteo de clics GLOBAL, opt-in (Block 11A) ----------------------
# NIMVLETS_DEV_CLICK_COUNTING=nimvlet_only|anywhere -> pide ese modo por
#   el MISMO camino que un click del owner en Settings
#   (ApplyPreferenceChange -> EvaluateGlobalClickRequest). Con "anywhere"
#   y el permiso ausente deja la explicación de primera parte VISIBLE, sin
#   pedir nada todavía. Necesita NIMVLETS_DEV_OPEN_COLLECTION.
# NIMVLETS_DEV_GLOBAL_CLICK_ACTION=continue|notnow|recheck -> acciona un
#   botón del flujo de permiso por su camino real. OJO: "continue" SÍ
#   llama al pedido nativo (puede mostrar el diálogo del OS) — es
#   exactamente lo que hace ese botón.
# NIMVLETS_DEV_GLOBAL_CLICKS=<n> -> empuja n clics primarios GLOBALES por
#   el mismo camino que el monitor nativo. NO finge que el OS concedió
#   ningún permiso: si el modo efectivo no es global, se IGNORAN — que es
#   justamente la mitad interesante del test (probar, sin permiso de TCC,
#   que un evento global no suma en modo local, y que en modo global
#   activo cada evento suma exactamente 1). Loguea el balance antes/después.
#
# NIMVLETS_DEV_GLOBAL_CLICK_EXPLAIN=1 -> muestra la explicación de
#   PRIMERA PARTE tal cual, para revisar la copy EN/ES aunque el permiso
#   del OS ya esté concedido en esta máquina. SOLO enciende el panel: no
#   pide ningún permiso ni cambia la preferencia. Necesita
#   NIMVLETS_DEV_OPEN_COLLECTION + NIMVLETS_DEV_SECTION=settings.
#
#   Ejemplo — la regresión de doble conteo, sin tocar el estado real:
#     NIMVLETS_DEV_APPDATA_DIR=/tmp/nv NIMVLETS_DEV_HIDE_PET=1 \
#     NIMVLETS_DEV_CLICK_TEST_COUNT=4 NIMVLETS_DEV_GLOBAL_CLICKS=6 \
#     ./build/macos-debug/src/app/nimvlets_spike
#   Con el modo global ACTIVO, los 4 clics del pet suman 0 y los 6 globales
#   suman 6. Ver docs/GLOBAL_CLICK_MODE.md §16 para la QA manual completa.
#
# NIMVLETS_DEV_STARTER_OFFER=<petId>[/<variant>]  -> selecciona esa oferta
#   EXACTA como hero. NIMVLETS_DEV_STARTER_HOVER=<petId>[/<variant>] ->
#   hover (revela el precio). NIMVLETS_DEV_STARTER_CONFIRM=1 -> abre la
#   confirmación. NIMVLETS_DEV_STARTER_FOCUS=<focusId> -> foco de teclado
#   ("starter:back", "starteritem:frin/male", "get", …).

# --- Smokes EN VIVO de Block 11A (corrección de QA del owner) --------
# Los dos corren contra la ventana REAL del Product UI y salen solos con
# código 0 (todo PASS) o 2. Usalos con NIMVLETS_DEV_APPDATA_DIR aislado:
# el primero SUMA clics al wallet.
#
# NIMVLETS_DEV_WALLET_LIVE_SMOKE=1 -> por cada sección (Collection, Shop,
#   Settings): deja el frame limpio, cuenta UN clic por el camino
#   canónico, y verifica que la ventana REPINTA en el acto y que la otra
#   fuente de clic no suma nada (no doble conteo). Con el permiso de
#   Input Monitoring concedido, agregá NIMVLETS_DEV_CLICK_COUNTING=anywhere
#   para correr la mitad GLOBAL (el log dice qué modo efectivo corrió).
#   Este smoke FALLA en Settings con el código anterior a la corrección.
# NIMVLETS_DEV_RESTORE_SMOKE=1 -> cerrada->abre, visible->la misma ventana
#   al frente, y minimizada->restaurada desde Collection, Shop y Settings
#   (mismo SDL_WindowID, misma sección). Minimiza de verdad y espera al
#   window server, así que necesita una sesión gráfica.

# Refresco en vivo del wallet, las tres secciones:
NIMVLETS_DEV_APPDATA_DIR=/tmp/nv_live NIMVLETS_DEV_HIDE_PET=1 \
  NIMVLETS_DEV_WALLET_LIVE_SMOKE=1 ./build/macos-debug/src/app/nimvlets_spike

# Lo mismo, pero contando por el monitor GLOBAL (requiere el permiso ya
# concedido; si no lo está, el smoke corre igual en modo local y lo dice):
NIMVLETS_DEV_APPDATA_DIR=/tmp/nv_live_g NIMVLETS_DEV_HIDE_PET=1 \
  NIMVLETS_DEV_CLICK_COUNTING=anywhere NIMVLETS_DEV_WALLET_LIVE_SMOKE=1 \
  ./build/macos-debug/src/app/nimvlets_spike

# "Open Nimvlets…" recupera la ventana minimizada, sin duplicarla (el
# ítem se llamaba "Collection…" hasta Block 11B; la semántica no cambió):
NIMVLETS_DEV_APPDATA_DIR=/tmp/nv_restore NIMVLETS_DEV_HIDE_PET=1 \
  NIMVLETS_DEV_RESTORE_SMOKE=1 ./build/macos-debug/src/app/nimvlets_spike

# La explicación previa al permiso, EN y ES (revisión de copy):
NIMVLETS_DEV_APPDATA_DIR=/tmp/nv_gx NIMVLETS_DEV_LANGUAGE=es \
  NIMVLETS_DEV_HIDE_PET=1 NIMVLETS_DEV_OPEN_COLLECTION=1 \
  NIMVLETS_DEV_SECTION=settings NIMVLETS_DEV_GLOBAL_CLICK_EXPLAIN=1 \
  NIMVLETS_DEV_PRODUCT_SHOT=/tmp/settings_explain_es.bmp \
  ./build/macos-debug/src/app/nimvlets_spike

# El Starter Shop tras elegir Frin hembra, ABIERTO POR EL HOTSPOT
# INVISIBLE de la esquina inf-der (oferta frin/male + 3 dev normales):
NIMVLETS_DEV_APPDATA_DIR=/tmp/nv_ss1 NIMVLETS_DEV_ONBOARDING=1 \
  NIMVLETS_DEV_ONBOARDING_CHOOSE=frin/female NIMVLETS_DEV_ONBOARDING_REVEAL=1 \
  NIMVLETS_DEV_GRANT_CLICKS=500 NIMVLETS_DEV_HIDE_PET=1 \
  NIMVLETS_DEV_OPEN_COLLECTION=1 NIMVLETS_DEV_SECTION=shop NIMVLETS_DEV_STARTER_HOTSPOT=1 \
  NIMVLETS_DEV_PRODUCT_SHOT=/tmp/starter_shop.bmp \
  ./build/macos-debug/src/app/nimvlets_spike

# El Shop público de un usuario legacy: NINGUNA pista visible; el click
# de la esquina inf-der es no-op (hotspot no armado):
NIMVLETS_DEV_APPDATA_DIR=/tmp/nv_ss0 NIMVLETS_DEV_CLICK_TEST_COUNT=500 \
  NIMVLETS_DEV_HIDE_PET=1 NIMVLETS_DEV_OPEN_COLLECTION=1 NIMVLETS_DEV_SECTION=shop \
  NIMVLETS_DEV_STARTER_HOTSPOT=1 NIMVLETS_DEV_PRODUCT_SHOT=/tmp/public_shop_no_clue.bmp \
  ./build/macos-debug/src/app/nimvlets_spike

# Frin Male seleccionado + confirmación (ES):
NIMVLETS_DEV_APPDATA_DIR=/tmp/nv_ss2 NIMVLETS_DEV_ONBOARDING=1 NIMVLETS_DEV_LANGUAGE=es \
  NIMVLETS_DEV_ONBOARDING_CHOOSE=frin/female NIMVLETS_DEV_ONBOARDING_REVEAL=1 \
  NIMVLETS_DEV_GRANT_CLICKS=500 NIMVLETS_DEV_HIDE_PET=1 \
  NIMVLETS_DEV_OPEN_COLLECTION=1 NIMVLETS_DEV_SECTION=shop NIMVLETS_DEV_STARTER_SHOP=1 \
  NIMVLETS_DEV_STARTER_OFFER=frin/male NIMVLETS_DEV_STARTER_CONFIRM=1 \
  NIMVLETS_DEV_PRODUCT_SHOT=/tmp/starter_confirm_es.bmp \
  ./build/macos-debug/src/app/nimvlets_spike

# Comprar frin/male, luego ver la Collection con ambas variantes poseídas:
NIMVLETS_DEV_APPDATA_DIR=/tmp/nv_ss3 NIMVLETS_DEV_ONBOARDING=1 \
  NIMVLETS_DEV_ONBOARDING_CHOOSE=frin/female NIMVLETS_DEV_ONBOARDING_REVEAL=1 \
  NIMVLETS_DEV_GRANT_CLICKS=500 NIMVLETS_DEV_STARTER_BUY=frin/male \
  NIMVLETS_DEV_HIDE_PET=1 NIMVLETS_DEV_OPEN_COLLECTION=frin/male NIMVLETS_DEV_SECTION=collection \
  NIMVLETS_DEV_PRODUCT_SHOT=/tmp/collection_both.bmp \
  ./build/macos-debug/src/app/nimvlets_spike
```

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
pet (sin click) durante 0.4s continuos dispara la misma acción, con su
propio dwell desacoplado del timer ambient. Frin tiene el mismo ritmo
que Bunny/Nidir: tras ~12s de reposo genuino sentado se acuesta
(`sit_to_lie` -> `lying`), y acostado no tiene timer ambient — un click
lo levanta.
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
tests/           CTest-driven unit tests for src/core, src/content, src/catalog, src/persistence, src/productui/*_core
tools/           dev tooling (stats_loc.py, prep_dev_sprite.py, compile_pet_pack.py, compile_pet_catalog.py,
                 compile_pet_preview.py, compile_pet_previews.py,
                 generate_bunny_pack.py, generate_nidir_pack.py, generate_frin_pack.py,
                 validate_frame_sequence.py, test_asset_pipeline.py)
assets/dev/      compiled runtime packs (*.nvpack) + catalog (*.nvcat) + Product UI previews (*.nvprev) -- see assets/dev/README.md
assets/source/nimvlets/  real Nimvlet source art (frames, spritesheets, DESCRIPTION.txt — see
                 assets/source/nimvlets/README.md and docs/NIDIR_CONTENT.md/BUNNY_CONTENT.md/FRIN_CONTENT.md)
cmake/           CMake helper modules (warnings, SDL3 fetch)
docs/            product + engineering contracts (see AGENTS.md §18)
```

See [`AGENTS.md`](AGENTS.md) for the full engineering contract
(architecture, privacy/security rules, dependency rules, Git workflow).
