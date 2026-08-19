# Nimvlets — Nidir: assets reales + pipeline direccional (Block 04.2)

Nidir es el primer Nimvlet con arte real de producción integrado al
runtime (a diferencia de Bunny, que sigue siendo un fixture de QA —
ver AGENTS.md §11 y `docs/DECISION_LOG.md` DEC-018). Este documento
describe la convención de asset source que este bloque establece, el
pipeline de importación/normalización/espejado, la extensión
direccional del content model, la semántica real de animación (pose
base estática + idle esporádico + click, corregida en la segunda
pasada de este bloque), la política genérica de tamaño de canvas
lógico, y cómo un futuro Nimvlet sigue el mismo patrón. Ver
`docs/ANIMATION_RUNTIME.md` para el runtime en el que esto se enchufa
y `docs/CATALOG.md` para el catálogo en el que Nidir ahora es una
segunda entrada real.

**Nota de alcance de esta versión del documento:** este bloque tuvo
tres pasadas. La primera integró a Nidir con una semántica de
animación INCORRECTA (idle en loop continuo) y un canvas del tamaño
nativo del arte fuente (Nidir aparecía mucho más grande que Bunny en
pantalla). La segunda corrige ambas cosas y agrega el intento de
importar la animación real de click-fire — bloqueado por falta de
acceso a `~/Downloads` en esa sesión. La tercera pasada, tras
resolverse el bloqueo de acceso (ver §10), importa el click-fire real
("blue-fire") y reemplaza el placeholder estructural que las dos
pasadas anteriores mantuvieron, además de encontrar y corregir un bug
real de cobertura de texturas descubierto al ejercitar ese contenido
(ver §6). Este documento describe el estado FINAL (tercera pasada);
`docs/DECISION_LOG.md` conserva el registro histórico de las tres
pasadas con sus propias entradas.

## 1. Convención de asset source (permanente, para todo Nimvlet futuro)

Regla de fuente de verdad, establecida por este bloque como permanente:

- **Los frames PNG individuales, transparentes, son la fuente
  canónica de la animación.** Todo lo demás se deriva de ellos.
- **El spritesheet es un artefacto exportado/de referencia
  secundario** — útil para inspección visual rápida o para otras
  herramientas, pero nunca lo que `tools/compile_pet_pack.py` lee. Si
  el spritesheet y los frames individuales alguna vez difirieran, los
  frames individuales ganan.
- **El pack de runtime compilado (`.nvpack`) es un artefacto
  generado** — nunca se edita a mano, siempre se regenera desde la
  fuente.

Estructura de directorio (ver `assets/source/nimvlets/nidir/` como el
primer ejemplo real; `assets/source/nimvlets/README.md` mantiene la
versión general/target de este layout para Nimvlets futuros):

```
assets/source/nimvlets/<pet>/
  DESCRIPTION.txt              -- rasgos físicos estables (§4)
  master.png                   -- imagen de referencia estática
  pack_manifest.json           -- entrada de tools/compile_pet_pack.py
  animations/
    <animación>/
      right/
        frames/frame_000.png .. frame_NNN.png   -- fuente canónica
        spritesheet/spritesheet.png              -- artefacto secundario
      left/
        frames/frame_000.png .. frame_NNN.png   -- generado por espejado (§3)
        spritesheet/spritesheet.png              -- generado, no importado
```

No se crean categorías de animación vacías por adelantado (Nidir solo
tiene `animations/idle/` en este bloque — sin `click/`/`passive/`
todavía, ver §6) — mismo principio de "no construir por adelantado"
que `assets/source/nimvlets/README.md` ya documentaba desde Block 02.

## 2. Import de los exports de Ludo.ai (§2 del brief)

El owner ya había exportado la primera animación real de Nidir
(idle-right) a `~/Downloads/nidir-idle-right/` (25 PNG individuales,
`frame_000.png`..`frame_024.png` — el propio export de Ludo.ai ya
seguía la convención de nombrado determinista que este bloque pide,
sin necesitar renombrado) y
`~/Downloads/nidir-idle-right-spreadsheet/` (un único spritesheet
PNG). Un tercer archivo, `~/Downloads/Nidir.png`, se identificó como
la imagen de referencia estática — ver §7, "decisiones fuera del
prompt".

**Verificado antes de copiar** (nunca asumido): `sips` confirmó las 25
PNG a 513×525, RGBA (`hasAlpha: yes`), y el spritesheet a
2565×2625 — exactamente 5×513 por 5×525, confirmando una grilla 5
columnas × 5 filas, row-major. `~/Downloads` se copió, nunca se movió
ni se modificó — verificado comparando timestamps antes/después.

## 3. Normalización y espejado determinista

`tools/validate_frame_sequence.py` (nuevo, reusable para cualquier
Nimvlet futuro) valida: dimensiones consistentes, orden sin huecos ni
duplicados, PNG 8-bit RGBA real, y alpha no degenerado (ningún frame
100% opaco ni 100% transparente). Reporte real sobre los 25 frames
importados de Nidir:

```
25 frames, 513x525
fracción de pixeles transparentes: 0.500 .. 0.511
borde inferior del bbox opaco (alpha>=128): fila 506 .. 522
```

**Ninguna transformación de normalización fue necesaria** — el export
ya cumplía el contrato tal cual (dimensiones consistentes, alpha real,
nombrado ya determinista). Por eso este bloque **no** tiene una
carpeta "raw" separada de la "normalizada": son la misma, y esa
decisión está documentada acá en vez de crear una duplicación que
nada requiere (block brief §4: "if normalization is required,
preserve..." — condicional; no aplicó).

**Espejado (`tools/generate_nidir_pack.py`, nuevo):**
`prep_dev_sprite.mirror_rgba_horizontal()` (nueva función pura en el
codec PNG compartido) invierte el orden de columnas de cada fila,
moviendo los 4 canales RGBA juntos — el canal alpha nunca se recalcula
ni se aproxima, se preserva exactamente. Nunca se usó IA para generar
el lado izquierdo (block brief §3). El spritesheet "left" se
**ensambla desde los frames "left" ya espejados**, en la misma grilla
5×5 que el spritesheet "right" — nunca se espeja la imagen del
spritesheet completo de una sola vez, porque eso invertiría también el
ORDEN de las celdas en la grilla, no solo el contenido de cada una.
Ambos frames sets (25 right + 25 left) se re-validaron con
`validate_frame_sequence.py` después de generarse — mismo reporte
exacto que el de arriba, confirmando que el espejado no altera
dimensiones ni estadísticas de alpha.

`tools/test_asset_pipeline.py` (nuevo, `unittest` de stdlib, sin
dependencias — corrido a mano, no vía `ctest`, ver su propio
docstring) cubre espejado (reversión de columnas exacta, doble
espejado == identidad, preservación exacta de alpha) y validación de
secuencias (huecos, duplicados, dimensiones distintas, alpha
degenerado) con fixtures RGBA8 sintéticos pequeños — nunca los assets
reales de Nidir.

## 4. `DESCRIPTION.txt`

Un archivo de texto en español, por pet, en la raíz de su carpeta de
source — registra únicamente rasgos físicos ESTABLES que deben
preservarse en cualquier generación/edición futura (color de ojos,
paleta de escamas, forma de cuernos, proporciones, cantidad de colas,
etc.), nunca personalidad ni datos de producto (eso vive en
`docs/PET_CONTENT_SPEC.md`/`docs/PRD_V1.md`) ni procedencia/
herramientas (eso es el rol de `provenance.json`, ver
`assets/source/nimvlets/README.md` — no agregado para Nidir en este
bloque, ver §7). Ver `assets/source/nimvlets/nidir/DESCRIPTION.txt`
para el contenido real de Nidir: dragón negro, masculino, rasgos
faciales marcados, cuerpo robusto y compacto, escamas negro/carbón con
reflejos gris-azulados, ojos violeta/púrpura, **dos colas** (dato fijo
de diseño aunque solo una sea visible en la pose de referencia actual
— documentado explícitamente como tal, no inventado ni omitido),
alas, cuernos/púas, expresión confiada.

## 5. Modelo de dirección en runtime

**Extensión aditiva y retrocompatible del formato "NVPACK1"**
(`src/content/PetPackLoader.cpp`, `tools/compile_pet_pack.py`): tres
secciones finales, opcionales, en este orden fijo — overrides de
`idle`, de `clickReaction`, y de `passiveActions[]`:

```
[... exactamente el layout NVPACK1 existente, sin cambios ...]

-- tres secciones nuevas, SOLO presentes si el manifest pidió alguna --
directionalIdleOverrideCount : uint32
directionalIdleOverrides[count]: { direction: uint8, animation: AnimationBlock }

directionalClickReactionOverrideCount : uint32
directionalClickReactionOverrides[count]: { direction: uint8, animation: AnimationBlock }

directionalPassiveActionOverrideCount : uint32
directionalPassiveActionOverrides[count]: { passiveActionIndex: uint32, direction: uint8, animation: AnimationBlock }
```

Un pack compilado antes de este bloque (el `bunny_pack.nvpack` ya
comiteado, nunca recompilado para esto) simplemente termina justo
después de `passiveActions` — sin ningún byte extra. El loader
(`ByteReader::HasMoreData()`) chequea, EN SECUENCIA, si queda algo por
leer antes de cada una de las tres secciones — así que un pack solo
necesita tener tantos bytes de más como secciones realmente use.
`tools/compile_pet_pack.py` escribe las tres juntas (la ausente como
sección vacía explícita) en cuanto el manifest menciona *cualquiera*
de `idle_direction_overrides`/`click_reaction_direction_overrides`/
`passive_action_direction_overrides` — necesario para que ambos lados
(compilador y loader) coincidan posicionalmente sin ambigüedad; ver el
docstring de `tools/compile_pet_pack.py` para el detalle exacto. El
manifest de Bunny no menciona ninguna de las tres, así que su pack
compilado sigue siendo byte-a-byte idéntico al de antes de este
bloque.

**Modelo C++** (`src/content/AnimationDefinition.h`):

```
enum class Direction { kRight = 0, kLeft = 1 };

struct DirectionalAnimationOverride {
    Direction direction;
    AnimationDefinition animation;
};

struct PassiveActionDirectionalOverride {
    std::size_t passiveActionIndex;
    Direction direction;
    AnimationDefinition animation;
};

struct PetDefinition {
    ...
    AnimationDefinition idle;                                          // sin cambios de significado
    std::vector<DirectionalAnimationOverride> idleDirectionOverrides;  // aditivo

    AnimationDefinition clickReaction;                                            // sin cambios de significado
    std::vector<DirectionalAnimationOverride> clickReactionDirectionOverrides;    // aditivo

    std::vector<AnimationDefinition> passiveActions;                              // sin cambios de significado
    std::vector<PassiveActionDirectionalOverride> passiveActionDirectionOverrides; // aditivo
    ...
};

const AnimationDefinition& ResolveIdleAnimation(const PetDefinition& pet, Direction direction);
const AnimationDefinition& ResolveClickReaction(const PetDefinition& pet, Direction direction);
const AnimationDefinition& ResolvePassiveAction(const PetDefinition& pet, std::size_t passiveActionIndex, Direction direction);
```

`pet.idle`/`pet.clickReaction`/`pet.passiveActions` conservan
exactamente su significado anterior (la variante canónica — por
convención, `Direction::kRight`). Las tres funciones `Resolve*()` son
la única forma soportada de resolver dirección: cada una retorna la
entrada dedicada de la lista de overrides correspondiente si existe
una para `direction`, si no cae a la canónica — política de fallback
documentada explícitamente (block brief §6: "unsupported direction
fails clearly or uses an explicitly documented safe fallback"), nunca
fallan. Este diseño aditivo (en vez de reemplazar los campos
canónicos por una lista genérica) fue deliberado para minimizar el
blast radius: `AnimationController`, `SpikeApp`, y todos los tests/
fixtures existentes que ya referenciaban `pet.idle`/`pet.clickReaction`/
`pet.passiveActions` directamente siguieron compilando y pasando sin
ningún cambio en la primera pasada; la segunda pasada extendió el
mismo patrón a `clickReaction`/`passiveActions` cuando el arte
direccional real de click-fire lo requirió (ver §9).

`content::AnimationController` gana `direction_` (default
`Direction::kRight`, igual que todo el runtime) y `SetDirection(direction,
nowMs)`: si el controller está Idle, el frame mostrado se actualiza de
inmediato (frame 0 de la nueva variante); si no (ClickReaction/
PassiveAction en curso), el cambio queda guardado y se aplica recién
cuando `TransitionToIdle()` corra por su cuenta — dirección es
metadata, nunca interrumpe un gesto en curso. `TriggerClick()`/
`TriggerPassiveAction()` ahora resuelven vía `ResolveClickReaction()`/
`ResolvePassiveAction()` en vez de leer `pet_.clickReaction`/
`pet_.passiveActions[i]` directamente, así que un click o una acción
pasiva disparados mientras `direction_ == kLeft` automáticamente
reproducen la variante izquierda si existe una.
`SpikeApp::SetActiveDirection()` es el "runtime method to change
direction" que pide el block brief §7 — sin ningún control de UI que
lo dispare todavía (explícitamente fuera de alcance).
`NIMVLETS_DEV_DIRECTION_TEST_COUNT` (mecanismo solo-DEV, mismo patrón
que `NIMVLETS_DEV_SWITCH_TEST_COUNT`) permite smoke-testear cambios de
dirección repetidos contra el binario real sin QA manual.

**Sin persistencia de dirección** — decisión explícita, ver §7 del
brief ("not required unless essentially free and clearly justified")
y el comentario de `appState_` en `src/app/SpikeApp.h`: agregar un
campo al formato NVSTATE1 exigiría un bump de schema por un beneficio
que nadie pidió. Cada `Init()`/`TrySwitchActivePet()` arranca en
`Direction::kRight`.

## 5.1 Semántica real de animación (corregida en la segunda pasada)

**La primera pasada de este bloque integró a Nidir con una semántica
INCORRECTA:** su `idle` se clasificó como `PlaybackKind::kLoop`
reproduciendo los 25 frames importados en loop continuo, sin nunca
volver a un reposo estático — el resultado real medido fue ~11-12% de
CPU en steady state (o ~4-5% tras un primer intento de bajar el fps),
nunca 0%. Esto contradecía tanto el comportamiento ya establecido para
Bunny (idle estático, evento-driven, 0% CPU en reposo) como el
producto real que el owner pidió.

**Semántica corregida y final:**

- **`idle` es la pose base, `PlaybackKind::kStatic`, un solo frame**
  (`frame_000` de la secuencia importada, para cada dirección) — lo
  que se muestra la gran mayoría del tiempo. `AnimationController`
  nunca calcula ningún deadline de frame mientras está acá
  (`NextFrameDeadlineMs()` retorna `std::nullopt`) — el mismo mecanismo
  event-driven que Bunny ya demuestra desde Block 02, sin ningún
  cambio de arquitectura.
- **La secuencia completa importada es un `passive_action`, `PlaybackKind::kOneShot`**
  — se dispara esporádicamente a través del scheduler de acciones
  pasivas que ya existe desde Block 02
  (`SpikeApp::nextPassiveDeadlineMs_`/
  `AnimationController::TriggerPassiveAction()`), reproduce sus frames
  una vez, y al terminar (`returnsToIdle: true`) vuelve a la pose base
  estática — exactamente el mismo mecanismo que ya usa cualquier
  `click_reaction` de un solo tiro. **Nunca vuelve a arrancar sola** —
  cada disparo es un evento discreto, no un loop.
- **`click_reaction` también es `PlaybackKind::kOneShot`** y ya lo era
  correctamente desde la primera pasada (el bug estaba en `idle`, no en
  el click) — reproduce una vez y vuelve a la pose base estática.
- **Un click SÍ interrumpe una acción pasiva en curso** (mecanismo
  genérico ya existente desde Block 02, sin cambios — ver
  `tests/AnimationControllerTest.cpp`'s
  `ClickReactionInterruptsPassiveAction`, y
  `tests/DirectionTest.cpp`'s
  `ClickInterruptsPeriodicIdleAndReturnsToStaticBaseAfterward` para la
  variante direccional de Nidir): click reaction tiene prioridad más
  alta que acción pasiva, y ambos vuelven a la misma pose base al
  terminar.
- **Nunca hay ningún loop de render/FPS permanente** mientras Nidir
  está inactivo — el requisito explícito de esta corrección.

La cadencia de reproducción del `passive_action` (fps) se deriva de la
configuración REAL de Ludo.ai para este export ("3 seconds, Max
Frames 25" — dato provisto por el owner, no medido ni supuesto por
este repo): `fps = frame_count_real / 3.0`. Como la animación ahora es
esporádica (no un loop continuo), esta cadencia ya no necesita
balancearse contra un costo de CPU permanente — solo importa mientras
el one-shot está efectivamente en pantalla, unas pocas veces por hora
como mucho (el intervalo real entre disparos es política de producto
todavía no decidida — se dejó el mismo default de 300s/5min que el
esquema ya usaba, ver `tools/generate_nidir_pack.py`).

## 6. Nidir como entrada real de catálogo

`assets/dev/pet_catalog_manifest.json` gana una segunda entrada real
(`nidir`, `assets/dev/nidir_pack.nvpack`), **`is_default: false`** —
Bunny sigue siendo el default (block brief: "Do not make Nidir default
unless current contracts require it" — nada lo exige). Recompilado vía
`tools/compile_pet_catalog.py` → `assets/dev/pet_catalog.nvcat` (2
entradas). El catálogo/loader/resolución de arranque/switching en
runtime (`docs/CATALOG.md`) no necesitaron ningún cambio de código —
agregar Nidir fue puramente agregar una fila de manifest + recompilar,
exactamente la promesa que `docs/CATALOG.md` §4 ya hacía.

`click_reaction` de Nidir es, desde la tercera pasada de este bloque,
la animación real de "blue-fire" que el owner exportó
(`nidir-click-fire-right`/`-spritesheet`, staging temporal en
`local_imports/nidir/` — nunca commiteado, ya eliminado tras la
importación). El placeholder estructural de un solo frame de las dos
primeras pasadas fue reemplazado por completo, no conservado en
paralelo.

**Estructura real del export, inspeccionada antes de asumir nada**
(el brief advertía explícitamente: "the frames export may contain one
additional nested folder"): `nidir-click-fire-right/` sí traía esa
carpeta anidada extra (`Nidir-a-masculine-b/`, el nombre interno del
asset en la herramienta de origen) conteniendo los 25 PNG reales
(`frame_000.png`..`frame_024.png`, ya deterministas, sin necesidad de
renombrar); `nidir-click-fire-right-spritesheet/` traía un único
`Nidir-a-masculine-b.png` (3120×3060 = grilla 5×5 de frames de
624×612, confirma que los 25 frames y el spritesheet son
consistentes entre sí).

**Import, mismo pipeline que idle:** los 25 frames se copiaron (nunca
movidos) a
`assets/source/nimvlets/nidir/animations/click_reaction/right/frames/`
sin renombrar (ya cumplían la convención `frame_NNN.png`); el
spritesheet se copió tal cual a
`.../click_reaction/right/spritesheet/spritesheet.png` (referencia
secundaria, igual que el de idle); `tools/generate_nidir_pack.py`
deriva `left` por el mismo espejado horizontal determinista que ya
usaba para idle (ahora factorizado en una función reusada por ambas
animaciones — `_derive_left_direction()`), preservando alpha
exactamente y validado con `tools/validate_frame_sequence.py`
(dimensiones/orden/sin huecos/alpha no degenerada, igual contrato que
idle).

**Frame count real: 25** (nativo 624×612 — resolución distinta a la de
idle, 513×525, porque el efecto de fuego extiende el bounding box
visible más allá del personaje). No se forzó ningún número — es
exactamente lo que el export trajo, igual que idle. El fps de
reproducción se derivó de la misma duración de generación de Ludo.ai
que idle (3s), asumiendo que el export de click-fire usó la misma
configuración — una suposición explícita y documentada (el owner no
confirmó la duración de este export puntual por separado), no medida
de forma independiente: `25 / 3.0 ≈ 8.33 fps`. El canvas lógico del
pet (156×160, ver §7) NO cambia por esta importación — se mantiene
derivado únicamente de idle; los frames de click-fire, con un aspect
ratio nativo ligeramente distinto (624/612 ≈ 1.020 vs. 156/160 =
0.975), se estiran ~4.5% al renderizarse en el mismo canvas que
cualquier otro frame de Nidir (`SDL_RenderTexture` siempre estira el
texture completo al rect de destino exacto — comportamiento genérico
preexistente desde Block 02, no nuevo de esta importación, y
suficientemente sutil para un efecto de fuego que no amerita un canvas
dedicado solo para esta animación).

**Hallazgo honesto de primer/último frame** (mismo método que idle,
§6.1): comparando `frame_000` y `frame_024` del click-fire real,
~94.3% del área coincide (65.0% ambos transparentes + 29.3% visible y
esencialmente igual), ~5.2% muestra una diferencia visible real
(probablemente el fuego apagándose hacia el frame final), 0.5% es
jitter de borde antialiaseado. Documentado, no oculto — y, como con
idle, irrelevante para la garantía de "vuelve a la pose base": 
`AnimationController::TransitionToIdle()` siempre re-muestra la
verdadera pose base estática de Nidir al terminar, nunca el último
frame de `click_reaction` en sí.

Debe ser `one_shot`, nunca `static`: un `click_reaction` estático
dejaría al `AnimationController` trabado en `ClickReaction` para
siempre, ya que `Advance()` nunca transiciona de vuelta a Idle para
una animación `kStatic` (solo lo hace al terminar naturalmente un
one-shot).

**Bug real encontrado y corregido al importar este contenido:**
`SpikeApp::AttachAllTextures()`/`ReleaseAllTextures()` (agregadas en
Block 02, nunca actualizadas cuando la segunda pasada de este bloque
agregó `clickReactionDirectionOverrides`/`passiveActionDirectionOverrides`)
no cubrían esas dos colecciones — `AnimationController` resolvía el
override "left" correctamente, pero sus frames nunca tenían una
textura SDL adjunta, así que `RenderFrame()` los dibujaba
completamente transparentes: el pet "desaparecía" en silencio durante
cualquier click o idle periódico reproducido en dirección "left" (o
cualquier dirección no canónica). El hit-mask no se veía afectado
(usa `frame.pixels` directamente, no la textura), así que el
click-through seguía siendo correcto — solo el render era el
problema. Pasó desapercibido en las dos pasadas anteriores porque el
placeholder de click de un solo frame nunca se verificó visualmente
(los smoke tests solo revisaban logs, no pixeles) y el idle periódico
en dirección left tampoco. Se reprodujo deliberadamente (revirtiendo
temporalmente el fix, confirmando el log de advertencia nuevo — ver
abajo — y luego restaurando el fix) antes de darlo por corregido, no
solo se infirió de leer el código. Corregido agregando las dos
colecciones faltantes a ambas funciones (`src/app/SpikeApp.cpp`), y se
agregó un log defensivo permanente en `RenderFrame()` que reporta
cualquier frame con pixels reales pero sin textura adjunta — detecta
automáticamente cualquier regresión futura de esta misma clase de bug
sin depender de inspección visual manual. `src/content/AnimationDefinition.h`
gana un comentario explícito junto a `PetDefinition` advirtiendo que
cualquier colección de animaciones nueva debe actualizar esas dos
funciones.

Este bug no se pudo cubrir con un test unitario en `tests/` porque
`SpikeApp`/`AttachAllTextures` viven en el ejecutable `nimvlets_spike`
(SDL-dependiente), no en ninguna librería que `nimvlets_tests` enlace
— consistente con la convención ya establecida de este proyecto de
mantener `tests/` completamente libre de SDL (ver DEC-022). La
verificación real fue: reproducir el bug contra el binario compilado
real (confirmando el log de advertencia), corregirlo, y re-confirmar
(0 advertencias) — documentado acá en vez de fingir cobertura vía un
test que no puede existir con la arquitectura actual.

**Nota de organización, no resuelta en este bloque:** el pack
compilado de Nidir vive en `assets/dev/nidir_pack.nvpack`, la misma
carpeta que el fixture de QA de Bunny — aunque Nidir es contenido real
de producción, no un fixture. Se mantuvo así por continuidad con las
convenciones de ruta ya establecidas en `tools/`/`src/catalog` (cambiar
la carpeta habría sido una reorganización de alcance mayor, no pedida
por este bloque) — ver "Bugs/debt/limitations" del informe final.

## 6.1 First/last frame contract: hallazgo real

El brief pide validar que la secuencia de idle "empiece/termine
consistentemente" con la pose base, para una vuelta sin sobresaltos.
Medición real (pixel a pixel) entre `frame_000` (la pose base elegida)
y `frame_024` (el último frame realmente exportado) de idle-right:

```
50.63% ambos totalmente transparentes (fondo, coincide)
0.53%  cruzan el límite alpha=0 (jitter de antialiasing de borde, normal)
5.84%  ambos visibles, con diferencia REAL >10 por canal (~12% del área visible)
43.00% ambos visibles, esencialmente idénticos (≤10 por canal)
```

**Hallazgo:** `frame_024` NO es pixel-idéntico a `frame_000` — hay una
variación real y visible (~12% del área visible del personaje,
probablemente boca/expresión/cola) entre el primer y el último frame
de la secuencia exportada. Esto es normal para una animación de
"respiración" real (un ease-back, no un corte duro), y NO se corrigió
inventando/duplicando un frame de reemplazo (el brief lo prohíbe
explícitamente). El runtime maneja esto correctamente de todos modos:
`AnimationController::TransitionToIdle()` siempre vuelve a mostrar la
pose base VERDADERA (`pet.idle`/su override), nunca el último frame de
la animación que acaba de terminar — así que la vuelta a reposo
siempre es exacta, aunque pueda haber un "pop" visual perceptible de
un frame en el instante exacto de la transición (idle-breathing ->
base). Documentado acá como una limitación real y conocida, no oculta.

## 7. Política genérica de tamaño de canvas lógico vs. resolución de frame

**El problema real:** la primera pasada de este bloque fijó el canvas
de Nidir a su resolución nativa exacta (513×525) porque
`PetDefinition::canvasWidth/canvasHeight` YA gobierna tanto el tamaño
de renderizado (`SDL_RenderTexture`) como el del hit-mask
(`core::AlphaMask::FromAlphaChannel`) desde Block 02 — ambos ya
escalan desde CUALQUIER resolución nativa hacia el canvas, así que
"copiar la resolución nativa 1:1" pareció evitar una transformación
innecesaria. El costo real: como `SDL_CreateWindow` usa `canvasWidth`/
`canvasHeight` directamente como tamaño LÓGICO de la ventana (ver
`src/app/SpikeApp.cpp`), Nidir terminó ocupando una ventana ~3.2x más
grande que Bunny en cada eje — un problema visual real, no solo
estético (tensiona con el invariante de producto "ventana pequeña",
AGENTS.md §2).

**La corrección NO necesitó ningún cambio de runtime** — el mecanismo
para desacoplar "tamaño en pantalla" de "resolución del arte fuente"
ya existía desde Block 02; el bug era el VALOR elegido, no la
arquitectura. Dos políticas GENÉRICAS nuevas, ambas en
`tools/prep_dev_sprite.py` (reusables por cualquier Nimvlet futuro, sin
ninguna rama por pet):

- **`compute_logical_canvas_size(native_width, native_height, reference_size=160)`**
  — deriva el canvas LÓGICO (puntos en pantalla) del aspect ratio
  nativo del arte fuente: el lado más largo queda en `reference_size`
  (160 — el mismo valor que Bunny ya usa desde Block 02, hecho
  explícito como convención en vez de quedar implícito en un solo
  script), el otro se escala proporcionalmente. Para Nidir (513×525
  nativo): **canvas lógico = 156×160** — comparable en escala a Bunny,
  aspect ratio preservado casi exacto (513/525 = 0.9771 vs.
  156/160 = 0.9750, una desviación de redondeo de ~0.2%, imperceptible).
- **`resize_rgba_nearest(width, height, pixels, target_width, target_height)`**
  — reescalado determinista nearest-neighbor (misma fórmula de mapeo
  que `core::AlphaMask::FromAlphaChannel` y el `resize_nearest()` de
  `tools/generate_bunny_dev_pack.py` ya usan), reusado por
  `tools/compile_pet_pack.py`'s downscale opcional en tiempo de
  compilación (`runtime_max_frame_dimension`, ver §8) — **nunca toca
  los PNG fuente en disco**, solo los bytes que terminan en el pack
  compilado.

**Ningún cambio de C++/runtime fue necesario:** el hit-mask sigue
alineado con el renderizado exactamente igual que antes (ambos siguen
leyendo `pet.canvasWidth/canvasHeight`, sin importar la resolución
nativa de cada frame — ya probado por
`tests/DirectionTest.cpp`'s `HitMaskDimensionsStayConsistentAcrossDirectionSwitch`,
que deliberadamente usa frames de DISTINTA resolución nativa entre
right/left para confirmar esto); el comportamiento de high-DPI
(`SDL_WINDOW_HIGH_PIXEL_DENSITY` + `SDL_SetRenderLogicalPresentation`)
tampoco cambió, ya que sigue operando sobre el mismo canvas LÓGICO de
siempre; el switching de dirección sigue siendo el mismo mecanismo de
`ResolveIdleAnimation()`/etc., completamente ortogonal al tamaño de
canvas; Bunny no se tocó (su manifest no define
`runtime_max_frame_dimension` y su `canvas_width`/`canvas_height` ya
eran 160×160 antes de esta política existir).

## 8. Downscale opcional en tiempo de compilación (RSS)

`runtime_max_frame_dimension` (manifest, opcional — ver
`tools/compile_pet_pack.py`): si se define, cada frame decodificado
cuyo lado más largo exceda ese valor se reescala (nearest-neighbor,
aspect ratio preservado, determinista) ANTES de escribirse al pack
compilado — el PNG fuente en disco nunca se toca. Para Nidir:
`runtime_max_frame_dimension = 2 × 160 = 320` — el doble del tamaño de
referencia lógico, elegido para verse nítido hasta 2x densidad de
pixel (Retina, el mismo factor que este proyecto ya mide/loguea desde
Block 01 — "pixel density=2.00") sin almacenar mucho más detalle del
que cualquier pantalla real puede mostrar. Resultado: los frames de
513×525 se comprimen a 313×320 (~37% de los pixeles originales) — el
pack compilado de Nidir bajó de ~58 MB a **~21.6 MB**, y el RSS medido
en runtime de ~259 MB a **~128 MB** (ver `docs/PERFORMANCE_BUDGETS.md`
para la medición completa por escenario). "Optimizar solo donde la
evidencia lo justifica": esta reducción es real y medida, no una
suposición — el canvas lógico de 156×160 puntos, incluso a 2x Retina,
nunca necesita más de ~312×320 pixeles físicos reales en pantalla, así
que 513×525 era objetivamente más resolución de la que cualquier
renderizado real de Nidir puede llegar a mostrar.

## 9. Decisiones tomadas fuera del prompt

- **`~/Downloads/Nidir.png` se usó como `master.png`.** No estaba
  entre los dos folders que el brief original nombra explícitamente,
  pero coincide en nombre/ubicación/rol con lo que
  `assets/source/nimvlets/README.md` ya definía como `master.png` —
  inferencia con evidencia suficiente, no una adivinanza.
- **fps del `passive_action` de idle: derivado de `frame_count / 3.0`**
  (la configuración real de Ludo.ai, dato provisto por el owner en la
  segunda pasada — "3 seconds, Max Frames 25"), NO una medición de
  costo de CPU como en el intento fallido de la primera pasada (esa
  medición dejó de ser relevante en cuanto `idle` dejó de ser un loop
  continuo — ver §5.1).
- **Canvas lógico de Nidir corregido a 156×160** (política genérica,
  §7) — corrige el problema visual real reportado ("Nidir currently
  appears much larger on screen than Bunny").
- **Downscale opcional en tiempo de compilación a 320px máximo por
  lado** (§8) — reduce RSS de forma real y medida, sin tocar ningún
  PNG fuente.
- **`click_reaction` real importado en la tercera pasada** (§6) —
  reemplaza por completo el placeholder de las dos pasadas anteriores,
  una vez resuelto el bloqueo de acceso a `~/Downloads` (§10).
- **`ResolveClickReaction()`/`ResolvePassiveAction()` nuevos** — la
  extensión direccional de la primera pasada solo cubría `idle`; la
  segunda pasada la generalizó a `clickReaction`/`passiveActions`
  siguiendo el mismo patrón aditivo exacto, necesario para poder
  resolver right/left del click-fire real, que se terminó de importar
  en la tercera pasada.
- **`SpikeApp::AttachAllTextures()`/`ReleaseAllTextures()` corregidas**
  (§6) — no cubrían `clickReactionDirectionOverrides`/
  `passiveActionDirectionOverrides`, un bug real de cobertura de
  texturas descubierto al ejercitar el click-fire real en dirección
  "left" por primera vez.
- **Residencia dual de dirección NO optimizada, a propósito** — ver
  "Bugs/debt/limitations" del informe final: `AttachAllTextures()`
  mantiene ambas direcciones (right y left) de TODAS las animaciones
  residentes en memoria de forma permanente mientras el pet está
  activo, aunque solo una dirección se muestra a la vez. Es una
  oportunidad de optimización real e identificada (ahorraría del
  orden de 15MB de RSS para Nidir), pero implementar carga/descarga de
  texturas por dirección bajo demanda es un cambio de arquitectura más
  grande y con más riesgo (manejar una animación en reproducción
  cuando cambia la dirección activa, etc.) que lo proporcional a esta
  pasada de corrección puntual — se documenta como hallazgo real,
  deliberadamente no implementado, no como una omisión no examinada.
- **`NIMVLETS_DEV_CLICK_TEST_COUNT` nuevo** — mecanismo solo-DEV para
  disparar clicks sintéticos sin necesitar un evento de mouse real,
  mismo patrón que `NIMVLETS_DEV_SWITCH_TEST_COUNT`/
  `NIMVLETS_DEV_DIRECTION_TEST_COUNT` — necesario para poder medir/
  smoke-testear el click reaction de forma no interactiva.
- **Dirección no persistida** — ver §5.
- **`assets/dev/nidir_pack.nvpack`** en vez de una carpeta nueva — el
  pack compilado de Nidir sigue viviendo junto al fixture de QA de
  Bunny por continuidad con las convenciones de ruta ya establecidas.
- **Sin `provenance.json` para Nidir** — el brief pide `DESCRIPTION.txt`
  específicamente (rasgos físicos), no procedencia.

## 10. Blocker de acceso a `~/Downloads`/`~/Documents` — histórico, RESUELTO

Durante la segunda pasada de este bloque, el owner exportó la
animación real de click-fire de Nidir a `~/Downloads/nidir-click-fire-right`/
`~/Downloads/nidir-click-fire-right-spritesheet`. **El acceso a
`~/Downloads` estuvo denegado durante toda esa sesión**
("Operation not permitted" — `ls`, `find`, y `osascript`/Finder vía
Apple Events fallaron todos de la misma forma, consistentemente, en
más de 6 intentos separados a lo largo de dos turnos de esa
conversación; `~/Documents` y el resto del filesystem del usuario sí
eran accesibles con normalidad en ese momento). No se pudo inspeccionar
ni copiar ninguno de los dos folders — se documentó el bloqueo
explícitamente en vez de inventar o asumir contenido.

**Al retomarse esta pasada (tercera), el bloqueo había empeorado
temporalmente antes de resolverse:** el acceso vía Bash (la herramienta
usada para `git`/`cmake`/`ctest`/scripts de Python) se había perdido
para `~/Documents` COMPLETO (no solo `~/Downloads`) — listar el propio
directorio del repositorio, `git status`, y cualquier comando que
necesitara leer el árbol de trabajo fallaban con el mismo
"Operation not permitted", mientras que lecturas/escrituras a rutas
YA CONOCIDAS (no un listado de directorio) seguían funcionando. El
patrón es consistente con la protección de privacidad de macOS (TCC)
para la categoría "Documentos" — la misma categoría que ya afectaba a
Downloads —, aplicada esta vez al proceso detrás de la herramienta de
shell de esta sesión específicamente (las herramientas de
lectura/escritura de archivo directo NO se vieron afectadas). El owner
otorgó **Acceso Total al Disco** ("Full Disk Access") a la app Claude
en Ajustes del Sistema → Privacidad y Seguridad, y reinició la app —
tras eso, tanto el listado de directorios como `git status` volvieron
a funcionar con normalidad de inmediato, sin ningún cambio de este
repositorio.

**Una vez resuelto:** se inspeccionó la estructura real de
`local_imports/nidir/` (staging temporal que el owner preparó como
alternativa a `~/Downloads` para esta pasada — nunca commiteado, ver
`.git/info/exclude`, no modificado por este bloque) y se confirmó
exactamente la advertencia del brief original: el export de frames
traía una carpeta anidada adicional
(`nidir-click-fire-right/Nidir-a-masculine-b/`) antes de los PNG
reales — inspeccionado, no asumido. El import completo se describe en
§6. `local_imports/nidir/` se eliminó tras copiar y verificar (checksum
MD5) los 25 frames + el spritesheet en su ubicación canónica dentro de
`assets/source/nimvlets/nidir/` — el staging ya no es necesario y
nunca debía commitearse.

## 11. Cómo un Nimvlet futuro sigue el mismo patrón

1. Crear `assets/source/nimvlets/<pet>/` con `DESCRIPTION.txt` (rasgos
   físicos estables, en español) y `master.png`.
2. Importar (copiar, nunca mover) los exports reales a
   `animations/<animación>/right/{frames,spritesheet}/` — frames
   individuales como fuente canónica, spritesheet como referencia
   secundaria.
3. Correr `tools/validate_frame_sequence.py` sobre los frames
   importados — confirma el contrato de normalización antes de seguir.
4. Si el pet necesita variantes izquierda/derecha: escribir un script
   `generate_<pet>_pack.py` análogo a `tools/generate_nidir_pack.py`
   que reuse `prep_dev_sprite.mirror_rgba_horizontal()` para derivar
   `left/` de forma determinista (nunca con IA), ensamble su
   spritesheet desde los frames ya espejados, y compile el pack final
   con `tools/compile_pet_pack.py` (usando `"idle_direction_overrides"`
   en el manifest si aplica).
5. Agregar una entrada a `assets/dev/pet_catalog_manifest.json` y
   recompilar con `tools/compile_pet_catalog.py` — cero cambios en
   `src/catalog`/`src/app`.
6. El canvas lógico se deriva, no se copia de la resolución nativa —
   usar `prep_dev_sprite.compute_logical_canvas_size()` (§7); si el
   pack crece de tamaño (más animaciones/direcciones), considerar
   `runtime_max_frame_dimension` en el manifest (§8) para acotar RSS,
   siempre sin tocar los PNG fuente.
7. **Si el pet agrega una animación/override direccional NUEVO**
   (algo más allá de `idle`/`clickReaction`/`passiveActions` y sus
   respectivos overrides ya existentes): confirmar que
   `SpikeApp::AttachAllTextures()`/`ReleaseAllTextures()` cubran la
   colección nueva (ver el comentario junto a `PetDefinition` en
   `src/content/AnimationDefinition.h`) — omitir esto no rompe la
   resolución de contenido (`AnimationController` sigue funcionando
   bien), pero hace que esa animación se renderice completamente
   transparente en silencio. Ver §6 para el caso real que motivó este
   punto.
