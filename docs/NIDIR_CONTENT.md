# Nimvlets — Nidir: assets reales + pipeline direccional (Block 04.2)

Nidir es el primer Nimvlet con arte real de producción integrado al
runtime (a diferencia de Bunny, que sigue siendo un fixture de QA —
ver AGENTS.md §11 y `docs/DECISION_LOG.md` DEC-018). Este documento
describe la convención de asset source que este bloque establece, el
pipeline de importación/normalización/espejado, la extensión
direccional del content model, y cómo un futuro Nimvlet sigue el mismo
patrón. Ver `docs/ANIMATION_RUNTIME.md` para el runtime en el que esto
se enchufa y `docs/CATALOG.md` para el catálogo en el que Nidir ahora
es una segunda entrada real.

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
(`src/content/PetPackLoader.cpp`, `tools/compile_pet_pack.py`): una
sección final, opcional, de "overrides de idle por dirección":

```
[... exactamente el layout NVPACK1 existente, sin cambios ...]

-- sección nueva, SOLO presente si el manifest la pidió --
directionalIdleOverrideCount : uint32
overrides[count]:
  direction : uint8  (0 = right, 1 = left)
  animation : AnimationBlock
```

Un pack compilado antes de este bloque (el `bunny_pack.nvpack` ya
comiteado, nunca recompilado para esto) simplemente termina justo
después de `passiveActions` — sin ningún byte extra. El loader
(`ByteReader::HasMoreData()`) distingue ambos casos sin necesitar un
bump de `schemaVersion`: si no queda ningún byte, `idleDirectionOverrides`
queda vacío (comportamiento no-direccional, idéntico al de antes de
este bloque); si sí quedan bytes, se interpretan como esta sección, y
cada campo adentro sigue siendo obligatorio (falla ruidosamente igual
que cualquier otro campo truncado). `tools/compile_pet_pack.py` solo
escribe la sección cuando el manifest incluye
`"idle_direction_overrides"` explícitamente — el manifest de Bunny no
lo incluye, así que su pack compilado es byte-a-byte idéntico al de
antes de este bloque.

**Modelo C++** (`src/content/AnimationDefinition.h`):

```
enum class Direction { kRight = 0, kLeft = 1 };

struct DirectionalAnimationOverride {
    Direction direction;
    AnimationDefinition animation;
};

struct PetDefinition {
    ...
    AnimationDefinition idle;                                  // sin cambios de significado
    std::vector<DirectionalAnimationOverride> idleDirectionOverrides;  // nuevo, aditivo
    ...
};

const AnimationDefinition& ResolveIdleAnimation(const PetDefinition& pet, Direction direction);
```

`pet.idle` conserva exactamente su significado anterior (el idle
canónico de un pet — para Bunny, el único; para Nidir, por
convención, el de `Direction::kRight`, ya que ese es el que el export
real trae). `ResolveIdleAnimation()` es la única forma soportada de
resolver dirección: retorna la entrada dedicada de
`idleDirectionOverrides` si existe una para `direction`, si no cae a
`pet.idle` — política de fallback documentada explícitamente (block
brief §6: "unsupported direction fails clearly or uses an explicitly
documented safe fallback"), nunca falla. Este diseño aditivo (en vez
de reemplazar `pet.idle` por una lista genérica) fue deliberado para
minimizar el blast radius: `AnimationController`, `SpikeApp`, y todos
los tests/fixtures existentes que ya referenciaban `pet.idle`
directamente siguieron compilando y pasando sin ningún cambio — ver
§7.

`content::AnimationController` gana `direction_` (default
`Direction::kRight`, igual que todo el runtime) y `SetDirection(direction,
nowMs)`: si el controller está Idle, el frame mostrado se actualiza de
inmediato (frame 0 de la nueva variante); si no (ClickReaction/
PassiveAction en curso), el cambio queda guardado y se aplica recién
cuando `TransitionToIdle()` corra por su cuenta — dirección es
metadata, nunca interrumpe un gesto en curso. `SpikeApp::SetActiveDirection()`
es el "runtime method to change direction" que pide el block brief
§7 — sin ningún control de UI que lo dispare todavía (explícitamente
fuera de alcance). `NIMVLETS_DEV_DIRECTION_TEST_COUNT` (mecanismo
solo-DEV, mismo patrón que `NIMVLETS_DEV_SWITCH_TEST_COUNT`) permite
smoke-testear cambios de dirección repetidos contra el binario real
sin QA manual.

**Sin persistencia de dirección** — decisión explícita, ver §7 del
brief ("not required unless essentially free and clearly justified")
y el comentario de `appState_` en `src/app/SpikeApp.h`: agregar un
campo al formato NVSTATE1 exigiría un bump de schema por un beneficio
que nadie pidió. Cada `Init()`/`TrySwitchActivePet()` arranca en
`Direction::kRight`.

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

`click_reaction` de Nidir es un **placeholder estructural**: un solo
frame (reutiliza `idle/right/frames/frame_000.png`), `one_shot`,
~100ms, `returns_to_idle: true` — Nidir no tiene todavía arte de click
dedicado (el export del owner solo cubre idle). Debe ser `one_shot`,
nunca `static`: un `click_reaction` estático dejaría al
`AnimationController` trabado en `ClickReaction` para siempre, ya que
`Advance()` nunca transiciona de vuelta a Idle para una animación
`kStatic` (solo lo hace al terminar naturalmente un one-shot). Sin
`passive_actions` (lista vacía — sin arte fuente para eso todavía).

**Nota de organización, no resuelta en este bloque:** el pack
compilado de Nidir vive en `assets/dev/nidir_pack.nvpack`, la misma
carpeta que el fixture de QA de Bunny — aunque Nidir es contenido real
de producción, no un fixture. Se mantuvo así por continuidad con las
convenciones de ruta ya establecidas en `tools/`/`src/catalog` (cambiar
la carpeta habría sido una reorganización de alcance mayor, no pedida
por este bloque) — ver "Bugs/debt/limitations" del informe final.

## 7. Decisiones tomadas fuera del prompt

- **`~/Downloads/Nidir.png` se usó como `master.png`.** No estaba
  entre los dos folders que el brief nombra explícitamente, pero
  coincide en nombre/ubicación/rol con lo que `assets/source/nimvlets/README.md`
  ya definía como `master.png` — inferencia con evidencia suficiente,
  no una adivinanza (§2 del brief solo pedía STOP ante folders
  faltantes/ambiguos, y ninguno de los dos folders requeridos lo
  estaba).
- **fps del idle loop: 6.0**, no un dato del export. Medido contra el
  binario Release real antes de fijarlo: a 12fps el loop de Nidir
  promedia ~11-12% CPU en steady state; a 6fps, ~5%. Se prefirió el
  costo de CPU más bajo — ver `docs/PERFORMANCE_BUDGETS.md`.
- **Canvas de Nidir = resolución nativa exacta (513×525)**, sin
  reescalar a algo más chico tipo Bunny (160×160), por instrucción
  explícita del brief ("Do NOT silently crop/resize/recenter unless
  required by the runtime contract" — el contrato no lo exige, SDL ya
  escala cualquier resolución nativa al canvas). Esto tensiona con el
  invariante de producto "ventana pequeña" de AGENTS.md §2 — ver
  "Bugs/debt/limitations".
- **`click_reaction` placeholder de un solo frame** en vez de omitir
  el campo — el esquema actual de `PetDefinition` lo exige; ver §6.
- **`ClickThroughPollingIsMeaningful`/dirección no persistida** — ver
  §5.
- **`assets/dev/nidir_pack.nvpack`** en vez de una carpeta nueva — ver
  §6.
- **Sin `provenance.json` para Nidir** — el brief pide `DESCRIPTION.txt`
  específicamente (rasgos físicos), no procedencia; se dejó
  `provenance.json` como concepto documentado pero no instanciado,
  sin contradecir `assets/source/nimvlets/README.md`.

## 8. Cómo un Nimvlet futuro sigue el mismo patrón

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
