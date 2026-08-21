# Nimvlets — Nidir: assets reales + pipeline direccional (Block 04.2 + 04.3)

Nidir es el primer Nimvlet con arte real de producción integrado al
runtime (a diferencia de Bunny, que sigue siendo un fixture de QA —
ver AGENTS.md §11 y `docs/DECISION_LOG.md` DEC-018). Este documento
describe la convención de asset source que Block 04.2 establece, el
pipeline de importación/normalización/espejado, la extensión
direccional del content model, la semántica real de animación (pose
base estática + idle esporádico + click), la política genérica de
tamaño de canvas lógico y de encuadre por contenido (corregida en
Block 04.3 — ver §12), la dirección automática por mitad de pantalla
(Block 04.3 — ver §13), y cómo un futuro Nimvlet sigue el mismo
patrón. Ver `docs/ANIMATION_RUNTIME.md` para el runtime en el que esto
se enchufa y `docs/CATALOG.md` para el catálogo en el que Nidir ahora
es una segunda entrada real.

**Nota de alcance de esta versión del documento:** Block 04.2 tuvo tres
pasadas (semántica de animación, política de tamaño de canvas, e
import del click-fire real — ver más abajo) y todavía no se mergeó a
main. Block 04.3 es un bloque CORRECTIVO separado, abierto tras
encontrar en QA manual del owner problemas visuales reales (clipping,
tamaño inconsistente entre idle y click-fire, "sprites incompletos" al
volver a idle) que Block 04.2 no había detectado — ver §12/§13/§14
para el diagnóstico y la corrección. Este documento describe el estado
FINAL (incluyendo Block 04.3); `docs/DECISION_LOG.md` conserva el
registro histórico de todas las pasadas con sus propias entradas.

Resumen de las pasadas de Block 04.2, por si hace falta el contexto
histórico exacto: la primera integró a Nidir con una semántica de
animación INCORRECTA (idle en loop continuo) y un canvas del tamaño
nativo del arte fuente (Nidir aparecía mucho más grande que Bunny en
pantalla). La segunda corrigió ambas cosas y agregó el intento de
importar la animación real de click-fire — bloqueado por falta de
acceso a `~/Downloads` en esa sesión. La tercera pasada, tras
resolverse el bloqueo de acceso (ver §10), importó el click-fire real
("blue-fire") y reemplazó el placeholder estructural que las dos
pasadas anteriores mantenían, además de encontrar y corregir un bug
real de cobertura de texturas descubierto al ejercitar ese contenido
(ver §6).

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
de forma independiente: `25 / 3.0 ≈ 8.33 fps`.

> **Nota (Block 04.3, superseded):** en el estado de la tercera pasada
> de Block 04.2, este párrafo decía que el canvas lógico del pet no
> cambiaba por esta importación y que los frames de click-fire se
> estiraban de forma independiente (~4.5%) para llenar el mismo canvas
> que idle -- eso era exactamente correcto para ESE estado, pero
> resultó ser la causa real del "tamaño visual inconsistente entre
> idle y click-fire" que el owner reportó en QA manual. Block 04.3 lo
> corrige con una política de canvas de trabajo compartido, anclado
> por contenido -- ver §12. El canvas lógico de Nidir SÍ cambia ahora
> por esta importación (156×160 -> 160×157), y los frames de
> click-fire y de idle ya NO se estiran de forma independiente entre
> sí -- comparten el mismo canvas de trabajo interno antes de
> renderizarse. `SDL_RenderTexture` (`src/app/SpikeApp.cpp`) sigue
> siendo, sin cambios, un simple stretch-to-fill del texture completo
> al rect de destino exacto -- comportamiento genérico preexistente
> desde Block 02; lo que cambió en Block 04.3 es qué se le da de
> comer a ese stretch (ver §12), no el mecanismo de render en sí.

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

**Actualización (Block 04.3):** `compute_logical_canvas_size()` en sí
NO cambió -- sigue siendo la misma función pura de arriba. Lo que
cambió es qué dimensiones se le pasan: en vez de la resolución nativa
cruda de idle (513×525), ahora recibe el canvas de TRABAJO compartido
que la nueva política de §12 deriva del contenido real de TODAS las
animaciones del pet (idle + click_reaction), no solo de idle. Para
Nidir esto da **canvas lógico = 160×157** (antes: 156×160) -- muy
similar en magnitud, pero ahora corrige el bug real de tamaño/encuadre
inconsistente entre animaciones que la política de esta sección, por
sí sola, no podía resolver (solo sabía escalar el aspect ratio de UNA
animación de referencia, nunca reconciliar el encuadre relativo entre
varias). Ver §12 para el detalle completo.

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
  inferencia con evidencia suficiente, no una adivinanza. **Superseded
  en Block 05, segunda pasada (ver `docs/DECISION_LOG.md` DEC-073):**
  ese archivo no era RGBA y nunca se alimentó al pipeline de
  compilación; `master.png` ahora es una copia determinística, RGBA,
  del frame 0 de la animación idle canónica, escrita por
  `tools/generate_nidir_pack.py` — nunca a mano.
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
8. **Si el pet tiene más de una animación con encuadre nativo
   distinto** (idle vs. una animación de efecto que necesita más
   margen, como el click-fire de Nidir): agregar
   `"normalize_visual_scale": true` al manifest -- ver §12. Sin esto,
   cada animación se estira de forma independiente al mismo canvas
   lógico fijo y el personaje puede aparecer a tamaños/posiciones
   distintos según qué animación esté activa. Con un solo booleano
   (sin ningún cálculo manual por pet), `tools/compile_pet_pack.py`
   deriva el canvas de trabajo compartido y el reescalado de contenido
   necesario a partir de los pixeles reales.

## 12. Clipping y tamaño visual inconsistente entre animaciones (Block 04.3)

Block 04.3 es un bloque CORRECTIVO abierto tras QA manual real del
owner sobre el estado de Block 04.2 (todavía sin mergear a main).
Problemas reportados: (1) la animación de click-fire se ve cortada/
clipped, (2) pérdida de calidad visual en general, (3) al volver a
idle después de una animación, a veces queda mostrando solo parte del
sprite o se "buguea", (4) el tamaño visual de Nidir no está bien
normalizado frente a Bunny.

### Diagnóstico, con evidencia -- no una suposición

**(1) Clipping real, confirmado a nivel de pixel.** Se midió el
bounding box de contenido visible (alpha > 8) de los 25 frames reales
de click-fire: el bounding box UNIÓN a través de toda la secuencia
abarca literalmente el ancho completo del frame nativo (columnas 0 a
623 de 624) y casi toda la altura (filas 39 a 611 de 612) -- 18 de los
25 frames tienen contenido que toca o excede su propio borde nativo
(frames 006-022, el pico del efecto de fuego). **Esto está en el
export original de Ludo.ai, no es algo que este pipeline le haga al
contenido** -- confirmado componiendo esos frames tal cual (sin ningún
cambio de este repositorio) y viéndolos directamente: el fuego llega
al límite izquierdo del canvas exportado en su pico. `SDL_RenderTexture`
(el único llamado que dibuja un frame en pantalla, sin cambios en este
bloque) nunca recorta nada -- siempre pasa el texture COMPLETO como
fuente, así que el pipeline de este repositorio no le agrega ningún
recorte adicional al que ya trae el export. **Limitación real,
documentada honestamente:** si este recorte del efecto de fuego en su
pico es inaceptable para el producto final, la única corrección real
es un nuevo export de Ludo.ai con más margen alrededor del personaje
para el efecto -- no es algo que se pueda arreglar synthesizando
pixeles que nunca se exportaron, y este bloque NO inventa contenido
de reemplazo (prohibido explícitamente por el brief de Block 04.2,
principio que se mantiene acá).

**(2)+(4) Pérdida de calidad / tamaño inconsistente: bug real de
este pipeline, con causa raíz identificada y corregida.** Antes de
este bloque, CADA animación de Nidir se estiraba de forma
INDEPENDIENTE para llenar el mismo canvas lógico fijo (156×160) vía
`SDL_RenderTexture(renderer, texture, nullptr, &dst)` -- el texture
completo de la animación activa siempre se estira a exactamente
`(0,0,canvasWidth,canvasHeight)`, sin importar su tamaño ni encuadre
nativo. El problema: idle (frame nativo 513×525) y click_reaction
(frame nativo 624×612) NO tienen el mismo margen alrededor del
personaje dentro de su propio frame -- click-fire necesita espacio
extra para el efecto de fuego. Medido con el bounding box de contenido
real: el personaje (sin el fuego, comparando el frame "de reposo"
frame_000 de cada animación) ocupa casi el MISMO tamaño absoluto en
pixeles en ambas animaciones (idle: 434×498, click_reaction: 435×498
-- prácticamente idénticos), pero click_reaction lo tiene centrado
dentro de un frame nativo más grande (624×612 vs. 513×525). Al
estirar cada frame de forma independiente al mismo canvas lógico
fijo, el personaje terminaba mostrándose visiblemente más chico
durante click-fire que durante idle -- y el "salto" de tamaño/posición
exactamente en el instante de la transición click-fire → idle es,
casi con certeza, lo que se percibió como "solo parte del sprite" o
"se buguea" en el reporte (3): no es un bug del state machine (el
retorno a Idle en `AnimationController::TransitionToIdle()` siempre
muestra la pose base VERDADERA, sin cambios en este bloque -- ver
§5.1), sino una discontinuidad visual real por el estiramiento
independiente descrito acá.

### La corrección: canvas de trabajo compartido, anclado por contenido

Política GENÉRICA nueva (sin ninguna rama por pet), implementada en
`tools/prep_dev_sprite.py` (`compute_content_bbox()`,
`compose_on_canvas()`, `compute_frame_normalization_plan()`) y usada
por `tools/compile_pet_pack.py` vía el campo de manifest opcional
`normalize_visual_scale` (default `false` -- sin cambio de
comportamiento para cualquier pack que no lo pida, verificado
byte-a-byte con el pack de Bunny después de este cambio):

1. Para cada animación compilable (canónica + overrides
   direccionales), se calcula el bounding box de contenido de su
   PRIMER frame (la pose base/de-reposo, por la misma convención de
   "first/last frame contract" ya establecida -- §6.1).
2. Un **content_scale** por grupo lógico (idle/click_reaction/cada
   passive_action, compartido entre una animación canónica y sus
   overrides direccionales -- nunca escalas distintas para right/left
   de la misma animación): factor para que el contenido visible de
   cada grupo ocupe el mismo tamaño absoluto en pixeles que el grupo
   de referencia (`idle`, por la misma convención que DEC-045 ya usa
   para el tamaño de canvas). Con una tolerancia del 2% para evitar un
   resample innecesario cuando la diferencia real es ruido de medición
   -- para Nidir, `content_scale` termina siendo exactamente 1.0 en
   los cuatro grupos (idle/idle_left/click_reaction/click_reaction_left),
   ya que el personaje YA estaba dibujado al mismo tamaño absoluto en
   ambos exports.
3. Un **canvas de trabajo compartido por todo el pet** (una sola
   dimensión para TODAS las animaciones, calculada para que ninguna
   se recorte), y un **offset por entrada** que alinea el centro del
   bounding box de contenido de cada animación al centro de ese canvas
   compartido. Para Nidir: 624×612 (dominado por el frame nativo de
   click_reaction, ya que necesita más margen que idle) -- idle queda
   colocado con un margen de (86,60) puntos dentro de ese canvas
   compartido; click_reaction queda en (0,0) exacto (es, sin cambios,
   su propio frame nativo completo).
4. `compute_logical_canvas_size()` (sin cambios en sí misma, §7) se
   aplica ahora sobre ESE canvas de trabajo compartido (624×612) en
   vez de la resolución nativa cruda de idle sola -- da **160×157**
   para Nidir (antes: 156×160).
5. `runtime_max_frame_dimension` (§8, sin cambios en su mecanismo)
   sigue aplicándose DESPUÉS, sobre el frame ya compuesto en el canvas
   de trabajo -- el resultado: idle y click_reaction terminan
   compilando a EXACTAMENTE las mismas dimensiones de runtime
   (320×314 los dos), algo que antes de este bloque nunca coincidía
   (idle compilaba ~313×320, click_reaction ~320×314 -- valores
   distintos, no solo visualmente sino a nivel de bytes).

**Nunca se recorta contenido** -- `compose_on_canvas()` solo agrega
margen transparente, nunca resamplea ni corta (verificado con un test
dedicado, `ContentBboxTest`/`ComposeOnCanvasTest`/
`FrameNormalizationPlanTest` en `tools/test_asset_pipeline.py`).
Verificado visualmente antes de integrar: se compusieron a mano
frame_000 de idle y de click_reaction sobre el mismo canvas de trabajo
y se inspeccionaron lado a lado -- el personaje aparece al mismo
tamaño y posición en ambos, confirmando que la política funciona como
se diseñó, no solo en teoría.

**Trade-off de calidad, documentado honestamente:** como el canvas de
trabajo de Nidir (624×612) es más grande que la resolución nativa de
idle sola (513×525), el mismo límite de `runtime_max_frame_dimension`
(320px) ahora reparte ese presupuesto de pixeles sobre un área
ligeramente mayor para idle -- su personaje compilado pasa de ~265×304
pixeles efectivos a ~222×255. Esto NO es una pérdida real frente a lo
que la pantalla necesita: el canvas lógico final (160×157, a 2x Retina
= 320×314 pixeles físicos) es exactamente lo que idle y click_reaction
ahora ocupan, ni más ni menos -- antes, idle estaba efectivamente
"sobre-provisto" de resolución respecto de lo que su propio canvas
lógico (156×160) necesitaba. No se cambió `RUNTIME_MAX_FRAME_DIMENSION`
(sigue siendo `2 × REFERENCE_LOGICAL_SIZE = 320`, la misma
justificación de "margen para 2x Retina" que ya tenía) porque sigue
siendo el valor correcto para el nuevo canvas lógico real.

## 13. Dirección automática por mitad de pantalla (Block 04.3)

Requisito nuevo del owner: mitad derecha de la pantalla → animaciones/
sprites `right`; mitad izquierda → `left`. Antes de este bloque no
existía ningún disparador real para `SpikeApp::SetActiveDirection()`
más allá de los mecanismos solo-DEV (`NIMVLETS_DEV_DIRECTION_TEST_COUNT`)
-- el brief original de Block 04.2 dejaba esto explícitamente fuera de
alcance.

`SpikeApp::UpdateDirectionFromWindowPosition()` (nueva): calcula el
centro de la ventana, obtiene el display que la contiene
(`SDL_GetDisplayForWindow()`) y sus límites (`SDL_GetDisplayBounds()`),
y llama a `SetActiveDirection(kRight)` si el centro de la ventana cae
en la mitad derecha de ESE display (no de la pantalla completa "0";
funciona correctamente en configuraciones multi-monitor, ya que cada
display tiene sus propios límites) o `kLeft` si cae en la mitad
izquierda. Si `SDL_GetDisplayForWindow()` falla (devuelve 0, su
valor documentado de fallo), no se toca la dirección actual -- nunca
se asume un lado por defecto sin evidencia.

Se dispara desde dos lugares, cubriendo todos los casos reales sin
necesidad de instrumentar cada sitio que mueve la ventana por
separado:
- Una vez, explícitamente, en `Init()` justo después de que
  `animController_` existe y antes del primer `RenderFrame()` -- para
  que el primer frame mostrado ya refleje la dirección correcta según
  la posición inicial de la ventana (restaurada o centrada), no
  arranque siempre en `kRight` y se corrija recién en el primer evento.
- En `HandleEvent()`, ante `SDL_EVENT_WINDOW_MOVED` -- SDL dispara este
  evento ante CUALQUIER cambio de posición de ventana (drag del
  usuario o una llamada programática a `SDL_SetWindowPosition()`,
  incluida la que el propio drag hace frame a frame), así que un único
  hook cubre drag en vivo y reposicionamiento programático sin
  duplicar lógica.
- También se reaplica dentro de `TrySwitchActivePet()`, después de
  reconstruir `animController_` -- ese `emplace()` reinicia la
  dirección a `kRight` (su default) sin importar cuál estaba activa
  antes del switch; sin este re-aplicado, un pet recién activado
  podría arrancar mostrando el lado equivocado hasta el próximo evento
  de movimiento.

**Estable y sin glitches, por diseño:** `SetActiveDirection()` (sin
cambios, Block 04.2) ya es un no-op si la dirección no cambió, y nunca
interrumpe un click/passive action en curso -- solo afecta a qué
animación resuelve el próximo `TransitionToIdle()`. Disparar el hook
muchas veces por segundo durante un drag activo no tiene ningún costo
real más allá de esa comparación barata.

**Verificado contra el binario real** (no solo por lectura de código):
se fabricó un archivo `state.nvstate` con una posición de ventana
conocida en el borde izquierdo de la pantalla (x=0) y otro en el borde
derecho (x=1300, con el display real de este entorno midiendo
1470×956) -- ambos casos resolvieron la dirección esperada
(`left`/`right` respectivamente), confirmado por un log dedicado
(`screen-half direction check: windowCenterX=... displayBounds=...
-> ...`) que reporta los valores reales usados en la decisión, útil
tanto para esta verificación como para QA futura.

No se pudo cubrir con un test unitario en `tests/` por la misma razón
que el fix de cobertura de texturas de Block 04.2 (DEC-049): esta
lógica vive en `SpikeApp.cpp`, dentro del ejecutable SDL-dependiente
`nimvlets_spike`, no en ninguna librería que `nimvlets_tests` enlace.

## 14. Idle periódico: 60 segundos (Block 04.3)

`passive_interval_seconds` de Nidir pasa de 300s (5min, el placeholder
explícito de Block 04.2) a **60s (1 minuto)**, por pedido directo del
owner. Ya no es un placeholder -- es un valor de producto real para
este pet. `PetDefinition::passiveIntervalSeconds` sigue siendo el
único lugar donde este valor se define (sin cambios de mecanismo,
`SpikeApp::ComputeEffectivePassiveIntervalSeconds()` sigue leyendo
este campo tal cual, con el mismo override solo-DEV
`NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS` de Block 02 disponible para
QA/smoke tests). Cambio de un solo valor en
`tools/generate_nidir_pack.py` (`PASSIVE_INTERVAL_SECONDS`), no
requiere ningún cambio de código C++. Bunny no se ve afectado (su
propio manifest define su propio `passive_interval_seconds`,
independiente).

## 15. Bug real de QA manual: sprite estático parcial tras un cambio de dirección (corrección post-QA)

QA manual real del owner sobre el estado cerrado de Block 04.3 (antes
de esta corrección) confirmó que la calidad/tamaño visual ya estaban
bien, pero encontró un bug reproducible: al arrastrar Nidir a través
del punto medio de la pantalla (la dirección automática cambia
left↔right, ver §13), el sprite estático recién mostrado a veces
aparecía parcial/corrupto (p. ej. el hocico faltante). Reproducir
cualquier animación (idle o click) después "arreglaba" la vista.

### Diagnóstico

Se inspeccionó el camino completo de un cambio de dirección antes de
tocar código, punto por punto:

- **`AnimationController::SetDirection()`** (sin cambios desde
  Block 04.2): actualiza `direction_` y, si el controller está Idle,
  re-resuelve `currentAnimation_` a `ResolveIdleAnimation(pet_,
  direction_)` con `currentFrameIndex_=0` -- correcto, sin overrides
  colgantes ni frames a medio actualizar.
- **Textura del frame estático de la nueva dirección**: ya adjuntada
  desde la carga del pet (`AttachAllTextures()`, corregida en Block
  04.2 tercera pasada, cubre `idleDirectionOverrides` completo) --
  confirmado que el bug de cobertura de texturas de esa pasada NO
  reaparece acá.
- **`ApplyCurrentHitMask()`/`SDL_SetWindowShape()`**: se leyó
  DIRECTAMENTE la fuente pineada de SDL 3.4.12
  (`src/video/cocoa/SDL_cocoashape.m`, función `Cocoa_UpdateWindowShape`)
  para confirmar, no asumir, su comportamiento real -- en macOS esta
  función ÚNICAMENTE actualiza `NSWindow.ignoresMouseEvents` según la
  posición actual del cursor; no compone, recorta, ni toca ningún
  pixel renderizado (reconfirma DEC-017 de Block 01). **Esto descarta
  con evidencia directa de código fuente que el mecanismo de
  click-through/shape sea la causa de cualquier corrupción visual en
  macOS** -- por diseño, no puede serlo.
- **Canvas lógico/tamaño de renderizado**: `pet_.canvasWidth/canvasHeight`
  es un valor a nivel de PET, no cambia con la dirección -- descartada
  cualquier discrepancia de tamaño entre direcciones.
- **`RenderFrame()`/`ApplyCurrentHitMask()`**: ambas leen
  `animController_->CurrentFrame()` de forma independiente pero
  consecutiva dentro del mismo bloque `if (needsRedraw_)`, sin ninguna
  mutación de estado entre medio -- ambas ven siempre el mismo frame
  resuelto, coherente.

Con la lógica de contenido/estado descartada por inspección directa
(y, en el caso del shape, por lectura de la fuente real de SDL), la
explicación que mejor encaja con el patrón reportado ("un present
frío se ve incompleto; presentar varias veces seguidas -- exactamente
lo que hace una animación reproduciéndose -- lo arregla") es una clase
de problema conocida de macOS/Metal: `SDL_RenderPresent()` puede
mostrar un drawable de `CAMetalLayer` todavía no completamente
asentado en su rotación de buffers cuando es el primer present después
de un período largo sin presentar nada -- exactamente la situación de
este runtime, deliberadamente event/deadline-driven (`SDL_WaitEventTimeout`
puede bloquear por minutos, ver §6 de `docs/ANIMATION_RUNTIME.md`).
**No se pudo confirmar visualmente en este entorno** (sin display
interactivo real disponible para esta sesión) -- documentado
honestamente como la explicación mejor sustentada por la evidencia
disponible, no como un hecho verificado con captura de pantalla.

### La corrección

Genérica -- aplica a CUALQUIER redraw de CUALQUIER pet, no solo a
Nidir ni solo a cambios de dirección:

1. **`SpikeApp::RenderFrame()`** presenta el mismo contenido dos veces
   seguidas (clear+draw+present, dos veces) en vez de una -- fuerza
   dos drawables físicos distintos a través de la rotación de buffers
   en cada redraw.
2. **`confirmRedrawDeadlineMs_`** (nuevo, `SpikeApp.h`): un cambio de
   dirección que sí cambia el frame mostrado arma, además del redraw
   inmediato, un SEGUNDO redraw completo ~120ms más tarde -- una
   separación real de wall-clock (no microsegundos), más parecida a
   cómo una animación en reproducción naturalmente presenta varias
   veces separadas en el tiempo. Esto es lo que hace que la corrección
   **no dependa de que una animación futura "arregle" el estado por
   casualidad** (instrucción explícita del brief de esta corrección):
   el propio cambio de dirección programa su propia confirmación,
   siempre, sin importar si el usuario dispara una animación después o
   no.

Verificado (no solo diseñado): con un build Debug (que loguea cada
redraw real vía el diagnóstico `[diag] animation redraw`), un solo
cambio de dirección mientras el pet está Idle produjo tres redraws
reales del mismo frame, separados en el tiempo (t=433ms, t=435ms,
t=531ms -- el último, ~98ms después del primero, es
`confirmRedrawDeadlineMs_` disparando). Se corrió además una ráfaga de
500 cambios de dirección automatizados
(`NIMVLETS_DEV_DIRECTION_TEST_COUNT=500`) contra el binario Release
real: 500/500 sin errores, RSS estable (~156.7MB, sin crecimiento),
apagado limpio -- confirma que el mecanismo no introduce ninguna
acumulación de recursos ni inestabilidad, aunque -- honestamente -- no
puede confirmar por sí solo que el bug visual específico esté
resuelto, ya que este entorno no tiene forma de capturar/inspeccionar
los pixeles realmente presentados en pantalla. Queda pendiente de
confirmación visual real en la próxima QA manual del owner.

## 16. Tamaño visual global: candidato +5% (Block 04.3, corrección post-QA)

El owner confirmó en QA manual que el tamaño actual de Nidir (tras la
corrección de §12) está bien, pero pidió evaluar TODOS los Nimvlets
~5% más grandes antes de decidir definitivamente. Implementado como
una política GENÉRICA, no una constante propia de Nidir:

`prep_dev_sprite.DISPLAY_SIZE_SCALE_FACTOR = 1.05` -- un único valor
de módulo, aplicado automáticamente por
`compute_logical_canvas_size()` a CUALQUIER pet que use esa función
(hoy: Nidir y Bunny, ambos migrados a `generate_<pet>_pack.py` con
`normalize_visual_scale`/canvas de trabajo compartido). El parámetro
`scale_factor` de la función sigue existiendo para poder pasar `1.0`
explícitamente (usado por los tests que verifican la matemática de
aspect ratio en sí, independiente del candidato de tamaño vigente).

**Para revertir al tamaño anterior:** cambiar `DISPLAY_SIZE_SCALE_FACTOR`
a `1.0` en `tools/prep_dev_sprite.py` y volver a correr
`tools/generate_nidir_pack.py`/`tools/generate_bunny_pack.py` -- ningún
otro cambio de código hace falta, ninguna recompilación de la política
en sí. Esto es intencional: este es un candidato de QA, no una decisión
de producto cerrada.

**Dimensiones lógicas resultantes, documentadas exactamente:**

| Pet | Canvas de trabajo | Canvas lógico ANTES (factor 1.0) | Canvas lógico AHORA (factor 1.05) |
|---|---|---|---|
| Nidir | 624×612 | 160×157 | **168×165** |
| Bunny | 428×563 | 122×160 | **128×168** |

Preservado, sin excepciones: PNG fuente intactos (el factor solo
afecta el cálculo de `canvas_width`/`canvas_height`, nunca los pixeles
de ningún frame); la política de normalización visual de §12 (el
factor se aplica DESPUÉS de derivar el canvas de trabajo, multiplicando
el resultado final -- el encuadre relativo entre animaciones no
cambia); la alineación del hit-mask (sigue leyendo el mismo
`canvasWidth/canvasHeight` que el render, sin excepción); el
click-through (sin cambios de mecanismo); el comportamiento de
high-DPI (`SDL_SetRenderLogicalPresentation` sigue operando sobre el
mismo canvas lógico, solo que un poco más grande); la escala relativa
entre estático/idle/click (ambos pets siguen usando el mismo canvas de
trabajo compartido para todas sus animaciones); y la consistencia
izquierda/derecha (el factor es un escalar aplicado igual a ambas
direcciones, no rompe la simetría del espejado).

## 17. QA manual del owner sobre el estado de §15/§16, y siguiente corrección

El owner hizo una segunda ronda de QA manual sobre el estado cerrado
descrito en §15/§16 y confirmó, textualmente resumido: Nidir se ve
bien -- animaciones correctas, sin partes faltantes, el click se ve
bien; el candidato +5% de tamaño (§16) le gustó. El bug de §15 (sprite
parcial tras un cambio de dirección) no reapareció en esta ronda de
QA -- confirmación indirecta de que el fix de doble-present +
`confirmRedrawDeadlineMs_` funciona en el binario real, algo que §15
había dejado honestamente sin confirmar visualmente en su momento por
falta de un método de captura real. El passive/idle periódico no se
observó explícitamente en esta ronda breve de QA (no es una
confirmación de que NO funcione, solo que no se ejercitó a propósito)
-- ver §17.1 para la reverificación explícita que se corrió por eso.

Con esa base confirmada, el owner pidió, para esta corrección:

1. Otro +5% de tamaño, pero **absoluto contra el baseline original**
   (160), no compuesto sobre el +5% ya aplicado -- ver §18.
2. Una segunda animación de idle real para Nidir ("wing_stretch") con
   selección ponderada 70/30 contra la existente -- ver §19.
3. El intervalo pasivo a 10 segundos para AMBOS Nimvlets (reemplaza el
   valor de 60s de §14) -- ver §19.
4. Disparo por hover, además del disparo por timer -- ver
   `docs/ANIMATION_RUNTIME.md` §8.1 (política genérica, no específica
   de Nidir).

### 17.1 Reverificación explícita del disparo pasivo por timer

Como la QA manual breve del owner no ejercitó a propósito el passive/
idle periódico, se corrió una reverificación explícita contra el
binario real antes de dar por buena esta corrección:
`NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS=2` (acorta el intervalo para
una corrida de smoke test corta) produjo, en los logs de Debug, la
secuencia esperada de transiciones de estado (`[diag] animation
redraw: state=PassiveAction` seguido, tras que la secuencia completa
termina de reproducirse, de `state=Idle`) -- confirmando que el
mecanismo de timer sigue disparando correctamente sobre el pack real
de Nidir tras todos los cambios de esta corrección (canvas de trabajo
de 3 grupos, dos acciones pasivas, pesos 70/30).

## 18. Tamaño visual global: +10% absoluto contra el baseline original (Block 04.3, corrección post-QA)

El candidato +5% de §16 (`DISPLAY_SIZE_SCALE_FACTOR = 1.05`) fue
confirmado por el owner en la QA de §17; esta corrección lo lleva a
`1.10` -- **absoluto contra `REFERENCE_LOGICAL_SIZE` (160), no
compuesto sobre el +5% ya aplicado**. Esta distinción importaba
tenerla bien: `1.05 * 1.05 = 1.1025` habría sido un +10.25%
compounding (un +5% adicional MULTIPLICADO sobre el +5% anterior, no
lo que el owner pidió) si `DISPLAY_SIZE_SCALE_FACTOR` se hubiera
aplicado sobre un tamaño ya candidato en vez de sobre el baseline
original. Por construcción, esto YA era correcto antes de este
cambio -- `compute_logical_canvas_size()` siempre multiplica
`DISPLAY_SIZE_SCALE_FACTOR` contra `REFERENCE_LOGICAL_SIZE` (160)
directamente, nunca contra un tamaño intermedio ya escalado -- así que
simplemente asignar `1.10` a la constante produce el +10% absoluto
correcto, sin ningún cambio de fórmula. Esta distinción está cubierta
por un test dedicado
(`tools/test_asset_pipeline.py::DisplaySizeScaleFactorTest::test_scale_factor_is_absolute_against_original_baseline_not_compounded`),
que compara la constante directamente contra `1.10` Y contra `1.05 *
1.05`, en vez de solo comparar dimensiones de canvas redondeadas (que,
para algunas resoluciones, redondean igual para ambos factores y no
discriminarían el bug).

**Dimensiones lógicas resultantes, actualizando la tabla de §16:**

| Pet | Canvas de trabajo | Canvas lógico (factor 1.0) | Canvas lógico (factor 1.05, §16) | Canvas lógico AHORA (factor 1.10) |
|---|---|---|---|---|
| Nidir | 624×612 (sin cambio al agregar `wing_stretch` -- ver §19) | 160×157 | 168×165 | **176×173** |
| Bunny | 428×563 (sin cambio al agregar `groom` -- ver `docs/BUNNY_CONTENT.md` §9) | 122×160 | 128×168 | **134×176** |

Mismos invariantes preservados que documentó §16 (PNG fuente intactos,
normalización visual sin cambios de encuadre relativo, hit-mask
alineado, sin cambios de mecanismo de click-through/high-DPI,
simetría izquierda/derecha preservada) -- este cambio es puramente un
ajuste del valor de la constante, no de ninguna fórmula ni política.

## 19. Segunda animación de idle real: "wing_stretch", 70/30, intervalo de 10s (Block 04.3, corrección post-QA)

El owner exportó una segunda animación de idle real de Nidir
("estiramiento de alas") vía Ludo.ai, entregada en
`local_imports/nidir/nidir-idle-wing-stretch-right/` (25 frames,
624×519 nativo) y su spritesheet correspondiente
(`local_imports/nidir/nidir-idle-wing-stretch-right-spritesheet/`,
3120×2595 = 5×624 × 5×519, grilla 5×5 confirmada, no asumida) --
**instrucción explícita del owner: usar `local_imports/` como staging
en vez de `~/Downloads`** (el blocker histórico de §10 hacía de
`~/Downloads` un lugar poco práctico para este flujo de todos modos).

Copiado (nunca movido) a su ruta canónica,
`assets/source/nimvlets/nidir/animations/wing_stretch/right/{frames,spritesheet}/`,
verificado por checksum MD5 contra el staging antes de considerar el
import completo. `tools/generate_nidir_pack.py` deriva "left" por el
mismo espejado horizontal determinista que ya usa para idle/
click_reaction (Nidir es canónicamente "right" -- sin inversión, a
diferencia de Bunny).

**Clipping, revisado con el mismo método que las demás animaciones**
(bounding box unión de contenido a través de la secuencia completa):
16/25 frames (frames 4-19) tocan el borde lateral izquierdo y/o
derecho con margen EXACTAMENTE cero -- mucho más extenso que idle o
click_reaction. Inspección visual directa de los frames más extremos
(10 y 15, ambos marcados) muestra alas completas, de forma natural,
con margen visible a ambos lados dentro del frame -- **no** el patrón
de corte plano/antinatural que sí se encontró en el idle real de
Bunny (ver `docs/BUNNY_CONTENT.md` §3.1). Conclusión: un export
"ajustado pero no realmente recortado" (margen nativo cero, no una
forma truncada a mitad de camino) -- las puntas de las alas
simplemente llegan hasta el borde del canvas nativo durante la
extensión completa del estiramiento, sin perder contenido real. No es
un hallazgo que amerite reexportar, a diferencia del caso de Bunny.

**Canvas de trabajo compartido:** agregar `wing_stretch` como tercer
grupo a la política de normalización de §12 NO cambió el canvas de
trabajo (sigue siendo 624×612, el mismo que ya dominaba idle/
click_reaction) -- su contenido, aun con el margen lateral ajustado,
cabe dentro de ese mismo canvas sin necesitar más espacio
(`content_scale=1.0000`, `offset=(0,56)`).

**Selección ponderada 70/30 y 10 segundos** -- ver
`docs/ANIMATION_RUNTIME.md` para el mecanismo genérico
(`content::ChooseWeightedPassiveActionIndex()`,
`passive_action_weights` en el pack). Para Nidir específicamente:
`passive_actions = [idle_breathing_right, wing_stretch_right]`,
`passive_action_weights = [0.7, 0.3]`, `passive_interval_seconds =
10.0` (reemplaza el valor de 60s de §14, pedido explícito del owner
para AMBOS Nimvlets). `content_version` avanza a
`"block04.3-nidir-3"`.

**Verificado** contra el binario real: carga sin errores ni
advertencias de cobertura de texturas (`AttachAllTextures()` ya
itera genéricamente sobre `pet_.passiveActions`, sin ningún ajuste
necesario para la segunda entrada); capturas de pantalla reales (ver
el informe de esta corrección) de `wing_stretch` en reproducción
muestran alas completas, sin pérdida de pixeles, consistente con el
fix genérico de calidad de downscale descrito en `docs/BUNNY_CONTENT.md`
§8 (que también beneficia a Nidir, cuyo canvas de trabajo 624×612
también excede `runtime_max_frame_dimension`).

## 20. Corrección post-QA (Block 05, segunda pasada): mismo bug de canvas compartido que Bunny, sin regresión visual confirmada

El bug de key-format de `tools/compile_pet_pack.py` documentado en
`docs/DECISION_LOG.md` DEC-071 (ver el detalle completo en
`docs/BUNNY_CONTENT.md` §12) afectaba a TODA `WeightedAction` de TODO
pet por igual -- Nidir incluido: `click_reaction`, `idle_breathing` y
`wing_stretch` se compilaban cada una fuera del canvas de trabajo
compartido (624×612, §12) descrito arriba, con su propio encuadre
nativo en vez de compartir la normalización anclada por contenido.
QA manual explícito de esta segunda pasada pidió validar que Nidir
"estaba bien antes y no debe regresar" -- confirmado: el fix de
DEC-071 es correctivo, no destructivo (recompila con la MISMA
`normalization_plan` ya derivada correctamente para la pose base;
las `WeightedAction` simplemente empiezan a usarla también). Verificado
con capturas reales del binario corriendo (pose base estática y
`idle_breathing`/respiración a mitad de reproducción): mismo tamaño
visual entre ambos estados, sin salto perceptible ni pérdida de
contenido -- sin regresión.

`AMBIENT_INTERVAL_SECONDS` pasa de `10.0` (§19, Block 04.3) a `15.0`
(Block 05, política de producto vigente, unificando a Nidir con
Bunny y Frin -- ver `docs/BUNNY_CONTENT.md` §12 y DEC-074 en
`docs/DECISION_LOG.md`). `content::PetDefinition::visualScale` queda
en `1.10` para Nidir (sin cambio en esta pasada -- ver §18 para el
+10% absoluto ya vigente; el campo nuevo de Block 05 simplemente
representa ese mismo ajuste como dato de contenido en vez de un
factor hardcodeado en el compilador).

## 21. Corrección post-QA (Block 05, tercera pasada): retuning de `visual_scale` a 1.25, sin regresión geométrica

Dos cambios independientes en esta pasada, ninguno específico de
Nidir:

**Geometría (DEC-075 en `docs/DECISION_LOG.md`):** `compute_frame_normalization_plan()`
se rediseñó para calibrar cada `BehaviorState` contra el `base_animation`
de SU PROPIO estado (vía un union-find de archivo de frame
compartido), en vez de siempre contra un único `reference_group`
pet-wide -- corrige un bug real en Frin (comparar bounding boxes entre
posturas de silueta distinta). Nidir tiene un solo `BehaviorState`
("default"), así que este cambio es matemáticamente idéntico al
comportamiento anterior para Nidir por construcción (el union-find
nunca fusiona nada porque idle/click_reaction/wing_stretch nunca
comparten archivo entre sí) -- **verificado**: el pack recompilado
produce EXACTAMENTE los mismos `content_scale` (1.0000 para los tres
grupos) y el mismo canvas de trabajo (624×612) que antes de este
cambio. `tools/compile_pet_pack.py` también combina ahora el resize de
normalización y el downscale de `runtime_max_frame_dimension` en una
sola pasada (menos pérdida de detalle acumulada, ver
`docs/BUNNY_CONTENT.md` §13) y falla fuerte si algún frame real
excediera el canvas compartido -- Nidir recompila limpio bajo ambas
verificaciones.

**Tamaño (DEC-076):** QA manual real seguía reportando que Nidir se
sentía más chico que Bunny pese al +10% de §18/§20. Medido con
evidencia real (bounding box de contenido visible del pack compilado,
no solo el canvas transparente): a `visual_scale=1.10`, la ALTURA
efectiva de Nidir (~155pt) quedaba por DEBAJO de la de Bunny (~159pt)
pese a ser más ancho -- consistente con "sigue sintiéndose más chico".
`VISUAL_SCALE` sube a `1.25` en `tools/generate_nidir_pack.py` --
efectivo ahora ~154×176pt, claramente por encima de Bunny en ambos
ejes. Verificado con capturas reales del binario corriendo (pose base
y ambas pasivas): presencia de escritorio visiblemente mayor, sin
pérdida de contenido ni cambio de proporciones.

