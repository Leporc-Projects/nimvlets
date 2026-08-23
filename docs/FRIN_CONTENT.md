# Nimvlets — Frin: import real + comportamiento con estados (Block 05)

Frin es un lobo blanco/crema con ojos azules — el Nimvlet que
`docs/PRD_V1.md` documentaba históricamente como "Tan" (nombre stale,
corregido en este bloque — ver ese archivo). Es el primer Nimvlet UN
Nimvlet lógico con DOS variantes visuales reales (macho/hembra) y el
primero con una transición de POSTURA real (sentado/acostado), no solo
idle/click/passive — ver `docs/ANIMATION_RUNTIME.md` para el grafo de
comportamiento genérico que este bloque construyó para soportarlo.

## 1. Import: estructura real, frame counts, normalización

Exports reales del owner, preservados en
`assets/source/incoming/2026-08-19/` (masters + 8 carpetas de
animación con su spritesheet correspondiente, cada una con una
subcarpeta anidada extra antes de los PNG reales — el mismo patrón ya
documentado para Nidir/Bunny). Inspeccionado antes de asumir nada:

```
frin-macho-{howl,lie-to-sit,sit-to-lie,tail-greet}-left/<subcarpeta>/frame_000..024.png  (25 c/u)
frin-hembra-{howl,lie-to-sit,sit-to-lie,tail-greet}-right/<subcarpeta>/frame_000..024.png (25 c/u)
```

**Frame count real: 25 por animación, 8 animaciones, sin excepción** —
no se forzó ningún número. Dimensiones nativas (distintas entre
animaciones, cada una con su propio encuadre — la misma situación que
ya resolvía la política de canvas de trabajo compartido de Nidir/
Bunny):

| Animación | Macho (left) | Hembra (right) |
|---|---|---|
| sit_to_lie | 453×657 | 475×531 |
| lie_to_sit | 423×515 | 472×499 |
| howl | 358×515 | 345×515 |
| tail_greet | 474×607 | 405×507 |

`tools/validate_frame_sequence.py` confirma las 8 secuencias:
dimensiones consistentes dentro de cada una, sin huecos/duplicados,
alpha real no degenerado. **Ninguna normalización fue necesaria** —
los 200 frames reales se copiaron (nunca movidos) tal cual a
`assets/source/nimvlets/frin/{male,female}/animations/<animación>/
{left,right}/frames/`, verificados por checksum MD5 contra el staging
antes de considerar el import completo.

Direcciones canónicas, de los nombres de export FINALES del owner —
**macho: LEFT, hembra: RIGHT** (mismo cuidado de inversión de campo
canónico/override que ya usa Bunny, canónico "left" — ver
`docs/BUNNY_CONTENT.md` §2 — vs. Nidir/hembra de Frin, canónico
"right", sin inversión). `tools/generate_frin_pack.py` deriva la
dirección opuesta por el mismo espejado horizontal determinista
(`prep_dev_sprite.mirror_rgba_horizontal`), nunca con IA.

`master.png` (`Frin_Macho.png`/`Frin_Hembra.png`, los originales del
owner) se inspeccionaron y resultaron NO ser RGBA (colortype 2, sin
canal alpha) — inservibles para el pipeline y, además, una ilustración
distinta a los frames de animación reales. **Corrección post-QA de este
mismo bloque (ver §5 más abajo y DEC-073):** `master.png` ahora es una
copia real, RGBA, del frame 0 de la pose sentada canónica
(`sit_to_lie/{left,right}/frame_000.png`) — escrita determinísticamente
por `tools/generate_frin_pack.py`, nunca a mano. Sigue sin alimentar el
pipeline de compilación (que solo lee `animations/`) — es puramente el
asset de referencia.

**`local_imports/`/staging temporal:** no se usó — los exports ya
estaban preservados de forma permanente en
`assets/source/incoming/2026-08-19/` por el owner (a diferencia de
Nidir/Bunny, cuyo staging vivió brevemente en `local_imports/` antes
de eliminarse) — ver el informe de este bloque para si `incoming/`
queda con contenido redundante tras esta migración.

## 2. Grafo de comportamiento: sentado / acostado

Frin es el primer pet que ejercita el modelo genérico de
`content::BehaviorState` con MÁS DE UN estado y transiciones reales —
ver `docs/ANIMATION_RUNTIME.md` para el diseño del mecanismo en sí.
Ambas variantes (macho/hembra) comparten exactamente el mismo grafo
(mismos ids de estado, mismos pesos) — `tools/generate_frin_pack.py`
construye los dos manifests con la MISMA función,
`_build_variant_manifest()`, así que "comparten el modelo" es una
garantía de código, no solo documentación.

```
seated (estado inicial):
  base_animation: sit_to_lie, frame 0 (la pose sentada real -- NUNCA
    se inventó/duplicó un asset nuevo: se REFERENCIA el mismo archivo
    que ya usa sit_to_lie).
  ambient: sit_to_lie (one_shot) -> lying, tras REST_DELAY_SECONDS=12s
    (dato de contenido por-estado, ver BehaviorState::
    ambientIntervalSeconds -- no hardcodeado por especie: Artu, con el
    mismo grafo en el futuro, define su propio valor sin tocar código).
  click (ponderado): howl 70% -> seated (self-loop) / tail_greet 30% ->
    seated (self-loop).
  hover: ninguno -- hoverUsesAmbientActions=false, hoverActions=[] (el
    owner no definió ningún hover para Frin todavía; el modelo permite
    autorarlo después sin ningún cambio de motor).

lying:
  base_animation: sit_to_lie, frame 24 (el ÚLTIMO -- "the lying pose
    can be represented by the proper final frame/state" -- de nuevo,
    referencia directa, ningún asset nuevo).
  sin ambient (ambientActions=[]) -- NUNCA hay un timer armado
    mientras lying, así que howl/tail-greet nunca ocurren ahí (ver
    tests/StatefulBehaviorTest.cpp, NoRandomClickActionsWhileLying).
  click: lie_to_sit (one_shot) -> seated.
  sin hover.
```

**REST_DELAY_SECONDS=12s** — unificado con el intervalo ambient de
Bunny/Nidir (ver `docs/DECISION_LOG.md` DEC-084; historial completo:
45s por DEC-066 (nunca confirmado por el owner) -> 15s por DEC-074 ->
12s por DEC-084). Trivial de diferenciar de nuevo (un solo número en
`tools/generate_frin_pack.py`, `REST_DELAY_SECONDS`) si el owner pide
un ritmo distinto para la transición de postura específicamente.

## 3. Dirección + estado

`content::AnimationController::SetDirection()` es genérico sobre
CUALQUIER `BehaviorState` activo — nunca asume "el" estado de un pet.
Verificado explícitamente (ver `tests/StatefulBehaviorTest.cpp`):
cambio de dirección mientras `seated`, mientras `lying`, y — el caso
más delicado — a MITAD de una transición (`sit_to_lie` en curso): el
cambio queda guardado (la dirección activa cambia de inmediato como
metadata, pero el frame en pantalla nunca se toca hasta que la
transición realmente termine) y se aplica coherentemente recién cuando
`lying` se vuelve el estado activo, nunca saltando a una pose no
relacionada por cruzar la mitad de pantalla a mitad de una animación.

## 4. Compatibilidad futura con Artu — solo arquitectura

`docs/PET_CONTENT_SPEC.md`/el brief de este bloque piden que el modelo
construido para Frin sirva para Artu (futuro, NO poblado en este
bloque) sin ningún `if (pet == "artu")`. El grafo genérico ya lo
permite tal cual: Artu tendría su propio `seated`/`lying` con
`ambient_actions: [sit_to_lie -> lying]`, `click_actions` en `lying`
(`lie_to_sit -> seated`), y `click_actions` en `seated` ponderado 70%
belly-roll / 30% stretch — la MISMA forma exacta que Frin, con
contenido distinto. Cero assets de Artu se agregaron en este bloque
(fuera de alcance, explícitamente).

## 5. Corrección post-QA: tamaño inconsistente / corrupción al quedar acostado (Block 05, segunda pasada)

QA manual real encontró que las animaciones de Frin se veían a un
tamaño más grande que la pose base sentada, y que la pose "lying" se
percibía corrupta al asentarse. Causa raíz real (ver
`docs/DECISION_LOG.md` DEC-071): un bug de key-format en
`tools/compile_pet_pack.py` hacía que NINGUNA `WeightedAction`
(`sit_to_lie`/`lie_to_sit`/`howl`/`tail_greet`) pasara nunca por el
canvas de trabajo compartido — cada una se compilaba a su propio
encuadre nativo. Corregido con una única función compartida
(`_weighted_action_context()`) para construir esa clave, con un test
de regresión de integración nuevo (`tools/test_asset_pipeline.py`'s
`CompileWeightedActionNormalizationTest`) que falla si el bug
reaparece. Ambos packs de Frin se recompilaron; verificado con capturas
reales del binario corriendo (sentado, a mitad de `sit_to_lie`, y
`lying` completamente asentado, macho y hembra) — sin corrupción, sin
salto de tamaño perceptible entre estados. El rest-delay también pasa
de 45s (DEC-066, nunca confirmado por el owner) a 15s (DEC-074,
unificado con el resto de los pets).

## 6. Verificación

- `tools/validate_frame_sequence.py` sobre las 8 secuencias reales:
  sin hallazgos.
- `tools/generate_frin_pack.py` corrido contra el binario real:
  compila ambos packs sin error (`frin_male_pack.nvpack` 55 625 732
  bytes, `frin_female_pack.nvpack` 63 459 334 bytes -- recompilado en
  la corrección post-QA de §7; el tamaño exacto en bytes puede variar
  levemente entre corridas del compresor PNG interno y no es una
  cifra de contrato, solo un dato de verificación).
- Canvas lógico derivado (canvas de trabajo compartido a través de las
  4 animaciones × 2 direcciones de cada variante, tras la corrección
  de §7: 543×815 macho / 496×653 hembra -- MÁS CHICO que el canvas
  inflado de §5, ver §7): macho 117×176, hembra 134×176 a
  `visual_scale=1.0`; efectivo en pantalla `visual_scale=1.30` (§7):
  macho 152×229, hembra 174×229.
- Cargado contra el binario real (`NIMVLETS_DEV_SELECT_PET=frin/male`
  y `frin/female`): arranca en `state='seated'`, click dispara
  `howl`/`tail_greet` ponderado, el timer ambient (acelerado vía
  `NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS=1` para no esperar 15s reales)
  dispara `sit_to_lie` y transiciona a `lying` — ver el informe de
  este bloque para el log real.
- `tests/StatefulBehaviorTest.cpp` (10 tests): seated inicial,
  sit-to-lie -> lying tras el rest-delay, sin acciones aleatorias
  mientras lying, lie-to-sit -> seated, selección ponderada 70/30 en
  seated, dirección en cada punto (seated/lying/a mitad de transición),
  hover-no-op cuando el estado no define pool, y dos instancias
  stateful (macho/hembra) sin contaminación cruzada de estado.

## 7. Corrección post-QA (Block 05, tercera pasada): transforma canónica por-estado, y visual_scale a 1.30

QA manual posterior a §5 seguía reportando pérdida/corrupción en
sit-to-lie/lying/lie-to-sit, y que Frin se sentía chico. Causa raíz
REAL (la de §5 mejoró el salto de tamaño pero no lo eliminó -- ver
`docs/DECISION_LOG.md` DEC-075 para el detalle completo): el bug de
key-format de §5 dejaba que TODA `WeightedAction` compartiera el
canvas de trabajo, pero `compute_frame_normalization_plan()` seguía
midiendo el "tamaño" de cada grupo como el LADO MÁS LARGO de su
bounding box de contenido -- válido dentro de un mismo estado, pero
inválido ENTRE estados de silueta genuinamente distinta: "seated"
(alto/angosto, el lado más largo es la altura) vs. "lying"
(bajo/ancho, el lado más largo es el ancho). Medido en el pack real
ANTES de esta corrección: `state[lying].base_animation` compilaba con
`content_scale`=1.31x (macho)/1.08x (hembra), y `lie_to_sit` con
1.52x/1.11x -- inflación real, exactamente el síntoma reportado.

Corrección: dado que este proyecto ya exige el contrato first/
last-frame (`state[seated].base_animation` Y `state[lying].base_animation`
son, literalmente, el mismo archivo que el frame 0 y el frame final de
`sit_to_lie`), `compute_frame_normalization_plan()` ahora detecta ese
vínculo real (un union-find sobre archivos de frame compartidos) y
hace que ambos hereden la MISMA escala por construcción, nunca por una
nueva comparación de pixeles -- `lying` pasa a `content_scale=1.0000`
EXACTO en ambas variantes. `lie_to_sit` (sin vínculo de archivo, un
export de reversa genuinamente distinto) se calibra ahora contra el
`base_animation` de SU PROPIO estado (`lying` -- misma orientación de
silueta) en vez de contra `seated` -- pasa a 1.16x (macho)/1.03x
(hembra), una corrección real y mucho más modesta. Como consecuencia,
el canvas de trabajo compartido también se achica (macho: 689×968 →
543×815; hembra: 534×683 → 496×653) -- ya no necesita espacio extra
para una "lying" artificialmente agrandada. Verificado con capturas
reales del binario corriendo (macho: seated, lying completamente
asentado, click howl/tail_greet; hembra: seated, lying completamente
asentado) -- sin corrupción, cuerpo completo visible, cola/orejas
intactas en todos los casos.

`tools/compile_pet_pack.py` también agrega una verificación fuerte:
si el contenido real de CUALQUIER frame (no solo el frame 0 usado
para calibrar) excedería el canvas de trabajo, la compilación falla
en vez de recortar en silencio -- detectó, durante el desarrollo de
esta corrección, que `lie_to_sit` llegaba a exceder el canvas
derivado-solo-de-frame-0 por una fracción de pixel en un frame
intermedio; con la escala corregida (1.16x/1.03x en vez de 1.52x/
1.11x) ambos packs recompilan limpios bajo esta verificación.

`VISUAL_SCALE` (DEC-076) sube de `1.0` a `1.30` para AMBAS variantes
-- QA manual: "Frin feels too small, should be comparable to Bunny/
Nidir." Medido con el bounding box de contenido visible del pack
compilado: macho pasa de ~85×128pt a ~111×167pt, hembra de ~91×138pt
a ~118×179pt -- ambos ahora comparables a Bunny (~114×159pt) y Nidir
(~154×176pt, ver `docs/NIDIR_CONTENT.md` §21).

## 8. Limitaciones honestas

- El fps de reproducción (8.33 para las 4 animaciones) asume la misma
  configuración de Ludo.ai que Nidir/Bunny ya documentan ("3 segundos,
  Max Frames 25") — no confirmada por separado para estos exports
  puntuales.
- Sin `provenance.json` (igual que Nidir/Bunny) — el contrato pide
  `DESCRIPTION.txt` (rasgos físicos), no procedencia.
- Sin hover propio — explícitamente fuera de alcance hasta que el
  owner lo defina.
- El rest-delay de 12s (§9, unificado con Bunny/Nidir, DEC-084) es un
  valor de producto pedido explícitamente, no un número confirmado por
  el owner — trivial de ajustar.
- `visual_scale=1.30` (§7) es un candidato de QA, no una cifra
  definitiva del owner -- trivial de reajustar (un único número en
  `tools/generate_frin_pack.py`).
- No se pudo probar en vivo el caso "click mientras lying" con un
  mecanismo DEV sintético (no existe uno con delay) -- cubierto por
  `tests/StatefulBehaviorTest.cpp::LyingClickTransitionsToSeatedViaLieToSit`,
  no por captura de pantalla directa.

## 9. Pasada de resolución de renderer (Block 05, cuarta pasada): renderer software en macOS, rest-delay a 12s

QA manual real e interactiva del owner en su propia máquina macOS
aisló la corrupción visual restante de sit-to-lie/lying/lie-to-sit a
la etapa de PRESENTACIÓN (el driver de renderer acelerado que macOS
elige por default), no al contenido/pipeline de Frin -- el driver de
software de SDL renderiza los MISMOS assets de Frin correctamente. Ver
`docs/DECISION_LOG.md` DEC-083 para el mecanismo completo
(`platform::RendererPolicy`) y la evidencia. Ningún PNG/frame/manifest
de Frin cambió por esta causa -- el pipeline de contenido de Frin
queda exonerado por completo.

`REST_DELAY_SECONDS` pasa de 15s a **12s** (DEC-084) -- mismo
intervalo unificado que Bunny/Nidir.

**Verificación real, con el fix de DEC-085 aplicado** (esa misma
validación encontró un segundo bug -- `SDL_SetWindowShape()`
corrompiendo el software renderer -- antes de poder confirmar esto;
ver DEC-085 para el mecanismo y el fix): volcado del backbuffer real
(`SDL_RenderReadPixels`, no una captura de pantalla) de una animación
completa de 28 frames de Frin macho (`seated` -> `sit_to_lie` en curso
-> `lying`, disparada por el rest-delay real de 12s) bajo el driver
"software" -- **0 de 28 frames con la corrupción de silueta blanca**,
colores correctos en todos. Frin hembra comparte el mismo pipeline de
contenido y el mismo código de runtime que macho (ninguno de los dos
tiene una ruta de renderizado separada) pero no se relanzó por
separado en esta pasada -- no verificado en vivo, riesgo bajo dado que
no hay ninguna diferencia de código entre variantes.
