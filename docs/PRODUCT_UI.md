# Nimvlets — Product Shell + Collection + Quick Menu (Block 06)

Este documento describe la capa de **producto** que Block 06 agrega
sobre el runtime de pet de Block 01–05: una ventana de aplicación
normal con una **Collection**, un **menú rápido nativo** en la barra de
menús de macOS, y los controles de usuario (mostrar/ocultar, bloquear
posición, tamaño, opacidad). Es el primer bloque en el que Nimvlets se
siente como una aplicación de escritorio coherente y no solo un runtime
de pet.

Ver `docs/DECISION_LOG.md` DEC-106 en adelante para por qué se tomó
cada decisión, `docs/CATALOG.md` §11 para el modelo de propiedad,
`docs/PERSISTENCE.md` §3 para el schema v2 del archivo de estado, y
`docs/PERFORMANCE_BUDGETS.md` para las mediciones.

## 1. Alcance de Block 06

Dentro de alcance:

- una **ventana de aplicación normal** (con marco, enfocable,
  redimensionable) para el Product UI;
- la **Collection**: un álbum de los Nimvlets del owner con tres
  estados de propiedad (activo, poseído-inactivo, bloqueado), un panel
  de detalle expandido, y el modelo de variantes de Frin (un Nimvlet
  lógico, macho/hembra);
- **switching de pet en vivo** desde la Collection, sin reiniciar;
- el **click balance** visible SOLO dentro del Product UI;
- un **menú rápido nativo** en macOS (`NSStatusItem`): pet actual,
  Show/Hide, Collection…, Size ▸, Opacity ▸, Lock Position, Quit;
- **Show/Hide** del pet, **Lock Position**, **Size** (small/medium/
  large), **Opacity** (100/85/70/55 %), todos persistidos salvo la
  visibilidad;
- separación de ciclo de vida: cerrar la Collection NO termina la app,
  no resetea el pet ni el balance, no detiene el runtime.

Explícitamente fuera de alcance (ver §11): compras / precios / gastar
clicks, onboarding / selección de starter / el secreto de 44 s, la
bandeja de Windows y su equivalente en Linux, conteo global de clicks,
preferencia de fullscreen, launch-at-login, dark mode, una página de
Settings avanzada, animaciones nuevas de pet, y la corrección visual de
`lie_to_sit` de Frin (deuda conocida, congelada — AGENTS.md, brief §21).

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
|   +-- Collection          (Shop y Settings son bloques futuros — NO
|   +-- [Shop]  Block 07     existen ni como pantallas vacías, brief §7.)
|   +-- [Settings] Block 08
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
  Bunny + Frin como `initially_owned`; Nidir queda **bloqueado**, para
  que la Collection ejercite los tres estados con solo los packs
  reales que ya existen.
- **Autoridad de runtime**: `persistence::AppState::ownedPetIds` (schema
  `NVSTATE1` v2), un conjunto de `petId` (nunca por variante — poseer
  "frin" da acceso a macho y hembra). `ownershipSeeded` (bool)
  distingue "nunca se inicializó la propiedad" de "posee cero
  Nimvlets": `SpikeApp::Init()` siembra desde el catálogo **solo
  cuando `ownershipSeeded` es false**, y lo pone en true.
- **Invariante**: `catalog::EnsureActivePetOwned()` garantiza que el
  pet que está en el escritorio siempre sea propio (brief §9).
- **Frontera para el Shop (Block 07)**: cuando exista una compra, solo
  tiene que agregar un `petId` a `ownedPetIds` y descontar
  `clickBalance` — el modelo de Collection (`kActive`/`kOwnedInactive`/
  `kLocked`) NO necesita rediseño (brief §9). Un bloque futuro de
  onboarding (Block 09) reemplaza la siembra por la elección real de
  starter escribiendo `ownedPetIds` + `ownershipSeeded = true` con su
  propia lógica, sin tocar el catálogo.

`catalog::CollectionModel` (puro) deriva la vista: agrupa las filas del
catálogo por `petId` lógico (las dos entradas de Frin colapsan a una
con dos variantes), y calcula el estado de cada una.

## 6. La Collection (estructura visual)

Tamaño de contenido objetivo: ~760 × 540 pt, redimensionable.

```
Nimvlets                                          1 248 clicks   <- cabecera discreta

Collection                                                        <- etiqueta de sección

     [art]              [art]              [art]                   <- grid: arte flotando,
     Bunny              Nidir              Frin                        sin cards fuertes
     On desktop         Not in your        Male · Female
                        collection
  ---------------------------------------------------------------  <- hairline (solo con detalle)
     [art grande]   Frin
                    [ Male ] [ Female ]                            <- panel de detalle expandido
                    [ Use Frin ]                                      (composición, no modal)
```

Principios visuales aplicados (brief §2/§3/§8/§22):

- fondo blanco hueso cálido (`#F6F3EE`), texto casi-negro (`#26221E`),
  no negro puro;
- **el arte del Nimvlet domina cada entrada** — flota sobre una caja
  de fondo apenas insinuada (`kArtBed`), nunca una card con borde
  fuerte;
- texto de estado **humano** ("On desktop" / "Use" / "Not in your
  collection"), nunca badges "ACTIVE"/"LOCKED";
- un único acento (terracota `#B46E3C`): el anillo de foco de teclado,
  la variante seleccionada, y el hint "Use" bajo un pet poseído-
  inactivo;
- animación de UI mínima: emphasis de hover instantáneo (un wash
  sutil), sin fades decorativos, sin transiciones de página, sin
  gradientes animados. El arte del pet aporta la vida;
- sin sidebar permanente, sin toolbar gigante, sin cabecera
  sobredimensionada, sin dashboard, sin "Welcome back", sin
  estadísticas.

### Estados de propiedad en el grid

| Estado | Sub-línea bajo el arte | Arte |
|---|---|---|
| poseído + activo | `On desktop` | sí |
| poseído + inactivo, sin variantes | `Use` (en acento) | sí |
| poseído + inactivo, con variantes (Frin) | `Male · Female` | sí |
| bloqueado | `Not in your collection` (tenue) | no — solo una caja muy tenue mantiene el ritmo de columnas (brief §8/§9) |

### Panel de detalle

Se abre al hacer click (o Enter) sobre una entrada. Composición
expandida DEBAJO del grid, no un modal (brief §10). Arte grande a la
izquierda (salvo bloqueado: sin arte), nombre, y:

| Estado del pet en detalle | Botón de acción |
|---|---|
| activo, sin variantes | `On desktop` — deshabilitado (contorno tenue) |
| activo, con variantes, variante mostrada == la activa | `On desktop` — deshabilitado |
| activo, con variantes, variante mostrada != la activa | `Use <name>` — **habilitado** (re-activa con la otra variante) |
| poseído-inactivo | `Use <name>` — habilitado |
| bloqueado | `Not in your collection` — sin acción, **sin botón de compra** (brief §9) |

Cerrar el detalle: `Esc`, o click en el vacío. `Esc` sin detalle
abierto cierra la ventana.

## 7. Frin: presentación de variantes

Frin es **UN Nimvlet lógico** con dos variantes visuales (macho,
hembra) — nunca dos entradas no relacionadas en la Collection (brief
§11). El modelo colapsa `{"frin","male"}` y `{"frin","female"}` en un
`CollectionItem` con `variants = [male, female]`.

- **Grid**: una fila "Frin", sub-línea `Male · Female`.
- **Detalle**: chips `Male` / `Female`; el seleccionado tiene relleno
  suave + contorno terracota. Cambiar de chip actualiza la preview y
  la etiqueta del botón. `Use Frin` activa `{petId:"frin",
  variantId:<chip seleccionado>}`.
- **Persistencia**: la variante ACTIVA persiste en
  `AppState::activeVariantId` vía `TrySwitchActivePet()` — al reabrir la
  app, Frin vuelve con la variante que estaba en el escritorio. La
  variante "hovering" en un detalle sin activar es efímera (no se
  persiste).

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
| Large  | 1.30 | un tercio más grande, todavía una ventana chica |

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

### Estructura del menú (block brief §14)

```
Bunny                    <- header, deshabilitado (nombre del pet activo)
--------
Hide Nimvlet             <- o "Show Nimvlet" según el estado
Collection…
--------
Size      ▸  Small · Medium · Large      (checkable, exactamente uno)
Opacity   ▸  100% · 85% · 70% · 55%      (checkable)
Lock Position                            (checkable)
--------
Quit Nimvlets
```

El `NSMenu` real se construye a partir de `platform::
BuildQuickMenuModel(ShellState)` — un modelo PURO que
`tests/QuickMenuModelTest.cpp` cubre etiqueta por etiqueta, así que el
test verifica exactamente la estructura que se envía.

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
- **Costo transitorio al abrir la Collection**: se carga el pack del
  pet poseído-inactivo (Frin, ~72 MB) para extraer su frame de preview;
  el `PetDefinition` se descarta de inmediato, solo queda una textura
  chica. Pagado una sola vez mientras la Collection está abierta. Con
  muchos pets poseídos esto escalaría — un bloque futuro usaría un
  thumbnail precompilado o un loader en background; se registra como
  limitación.

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

Block 06 **no introduce ningún permiso nuevo** (brief §20). Ver
`docs/PRIVACY_SECURITY.md`. En concreto: un `NSStatusItem` y un `NSMenu`
son UI de nuestra propia app; Core Text dibuja en memoria propia;
`activateIgnoringOtherApps:` no requiere TCC. Sin cuenta, sin telemetría,
sin red, sin captura de pantalla en el producto, sin hooks de input
globales, sin conteo global de clicks.

Las capturas de pantalla de QA de este bloque son diagnóstico de
DESARROLLO de nuestra propia ventana (AGENTS.md §5) — nunca se
comitean, nunca son comportamiento del producto.

## 14. Intencionalmente diferido

| Feature | Bloque |
|---|---|
| Shop: comprar Nimvlets, precios, gastar clicks | Block 07 |
| Página de Settings avanzada | Block 08 |
| Onboarding / selección de starter / secreto de 44 s / shop oculto de starters | Block 09 |
| Bandeja de Windows / equivalente de Linux para el System Shell | futuro, hardware mediante |
| Texto de producto en Windows/Linux (DirectWrite / fontconfig) | futuro |
| Conteo global de clicks, permiso de input global | futuro, opt-in explícito (AGENTS.md §14) |
| Preferencia de fullscreen, launch-at-login, dark mode | futuro |
| Corrección visual de `lie_to_sit` de Frin | deuda conocida, congelada (brief §21) |

Nada de lo anterior está implementado ni insinuado en el código de
Block 06.
