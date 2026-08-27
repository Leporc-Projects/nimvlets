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

## 8. Corrección post-QA (Block 05, pasada de estabilización): continuidad de punta, rest delay a 10s, visual_scale re-derivada

QA manual del owner tras §7: la corrupción visual grande ya no está, y
las animaciones se ven bien. Quedaban dos cosas chicas, las dos en las
PUNTAS de las transiciones de postura:

> `sit_to_lie` termina visualmente y, al entrar a la base de `lying`,
> el lobo salta un poco hacia ARRIBA; `lie_to_sit` termina y, al entrar
> a la base de `seated`, salta un poco hacia ABAJO. En las dos
> variantes.

### 8.1 Lo que se midió antes de tocar nada

Comparando los frames COMPILADOS que el runtime realmente muestra
(vía `tools/read_pet_pack.py`, nuevo en esta pasada), macho, dirección
`right`:

| par comparado | dims | bbox | suma de alpha | delta |
|---|---|---|---|---|
| `sit_to_lie` frame 24 vs. base de `lying` | iguales | **idéntico** (178×125) | **idéntica** | `dx=-9, dy=-62` |
| `sit_to_lie` frame 0 vs. base de `seated` | iguales | idéntico | idéntica | pixel a pixel igual |
| `lie_to_sit` frame 24 vs. base de `seated` | iguales | 155×230 vs 155×233 | +1.5% | `dx=-11, dy=+52` |

La primera fila es la prueba: **mismo bounding box, misma suma de
alpha, desplazados**. Son literalmente los mismos pixeles puestos en
dos lugares distintos — o sea un problema de COLOCACIÓN del compilador,
no de escala ni de contenido. (Y tiene sentido: la base de `lying`
REFERENCIA el frame final de `sit_to_lie`, no es un asset aparte.)

### 8.2 La corrección

Ver `docs/DECISION_LOG.md` DEC-087 para el diseño. En una línea: la
colocación ahora se HEREDA por archivo compartido en vez de
recalcularse — la misma idea que DEC-075 ya aplicaba a la escala,
extendida a la traslación.

Resultado, ambas variantes, ambas direcciones:

| par comparado | antes | después |
|---|---|---|
| `sit_to_lie` último vs. base de `lying` | `dy` -62 (M) / -60 (F) | **idéntico pixel a pixel** |
| `sit_to_lie` primero vs. base de `seated` | idéntico | idéntico (sin cambios) |
| `lie_to_sit` último vs. base de `seated` | `dy` +52 (M) / +54 (F) | centroide a <1.2px |

### 8.3 Lo que NO se arregló, y por qué

El ARRANQUE de `lie_to_sit` ahora difiere de la base de `lying` en
(25.5, 10.7)px en macho y (3.0, 5.8)px en hembra.

Eso es **arte fuente**, no compilador. Medido sobre los PNG nativos,
sin compilador de por medio, comparando cuánto mueve cada export al
lobo dentro de su propio frame (centro de bbox, frame 0 -> frame 24):

| variante | `sit_to_lie` mueve | `lie_to_sit` mueve | suma (0 si fueran reversas exactas) |
|---|---|---|---|
| macho | (-23, +158) | (-23, -118) | **(-46, +40)** |
| hembra | (+4.5, +122) | (-7.5, -109) | (-3, +13) |

El macho se acuesta desplazándose 23px hacia un lado y se levanta
desplazándose 23px hacia **el mismo** lado — si fueran reversas, el
segundo debería ir para el otro. La hembra casi coincide, y por eso su
residual es chico.

Ningún offset rígido puede satisfacer las dos puntas de un desacuerdo
así. Se eligió que quede bien la punta donde el personaje QUEDA QUIETO
(entrar a la pose base estable), porque ahí un salto se ve; el arranque
de una transición ya está en movimiento y lo disimula. **Es una
pregunta de contenido para el owner** (¿re-exportar `lie_to_sit` del
macho como reversa real de `sit_to_lie`?), no deuda de compilador.

### 8.4 Efecto secundario: canvas más ajustado, `visual_scale` re-derivada

Al no exigir `lying` su propio centrado, el canvas de trabajo
compartido pierde margen transparente muerto:

| variante | canvas de trabajo | canvas lógico |
|---|---|---|
| macho | 543×815 -> **546×657** | 117×176 -> **146×176** |
| hembra | 496×653 -> **495×531** | 134×176 -> **164×176** |

Eso haría que Frin se viera ~24% más grande a `visual_scale` constante,
así que la escala se **re-deriva** 1.30 -> **1.05** para dejar el tamaño
en pantalla aprobado por DEC-076 exactamente igual (la aritmética está
escrita en el comentario de `VISUAL_SCALE` en
`tools/generate_frin_pack.py`). Efectivo en pantalla: macho 153×185,
hembra 172×185.

De yapa: con el canvas más ajustado el tope de
`runtime_max_frame_dimension` (320) recorta menos — el ratio de
downscale del macho pasa de 0.393 a 0.487, o sea **~24% más resolución
efectiva** para el mismo tamaño en pantalla.

### 8.5 Rest delay: 10 segundos

`REST_DELAY_SECONDS` pasa de 12.0 a **10.0** (pedido de producto
explícito — ver DEC-089). Ya **no** está unificado con el intervalo
ambient de Bunny/Nidir, que se queda en 12s: son dos ritmos distintos a
propósito ("cada cuánto hace un gesto ocioso" vs. "cuánto tarda en
cambiar de postura"). El dwell de hover se queda en 0.5s.

Semánticas de reset sin cambios: click, hover, drag y la terminación de
cualquier acción reinician la cuenta. `lying` sigue sin
`ambient_actions` — nunca hay timer armado mientras el lobo está
acostado.

### 8.6 Tests de regresión

`tools/test_asset_pipeline.py`:
- `CompiledFrinEndpointContinuityTest` — decodifica los packs REALES que
  se envían y compara las dos puntas de cada transición, macho/hembra ×
  right/left. La igualdad pixel a pixel se exige donde el contenido
  declara el mismo asset de punta; donde el export es independiente
  (`lie_to_sit`) se exige registración del centro de contenido.
- `TransitionEndpointContinuityTest` — el mecanismo sobre grafos
  sintéticos, **con control negativo**: sin reuso de archivo las dos
  colocaciones sí divergen, así que el test positivo no puede pasar por
  casualidad.
- `ContentTimingPolicyTest` / `CompiledClickScaleTest` — 10s de Frin,
  12s de Bunny/Nidir, y desvío de tamaño de cada acción contra la base
  de su estado.

## 9. Pasada de pulido final (Block 05): inversión de dirección runtime, extensión de continuidad a howl/tail_greet, hover 0.2s

Continuación directa de §8 -- misma pasada de pulido final que también
tocó Bunny (ver `docs/BUNNY_CONTENT.md`) y el motor (hover dwell). Tres
cambios independientes, cada uno con su propio DEC:

### 9.1 Inversión de dirección runtime (DEC-091)

Pedido de producto explícito: TODO lo que se veía corriendo con
`Direction::kRight` pasa a verse con `Direction::kLeft`, y viceversa --
las DOS variantes, TODO contenido direccional (pose base sentada, pose
base acostada, `sit_to_lie`, `lie_to_sit`, `howl`, `tail_greet`).

Las carpetas de import (`animations/<anim>/{left,right}/frames/`) **no
se tocan** -- siguen registrando la orientación real que el owner
exportó (PROVENANCE). Solo cambia, en un único lugar
(`_invert_runtime_direction()` dentro de
`tools/generate_frin_pack.py`'s `entries_for()`), qué carpeta alimenta
el slot runtime `kRight` vs. el slot runtime `kLeft` del pack
compilado.

Invariante resultante, verificado, variante-independiente: el slot
runtime `kRight` ahora SIEMPRE lee de la carpeta física
`.../left/frames/`, y `kLeft` SIEMPRE de `.../right/frames/` -- para
las dos variantes, sin excepción (`§1` de arriba documenta que macho es
canónicamente "left" y hembra "right"; después de la inversión esa
distinción deja de importar para efectos de qué se ve en cada slot
runtime -- ambas quedan mapeadas de la misma forma).

Ambos packs regenerados. La continuidad de `sit_to_lie -> lying` de
§5/§7/§8 (pixel-idéntica) se preservó exacta en las dos direcciones tras
la inversión -- esperado, porque el mecanismo de containment opera
sobre el mismo `entries_for()` ya invertido.

### 9.2 Continuidad de punta extendida a `howl`/`tail_greet` (DEC-092/093)

§8 resolvió la continuidad de las transiciones que CAMBIAN de estado
(`sit_to_lie`/`lie_to_sit`). QA manual encontró el mismo síntoma, más
chico, en el click SENTADO: "seated click actions appear slightly
wider/larger than the seated base."

Medido (centroide de alpha, último frame de la acción contra la base de
"seated", packs compilados reales, ANTES de tocar nada):

| acción | delta LAST-vs-BASE |
|---|---|
| macho `howl` | 0.83-0.85px |
| macho `tail_greet` | 0.46-0.56px |
| hembra `howl` | 0.81px |
| hembra `tail_greet` | 0.66px |

Ninguno es corrupción -- todos sub-1px sobre frames nativos de
~350-550px -- pero son reales y consistentes con el reporte: `howl` y
`tail_greet` son self-loop (`target_state_id == "seated"`, el propio
estado), así que NINGÚN mecanismo protegía su punta de regreso; solo se
anclaban por su propio frame 0.

Corrección: `align_endpoint_to_target_base: true` en `howl` y
`tail_greet` (nuevo parámetro del helper `action()` en
`tools/generate_frin_pack.py`, `sit_to_lie`/`lie_to_sit` lo dejan en
`False` -- no hace diferencia para ellas, ya se registran vía
`changes_state` sin condición). Ver `docs/DECISION_LOG.md` DEC-092 para
el mecanismo genérico (compartido con Bunny, sin ninguna rama de código
por-pet) y DEC-093 para el punto de anclaje (centroide de alpha, no
bbox -- un refinamiento medido, no supuesto).

Resultado tras recompilar:

| acción | antes | después |
|---|---|---|
| macho `howl` | 0.83-0.85px | **0.59-0.64px** |
| macho `tail_greet` | 0.46-0.56px | 0.56px *(sin cambio en una dirección -- redondeo)* |
| hembra `howl` | 0.81px | **0.32-0.81px** *(mejora en una dirección)* |
| hembra `tail_greet` | 0.66px | 0.66px *(sin cambio)* |

Ningún caso empeora. Efecto colateral honesto sobre `lie_to_sit`
(§8, ya registrada desde antes, comparte la misma rama de código): al
cambiar el punto de anclaje de esa rama (DEC-093), su número TAMBIÉN se
recalculó -- mejora en dos de los cuatro casos (macho-derecha 1.21px ->
0.40px, hembra-derecha 1.20px -> 0.25px) y empeora levemente en los
otros dos (macho-izquierda 0.44px -> 0.87px, hembra-izquierda 0.23px ->
0.86px). Ningún caso supera 1px -- muy por debajo de lo perceptible, y
muy por debajo del ~53px que existía antes de §8. Ver DEC-093 para por
qué se aceptó ese trade-off en vez de mantener dos convenciones de
anclaje distintas.

Canvas de trabajo: efecto mínimo (macho 546x657 -> 548x657 tras sumar
la inversión de §9.1; hembra 495x531 -> 494x531). `visual_scale` se
queda en 1.05 (§8) -- el cambio de canvas es demasiado chico para
justificar re-derivarla de nuevo.

### 9.3 Hover dwell: 0.5s -> 0.2s (DEC-090)

Sin relación específica con Frin -- pedido de producto que aplica a
los cuatro pets por igual (el mecanismo vive en `core::
kDefaultHoverDwellSeconds`, no en ningún manifest por-pet). Frin sigue
sin definir hover propio (`hover_uses_ambient_actions: false`,
`hover_actions: []` en las dos variantes) -- sin cambios.

### 9.4 Tests de regresión

`tools/test_asset_pipeline.py`:
- `FrinRuntimeDirectionInversionTest` — el invariante de §9.1 contra el
  pack real (right<->left carpeta física), macho + hembra, base + las 4
  animaciones, más una tabla de verdad algebraica aislada.
- `CompiledSelfLoopEndpointContinuityTest` — el delta de §9.2 contra
  los packs reales, `howl`/`tail_greet` macho y hembra, con guard
  explícito de que el flag esté realmente declarado en el manifest.
- `AlignEndpointToTargetBaseTest` — el mecanismo genérico (compartido
  con Bunny) contra un fixture propio compilado de verdad, no un pet
  real -- confirma placement/no-op/escala-nunca-tocada de forma
  aislada.

## 10. Pasada de continuidad de frontera (Block 05): escala del retorno, dos-puntas para lie_to_sit, timing

Continuación directa de §9, misma familia de correcciones -- ver
`docs/DECISION_LOG.md` DEC-094/095/096 para el detalle completo.

### 10.1 Timing (DEC-094)

`REST_DELAY_SECONDS` vuelve de 10.0 (§9.3/DEC-089) a **12.0** --
unificado otra vez con Bunny/Nidir por pedido de producto explícito.
Hover dwell (motor, no específico de Frin) sube de 0.2s a **0.4s**.

### 10.2 Escala de `howl`/`tail_greet` derivada del retorno, no del arranque (DEC-095)

QA manual: "Frin still feels slightly 'fatter' during animations than
while in the stable pose" -- distinto del salto de COLOCACIÓN que §9.2
ya había resuelto. `align_endpoint_to_target_base` (el mismo flag de
§9.2) ahora gobierna TAMBIÉN la escala: se deriva del ÚLTIMO frame
contra la base, no del primero.

| acción | scale drift antes (último frame vs base) | después |
|---|---|---|
| macho `howl` | 0.26% | **0.002%** |
| macho `tail_greet` | 0.33% | **0.05%** |
| hembra `tail_greet` | 0.01% | 0.03% (sigue bueno) |
| hembra `howl` | 0.13% | 0.39% *(ruido de resampleo -- ver DEC-095, `content_scale` sigue en 1.000000 exacto en ambos casos; el canvas compartido creció levemente por §10.3 y cambió el ratio de downscale de runtime aplicado a TODO el pet)* |

Todos los casos quedan bajo el 1% de tolerancia del test de regresión.

### 10.3 `lie_to_sit`: registro de dos puntas (DEC-096) — **SUPERSEDED por §11**

> **Lo que esta sub-sección describe ya no se envía.** La interpolación
> de traslación por frame fue rechazada por QA (root-motion artificial
> visible). Se conserva como registro; el comportamiento actual está en
> §11.

QA manual, el hallazgo central de esta pasada: "when lying and the
owner clicks to stand up, the stable lying pose immediately jumps in
position when the FIRST lie_to_sit frame appears". §9/§8 (pasadas
anteriores) solo habían anclado la punta FINAL de `lie_to_sit`; la
INICIAL nunca tuvo ningún registro.

`align_transition_both_endpoints: true` en `lie_to_sit` (macho y
hembra) -- registra TAMBIÉN el primer frame contra `lying_base`. Medido
ANTES de decidir cómo resolverlo: las dos puntas divergen 10-50px entre
sí (según variante/dirección), muy por encima de cualquier tolerancia
de redondeo -- un solo transform constante NO alcanzaba, así que el
compilador interpola LINEALMENTE la traslación (nunca la escala) a lo
largo de los 25 frames reales.

| medición | antes | después |
|---|---|---|
| frame 0 vs `lying_base` | sin registro (el salto reportado) | **0.80-1.16px** |
| último frame vs `seated_base` | 0.25-0.87px (ya bueno) | 0.40-0.87px (se mantiene bueno) |

Movimiento verificado suave sobre los 25 frames -- ningún salto grande
escondido en medio del movimiento autorado real. `sit_to_lie` NO se
tocó: sus dos puntas ya son archivos compartidos con las bases
(containment exacto, §5/§7), así que el flag no aplicaría ahí.

Escala de `lie_to_sit`: sin cambios (ver DEC-096) -- las dos puntas
midieron consistentes entre sí (±0.1-0.7% de 1.0), sin evidencia de que
el export cambie de escala, así que se mantiene un único
`content_scale` uniforme derivado del primer frame contra `lying_base`,
como siempre.

Canvas de trabajo: creció levemente para contener el frame en las DOS
posiciones (macho 548x657 -> 575x657; hembra 494x531 -> 492x532) --
`visual_scale` se queda en 1.05, el cambio es demasiado chico para
justificar re-derivarla.

### 10.4 Regresión verificada

- `sit_to_lie` -> `lying`: sigue pixel-idéntico, las dos direcciones,
  las dos variantes.
- Inversión de dirección runtime (§9.1): sin cambios, sigue verificada.
- `assets/dev/nidir_pack.nvpack`: **byte-idéntico** (Nidir nunca activa
  ninguno de los dos flags de esta pasada).

### 10.5 Tests

`tools/test_asset_pipeline.py`: `ReturnEndpointScaleContinuityTest`
(§10.2), `LerpOffsetScheduleTest`/`TwoEndpointFrameOffsetsTest`
(mecanismo puro de §10.3) y `LieToSitTwoEndpointContinuityTest`
(contra los packs reales, incluyendo el chequeo de movimiento suave y
de que `sit_to_lie` NO recibió el flag).

## 11. Pasada de resolución de root-motion (Block 05): se retira la interpolación, `lie_to_sit` se ancla por su arranque

§10.3 había resuelto la continuidad de `lie_to_sit` interpolando la
traslación por frame. **QA manual del owner lo rechazó**: "mientras
Frin se levanta, el sprite entero parece desplazarse por la ventana".

Medido, y es exactamente lo que el owner vio -- comparando el recorrido
del centroide en el pack compilado contra el del arte nativo:

| variante | recorrido CON interpolación | recorrido con transforma rígida (= autorado) | inventado por el compilador |
|---|---|---|---|
| macho | 78.1 px | 62.1 px | **+16.0 px (+26%)** |
| hembra | 72.2 px | 64.7 px | **+7.5 px (+12%)** |

Las métricas de punta de §10.3 eran correctas (~1px en los dos
extremos); lo que faltaba medir era el RECORRIDO. Ver
`docs/DECISION_LOG.md` DEC-097.

### 11.1 Qué se envía ahora

`lie_to_sit` usa `anchor_start_to_source_base: true`: **una** transforma
rígida (una escala uniforme + una traslación constante) que registra su
PRIMER frame contra `lying_base`. El arranque -- el instante exacto del
click, con el lobo todavía quieto -- queda continuo; el residual del
export queda visible al final.

| medición | resultado |
|---|---|
| arranque vs `lying_base` | **0.80-1.13 px** (anclado) |
| final vs `seated_base`, macho | **26.58 / 25.72 px** = ~15 pt en pantalla |
| final vs `seated_base`, hembra | **6.75 / 6.70 px** = ~3.9 pt en pantalla |

### 11.2 Por qué eso es deuda de CONTENIDO, no del compilador

El root-motion del export de `lie_to_sit` no es el inverso del de
`sit_to_lie`. Medido sobre los PNG nativos del macho: `sit_to_lie`
mueve al lobo (-23, +158) px, `lie_to_sit` lo mueve (-23, -118) px. Si
fueran reversas exactas la suma sería (0,0); es **(-46, +40)**. Ninguna
transforma rígida reconcilia eso, y ninguna debería intentarlo
deformando al personaje.

Medido con las dos prioridades de anclaje, el residual es el MISMO
vector -- solo cambia en qué punta cae: 54.08 px nativos (macho),
9.67 px nativos (hembra), en las dos direcciones.

**Recomendación:** regenerar el export de `lie_to_sit` en Ludo,
prioritariamente el del **macho** (~15 pt de salto al asentarse, ~8%
del alto del pet). El de la hembra (~3.9 pt) es tolerable si hace
falta. Fijado como techo en
`CompiledFrinEndpointContinuityTest.test_lie_to_sit_end_residual_is_known_content_debt`
para que no pueda empeorar en silencio.

### 11.3 Escala estricta en el retorno de `howl`/`tail_greet`

`align_endpoint_to_target_base` ahora aplica la escala EXACTA, sin la
tolerancia de 0.5% (DEC-098). Radio RMS del último frame contra la base
sentada, antes -> después:

| acción | antes | después |
|---|---|---|
| hembra `howl` | 1.003907 | **0.999985** |
| hembra `tail_greet` | 1.000298 | **0.998843** |
| macho `howl` | 1.000016 | sin cambio (ya exacto) |
| macho `tail_greet` | 1.000529 | sin cambio (ya exacto) |

### 11.4 Sin cambios

`sit_to_lie` congelado (aprobado por QA, sus dos puntas siguen siendo
pixel-idénticas contra `seated_base`/`lying_base`). Inversión de
dirección runtime (§9.1) intacta. Timings: rest 12s, hover 0.4s.

## 12. Pasada de simplificación geométrica (Block 05): las cuatro animaciones aterrizan en poses exactas, y la saga de `lie_to_sit` termina

### 12.1 El contrato

Cada acción de Frin declara ahora qué pose ESTABLE representa cada una
de sus dos puntas (ver DEC-099 y `docs/ANIMATION_RUNTIME.md` §19):

| acción | primer frame | último frame |
|---|---|---|
| `sit_to_lie` | `seated` | `lying` |
| `lie_to_sit` | `lying` | `seated` |
| `howl` | `seated` | `seated` |
| `tail_greet` | `seated` | `seated` |

El compilador toma esas puntas del ARCHIVO de la base correspondiente,
así que salen idénticas byte a byte al frame que el runtime muestra con
el lobo quieto en ese estado. Verificado sobre los packs compilados
para macho y hembra, en las dos direcciones de runtime: 16 puntas,
igualdad RGBA pixel por pixel, sin tolerancia.

### 12.2 `lie_to_sit`: por qué esto cierra la saga

Historial de este clip: se ancló por su ÚLTIMO frame (saltaba al
arrancar); se interpoló la traslación por frame para cerrar las dos
puntas (QA lo rechazó: el lobo entero derivaba por la ventana,
DEC-096/097); se ancló por su PRIMER frame con una transforma rígida
(el residual se mudó al final, ~26px ≈ 15pt).

El problema estructural era que UNA transforma rígida sólo puede
registrar UNA punta, y este export -- independiente en los dos
extremos -- no cierra contra las dos bases. La salida no fue una
transforma mejor: fue dejar de derivar las puntas de una transforma.
Ahora las dos son exactas, y el desajuste real del export se absorbe
DENTRO del clip, donde el lobo ya está en movimiento, en vez de en el
instante en que queda quieto.

Ese residual interno se mide y se fija en un test
(`LieToSitInternalResidualTest`), con `sit_to_lie` -- cuyas puntas
comparten archivo real y por tanto es exacto por construcción -- como
control de cuánto salto es normal en contenido bueno.

El owner decidió explícitamente NO regenerar animaciones de Ludo, así
que esto queda como deuda de contenido conocida y aceptada, medida en
vez de disimulada.

### 12.3 Un matiz: la colocación de arranque no era una capa de compensación

La primera versión de esta pasada eliminó también el anclaje de
arranque, por considerarlo parte de la complejidad acumulada. Medido,
resultó ser una simplificación de más: el frame 0 sustituido y el frame
1 autorado quedaban a 66.3px (macho) / 61.8px (hembra), y el canvas de
trabajo compartido se inflaba de 657 a 767px de alto, encogiendo a Frin
~14% en pantalla.

La causa es real y no es una tolerancia: la colocación por default
centra el contenido del frame 0 en el ancla compartida, lo que asume
que la pose de arranque del clip vive ahí. Cierto para cualquier pet de
un solo estado; falso para un clip que arranca acostado, porque la pose
acostada hereda la colocación de `sit_to_lie`.

La corrección no fue devolver el flag eliminado, sino DERIVAR la
colocación de la declaración que ya existe: si el frame 0 ES la pose
base del estado X, el clip arranca donde vive esa base.

### 12.4 Límite honesto: `howl` del macho

`howl` es el peor caso de anisotropía de export del proyecto: su
bounding box de contenido nativo es 353x511 contra 395x593 de la pose
sentada, o sea ratios de 1.1190 (W) y 1.1605 (H) -- **3.58% de
desacuerdo entre ejes**. Confirmado por un segundo método independiente
(ajuste de IoU con ejes libres sobre frames compilados: 3.96%, y el IoU
sube de 0.9656 a 0.9851 al soltar los ejes, que es la firma de una
diferencia real de escala por eje y no de pose).

Con la mejor escala uniforme posible, `howl` se muestra ~1.8% más ancho
y ~1.8% más bajo que la pose sentada durante todo el clip. Eso es lo que
queda del "se ve más gordo mientras anima". No se puede quitar sin
escala no uniforme, que deformaría la pose real del aullido. Convivir
con ello o re-exportar son las dos opciones honestas; el owner eligió
no re-exportar.

## 13. Pasada de consistencia visual final (Block 05): aspecto de `howl`, frontera de `lie_to_sit`, color de `tail_greet`

Tres defectos distintos, tres causas distintas, tres correcciones
distintas -- y una secuencia que deliberadamente no se toca.

### 13.1 `howl` se veía más gordo: el export usa otra relación de aspecto

Medido con momentos ponderados por alpha sobre los PNG nativos, en las
dos puntas de reposo (los únicos frames cuya pose se sabe igual a la de
la base):

| secuencia | punta f000 | punta f024 |
|---|---|---|
| macho `howl` | **-3.90%** | **-3.90%** |
| hembra `howl` | -2.13% | -2.05% |
| macho `tail_greet` | +0.20% | +0.21% |
| hembra `tail_greet` | -0.71% | -0.76% |

Que dos frames autorados separados por 3 segundos den EL MISMO valor
(-3.90% contra -3.90%) no puede ser una coincidencia de pose.

**Triangulación, la prueba decisiva.** Frin macho tiene tres exports
independientes de la misma pose sentada de reposo. Entre sí: base ↔
`tail_greet` = **-0.20%** (coinciden), base ↔ `howl` = **+4.05%**,
`tail_greet` ↔ `howl` = **+4.26%**. Dos exports independientes están de
acuerdo y el tercero discrepa con los dos: el anómalo es `howl`.

`howl` declara `match_aspect_to_stable_poses` (DEC-100). Resultado
medido sobre el pack compilado: de +2.7%..+5.5% de "más ancho relativo
a alto" a **media -0.64%, rango [-1.43%, +1.49%]** (macho) y **media
+0.84%** (hembra).

### 13.2 `tail_greet` NO se corrige geométricamente -- es el control negativo

QA dijo que su geometría es la mejor de Frin, y la medición coincide:
su export ya está dentro del 0.2%-0.8% de la pose sentada. Su escala
sigue siendo perfectamente uniforme y no declara ninguna corrección de
aspecto; hay un test que lo fija.

Sus oscilaciones de aspecto en medio del clip (hasta -21%) son la cola
abriéndose: animación real. De paso, eso demuestra por qué la
derivación NUNCA usa frames intermedios -- ahí, medir sería confundir
animación con error.

### 13.3 `lie_to_sit`: el defecto era quietud en el lugar equivocado

QA: cerca del final, el frame autorado está visiblemente en otro sitio
que la pose sentada final, y después aparece la base en el lugar
correcto -- sin que nadie moviera la ventana.

Perfilando el pack frame a frame apareció la forma exacta: el lobo
termina de levantarse y **se queda quieto** (pasos de centroide
<=0.55px desde f017 en el macho, <=1.04px desde f020 en la hembra) pero
quieto a ~26px / ~6px del lugar donde está la pose sentada real, porque
el root-motion de este export no cierra contra el de `sit_to_lie`.
Recién al terminar saltaba. Casi un segundo inmóvil en el lugar
equivocado y después un teletransporte: por eso salta a la vista.

La reparación es de CONTENIDO (`stable_pose_tail_frames`, DEC-101), no
de movimiento: macho 8 frames finales, hembra 5, elegidos del perfil
medido en el primer frame donde el lobo ya llegó Y dejó de moverse, de
modo que ningún frame autorado que todavía se mueva se pierde. Los dos
valores difieren porque los dos exports difieren.

Resultado: ya no hay ningún frame con el lobo inmóvil fuera de lugar
(desplazamiento posterior medido: 0.00px). El desajuste se reubicó a UN
solo paso -- 24.46px (macho) / 5.59px (hembra) -- que ocurre mientras
el lobo todavía se mueve. **La magnitud casi no bajó** (25.85 ->
24.46); lo que cambió es cuándo ocurre.

### 13.4 `tail_greet` se veía más oscuro: viene así del export, y solo en el macho

Medido sobre pixeles interiores (alpha>=250, sin borde antialiaseado):
`tail_greet` del macho está -2.5%/-2.8%/-2.4% (R/G/B) contra la pose
sentada ya EN EL PNG FUENTE. El pipeline de compilación se midió fiel a
+0.15%, así que la causa no es el downscale, ni la compilación, ni el
formato de textura, ni el blending. **Ningún cambio bajo `src/`.**

La hembra está en -0.5%, dentro del ruido y del mismo orden que su
propio `howl`, que nadie reportó: no se le aplica nada.

`match_color_to_stable_poses` (DEC-102) deriva una ganancia RGB
constante -- macho: (1.0265, 1.032, 1.024) -- de la misma
correspondencia de poses estables. Alpha intacto, sin variación entre
frames. Frames de reposo: -2.5% -> +0.2%. Mitad de clip: -3.9%/-5.0%/
-4.1% -> -1.5%/-2.1%/-1.9%; ese resto es sombreado autorado y se
conserva a propósito.

### 13.5 `sit_to_lie` congelado

Sin cambios: su interior autorado no se toca, sus dos puntas siguen
compartiendo archivo real con `seated_base`/`lying_base`, su escala
sigue siendo (1.0, 1.0) y no declara ninguna corrección.

## 14. Pasada de pulido final (Block 05): tamaño del macho +5%, y por qué el corte terminal de `lie_to_sit` ya no puede mejorar más sin re-exportar

### 14.1 Escala visual por-variante

`VISUAL_SCALE` (común a las dos variantes desde DEC-087) se extiende a
un multiplicador por-variante: `VARIANT_VISUAL_SCALE_MULTIPLIER =
{"male": 1.05, "female": 1.0}` (ver DEC-103). Puramente runtime; el
canvas lógico del macho sigue siendo 153x176, idéntico a antes.
Tamaño en pantalla: macho 161x185pt -> **169x194pt** (+5.0% exacto);
hembra sin cambio, 168x185pt.

### 14.2 `lie_to_sit`: re-derivación rigurosa, y un hallazgo honesto

QA tras DEC-101: el salto terminal seguía siendo visible en las dos
variantes, y el criterio de selección anterior ("primer frame ya
detenido") quedó explícitamente invalidado como insuficiente.

Se re-derivó con una búsqueda EXHAUSTIVA sobre los 25 candidatos
posibles, minimizando distancia de centroide a la base sentada sujeto
a que el frame elegido siga en movimiento visible (paso de entrada
>0.5px) -- evita reproducir el patrón "quieto en el lugar equivocado,
después salta" que esta familia de correcciones existe para eliminar.
Implementada dos veces (a mano y como test automatizado que recompila
y reproduce la búsqueda), con resultado idéntico las dos veces.

**El macho no mejora: 3b4fa66 (8 frames de cola) ya era el óptimo.**
Ningún frame de los 25 del clip queda genuinamente más cerca de 24.46px
de la base sentada -- confirmado visualmente con una superposición
alpha (rojo=frame autorado, verde=base sentada): el lobo está
desplazado ~24px hacia un lado de forma consistente en TODA la meseta
final del clip, sin diferencia de escala significativa. Es un PISO
GEOMÉTRICO real de este export: su root-motion no cierra contra
`sit_to_lie` lo suficiente como para que NINGUNA elección de frontera
lo reduzca, dentro de las reglas (sin síntesis, sin interpolación, sin
re-export).

**La hembra sí mejoró: 5 frames -> 4, salto 5.59px -> 5.32px (~4.8%)**,
preservando un frame autorado más.

Se investigó (y se descartó deliberadamente) invertir el anclaje del
clip -- del inicio (`lying_base`) al final (`seated_base`) -- lo que
desplazaría el MISMO residual desde el final del clip hacia el
instante justo después del click. Como el frame 0 ya es exacto bajo
cualquier anclaje (DEC-099), hoy el INICIO de `lie_to_sit` es
perfectamente continuo; cambiar de anclaje introduciría un defecto
nuevo justo ahí, a cambio de resolver uno que el owner no reportó en
ese punto. Mover el defecto no es eliminarlo -- ver DEC-104 para los
números completos de esta opción, documentados por si se prefiere a
futuro.

**Conclusión honesta:** para eliminar (no solo minimizar) el defecto
del macho hace falta un re-export de `lie_to_sit` en Ludo cuyo
root-motion cierre contra `sit_to_lie` -- la recomendación de DEC-101
sigue en pie. La hembra queda con una mejora real; su residual
(~3.2pt en un pet de 185pt) es probablemente ya imperceptible.

## 15. Pasada de corrección posicional final (Block 05): `lie_to_sit` traslada en vez de sustituir

QA sobre §14: el salto terminal seguía siendo visible, pero esta vez la
observación fue más específica -- cerca del final, el lobo YA está
casi/completamente sentado, solo desplazado unos pixeles de la
posición correcta. La petición: tomar ese frame autorado real y
componerlo unos pixeles más allá, no reemplazarlo por una copia
congelada de la base.

### 15.1 El mecanismo

`terminal_rigid_translation` (DEC-105): desde el frame K declarado, se
suma un `(dx, dy)` CONSTANTE a la colocación de cada frame -- los
mismos pixeles autorados, en otro lugar del canvas. K sigue siendo el
mismo que §14 ya había re-derivado (17 macho, 21 hembra): matemáticamente,
anclar un frame contra la base sentada produce el mismo salto en el
borde anterior que sustituirlo directamente, así que el K óptimo no
cambia -- solo QUÉ pixeles se muestran desde ahí en adelante.

`dx`/`dy` se midieron por separado para las dos direcciones runtime
sobre el pack ya compilado, sin asumir simetría de espejo:

| variante | right (dx, dy) | left (dx, dy) |
|---|---|---|
| macho | (+23.44, -6.33) | (-23.52, -6.33) |
| hembra | (-1.71, -5.11) | (+1.50, -5.10) |

### 15.2 El resultado, medido con honestidad

El salto ÚNICO (borde K-1 -> K) no se redujo -- sigue siendo ~24px
(macho) / ~5.3px (hembra), el mismo piso geométrico de §14, y no podía
ser distinto: es el mismo argumento de posición. Lo que cambió es qué
se ve DESPUÉS de ese salto: antes, 7-8 frames (macho) idénticos entre
sí, una copia congelada de la base; ahora, esos mismos frames muestran
la micro-variación autorada real del export (pasos de 0.07-0.56px), ya
visiblemente cerca de la posición correcta (0.5-2.1px, no 24-25px).

Antes: [movimiento real] -> [salto] -> [pose estática repetida].
Ahora: [movimiento real] -> [salto] -> [movimiento real minúsculo, ya
en la posición correcta] -> [base exacta].

### 15.3 Sin re-export, otra vez

Como en §13/§14, no se tocó ningún PNG fuente. La traducción es
puramente de composición: los mismos bytes RGBA, compuestos en otra
posición del mismo canvas de trabajo compartido -- verificado por test
(`test_frames_from_k_preserve_rgba_content_translated_only`): el
multiconjunto de valores RGBA de cada frame traducido es idéntico al
de la versión sin traducir.
