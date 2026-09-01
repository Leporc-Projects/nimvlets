# Nimvlets — Product Shell + Collection + Shop + Quick Menu (Block 06 / 06.1 / 06.2 / 07 / 09C / 10)

Este documento describe la capa de **producto** que Block 06 agrega
sobre el runtime de pet de Block 01–05: una ventana de aplicación
normal con una **Collection**, un **menú rápido nativo** en la barra de
menús de macOS, y los controles de usuario (mostrar/ocultar, bloquear
posición, tamaño, opacidad). Es el primer bloque en el que Nimvlets se
siente como una aplicación de escritorio coherente y no solo un runtime
de pet.

**Actualización Block 07 (wallet + Shop + autorizaciones capaces de
variantes).** El click counter pasa a ser un **wallet real** y se
agrega una segunda sección al Product UI, el **Shop**, alcanzable por
una navegación de texto "Collection · Shop". Comprar consume clicks y
otorga propiedad permanente, visible al instante en las dos secciones y
activable sin reiniciar. La propiedad deja de ser "un conjunto de
`petId`" y pasa a ser **autorizaciones** (`catalog::PetEntitlement`: un
`petId`, opcionalmente una variante concreta) — ver §16–§19,
`docs/CATALOG.md` §12 y DEC-123..DEC-127. **Frin no aparece en el Shop
normal.** Las descripciones de los tres pets con arte real se alargaron
a un par de frases (§6.4-editorial). Lo demás de Block 06/06.1/06.2
queda **congelado** (brief §2).

**Actualización Block 09C (Shop BROWSE-FIRST — DEC-135).** El owner
rechazó la jerarquía visual de entrada de Block 07: al abrir el Shop, el
hero seleccionado dominaba de inmediato y la sección se sentía "como la
Collection con controles de compra". Block 09C rediseña SOLO la
presentación (la transacción de compra de Block 07, la exclusión de
Frin, el modelo `catalog::ShopModel` y las previews `.nvprev` quedan
intactos): el Shop abre en modo **BROWSE** — una estantería de
personajes que se puede conocer, con el arte de cada uno como contenido
primario. El pointer/foco sobre una tarjeta **revela** info liviana
(precio, o "In your collection") sin seleccionar ni comprar. **Recién
al seleccionar** un personaje ese personaje se promueve a un **hero**
grande con especie / descripción / precio / acción de compra, y la
estantería baja a un **rail** compacto que sigue permitiendo elegir
otro. `hovered != selected != confirming`. Ver §16 (reescrito) y §16.2.
El estado de entrada hero-first de DEC-127 queda **superseded**.

**Actualización Block 09C (Collection = SOLO lo poseído — DEC-136).**
Pasada de corrección de QA del owner: la Collection ya NO muestra
Nimvlets que el owner no tiene (antes un Nidir no poseído aparecía en la
gallery con "Not in your collection"). `BuildCollectionModel` descarta
los ítems que quedarían `kLocked` — lo públicamente comprable pero no
poseído vive en el Shop, no en la Collection ("Collection = lo que ya
tengo; Shop = lo que puedo mirar / comprar"). Un Frin con al menos una
variante poseída SÍ aparece (con la otra variante marcada no poseída,
sin ruta de compra). Con un solo Nimvlet poseído la gallery queda vacía:
sin divisor ni segundo plano, una sola línea quieta hacia el Shop
(`kCollectionOnlyActive`). Ver §5, §6.3 y DEC-136.

**Actualización Block 06.1 (pase de identidad visual + localización).**
La arquitectura y la funcionalidad de Block 06 quedaron aprobadas por
el owner; 06.1 solo refina la dirección visual y el idioma:

- la Collection pasa de un grid uniforme a una composición **hero +
  gallery** (el Nimvlet seleccionado es el protagonista) — ver §6;
- **acento de identidad por pet** sutil (forma del hero, foco, variante
  seleccionada) — ver §6.2;
- **localización EN/ES** de todo el texto de interfaz y del menú, con
  submenú `Language ▸`, cambio inmediato y persistido — ver §16;
- preset de tamaño **Large 1.30 → 1.15**;
- ventana **760×540 → 800×560**.

Lo que dice este documento vale para el estado ACTUAL (06.1); los
detalles de Block 06 que 06.1 reemplaza están marcados
*(superseded 06.1)*.

Ver `docs/DECISION_LOG.md` DEC-106..DEC-118 para por qué se tomó cada
decisión, `docs/CATALOG.md` §11 para el modelo de propiedad,
`docs/PERSISTENCE.md` §3 para el schema v3 del archivo de estado, y
`docs/PERFORMANCE_BUDGETS.md` para las mediciones.

## 1. Alcance de Block 06

Dentro de alcance:

- una **ventana de aplicación normal** (con marco, enfocable,
  redimensionable) para el Product UI;
- la **Collection**: un álbum de los Nimvlets del owner con dos estados
  de propiedad (activo, poseído-inactivo), un panel de detalle
  expandido, y el modelo de variantes de Frin (un Nimvlet lógico,
  macho/hembra). *(Block 06 listaba además un tercer estado, "bloqueado"
  / no poseído — Block 09C lo quita de la Collection: ver la
  Actualización Block 09C arriba y DEC-136.)*
- **switching de pet en vivo** desde la Collection, sin reiniciar;
- el **click balance** visible SOLO dentro del Product UI;
- un **menú rápido nativo** en macOS (`NSStatusItem`): pet actual,
  Show/Hide, Collection…, Size ▸, Opacity ▸, Lock Position, Quit;
- **Show/Hide** del pet, **Lock Position**, **Size** (small/medium/
  large), **Opacity** (100/85/70/55 %), todos persistidos salvo la
  visibilidad;
- separación de ciclo de vida: cerrar la Collection NO termina la app,
  no resetea el pet ni el balance, no detiene el runtime.

Explícitamente fuera de alcance de Block 06 (ver §11) — **compras /
precios / gastar clicks se agregaron en Block 07, ver §16–§19**;
sigue fuera: onboarding / selección de starter / el secreto de 44 s /
el shop oculto de starters, la bandeja de Windows y su equivalente en
Linux, conteo global de clicks, preferencia de fullscreen,
launch-at-login, dark mode, una página de Settings avanzada,
animaciones nuevas de pet, y la corrección visual de `lie_to_sit` de
Frin (deuda conocida, congelada — AGENTS.md, brief §21).

## 2. Tres capas, un proceso

```
Nimvlets (un proceso, un event loop — src/app/SpikeApp.cpp)
|
+-- Pet Runtime            ventana transparente, borderless, always-on-top.
|                          El runtime de contenido/animación/persistencia/
|                          catálogo de Block 01–05, SIN cambios de
|                          comportamiento (brief §21).
|
+-- Product UI             ventana normal (con marco, enfocable,
|   (src/productui/)        redimensionable). Se abre/cierra bajo demanda.
|   +-- Collection          Navegación por pestañas de texto
|   +-- Shop  (Block 07)     "Collection · Shop" — misma ventana, misma
|   +-- [Settings] Block 08   cabecera compartida (§17). Settings sigue
|                             siendo futuro — NO existe ni como pantalla
|                             vacía (brief §17).
|
+-- System Shell           presencia nativa fuera de las ventanas.
    (src/platform/*)        macOS: NSStatusItem + menú rápido (real).
    +-- macOS NSStatusItem   Windows: bandeja (futuro — adapter no-op).
    +-- [Windows tray]       Linux: equivalente (futuro — adapter no-op).
    +-- [Linux equivalent]
```

Las tres capas comparten **un solo `SDL_WaitEventTimeout` loop** en
`SpikeApp::Run()`. No hay threads nuevos, no hay estado global mutable
nuevo. La ventana del pet y la del Product UI son dos `SDL_Window` con
su propio `SDL_Renderer` cada una; los eventos de SDL llevan `windowID`
y `ProductWindow::HandleEvent()` filtra por ahí.

### Dependencias

Ninguna dependencia de terceros agregada (ver la sección 4 del informe
del bloque). El texto del sistema (SF Pro) se rasteriza con **Core
Text** — un framework del OS, no un paquete externo, igual que AppKit
(AGENTS.md §10). El menú usa **AppKit** (`NSStatusItem`/`NSMenu`). Todo
detrás de la costura `src/platform/`.

## 3. Product UI: arquitectura

`src/productui/` son dos librerías:

- **`nimvlets_productui_core`** — PURA (sin SDL), testeable en
  `nimvlets_tests` igual que `src/core`/`src/catalog`:
  - `FocusList` — anillo de foco de teclado cíclico sobre ids de widget.
  - `CollectionLayout` — convierte `catalog::CollectionModel` + tamaño
    de viewport + scroll en widgets posicionados (anclas de cabecera,
    grid de 1–3 columnas de arte "flotante", panel de detalle con chips
    de variante y un botón "Use <name>"), el orden de tabulación, y un
    hit-test por punto. Métricas en PUNTOS lógicos.
  - `Format` — `"1 248 clicks"` (dígitos agrupados con un espacio, sin
    localización sobre-ingenierada, brief §6).
- **`nimvlets_productui`** — capa SDL:
  - `UiTheme` — la paleta decidida (blanco hueso cálido, casi-negro,
    hairlines discretas, un único acento terracota) y la escala
    tipográfica.
  - `UiPaint` — painter delgado sobre `SDL_Renderer`: escalado
    lógico→píxel, round-rects y strokes por scanline, blit de imagen
    "contain-fit", blit de glyphs a tamaño de píxel real, un clip rect.
    Solo lo que la Collection necesita — NO es un framework de UI
    (brief §5).
  - `TextCache` — bitmaps de texto ya rasterizados (`platform::
    RasterizeText`) cacheados como `SDL_Texture`, indexados por
    contenido+tamaño+peso+color+escala. `Clear()` al cerrar la ventana.
  - `PetPreviewCache` — texturas del frame de reposo del arte de cada
    pet (ver §8).
  - `CollectionView` — input (mouse/teclado/rueda) sobre el layout puro
    + el anillo de foco, un flag `dirty_` (redibuja solo ante un cambio
    real — sin loop continuo, brief §19).
  - `ProductWindow` — dueña de la `SDL_Window` normal + su `SDL_Renderer`
    + los caches + la vista. `Open()`/`Close()` construyen y liberan
    TODOS los recursos de GPU. Rutea eventos por `windowID`. Redibuja
    solo cuando `dirty_` o un `EXPOSED` pendiente.

### Alto-DPI

El layout se hace en puntos lógicos. `ProductWindow` calcula
`scale = pixelW / logicalW` (2.0 en Retina) y `UiPaint` multiplica cada
rect por `scale` antes de llamar a SDL. El texto se rasteriza a
`pointSize * scale` píxeles y se dibuja 1:1 — nítido en Retina. No se
usa `SDL_SetRenderLogicalPresentation` en la ventana de producto (sí en
la del pet). Al cambiar la escala (mover a otro monitor), `TextCache`
se limpia (los bitmaps son específicos de la escala).

## 4. Ciclo de vida de la ventana de producto

| Evento | Efecto |
|---|---|
| `ShellAction::kOpenCollection` (menú "Collection…") | `ProductWindow::Open()` crea ventana + renderer + caches + vista, siembra el modelo/preview/balance, y activa la app (`platform::BringApplicationToForeground`). Si ya está abierta: solo la trae al frente. |
| El owner cierra la ventana (botón rojo) | `ProductWindow::Close()`: destruye renderer, texturas y caches; `view_` vuelve a su estado inicial. **NO** termina la app, **NO** resetea el pet activo, **NO** resetea el balance, **NO** detiene el runtime del pet (brief §18). |
| Reabrir | Reconstruye todo. La vista arranca en la Collection sin detalle abierto, con el modelo/balance actuales. Barato y correcto — no se mantiene un renderer pesado oculto (brief §18). |
| Un click en el pet mientras la Collection está abierta | El balance visible en la Collection se actualiza en vivo (brief §13). |
| Un switch de pet exitoso | El pet del escritorio, la preview de la Collection, el modelo, y el título del menú se refrescan. |
| `SpikeApp::Shutdown()` | `productWindow_.Close()` + `systemShell_->Shutdown()` ANTES de desmontar el renderer del pet. |

Verificado contra el binario real: `NIMVLETS_DEV_COLLECTION_CYCLES=8`
hace 8 pares open/close seguidos — el pet sigue activo, el renderer del
pet sigue vivo, shutdown limpio, sin crecimiento de RSS (§ perf).

## 5. Modelo de propiedad

Ver `docs/CATALOG.md` §11 para el detalle. En resumen:

- **Semilla de desarrollo/default**: `catalog::CatalogEntry::
  initiallyOwned` (schema `NVCATLG1` v2). El manifest de dev marca
  Bunny + Frin como `initially_owned`; Nidir queda sin poseer, comprable
  desde el Shop a 300 clics. *(Block 06 mostraba Nidir en la Collection
  como "bloqueado"; Block 09C lo quita de la Collection y lo deja solo
  en el Shop — DEC-136.)*
- **Autoridad de runtime**: `persistence::AppState::ownedPetIds` (schema
  `NVSTATE1` v2), un conjunto de `petId` (nunca por variante — poseer
  "frin" da acceso a macho y hembra). `ownershipSeeded` (bool)
  distingue "nunca se inicializó la propiedad" de "posee cero
  Nimvlets": `SpikeApp::Init()` siembra desde el catálogo **solo
  cuando `ownershipSeeded` es false**, y lo pone en true.
- **Invariante**: `catalog::EnsureActivePetOwned()` garantiza que el
  pet que está en el escritorio siempre sea propio (brief §9).
- **Block 07 — autorizaciones capaces de variantes** (ver §19):
  `ownedPetIds` (conjunto de `petId`) queda **superseded** por
  `AppState::ownedEntitlements` (pares `{petId, variantId}`). La compra
  del Shop agrega una autorización y descuenta `clickBalance` en un
  solo `AppState` atómico; el enum de estados de `CollectionModel`
  (`kActive`/`kOwnedInactive`/`kLocked`) NO necesitó rediseño. Un
  bloque futuro de onboarding (Block 09) reemplaza la siembra por la
  elección real de starter escribiendo `ownedEntitlements` +
  `ownershipSeeded = true` con su propia lógica, sin tocar el catálogo.

`catalog::CollectionModel` (puro) deriva la vista: agrupa las filas del
catálogo por `petId` lógico (las dos entradas de Frin colapsan a una
con dos variantes), y calcula el estado de cada una.

## 6. La Collection — composición hero + gallery (06.1)

Tamaño de contenido objetivo: **~800 × 560 pt**, redimensionable
*(superseded 06.1: Block 06 usaba 760×540 y un grid uniforme)*.

```
Nimvlets                                          1 248 clicks   <- cabecera discreta
Collection                                                        <- título de sección
Your companions                                                   <- subtítulo

 ╭─ hero stage (halo asimétrico teñido con el acento del pet) ─╮
 │      [ ARTE GRANDE ]      Frin                               │  <- HERO: el Nimvlet
 │      del Nimvlet          ──                                 │     seleccionado, protagonista
 │      seleccionado         White wolf                         │     (nombre / regla de acento /
 │                           Watchful, calm, and happiest…      │      especie / descripción /
 │                           Male · Female     (solo Frin)      │      selector / acción-o-estado)
 ╰───────────────────────────[ Use Frin ]  o  ● On desktop ─────╯
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  <- hairline = borde del 2º plano
              [art]          [art]         ← GALLERY sobre un neutro cálido un poco
              Nidir          Frin            más profundo (segundo plano); pedestal
              Not in your    Use             con un tinte de identidad muy tenue por pet
              collection
```

*(superseded 06.2: 06.1 usaba una sola forma tenue detrás del arte, un
botón de acción casi-negro, sin línea de descripción, y un solo plano
de fondo.)*

Jerarquía (brief 06.1 §7):

1. producto / cabecera
2. título + contexto de la Collection
3. **el Nimvlet seleccionado como HERO** — la razón por la que existe
   la pantalla
4. la gallery restante

Un click (o Enter) sobre una entrada de la gallery la **promueve a
hero**; el que era hero baja a la gallery. Siempre hay un hero — por
defecto, el pet activo. No hay un "panel de detalle" separado: el hero
ES el detalle.

### 6.1 Principios visuales (brief 06 §2/§3 + 06.1 §17 + 06.2 §11/§12/§17/§22)

- fondo blanco hueso cálido (`#F6F3EE`), texto casi-negro (`#26221E`) —
  el fondo cálido es parte de la identidad, PERO ya no es una hoja en
  blanco: la Collection se lee como **dos planos** (hero sobre el fondo
  base; gallery sobre `#F0EBE1`, un neutro cálido un poco más profundo,
  bajo el divisor), con whitespace y líneas estructurales discretas;
- **el arte del Nimvlet domina**, e integrado en la interfaz: el hero se
  apoya en un *hero stage* — un halo asimétrico de primitivas de primera
  parte (óvalo, o round-rect para un pet "angular" como Nidir) teñido
  con el acento del pet, que se extiende bastante más que el arte pero
  NO invade la columna de texto, más un lóbulo secundario descentrado y
  una regla de acento fina bajo el nombre (§6.2). Sin imágenes
  generadas, sin gradiente vívido, sin glassmorphism, sin cards con
  borde fuerte;
- texto de estado **humano y localizado** ("On desktop" / "Use" / "Not
  in your collection" — "En el escritorio" / "Usar" / "No está en tu
  colección"), nunca badges "ACTIVE"/"LOCKED";
- jerarquía por **tamaño / peso / espacio**, no por más contenedores:
  nombre grande, regla de acento, especie, descripción de una frase
  (§6.4-editorial), y luego selector + acción/estado; el bloque de
  texto se centra verticalmente contra el arte;
- **contraste del texto secundario** legible: `kTextMuted` ~5,0:1,
  `kTextFaint` ~3,5:1 sobre el fondo (subidos en 06.2 tras QA — brief
  §10 — sin volverlos casi-negro);
- animación de UI mínima: micro-lift de hover **instantáneo** (2pt) +
  wash + más contraste, sin tween temporizado — ver DEC-118 y §11;
- **focus-visible por modalidad de input**: el chrome de foco (anillo
  del botón, pill del chip de variante) se dibuja SOLO mientras el
  último input fue de teclado — un click de mouse lo apaga, así una
  selección de variante con mouse muestra solo el subrayado de acento,
  no un recuadro tipo control de formulario (brief §19);
- sin sidebar, sin toolbar gigante, sin cabecera sobredimensionada, sin
  dashboard, sin "Welcome back", sin barras de progreso / logros /
  rachas / métricas.

### 6.2 Acento de identidad por pet (`productui::PetAccent`, 06.1 §9 + 06.2 §17)

Cada Nimvlet lógico tiene un tono de identidad restringido. Campos:
`line` (línea de foco/selección, subrayado de variante, regla del
nombre, punto de "On desktop"), `shapeTint` (las primitivas del hero
stage y el pedestal de la gallery — dibujados a alpha bajo), `softFill`
+ `deepInk` (relleno / texto+borde del botón de acción — DEC-121),
`angularShape`. **Nunca** recolorea el resto de la UI, sin gradientes.

| Pet | Tono | Forma del hero |
|---|---|---|
| Bunny | apricot / crema cálida | óvalo |
| Nidir | violeta apagado | round-rect apenas más angular |
| Frin | azul hielo / neutro frío | óvalo |
| Rato · Rin Rin · Artu · Kyubi · Sweetie | apricot · verde bosque · marrón/oro · violeta oscuro · naranja quemado | (ids TENTATIVOS — sin arte todavía) |
| desconocido | terracota neutro | óvalo |

El hero stage: primitiva primaria ~92/255 + lóbulo secundario ~52/255,
sobre un tinte ya pálido — apoya el arte, no compite (brief §10/§11). El
botón usa `softFill` (claro) + `deepInk` (oscuro, contraste ≥ 5,5:1) —
nunca un negro arbitrario.

### 6.4 Previews compiladas livianas (`"NVPREV1"`, 06.2 §4-§7 — DEC-119)

La Collection **no abre el `.nvpack` de animación de un pet** (~46–76 MB)
para dibujar una preview estática. Al abrir la ventana,
`PetPreviewCache::LoadBundle` carga de una vez el artefacto liviano
`"NVPREV1"` (~0,3–0,4 MB c/u) de cada entrada del catálogo — el frame de
reposo canónico ya extraído por `tools/compile_pet_preview.py`. Vive al
lado del pack, mismo nombre + `.nvprev` (`productui::PreviewPathForPack`
— sin campo nuevo en el catálogo). `Get(petId, variantId)` es entonces
un lookup sobre texturas ya residentes: cambiar de variante Frin o de
hero es instantáneo. El pet activo además inyecta su frame de reposo
real a resolución completa (`SetActive`). `Clear()` libera todo al
cerrar. Regenerar tras regenerar un pack: `python3 tools/compile_pet_previews.py`.

### 6.3 Estados de propiedad

**Block 09C / DEC-136 — la Collection es SOLO lo poseído.** Un Nimvlet
sin ninguna variante en la colección del owner NO aparece (antes se
listaba como "bloqueado"). `BuildCollectionModel` descarta esos ítems;
`CollectionLayout` nunca los ve. Un pet no poseído seleccionado a mano
(un save viejo, un flag DEV) cae al pet activo — nunca hay un hero
"bloqueado". Un Frin con una sola variante poseída SÍ aparece
(`kOwnedInactive`), con la otra variante marcada no poseída y SIN ruta
de compra (§7). **Un solo Nimvlet poseído** → la gallery queda vacía: la
vista no dibuja el divisor ni el segundo plano, solo una línea quieta
`Meet more Nimvlets in the Shop.` / `Conoce más Nimvlets en la Tienda.`
(`kCollectionOnlyActive`).

**Gallery** (sub-línea bajo el arte) — solo Nimvlets poseídos:

| Estado | Sub-línea | Arte |
|---|---|---|
| poseído + activo | `On desktop` | sí |
| poseído + inactivo (sin variantes) | `Use` (en el tono del pet) | sí |
| poseído + inactivo (Frin) | `Use` | sí (variante por defecto) |

**Hero** — la línea de estado y el botón son **mutuamente excluyentes**
(`showStatusLine == !actionEnabled`), nunca los dos (brief 06.2 §18):

| Estado del hero | Muestra |
|---|---|
| activo, sin variantes | línea `● On desktop` (punto en el acento del pet) — **sin botón** |
| activo (Frin), variante mostrada == la activa | línea `● On desktop` — **sin botón** |
| activo (Frin), variante mostrada != la activa | botón `Use <name>` (re-activa con la otra variante) — sin línea |
| poseído-inactivo | botón `Use <name>` — sin línea |
| Frin con la variante mostrada NO poseída | línea `Not in your collection` — **sin botón, sin compra ni precio** (§7) |

El botón usa el `softFill` + borde `line` + texto `deepInk` del acento
del pet — nunca casi-negro (DEC-121). No se dibuja ningún botón
deshabilitado. `<name>` es el nombre propio, **nunca traducido**
("Use Frin" / "Usar Frin").

`Esc` cierra la ventana.

## 7. Frin: presentación de variantes

Frin es **UN Nimvlet lógico** con dos variantes visuales (macho,
hembra) — nunca dos entradas no relacionadas en la Collection (brief
§11). El modelo colapsa `{"frin","male"}` y `{"frin","female"}` en un
`CollectionItem` con `variants = [male, female]`.

- **Gallery**: una entrada "Frin".
- **Hero**: selector **tipográfico** `Male · Female` (`Macho · Hembra`
  en español) con un subrayado del acento de Frin bajo la variante
  seleccionada — NO dos botones (brief 06.1 §13). Cambiar de variante
  actualiza la preview y la etiqueta del botón de inmediato. `Use Frin`
  activa `{petId:"frin", variantId:<variante seleccionada>}`.
- **Persistencia**: la variante ACTIVA persiste en
  `AppState::activeVariantId` vía `TrySwitchActivePet()` — al reabrir la
  app, Frin vuelve con la variante que estaba en el escritorio. La
  variante mostrada en el hero sin activar es efímera (no se persiste).

Sin ramas de runtime especiales: se usa la infraestructura de
variante/catálogo que ya existía (Block 04/05).

## 8. Controles de usuario (menú rápido)

`core::DisplayControls` (puro) traduce las preferencias a comportamiento
GENÉRICO de runtime — ninguna rama por pet.

### Tamaño

Un conjunto finito: **Small / Medium / Large**. Es un MULTIPLICADOR
encima de `content::PetDefinition::visualScale` (dato de contenido,
congelado en Block 05 — brief §21), nunca lo reemplaza:

```
tamaño en pantalla = canvasW · visualScale · factor_de_usuario
```

| opción | factor | racional |
|---|---|---|
| Small  | 0.80 | cuatro quintos, "se aparta un poco" |
| **Medium** | **1.00** | exactamente el tamaño que declara el contenido — un owner que nunca toca el control ve el pet igual que antes de Block 06 |
| Large  | **1.15** | un poco más grande (06.1: bajado de 1.30 tras QA del owner — 1.30 estiraba los sprites detallados; DEC-114). El id persistido `"large"` se re-interpreta solo al nuevo factor, sin migración. |

`SpikeApp::EffectiveCanvasWidth()/Height()` doblan este factor; cambiar
el tamaño re-aplica `SDL_SetWindowSize` + presentación lógica + hit-mask
(la misma ruta que un switch de pet). Se persiste (`AppState::
sizeChoice`, string legible; un valor desconocido → "medium").

### Opacidad

Conjunto finito: **100 / 85 / 70 / 55 %**. `SDL_SetWindowOpacity`. 55 %
es el piso — por debajo el pet se vuelve difícil de encontrar y
clickear. Se persiste (`AppState::opacityPercent`; 0 = "sin preferencia"
→ 100 %). Un valor arbitrario se ajusta a la opción más cercana.

### Lock Position

Preferencia persistida (`AppState::lockPosition`). Cuando está
bloqueada: **el gesto sigue clasificándose** (un click corto sobre el
pet sigue contando) pero la ventana **no se mueve** —
`SpikeApp::HandleEvent` guarda SOLO la llamada a `SDL_SetWindowPosition`
del drag con `core::PetDragAllowed(lockPosition)`. Hover, click-through,
y animaciones quedan intactos (brief §16). Sin rama por pet.

## 9. System Shell: menú rápido de macOS

`platform::SystemShell` (interfaz) + `platform::CreateSystemShell()`.
macOS: `MacQuickMenu` con un `NSStatusItem` real en la barra de menús
del sistema. Windows/Linux: un adapter **no-op** (`Install()` devuelve
false; la bandeja de Windows y el StatusNotifierItem de Linux son
trabajo futuro — NO se finge una implementación, brief §24).

### Estructura del menú (block brief 06 §14 + 06.1 §2/§5)

```
Bunny                    <- header, deshabilitado (nombre del pet activo, NO traducido)
--------
Hide Nimvlet             <- o "Show Nimvlet" según el estado  (Ocultar/Mostrar Nimvlet)
Collection…                                                    (Colección…)
--------
Size      ▸  Small · Medium · Large      (checkable, exactamente uno)   (Tamaño · Pequeño/Mediano/Grande)
Opacity   ▸  100% · 85% · 70% · 55%      (checkable — los % no se traducen)   (Opacidad)
Lock Position                            (checkable)                          (Bloquear posición)
Language  ▸  English · Español           (checkable, exactamente uno)         (Idioma)
--------
Quit Nimvlets                                                  (Salir de Nimvlets)
```

Entre paréntesis, la etiqueta en español. `Language ▸` es la única
expansión de estructura de 06.1; el resto es re-etiquetado.

El `NSMenu` real se construye a partir de `platform::
BuildQuickMenuModel(ShellState)` — un modelo PURO que
`tests/QuickMenuModelTest.cpp` cubre etiqueta por etiqueta (EN y ES),
así que el test verifica exactamente la estructura que se envía. Toda
etiqueta traducible sale de `core::Localized(clave, state.language)`;
el nombre del pet y los porcentajes de opacidad no. Los endónimos
("English"/"Español") van siempre en su propio idioma.

### Cómo llegan las acciones al runtime

Cada item accionable empuja un `SDL_EVENT_USER` (con `.type` = el tipo
que `SpikeApp` registró con `SDL_RegisterEvents(1)` y `.code` =
`int(ShellAction)`) en el hilo principal. `SpikeApp::HandleEvent` lo
despacha en el MISMO event loop que todo lo demás — sin callbacks entre
hilos, sin estado global compartido.

### Icono

Un mark monocromo de primera parte (silueta de criatura sentada con
orejas), **dibujado por código** en `MakeMenuBarIcon()` y marcado como
`template` para que macOS lo tinte según el tema de la barra. **Es un
icono de desarrollo, reemplazable**: cuando exista un asset de marca
final, se reemplaza esa función por una carga de recurso. Sin emoji
(brief §14).

## 10. Hide vs Quit

| | Hide Nimvlet | Quit Nimvlets |
|---|---|---|
| Ventana del pet | oculta (`SDL_HideWindow`) | destruida |
| Aplicación | **sigue viva** | termina, limpio |
| Menú rápido | sigue disponible | desaparece |
| Collection | puede seguir abierta | se cierra con la app |
| Estado persistido | intacto | se flushea lo pendiente |
| Al reabrir "Show Nimvlet" | restaura el pet activo en su posición persistida | — |

La visibilidad **no se persiste**: al relanzar la app el pet siempre
arranca visible. Esconder ≠ salir (brief §17). `HandleShellAction`
para `kTogglePetVisibility` nunca toca la variable `running`.

## 11. Modelo de performance

Ver `docs/PERFORMANCE_BUDGETS.md` para las mediciones completas y su
metodología honesta. En resumen (macOS Release, Apple Silicon, muestras
chicas — NO presupuestos finales):

- **Product UI abierto, en reposo**: CPU ≈ 0 %. No hay loop de render
  oculto — `ProductWindow::RenderIfNeeded()` es un no-op salvo que la
  vista esté `dirty_` o haya un `EXPOSED` pendiente, y el cálculo de
  `waitMs` del event loop del pet NO se toca (no se agrega ningún
  término de deadline para la ventana de producto).
- **Pet-only en reposo tras abrir/cerrar la Collection**: CPU vuelve a
  ≈ 0 %, RSS en la misma banda que un arranque pet-only fresco — sin
  acumulación. `Close()` libera el renderer + todas las texturas +
  caches.
- **Costo al abrir la Collection (06.2 — DEC-119)**: `LoadBundle` lee
  los 4 artefactos `.nvprev` (~1,44 MB en total) y los sube como
  texturas chicas. **Ya NO se abre ningún `.nvpack`.** Antes de 06.2 se
  cargaba el pack completo (~46–76 MB) de cada pet visible no activo —
  con los locks incluidos, dos packs — para un frame de preview.
  Medido (Release, esta máquina):

  | | 06.1 (packs completos) | 06.2 (`.nvprev`) |
  |---|---|---|
  | RSS con la Collection abierta | ~324 MB | **~180 MB** (baseline pet-only ~166 MB) |
  | cambio de variante Frin (fetch) | 55–100 ms de I/O+parseo en el hilo de render | **lookup sub-ms** (+ redibujo completo ~8–10 ms) |
  | bytes de pack leídos por sesión de Collection | hasta ~244 MB | 0 |
  | 20 ciclos abrir/cerrar | sin fuga | sin fuga (RSS asienta ~128 MB, CPU 0 %) |

  El `"Use <pet>"` real sigue cargando el pack de runtime completo de
  forma transaccional — correcto (selección de preview ≠ carga del pet
  activo).
- **Nitidez del texto (06.2 — DEC-120)**: los glyphs se rasterizan a
  densidad de backing nativa y se blittean a **píxel entero del
  dispositivo** (`GlyphBlitOrigin` redondea el origen). No hay
  reescalado de texto ni segundo muestreo.
- **Microinteracción de hover (06.1)**: el micro-lift + wash es un
  cambio de estado **instantáneo**, sin tween temporizado — no agrega
  ningún deadline de render (DEC-118). El modelo event-driven de Block
  06 queda intacto.

## 12. Alcance de plataforma

- **macOS**: implementación real y única validada este bloque. Product
  UI (Core Text), menú rápido (`NSStatusItem`), activación de app.
- **Windows / Linux**: COMPILAN (todos los jobs de CI verdes). Las
  costuras existen — `platform::TextRasterizer`, `platform::SystemShell`,
  `platform::BringApplicationToForeground` — con stubs honestos que
  devuelven `false`/no-op. Una implementación real (DirectWrite /
  fontconfig+FreeType para texto; bandeja / StatusNotifierItem para el
  shell) es trabajo futuro. La arquitectura no lo impide; no se finge
  (brief §24).

## 13. Privacidad

Block 06 / 06.1 **no introduce ningún permiso nuevo** (brief §20). Ver
`docs/PRIVACY_SECURITY.md`. En concreto: un `NSStatusItem` y un `NSMenu`
son UI de nuestra propia app; Core Text dibuja en memoria propia;
`activateIgnoringOtherApps:` no requiere TCC; `SDL_GetPreferredLocales()`
(06.1, para el idioma inicial) es una consulta de configuración del OS,
sin diálogo. Sin cuenta, sin telemetría, sin red, sin captura de
pantalla en el producto, sin hooks de input globales, sin conteo global
de clicks.

Las capturas de pantalla de QA de este bloque son diagnóstico de
DESARROLLO de nuestra propia ventana (AGENTS.md §5) — nunca se
comitean, nunca son comportamiento del producto.

## 14. Localización EN/ES (06.1 / 06.2)

Contrato de idioma de producto — ver DEC-115/DEC-116.

- **`core::Localization`** (puro, `src/core`): `Language {kEn, kEs}`,
  ids persistidos `"en"`/`"es"`, `enum class StringKey` con una entrada
  por string traducible, `Localized(key, lang) -> const char*`. Sin
  framework tipo ICU. La UI y el menú piden claves semánticas, nunca
  contienen copy inglés hard-codeado.
- **Nunca se traduce**: nombres propios de Nimvlet (Bunny, Nidir, Frin,
  Artu, Rato, Rin Rin, Kyubi, Sweetie), "Nimvlets"/"Nimvlet",
  porcentajes de opacidad. **`clicks` → `clics`**, nunca "coins"/
  "monedas". Endónimos ("English"/"Español") siempre en su propio
  idioma.
- **Persistencia**: `AppState::language` (schema v3). `""` = nunca
  elegido → `SpikeApp` resuelve el inicial de `SDL_GetPreferredLocales()`
  (solo distingue es/en) **sin persistirlo**. Una elección explícita
  desde `Language ▸` se persiste y gana desde ese momento.
- **Cambio inmediato, sin reinicio**: elegir idioma re-empuja
  `ShellState` (el shell reconstruye el `NSMenu`) y
  `ProductWindow::SetLanguage` (la vista redibuja). El id del widget con
  foco es semántico, así que el foco se conserva si ese widget sigue
  existiendo (brief §21).
- **Copy editorial** (`productui::PetEditorial`, DEC-122 → DEC-127):
  tabla pura por id de catálogo + idioma (data-driven, no hard-codeada
  en la vista). `Species(petId, lang)` y `ShortDescription(petId, lang)`
  sirven la copy bilingüe **aprobada por el owner**. Block 07 (brief
  §19) alargó las descripciones a un **par de frases** — un poco más de
  carácter, sin volverse un volcado de lore; la vista las envuelve con
  `TextCache::DrawTextWrapped` (word-wrap greedy) en la columna del
  hero, con alto reservado para hasta 3 líneas.

  | pet | especie EN / ES | descripción EN / ES (Block 07) |
  |---|---|---|
  | bunny | Rabbit / Conejo | Small, curious, and never in a hurry. Bunny prefers quiet corners, tiny adventures, and staying close while you work. / Pequeño, curioso y sin ninguna prisa. Bunny prefiere los rincones tranquilos, las pequeñas aventuras y quedarse cerca mientras trabajas. |
  | nidir | Black dragon / Dragón negro | Quiet wings, bright eyes, and fire when it matters. Nidir watches the desktop like a tiny guardian, calm until something catches his attention. / Alas quietas, ojos brillantes y fuego cuando hace falta. Nidir vigila el escritorio como un pequeño guardián, tranquilo hasta que algo llama su atención. |
  | frin | White wolf / Lobo blanco | Watchful, calm, and happiest close by. Frin carries the patience of a quiet wolf, always alert without needing to make a fuss. / Atento, tranquilo y más feliz cerca. Frin tiene la paciencia de un lobo sereno, siempre alerta sin necesidad de hacer ruido. |

  El resto del roster devuelve `""` (el hero omite la línea) hasta que
  se le escriba copy propia. La copy aprobada de Block 07 SÍ nombra al
  pet en su segunda frase (levanta la restricción de DEC-122); nunca
  nombra a OTRO Nimvlet ni a "Nimvlets".

- **Strings de Shop + wallet (Block 07)** — nuevas `StringKey`, EN/ES:
  `kShop` ("Shop"/"Tienda"), `kGetPetPrefix` ("Get "/"Obtener " — se
  concatena con un nombre propio SIN traducir), `kInYourCollection`,
  `kCancel`, `kConfirm`, `kNeedMoreClicksOne`/`kNeedMoreClicksMany`
  ("Need 1 more click"/"Need {n} more clicks" — "Te falta 1 clic"/"Te
  faltan {n} clics"), `kSpendPromptOne`/`kSpendPromptMany` ("Spend {n}
  clicks to add {pet} to your collection?"). `productui::Format`
  rellena los placeholders `{n}`/`{pet}` — sin ninguna rama de idioma
  fuera de la tabla. Singular correcto (`1 click`/`1 clic`). "Shop" SÍ
  se traduce; "Nimvlets" y los nombres propios de pet no.

## 15. Intencionalmente diferido

| Feature | Bloque |
|---|---|
| ~~Shop: comprar Nimvlets, precios, gastar clicks~~ | **hecho en Block 07 — ver §16–§19** |
| Página de Settings avanzada | Block 08 |
| Onboarding / selección de starter / secreto de 44 s / shop oculto de starters (incluida la 2ª variante de Frin) | Block 09 — la arquitectura de autorizaciones ya lo soporta (§19), NADA en Block 07 lo implementa ni lo insinúa |
| Bandeja de Windows / equivalente de Linux para el System Shell | futuro, hardware mediante |
| Texto de producto en Windows/Linux (DirectWrite / fontconfig) | futuro |
| Conteo global de clicks, permiso de input global | futuro, opt-in explícito (AGENTS.md §14) |
| Preferencia de fullscreen, launch-at-login, dark mode | futuro |
| Corrección visual de `lie_to_sit` de Frin | deuda conocida, congelada (brief §21) |
| Idiomas más allá de EN/ES | futuro (el catálogo de claves está listo; agregar un idioma es una columna más) |
| **Fondo escénico ilustrado por Nimvlet en el hero** | futuro — nota arquitectónica en §16.4; NO se generan ni envían imágenes en Block 07, no se agrega un sistema de escenas, sin red en runtime |
| Personalidades/lore extensas por Nimvlet | futuro con dirección de contenido — Block 07 alargó la descripción a un par de frases para Bunny/Nidir/Frin (DEC-127); el resto del roster sin copy |
| Menú `Language` / entrada al Shop en el menú de la barra (Windows/Linux, o macOS) | congelado — el Product UI provee el Shop, el menú NO cambia (brief §22) |
| Microanimación de hover temporizada (120–160 ms) | descartada a propósito (DEC-118) — la performance manda |
| Mover `PetEditorial` / accent / preview / precio a datos del catálogo o del pack | futuro (hoy `PetEditorial`/`PetAccent` son tablas en `src/productui`; el precio y `publiclyPurchasable` SÍ están en el catálogo desde Block 07 — DEC-125) |
| Notificación al ganar clicks / toast de compra exitosa | descartado a propósito (brief §3/§15) — el balance y el estado se actualizan en vivo, sin toast |

Nada de lo anterior está implementado ni insinuado en el código de
Block 06 / 06.1 / 06.2 / 07.

## 16. El Shop — BROWSE-FIRST (Block 07 + 09C)

El Shop es una **sección separada** del Product UI — no reemplaza a la
Collection ni la conoce. Se siente como "conocer a otro Nimvlet", no
una plantilla de tienda (brief 07 §8 / 09C §3): **sin** sidebar, card
wall, dashboard, carrito, búsqueda, filtros, categorías, banners ni
countdowns. Reusa el acento de identidad por pet, el hero stage, la
tipografía del sistema y las previews `.nvprev` livianas (§6.4 — el
Shop **no abre ningún `.nvpack`** para navegar).

**Block 09C** cambió la jerarquía de entrada. Antes (Block 07) el Shop
abría con el primer pet ya expandido como hero gigante — se sentía
"como la Collection con controles de compra". Ahora el Shop abre en
modo **BROWSE**: una estantería de personajes. `productui::ShopLayout`
tiene dos presentaciones (`ShopPresentation`), y `ShopView` distingue
**tres** cosas distintas — nunca una sola variable:

| | qué es | qué NO hace |
|---|---|---|
| `hoverId_` | la tarjeta bajo el mouse / foco de teclado | no selecciona, no compra |
| `selectedPetId_` | `""` ⇒ modo BROWSE; un `petId` ⇒ ese personaje es el hero | no compra, no muta wallet/propiedad |
| `confirming_` | sub-estado de SELECTED: la confirmación inline | solo `Confirmar` emite la compra |

Ninguna selección se persiste: cerrar/reabrir el Product UI, o volver a
entrar a la sección, devuelve el Shop a BROWSE (`ShopView::OnEnterSection`).

```
── BROWSE (entrada normal) ─────────────────────────────────────
Nimvlets                                          312 clicks   <- cabecera COMPARTIDA
Collection  ·  Shop  ·  Settings                                <- pestañas de texto (§17)

                 Nimvlets you can meet                         <- encabezado quieto (kTextMuted)

        ┌──────────┐   ┌──────────┐   ┌──────────┐
        │  [ arte  │   │  [ arte  │   │  [ arte  │              <- rejilla que envuelve;
        │  grande] │   │  grande] │   │  grande] │                 1..8 personajes, sin
        └──────────┘   └──────────┘   └──────────┘                 scroll horizontal
           Bunny          Nidir          Artu
        (300 clicks)                                            <- SOLO al hacer hover / foco:
                                                                   precio, o "In your collection"

── SELECTED (tras clic / Enter en una tarjeta) ─────────────────
 ╭─ hero stage teñido con el acento del pet ─╮
 │   [ ARTE GRANDE ]   Nidir                  │   <- el personaje elegido, ahora protagonista
 │   del Nimvlet       ──                     │      (nombre / regla de acento / especie /
 │                     Black dragon           │       descripción / precio / acción-o-estado)
 │                     Quiet wings, bright…    │
 │                     300 clicks             │
 ╰─────────────────────[ Get Nidir ]──────────╯
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  <- divisor = borde del 2º plano
        [·] Bunny      [·] Nidir̲               <- RAIL: la estantería completa, compacta;
                                                   la tarjeta abierta lleva un subrayado de
                                                   acento; un clic elige otro personaje
```

### 16.1 Contenido del Shop — DATO, no ramas

Qué pets aparecen, su precio, y a qué autorización dan derecho son
**datos del catálogo** (`NVCATLG1` v3 — `docs/CATALOG.md` §12): por
entrada, `priceClicks` (`u64`) y `publiclyPurchasable` (`u8`). NUNCA
hay un `if (pet == "nidir")` en el runtime/UI (brief §10). Una entrada
`publiclyPurchasable` con precio 0 la rechazan el compilador y el
loader.

`catalog::ShopModel` (puro) — `BuildShopModel(catalog, balance,
entitlements)` produce una fila por pet lógico con al menos una entrada
`publiclyPurchasable`. **Frin nunca aparece** (sus dos entradas están
`publiclyPurchasable: false`) — aunque su entrada de catálogo exista, el
modelo no la lista (brief §11). Cada `ShopItem` lleva `priceClicks`,
`status` (`kAffordable` / `kInsufficientBalance` / `kOwned`),
`clicksShort` y `entitlementTarget`.

**Precios PROVISIONALES de QA/economía** (no balanceo final): Bunny
120, Nidir 300, Frin no público. Con la semilla de dev el owner posee
Bunny + Frin y Nidir está locked, así que el Shop ejercita `kOwned`
(Bunny), `kAffordable`/`kInsufficientBalance` (Nidir según el balance)
y la ausencia total (Frin) con solo el contenido real que ya existe.

### 16.2 Modo BROWSE, hover/foco, y modo SELECTED

**BROWSE** (`ShopPresentation::kBrowse` — la entrada normal). La rejilla
de personajes es TODO el contenido; no hay hero. `BuildShopLayout`
elige columnas por cantidad (1..4 en una fila; 5/6 → 2 filas de ≤3;
7/8 → 2 filas de 4), centra cada fila, y reserva SIEMPRE el alto de la
línea revelada para que el hover no reordene nada. Cada tarjeta es
`[ arte grande ] + nombre`; nada más en reposo (brief §5). Un encabezado
quieto (`kShopBrowseHeading`) enmarca la estantería. Con el catálogo
DEV vacío, un mensaje quieto y localizado (`kShopEmpty`) — nunca se
menciona el catálogo sintético (brief §14).

**Hover / foco de teclado** sobre una tarjeta revela info liviana bajo
el nombre (brief §6) — **sin seleccionar ni comprar**:

| estado del ítem | línea revelada | color |
|---|---|---|
| `kOwned` | `In your collection` (localizado) | acento del pet |
| `kAffordable` | el precio (`300 clicks` / `300 clics`) | `kTextMuted` |
| `kInsufficientBalance` | el precio | `kTextFaint` (distinción "quiet" asequible/insuficiente — sin rojo, sin "no te alcanza") |

**SELECTED** (`ShopPresentation::kSelected`) — un clic o Enter/Espacio en
una tarjeta **SELECCIONA** ese personaje (nunca compra). Se promueve a
hero grande (arte `kHeroArt` 216 pt, ~3,6× una tarjeta del rail) con
especie, descripción editorial, precio y acción; la estantería completa
baja a un **rail** compacto bajo un divisor, sobre el segundo plano
`kGalleryShelf`, con la tarjeta abierta marcada por un subrayado de
acento. El foco se queda en esa misma tarjeta (ahora en el rail) — la
selección **nunca** salta el foco a la confirmación de compra (brief
§12). En el hero, la línea de estado, el botón y la confirmación son
**mutuamente excluyentes** (idénticos a Block 07):

| Estado del hero | Muestra |
|---|---|
| `kAffordable`, sin confirmar | precio + botón `Get <name>` (relleno/tinta del acento del pet — nunca casi-negro) |
| `kAffordable`, confirmando | precio + `¿Gastar N clics para añadir <name> a tu colección?` + `Cancelar` · `Confirmar` |
| `kInsufficientBalance` | precio + línea contenida `Need N more clicks` (tono atenuado), **sin botón** |
| `kOwned` | `● In your collection` (punto en el acento), **sin precio, sin botón, sin CTA de compra** |

`<name>` es el nombre propio, **nunca traducido** ("Get Nidir" /
"Obtener Nidir"). Un `selectedPetId` que no está en el Shop (Frin, o un
id desconocido / un save editado) cae a BROWSE — nunca un hero fantasma.

**Teclado.** Orden de tabulación BROWSE: `nav:collection` → `nav:shop` →
`nav:settings` → una tarjeta por pet en orden de catálogo. SELECTED:
las pestañas → los controles del hero (`get`, o `purchase:cancel` /
`purchase:confirm` al confirmar) → las tarjetas del rail. Enter/Espacio
sobre una tarjeta selecciona; sobre `get` abre la confirmación (foco →
`purchase:cancel`); `Esc` cancela la confirmación si hay una abierta, y
si no, cierra la ventana (semántica de Block 07 preservada).

### 16.3 `.nvprev` / performance

El Shop reusa `PetPreviewCache::LoadBundle` de Block 06.2 — el mismo
bundle liviano cargado una vez al abrir el Product UI sirve a las tres
secciones. La rejilla de BROWSE, el hero y el rail de SELECTED hacen
todos el mismo lookup `previews.Get(petId, "")` sobre la MISMA textura
residente — no se duplica ninguna textura entre browse y hero (brief
§15). Cambiar de modo, de hero o de idioma es un lookup en un mapa, sin
I/O, sin abrir packs. `ShopView::OnMouseMove` marca `dirty_` **solo**
cuando el objetivo de hover cambia de verdad — mover el mouse dentro de
una tarjeta no redibuja. `ProductWindow::RenderIfNeeded()` sigue siendo
un no-op salvo `dirty_`/`EXPOSED`; el cálculo de `waitMs` del event
loop del pet NO se toca. Idle del Shop abierto ≈ 0 % CPU, igual que la
Collection (medido: ~3 % en el primer frame, 0,0 % en reposo).

### 16.4 Fondo escénico del hero — nota arquitectónica, NO implementada

El owner mencionó (brief §20) que un futuro hero podría usar un fondo
ilustrado único del universo Nimvlets. **Block 07 no genera ni envía
esas imágenes** y no agrega un sistema de escenas. El `PetAccent` +
hero stage actuales ya son un "seam" de datos de presentación por pet;
agregar más adelante un `backdrop` estático opcional (un asset local
optimizado, sin red en runtime) no se vuelve más difícil por nada de lo
que hace Block 07.

## 17. Navegación Collection ↔ Shop

`productui::ProductSection { kCollection, kShop }`, dueño en
`ProductWindow`. La cabecera es **compartida** (`SectionNav` puro +
`SectionHeaderView` SDL): título "Nimvlets" + balance de clics + una
fila de pestañas de texto compacta `Collection · Shop`, idéntica en las
dos secciones. Reemplaza al viejo título/subtítulo de sección de Block
06 ("Collection" / "Your companions").

- **Mouse y teclado**: los ids `nav:collection` / `nav:shop` encabezan
  el anillo de foco de cada sección; Tab/Enter/Space navegan; el chrome
  de foco sigue el patrón "focus-visible por modalidad" de Block 06.2.
- Tocar una pestaña cambia de sección **en la misma ventana**. **El
  runtime del pet no se toca** — no se recrea, no se recarga ningún
  pack (brief §17).
- El foco se conserva razonablemente al cambiar de idioma (ids
  semánticos). Cambiar de idioma con el Shop en SELECTED **no pierde el
  personaje seleccionado** (Block 09C).
- Al **reabrir** el Product UI se vuelve a Collection — no se recuerda
  la última sección (sin una razón de bajo costo para hacerlo, brief
  §17). No se sobre-construye routing. **Entrar a la sección Shop**
  (`ShopView::OnEnterSection`) siempre arranca en BROWSE: ninguna
  selección de Shop se recuerda entre visitas (Block 09C, brief §13).
- El **menú rápido de la barra NO cambia** (brief §22): el Shop se
  alcanza solo desde la navegación del Product UI, no hay un item
  "Shop…" en el `NSStatusItem`.

## 18. Wallet + transacción de compra

Clicks son la **única** moneda (AGENTS.md §2). Block 07 los vuelve
**gastables**:

- **Confirmación inline, no accidental** (brief §12): "Get <pet>" abre
  una pregunta contenida en la columna del hero (no un modal gigante).
  El foco arranca en **Cancelar**; `Esc` o "Cancelar" la cierran sin
  tocar nada; solo "Confirmar" emite la compra. Un click perdido nunca
  gasta.
- **Política pura** `catalog::EvaluatePurchase(catalog, PetIdentity
  target, ...)` (testeable sin GUI — brief §14): el objetivo es una
  **identidad de catálogo completa** (`{petId, variantId}`), NO un petId
  suelto — se resuelve contra una entrada EXACTA del catálogo y lo que
  se otorga es la identidad de esa entrada (DEC-128). `ShopItem::
  entitlementTarget` la lleva; la vista la emite como `PurchaseRequest`.
  Para los pets de Block 07 (Bunny, Nidir) el objetivo es `{petId, ""}`;
  la misma política ya procesa `{frin, "male"}` en un catálogo sintético
  (`PurchasePolicyTest`) sin ninguna rama por pet — el seam para el shop
  oculto de starters (Block 09) existe sin código futuro. Resultados:
  `kSuccess` / `kAlreadyOwned` / `kInsufficientBalance` / `kNotPurchasable`
  (la entrada existe pero no es pública, o precio 0) / `kInvalidTarget`
  (ninguna entrada con esa identidad exacta — incluido comprar `{frin,
  ""}`). En cualquier fallo el estado resultante == el de entrada;
  **nunca una mutación parcial**. La resta solo corre tras `balance >=
  precio` → **sin underflow posible**.
- **Transacción atómica** `SpikeApp::HandlePurchaseRequest`: si es
  `kSuccess`, muta `clickBalance` **y** `ownedEntitlements` en el MISMO
  `AppState`, sin escrituras intermedias, y **flushea de inmediato**
  (`FlushPersistedState()` — un solo `SerializeAppState` + un solo
  `rename` atómico). Un crash no puede persistir "gasté el balance pero
  no tengo el pet". El per-click normal SIGUE con el debounce de ~2s
  (`docs/PERSISTENCE.md` §6); la persistencia inmediata es la única
  excepción, y es solo llamar al flush existente.
- **Consistencia inmediata** (brief §16): tras una compra exitosa el
  Shop pasa a `In your collection` y la Collection marca el pet como
  poseído, sin reiniciar el Product UI ni la app. Activar el pet recién
  comprado usa el switch transaccional de runtime que ya existía
  (`TrySwitchActivePet`, que gatea propiedad y NUNCA otorga — DEC-128);
  cambiar entre contenido ya poseído sigue siendo gratis.
- **Balance visible**: discreto, arriba a la derecha, en la cabecera
  compartida. `312 clicks` / `1 click` (singular) — `312 clics` / `1
  clic`. Se actualiza en vivo con cada click del pet y tras una compra.
  Sin icono de "monedas", sin wallet premium, sin toast de éxito.

## 19. Autorizaciones capaces de variantes (`catalog::PetEntitlement`)

`AppState::ownedPetIds` (conjunto de `petId`) queda **superseded** por
`AppState::ownedEntitlements`:

- **`catalog::PetEntitlement { petId, variantId }`** — misma forma y
  semántica de `variantId` que `PetIdentity`: `""` = el Nimvlet NO
  tiene variantes (Bunny, Nidir); no vacío = **solo esa variante**.
  **NO existe una autorización de "todas las variantes de un Nimvlet"**
  — la propiedad de Frin es siempre por variante, explícita (DEC-128).
  `Covers(PetIdentity)` es **coincidencia exacta** en los dos campos
  (`{frin, ""}` NO cubre `{frin, "male"}`); es el gate de activación.
  `CanonicalizePetEntitlements` = orden + dedup (sin subsunción — ya no
  hay "pet entero" que subsuma), sin conocer el catálogo.
- **`persistence::OwnedEntitlement`** — el mismo par, como dato plano en
  `src/persistence` (sin dependencia de `src/catalog`); `src/app`
  puentea, la misma división que `activePetId` (string) vs.
  `PetIdentity`.
- **`CollectionModel`** — cada `CollectionVariant` lleva un flag
  `owned`; `CanActivate(model, petId, variantId)` exige la variante
  EXACTA. Una variante no poseída de un Frin por lo demás poseído se
  muestra en el selector **atenuada**, no es activable, y **sin ninguna
  ruta de compra visible** (el shop oculto de starters es futuro — brief
  §6). Tras la migración el owner tiene las dos variantes de Frin, así
  que ese estado no le aparece.
- **Migración** (AppState `v2/v3 → v4`, DEC-124/DEC-128, afinada por
  DEC-129): el serializer parsea `ownedPetIds` a `{petId, ""}`
  PROVISIONAL (sin catálogo) y reporta la versión en disco por un
  out-param; `src/app` corre `catalog::ExpandHistoricalWholePetEntitlements(ents)`
  **solo si esa versión < la actual**, usando una **tabla histórica
  CONGELADA** (no recibe catálogo ni lo enumera) — un `{frin, ""}`
  legacy se **expande** a `{frin, "male"} + {frin, "female"}` (lo que
  Block 06 exponía), NO a "todo Frin" ni a variantes agregadas después
  del schema v3 aunque ya estén en el catálogo. El AppState persistido
  nunca contiene `{frin, ""}`. Ver `docs/PERSISTENCE.md` §3.
- **Resolución del pet activo, sin bypass de economía** (DEC-128):
  `catalog::ResolveOwnedActiveIdentity` reemplaza al viejo
  `EnsureActiveEntitlementOwned`. Si el pet/variante activo persistido
  NO está autorizado (save corrupto / editado a mano), cae a uno que sí
  lo esté (default del catálogo, o la primera entrada autorizada) — y
  **nunca otorga la autorización que falta**. `TrySwitchActivePet`
  gatea propiedad y tampoco otorga; establecer propiedad es solo cosa
  de la siembra / migración / compra.
- **Frontera para Block 09**: onboarding y shop oculto escriben
  `ownedEntitlements` con su propia lógica (una sola variante de Frin,
  la otra por separado) sin tocar el catálogo ni el modelo. **Nada en
  Block 07 lo implementa.**

## 20. Settings — tercera sección del Product UI (Block 08)

Block 08 agrega **Settings** a la navegación de secciones:
`Collection · Shop · Settings` (mismo `SectionNav` + `SectionHeaderView`
que Block 07; `ProductSection` gana `kSettings`). Reabrir el Product UI
vuelve a Collection — no se recuerda la última sección.

> **DEC-134 (corrección de QA).** La cabecera compartida dibuja las TRES
> pestañas en las tres secciones, pero Block 08 solo enseñó a rutear la
> pestaña `nav:settings` a `SettingsView`: `CollectionView` y `ShopView`
> seguían con el par `nav:collection`/`nav:shop` de Block 07, así que un
> click o Enter en "Settings" desde Collection o Shop se descartaba en
> silencio — y como el Product UI siempre abre en Collection, Settings
> quedaba INALCANZABLE. Ahora las tres vistas rutean sus pestañas por una
> única tabla, `productui::NavTargetSection(focusId, &outSection)` en
> `SectionNav`, para que la lista de secciones y sus consumidores no
> puedan volver a divergir. (La sección Settings en sí ya funcionaba;
> solo no se podía llegar.)

### 20.1 Qué expone

EXACTAMENTE las cuatro preferencias que Block 06/07 ya persisten, sin
ninguna nueva:

| Preferencia | Valores | Efecto de runtime |
|---|---|---|
| **Size** | Small 0.80 · Medium 1.00 · Large 1.15 | `ApplyPetWindowMetrics` (igual que un switch de pet) |
| **Opacity** | 100 · 85 · 70 · 55 % | `SDL_SetWindowOpacity` |
| **Lock position** | On / Off | gate de inicio de drag (`core::PetDragAllowed`) |
| **Language** | English / Español (endónimos) | relabela menú + las tres secciones, sin reiniciar |

No hay slider de opacidad, ni tamaños intermedios, ni Hide/Show, ni Quit,
ni acciones de Collection: Settings son **preferencias**, no un segundo
menú del sistema (brief §4/§5). Sin filas placeholder de features
futuras.

### 20.2 Una sola ruta canónica de preferencias (DEC-130)

```
Menú rápido nativo ──► SpikeApp::HandleShellAction ──┐
                                                     ├──► SpikeApp::Apply{Size,Opacity,Lock,UiLanguage}
Settings (SettingsView) ──► ProductWindowEvent ──────┘        │
   (emite productui::SettingsChange)                          │
                                                             mutación de UN campo de appState_
                                                             + MarkDirty (debounce de ~2s, SIN escritura inmediata)
                                                             + efecto de runtime
                                                             + PushShellState        (el menú refleja el valor)
                                                             + PushPreferencesToProductWindow (Settings refleja el valor)
```

- **No hay un segundo sistema.** Los cuatro `Apply*` son el único punto
  de mutación; el menú rápido y Settings los llaman a los dos.
- **`SettingsView` nunca muta sus propias preferencias**: emite un
  `SettingsChange` y espera que src/app le re-empuje el estado final vía
  `ProductWindow::SetPreferences` — la misma disciplina de "fuente de
  verdad única" que el Shop tras una compra. Así el menú rápido y
  Settings no pueden divergir (brief §6/§7).
- **`core::Preferences`** (puro, `src/core/Preferences.h`) es la moneda
  común: `PreferencesFromStored(...)` normaliza los campos crudos de
  AppState (0 de opacidad → 100, tamaño desconocido → medium, …), así un
  estado editado a mano nunca produce un valor imposible.
  `StepSize` / `StepOpacityPercent` / `OtherLanguage` ciclan las
  opciones del control segmentado.

### 20.3 Presentación

`productui::BuildSettingsLayout` (puro, `nimvlets_productui_core`) +
`productui::SettingsView` (SDL). Composición **quiet / cálida /
compacta**: dos grupos separados por aire y una regla hairline —
**Companion** (Size, Opacity, Lock position + una frase de ayuda) y
**Language** (el selector; el encabezado del grupo ya lo nombra, la fila
no repite label). Controles **segmentados**: el valor actual es un pill
relleno (tinta de texto, no casi-negro-puro del tema), el resto
contorno hairline. Sin cards, sin acento por pet, sin iconos, **sin
previews** (Settings no abre ningún `.nvpack` — brief §23). Cabe en
800×560 sin scroll en EN y ES.

### 20.4 Teclado / foco (brief §18)

El foco de teclado vive en la **fila** (`row:size` / `row:opacity` /
`row:lock` / `row:language`), no en cada segmento:

- **Tab / Shift+Tab**, y **↑ / ↓**: recorren el anillo de foco
  (pestañas de nav → filas, de arriba hacia abajo).
- **← / →**: sobre una fila de preferencia, opción anterior / siguiente
  (se detiene en los extremos), aplicada de inmediato. Sobre una
  pestaña de nav, recorren el foco.
- **Enter / Espacio**: sobre una pestaña, cambia de sección; sobre una
  fila, avanza a la siguiente opción (cíclico).
- **Esc**: cierra la ventana.

Chrome de foco solo tras input de teclado ("focus-visible por
modalidad", Block 06.2); un click de mouse lo apaga. El hit-test de
mouse SÍ resuelve el segmento exacto (`opt:<field>:<value>`), así un
click elige esa opción directo. El cambio de una preferencia **no**
mueve el foco a otro lado.

### 20.5 AppState / rendimiento / privacidad

- **Sin bump de schema**: AppState sigue en **v4**. `sizeChoice`,
  `opacityPercent`, `lockPosition`, `language` ya existían. Ver
  `docs/PERSISTENCE.md` §7.
- **Event-driven**: Settings redibuja solo ante un cambio real
  (`SettingsView::Dirty()`), sin loop, sin timer, sin thread. Abierto y
  en reposo = efectivamente ocioso.
- **Local, sin permisos nuevos, sin red.** "Accesibilidad" acá =
  teclado/foco usable dentro de nuestra ventana, nunca una API global
  del SO (brief §19/§24).
- El menú rápido de la barra **no cambia** (brief §21/§22): no hay item
  "Settings…"; Settings se alcanza solo por la navegación del Product
  UI.

### 20.6 Frontera para Settings futuros

Bloques futuros pueden agregar preferencias (launch-at-login,
fullscreen, modo de clics global opt-in). **Nada de eso se implementa ni
se insinúa acá** — sin placeholders deshabilitados, sin registry
especulativo. Un `PreferenceField` nuevo + su fila + su `Apply*` es todo
lo que haría falta.

## 21. El SHOP OCULTO DE STARTERS — submodo contextual del Shop (Block 10)

Fuentes: `catalog::StarterShopModel` / `catalog::StarterPurchasePolicy`
(puro), `productui::StarterShopLayout` (puro) +
`productui::StarterShopView` (SDL), `productui::ShopPaint` (pintado
compartido), `productui::ShopLayout` (helpers de geometría expuestos).
Decisión: **DEC-137**. Ver `docs/ONBOARDING.md` §15/§16 para la política
y el harness DEV.

### 21.1 Qué es

Un usuario que **completó el onboarding** (lifecycle `kCompleted`
EXACTO — NO `kLegacyComplete`, NO `kPending`) y eligió UNA variante de
Frin puede comprar la OTRA con clics. Genéricamente: un starter no
poseído (`starterRole != kNone` + `priceClicks > 0`) puede ofrecerse.
La **regla de no-divulgación** (DEC-137, brief §3): una oferta de un
starter SECRETO (Frin) solo es elegible si el owner ya posee alguna
variante hermana del mismo petId lógico — así Frin NUNCA se le revela a
quien nunca lo descubrió. No hay ni se persiste un flag de "revelado".

### 21.2 Acceso — NO una cuarta pestaña (brief §10/§11)

`SectionNav` sigue siendo EXACTAMENTE `Collection · Shop · Settings`.
Cuando `BuildStarterShopModel` tiene ≥ 1 oferta, el Shop **público**
dibuja UNA línea quieta cerca del pie:

- EN: `Starter choices…`  ·  ES: `Opciones iniciales…`

(`ShopLayout::starterAffordanceVisible`, `focusId "starter:enter"`, al
FINAL del focus order, en `kTextFaint`). **Sin** banner, badge, toast,
popup, notificación ni "secret unlocked". Con 0 ofertas la afordancia
NO existe. `ShopView::SetStarterAffordanceVisible(bool)` la controla;
`src/app` la pone en `!starterShopModel.Empty()`.

Activar la afordancia entra a un **submodo que la sección Shop POSEE**
(`ProductWindow::starterShopSubmode_`, solo relevante con `section_ ==
kShop && !onboarding_`):

- la cabecera compartida sigue marcando **Shop**;
- el input se rutea a `StarterShopView` en vez de `ShopView`;
- un back affordance quieto arriba a la izquierda: `← Shop` / `← Tienda`
  (`focusId "starter:back"`).

Cambiar de sección (tocar `Collection`/`Settings`, o `Shop` desde el
submodo) o reabrir el Product UI **limpia el submodo** — se vuelve al
Shop público en BROWSE.

### 21.3 Jerarquía visual — reusa el Shop browse-first

```
── Starter Shop BROWSE ─────────────────────────────────────────
Nimvlets                                          350 clicks   <- cabecera COMPARTIDA
Collection  ·  Shop  ·  Settings                                <- "Shop" activo (submodo)
← Shop                                                          <- back affordance quieto

                 Starter choices                               <- encabezado quieto

        ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐
        │  [ arte  │   │  [ arte  │   │  [ arte  │   │  [ arte  │
        └──────────┘   └──────────┘   └──────────┘   └──────────┘
         Frin · Male     Artu (dev)     Rato (dev)    Rin Rin (dev)
        (150 clicks)                                            <- SOLO al hover / foco

── Starter Shop SELECTED (tras clic / Enter) ───────────────────
 ╭─ hero stage teñido con el acento del pet ─╮
 │   [ ARTE GRANDE ]   Frin · Male            │   <- nombre compuesto: "Frin" (no se traduce)
 │   (frin/male.nvprev) ──                    │       + " · " + Male/Macho (kMale/kFemale)
 │                     White wolf             │
 │                     Watchful, calm…        │
 │                     150 clicks             │
 ╰─────────────────────[ Get Frin · Male ]────╯
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  <- divisor = borde del 2º plano
        [·] Frin · Male̲   [·] Artu (dev)  …        <- RAIL: todas las ofertas, compacto
```

Reusa, sin una segunda copia (brief §12):

- **geometría** — `ComputeBrowseGrid` / `LayoutBrowseGrid` /
  `LayoutShopRail` / `LayoutShopHero`, extraídos a helpers públicos en
  `ShopLayout.h`;
- **pintado** — `DrawShopTile` / `DrawShopHero` / `FillShopStagePrimitive`,
  extraídos a `productui/ShopPaint.{h,cpp}` y usados por igual por
  `ShopView` y `StarterShopView`.

Diferencia clave: `StarterShopLayout` opera en **IDENTIDADES EXACTAS**.
`focusId` de una oferta = `"starteritem:<petId>/<variantId>"` — dos
"frin" (male / female) conviven como ofertas distintas, cosa que el
`ShopModel` público (keyed por petId lógico) no podría. El hero y las
tarjetas resuelven la `.nvprev` por `(petId, variantId)` EXACTOS.

### 21.4 Compra

`Get <name>` → confirmación inline (`purchase:cancel` /
`purchase:confirm`, el foco arranca en Cancel) → `Confirmar` emite un
`PurchaseRequest` con la identidad EXACTA. `src/app`
(`HandleStarterPurchaseRequest`) evalúa `catalog::EvaluateStarterPurchase`
(canal DISTINTO — re-checa lifecycle == kCompleted, rol de starter,
precio, regla del secreto, propiedad, saldo — el modelo/UI NO es un
límite de seguridad) y, si es `kSuccess`, aplica `ApplyPurchasedState`
(el MISMO camino atómico balance+propiedad+flush que el Shop público —
DEC-126). **VARIANTE EXACTA:** comprar Frin otorga solo `{frin,"male"}`
o `{frin,"female"}`, nunca ambas, nunca `{frin,""}`.

**NO auto-activa (brief §17):** el pet del escritorio NO cambia. Balance
y Collection se refrescan al instante; la nueva variante es usable desde
el flujo "Use <name>" de la Collection sin reiniciar. La Collection
sigue siendo owned-only (DEC-136): tras comprar la 2ª variante de Frin,
la fila de Frin muestra `Male · Female` ambas poseídas y activables.

### 21.5 Teclado / Esc (brief §20)

| Estado del submodo | Esc |
|---|---|
| browse | vuelve al Shop público (BROWSE) |
| seleccionado (sin confirmar) | vuelve a browse |
| confirmando | cancela la confirmación (NUNCA sale del submodo) |

`Esc` en el submodo **NUNCA cierra la ventana** — siempre da un paso
atrás. Tab/Shift+Tab + flechas recorren el anillo (`starter:back` →
ofertas; en seleccionado: `starter:back` → `get` → tarjetas del rail).
Enter/Espacio selecciona una oferta / abre la confirmación / confirma.
Chrome de foco solo tras input de teclado (misma disciplina que el
resto del Product UI).

### 21.6 Estado vacío (brief §19)

Comprar la última oferta deja el submodo abierto con una línea quieta —
EN `No more starter choices.` / ES `No quedan opciones iniciales.` — +
`← Shop`. Sin confetti / toast / logro. Al volver al Shop público la
afordancia ya no existe.

### 21.7 AppState / performance / privacidad

- **Sin bump de schema** (AppState sigue en v5). El Starter Shop deriva
  todo de `onboardingLifecycle` + `ownedEntitlements` + `clickBalance`
  que ya se persisten. Una compra reusa el flush inmediato del Shop
  (DEC-126).
- **Event-driven:** `StarterShopView` redibuja solo ante un cambio real
  (`Dirty()`), sin loop, sin timer, sin thread. El submodo abierto y en
  reposo = efectivamente ocioso. Usa las `.nvprev` livianas ya
  residentes (`PetPreviewCache` de Block 06.2) — NUNCA abre un
  `.nvpack`.
- **Sin permisos nuevos, sin red, sin selección persistida** entre
  visitas al submodo.

### 21.8 Producción — intacta

El catálogo de producción NO se toca (`starterRole: kNone` + precio 0 en
sus 4 entradas) → el Starter Shop está **inerte en producción**. Se
ejercita solo por el harness sintético-DEV (`NIMVLETS_DEV_ONBOARDING`).
Ver `docs/ONBOARDING.md` §16 y `README.md`.
