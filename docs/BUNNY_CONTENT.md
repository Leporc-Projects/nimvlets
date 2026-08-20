# Nimvlets — Bunny: migración a assets reales (Block 04.3)

Bunny fue el fixture de QA de Block 01 (una ilustración real usada
solo para validar hit-testing/click-through sobre alpha real, nunca
pensada como contenido final — ver `docs/DECISION_LOG.md` DEC-018) y
siguió siendo la entrada "dev"/default del catálogo de forma sintética
(Block 02, `tools/generate_bunny_dev_pack.py`: pose estática +
squash/stretch/lean derivados por transformaciones de pixeles simples,
sin arte real de animación) hasta este bloque.

Block 04.3 migra a Bunny a contenido real de producción: el owner
exportó su idle y click reales (`local_imports/bunny/`, staging
temporal nunca commiteado), usando el MISMO pipeline genérico que
Block 04.2 construyó para Nidir — sin ningún cambio de arquitectura,
sin ninguna rama de código específica de Bunny. Este documento describe
esa migración; ver `docs/NIDIR_CONTENT.md` para el diseño completo del
pipeline en sí (import, normalización, direcciones, canvas de trabajo
compartido), que este documento no repite.

## 1. Por qué Bunny es una segunda validación real, no solo una migración

Todo lo que Block 04.2/04.3 construyeron para Nidir era, por diseño,
genérico -- pero hasta este bloque solo se había EJERCITADO con un
único pet real. Importar a Bunny con el mismo pipeline, sin tocar
ningún código de runtime ni ninguna política de `tools/`, es la prueba
real de esa genericidad:

- `tools/compile_pet_pack.py`'s `normalize_visual_scale` y
  `prep_dev_sprite.compute_frame_normalization_plan()` -- sin cambios
  -- procesaron a Bunny correctamente, e incluso ejercitaron por
  primera vez el caso `content_scale != 1.0` (ver §3): el idle y el
  click de Bunny NO tienen el personaje al mismo tamaño absoluto en
  pixeles (a diferencia de Nidir, donde coincidía casi exacto), así
  que la política tuvo que reescalar de verdad, no solo posicionar.
- `content::ResolveIdleAnimation()`/etc. y el formato "NVPACK1" de tres
  secciones opcionales (Block 04.2) -- sin cambios -- resolvieron
  correctamente la dirección canónica de Bunny, que es "left" (no
  "right" como Nidir) -- ver §2 para el detalle de por qué esto
  importaba tenerlo bien wireado.
- `SpikeApp::AttachAllTextures()`/`ReleaseAllTextures()` (ya corregidas
  en Block 04.2 tercera pasada) cubrieron correctamente el pack de
  Bunny sin ningún ajuste.

## 2. Dirección canónica: "left", no "right" — un detalle crítico

El export real de Bunny nombra su propia dirección canónica "left"
(`bunny-idle-left`, `bunny-click-left`) -- a diferencia de Nidir, cuyo
export venía nombrado "right". Esto importa porque
`content::ResolveIdleAnimation()`/`ResolveClickReaction()`/
`ResolvePassiveAction()` (sin cambios desde Block 04.2) tienen una
regla fija: el campo CANÓNICO de `PetDefinition` (`idle`,
`clickReaction`, `passiveActions[i]`, sin ningún override) se resuelve
SIEMPRE para `Direction::kRight` -- un override direccional solo se
consulta para direcciones DISTINTAS de `kRight`. `kRight` es también
la dirección por defecto de `AnimationController` al construirse.

Para Nidir esto funcionó "gratis" porque su export real YA era
"right". Para Bunny, wireado ingenuamente (poner los frames reales
"left" directamente en el campo canónico) habría sido un bug real: el
pet se mostraría con el arte "left" real cada vez que
`direction_ == kRight` (el estado inicial, y cualquier mitad derecha
de pantalla), exactamente al revés de lo esperado. `tools/generate_bunny_pack.py`
wirea esto correctamente: el campo canónico (`idle`, `click_reaction`,
`passive_actions[0]`) usa los frames DERIVADOS (espejados, "right"), y
el override `"direction": "left"` usa los frames REALES importados —
inverso de dónde vive cada uno en disco
(`assets/source/nimvlets/bunny/animations/*/left/` es la fuente real,
`.../right/` es la derivada), pero es lo que el runtime necesita.
Verificado contra el binario real (smoke test con
`NIMVLETS_DEV_DIRECTION_TEST_COUNT`) y visualmente (frames "left" real
y "right" derivado exportados e inspeccionados lado a lado -- son
espejos horizontales exactos entre sí, confirmando que el espejado y
el wireado de dirección son correctos juntos).

## 3. Import real: estructura, frame counts, y normalización de contenido

Estructura del staging (`local_imports/bunny/`), inspeccionada antes
de asumir nada -- igual que el export de click-fire de Nidir, traía la
carpeta anidada extra que el brief advertía podía existir:

```
local_imports/bunny/
  Bunny.png                                            (master, 1254x1254)
  bunny-idle-left/Preserve-Bunny-s-exa/frame_000..024.png   (25 frames, 384x537)
  bunny-idle-left-spritesheet/Preserve-Bunny-s-exa.png      (1920x2685 = 5x5 de 384x537)
  bunny-click-left/Preserve-Bunny-s-exa/frame_000..024.png  (25 frames, 419x587)
  bunny-click-left-spritesheet/Preserve-Bunny-s-exa.png     (2095x2935 = 5x5 de 419x587)
```

**Frame count real: 25 para idle, 25 para click** -- no se forzó
ningún número, es exactamente lo que ambos exports trajeron (misma
convención de reporte honesto que Nidir). Ambos ya venían con nombres
deterministas (`frame_NNN.png`), sin necesidad de normalización — los
25+25 frames se copiaron (nunca movidos) tal cual a
`assets/source/nimvlets/bunny/animations/{idle,click_reaction}/left/`,
verificados por checksum MD5 contra el staging antes de eliminar
`local_imports/bunny/`.

**Hallazgo de encuadre, honesto:** a diferencia de Nidir (donde idle y
click_reaction tenían al personaje al mismo tamaño absoluto en pixeles
casi exacto, `content_scale` terminaba en 1.0), Bunny's `click_reaction`
SÍ tiene al personaje dibujado a una escala distinta dentro de su
propio frame nativo que idle -- la política de canvas de trabajo
compartido (§12 de `docs/NIDIR_CONTENT.md`) lo detectó y corrigió
automáticamente:

```
idle:                 content_scale=1.0000  offset=(27,24)
idle (right, derivado): content_scale=1.0000  offset=(17,24)
click_reaction:        content_scale=0.9586  offset=(26,0)
click_reaction (right): content_scale=0.9586  offset=(0,0)
canvas de trabajo compartido: 428x563
```

`click_reaction` se reescala ~4.1% (a 0.9586x) para que el personaje
ocupe el mismo tamaño absoluto que en idle -- exactamente el
comportamiento que la política fue diseñada para producir, ahora
confirmado en un caso real donde el factor NO es 1.0 (Nidir nunca
ejercitó esta rama del código con datos reales). Sin recorte de
contenido en ningún caso (mismo invariante que Nidir).

**Clipping:** revisado con el mismo método que Nidir (bounding box
unión de contenido a través de toda la secuencia). El patrón es mucho
más leve que el de Nidir: el borde INFERIOR se toca en las 25 frames
de idle (consistente en cada una -- el personaje apoyado en el "piso"
del canvas, un encuadre deliberado, no un defecto) y solo 4/25 frames
de idle y 2/25 de click tocan además algún borde lateral/superior
(movimiento normal de animación -- orejas/postura, no un efecto que se
sale del canvas como el fuego de Nidir). No se identificó ningún
hallazgo de clipping real digno de reportarle al owner para este
export.

## 3.1 Corrección: el clipping lateral SÍ es real (Block 04.3, corrección post-QA)

**La conclusión de §3 arriba era demasiado apresurada y quedó
demostrada incorrecta.** El owner reportó en QA manual que las
animaciones de Bunny "se ven mal", con pixeles que parecen
desaparecer durante la reproducción. La investigación de esta
corrección volvió a inspeccionar el bounding box unión reportado en
§3 -- esta vez renderizando y observando DIRECTAMENTE, no solo
contando cuántos frames tocan qué borde -- y encontró que el conteo
de §3 estaba técnicamente correcto pero su interpretación ("movimiento
normal de animación... no un efecto que se sale del canvas") no lo
estaba: `idle/left/frame_019.png` (y el tramo consecutivo 018-021)
muestra la punta de la oreja izquierda del personaje CORTADA
exactamente en `minx=0` del frame nativo 384×537 -- un borde
plano/antinatural donde debería afinarse hasta un punto, el mismo
patrón cualitativo que ya está documentado para el fuego de Nidir
(§12 de `docs/NIDIR_CONTENT.md`), no un movimiento de postura
inofensivo. `click_reaction/left` muestra el mismo patrón en su frame
12 (de 9/25 frames que tocan algún borde lateral en total).

**Esto NO es corregible por código** -- no existe forma de sintetizar
pixeles que nunca se exportaron. Si el owner quiere eliminar este
recorte, hace falta un nuevo export con más margen alrededor del
personaje. Documentado acá honestamente, seguido, mirando hacia
adelante, del mismo estándar que ya aplica `docs/NIDIR_CONTENT.md`
para su propio hallazgo de clipping: no se "maquilla" ni se omite.

**Por qué la primera pasada lo descartó:** el análisis original
resumió el hallazgo a "cuántos frames tocan cuántos bordes" sin
efectivamente RENDERIZAR y mirar ninguno de esos frames marcados --
un conteo de bounding boxes por sí solo no distingue "una oreja que
se mueve dentro del frame, tocando el borde de forma incidental,
plausible" de "una oreja genuinamente cortada en un borde plano". La
lección para el resto de este pipeline: un hallazgo de clipping
reportado solo por conteo de bounding box debe confirmarse
visualmente (renderizar el frame marcado y mirarlo) antes de
descartarlo como inofensivo -- no basta con el número.

## 4. Manifest: solo lo que existe hoy

Por instrucción explícita del brief de este bloque, `tools/generate_bunny_pack.py`
integra ÚNICAMENTE lo que el export real de hoy trae:

- `idle`: pose base ESTÁTICA (un solo frame) + UN `passive_action`
  (la secuencia completa de 25 frames, one-shot, esporádica) -- misma
  semántica exacta que Nidir (Block 04.2, segunda pasada corregida):
  nunca un loop continuo.
- `click_reaction`: 25 frames reales, one-shot, vuelve a la pose base
  al terminar.
- `passive_interval_seconds`: 300.0 (el default de `PetDefinition`,
  sin cambios -- este bloque solo pidió cambiar el de Nidir a 60s, no
  el de Bunny).

**Deliberadamente NO implementado** (diferido a un futuro bloque de
interacción, después de que exista el arte nuevo, por instrucción
explícita del brief): una segunda animación de idle, el comportamiento
ponderado 70/30 entre dos idles, un disparador por hover. El esquema
de `content::PetDefinition::passiveActions` ya soporta una LISTA
arbitraria -- una segunda entrada real, cuando exista el arte, se
agrega ahí sin ningún cambio de arquitectura (mismo patrón "genérico,
no C++ por pet" que este bloque demuestra en todo lo demás). Ver
`docs/DECISION_LOG.md` para el registro de esta decisión de alcance.

## 5. Identidad de catálogo: migración sin romper contratos

`id` se mantiene como `"bunny_dev"` (NO se renombra a `"bunny"`) y
`assets/dev/bunny_pack.nvpack` se mantiene como el mismo path exacto
que el fixture sintético anterior -- así, cualquier estado persistido
existente (`persistence::AppState::activePetId == "bunny_dev"`,
`assets/dev/pet_catalog_manifest.json`'s `pet_id`/`pack_path`) sigue
resolviendo correctamente sin ningún cambio de código, aunque el
CONTENIDO real del pack ya no sea sintético. `display_name` sí se
actualizó (de `"Bunny (dev fixture)"` a `"Bunny"`, tanto en el pack
como en el catálogo) -- es solo una etiqueta legible, no una clave de
identidad, y ya no era honesto llamarlo "fixture" con contenido real.
`is_default: true` de Bunny en el catálogo no cambió -- sigue siendo el
default, sin que este bloque haya tenido que decidir nada al respecto.

`tools/generate_bunny_dev_pack.py` (el generador sintético anterior)
se conserva sin modificar como artefacto histórico, con una advertencia
explícita agregada a su docstring: correrlo contra el estado actual
del repositorio SOBRESCRIBIRÍA el pack real compilado con el contenido
sintético anterior -- un riesgo real que vale la pena documentar
prominentemente, no solo confiar en que nadie lo va a correr por
accidente.

## 6. Verificación

- Carga real contra el binario compilado: `pet 'bunny_dev' (Bunny)
  ready — 134x176 canvas...` (con el factor de tamaño +10% vigente
  desde la corrección post-QA de este bloque, ver §10 -- 134x176 es
  el resultado real derivado del canvas de trabajo 428x563 de Bunny,
  no un valor hardcodeado; era 128x168 con el candidato +5% original,
  ver §16 de `docs/NIDIR_CONTENT.md`).
- Cambios de dirección (`NIMVLETS_DEV_DIRECTION_TEST_COUNT`) y click
  (`NIMVLETS_DEV_CLICK_TEST_COUNT`) contra el binario real: sin
  errores, sin advertencias de "no attached texture" (la cobertura de
  `AttachAllTextures()` corregida en Block 04.2 cubre a Bunny sin
  ningún ajuste).
- 500 switches automatizados alternando Bunny/Nidir
  (`NIMVLETS_DEV_SWITCH_TEST_COUNT=500`): 500/500 exitosos, RSS estable
  tras completar la ráfaga, sin crecimiento -- confirma que el
  mecanismo de switching (Block 04, sin cambios) sigue funcionando
  correctamente con contenido real en ambos pets.
- Determinismo: re-correr `tools/generate_bunny_pack.py` contra los
  mismos frames importados produce un `.nvpack` byte-idéntico.

## 7. Limitaciones honestas

- El fps de reproducción (idle y click, ambos ~8.33fps) se derivó
  asumiendo la misma configuración de Ludo.ai que Nidir ("3 segundos,
  Max Frames 25") -- el owner no confirmó por separado la duración de
  generación de ESTE export específico de Bunny. Documentado como
  suposición explícita, no un dato medido de forma independiente
  (mismo patrón que Nidir, ver `docs/NIDIR_CONTENT.md`).
- Sin `provenance.json` para Bunny (igual que Nidir) -- el brief pide
  `DESCRIPTION.txt` (rasgos físicos), no procedencia.

## 8. Causa raíz real de "las animaciones se ven mal / pixeles que desaparecen" (Block 04.3, corrección post-QA)

El owner reportó, tras confirmar que el bug de pérdida de partes al
cambiar de dirección ya no ocurre (§15 de `docs/NIDIR_CONTENT.md` --
el mismo fix genérico de doble-present aplica a Bunny sin cambios),
que las animaciones de Bunny igual "se ven mal": pixeles que parecen
desaparecer durante la reproducción. Instrucción explícita: diagnosticar
la causa real, no asumir, y no "maquillar" el problema con un hack
específico de Bunny.

**Método de diagnóstico -- por primera vez con verificación visual
real contra el binario corriendo de verdad.** Hasta esta corrección,
toda la QA de este proyecto se apoyaba en análisis de frames
compilados (leídos con `prep_dev_sprite.read_png_rgba`) e inspección
de código, sin poder ver la ventana real en pantalla. Esta corrección
usó `screencapture` (con la ventana de la app posicionada/
dimensionada de forma conocida, capturas SIEMPRE acotadas a esa
región exacta -- nunca de pantalla completa) para comparar,
pixel-a-pixel, la salida REAL del renderer contra los frames fuente
sin procesar. Esto permitió encontrar DOS causas reales, distintas y
concurrentes:

**Causa 1: clipping genuino en el export fuente** -- ver §3.1 arriba.
No corregible por código; requiere un nuevo export si el owner quiere
eliminarlo.

**Causa 2: nearest-neighbor perdiendo detalle fino en cada downscale
de compilación -- genérica, SÍ corregible.** `tools/compile_pet_pack.py`
usaba `prep_dev_sprite.resize_rgba_nearest()` (un solo punto de
muestreo por pixel destino) para DOS pasos de downscale real que
Bunny es el PRIMER caso en ejercitar con `scale < 1.0` de verdad:

1. La normalización de escala por contenido (`normalize_visual_scale`,
   Block 04.3 §3 arriba) -- Bunny's `click_reaction` tiene
   `content_scale=0.9586`, un downscale real (Nidir siempre tiene
   `content_scale=1.0` exacto en la práctica, así que esta rama nunca
   se había ejercitado con datos reales antes de Bunny).
2. El downscale de `runtime_max_frame_dimension` -- el canvas de
   trabajo de Bunny (428×563) excede el límite (320), así que ESTE
   paso se dispara SIEMPRE, para cada frame de cada animación.

Nearest-neighbor, por mala suerte de exactamente dónde cae su único
punto de muestreo, puede saltarse por completo un detalle fino (p. ej.
un contorno de 1-2px de ancho) que SÍ existe en la fuente --
compuesto al aplicarse DOS VECES seguidas (paso 1 + paso 2) para
Bunny. Esto explica tanto la pérdida de calidad general (aliasing,
bordes dentados) como parte del "pixeles que desaparecen" (una
porción delgada de contenido, ya cerca de un borde recortado por la
Causa 1, desapareciendo del todo).

**Verificado empíricamente, no solo razonado:** downscalear
`idle/left/frame_019` (compuesto a 428×563) a 243×320 (el tamaño real
que produce `runtime_max_frame_dimension=320` sobre ese canvas) con
nearest-neighbor produjo 35,934 pixeles opacos; el mismo downscale con
un filtro de caja (box filter) produjo 36,125 -- ~0.5% más contenido
preservado, con bordes visiblemente menos dentados en comparación
lado a lado.

**El fix, genérico:** nueva función
`prep_dev_sprite.resize_rgba_area_average(width, height, pixels,
target_width, target_height)` -- un box filter determinista: cada
pixel destino promedia TODOS los pixeles fuente en su región mapeada
(misma convención de mapeo proporcional inverso que
`resize_rgba_nearest`, pero sobre un RANGO, no un punto). El promedio
de RGB está ponderado por el alpha de cada pixel fuente (previene que
el ruido de RGB de pixeles totalmente transparentes -- un fenómeno ya
documentado como inofensivo en reposo -- sangre/genere un fringing de
color hacia pixeles vecinos visibles al cruzar un límite de
transparencia durante el downscale); el alpha en sí se promedia sin
ponderar. `tools/compile_pet_pack.py` usa esta función para AMBOS
pasos de downscale de arriba cuando el downscale es real
(`content_scale < 1.0`; `runtime_max_frame_dimension`, que siempre es
un downscale cuando se dispara) -- un upscale (`content_scale > 1.0`,
caso raro) sigue usando `resize_rgba_nearest` (un box filter no tiene
sentido ahí). **El hit-mask de runtime NO se toca** --
`core::AlphaMask::FromAlphaChannel` sigue siendo nearest-neighbor,
exactamente igual que antes -- este cambio solo afecta los BYTES DE
TEXTURA que terminan en el pack compilado.

**Genérico, no un hack de Bunny:** el cambio vive enteramente en
`tools/compile_pet_pack.py`/`tools/prep_dev_sprite.py`, sin ninguna
rama de código específica de ningún pet. Beneficia también a Nidir
(su canvas de trabajo 624×612 también excede 320, así que el paso 2
también se dispara para Nidir) -- no verificado visualmente por
separado antes de esta corrección, pero arquitectónicamente seguro
(el `content_scale` de Nidir siempre es 1.0, así que solo el paso 2 lo
afecta).

Cobertura de test nueva:
`tools/test_asset_pipeline.py::ResizeRgbaAreaAverageTest` (7 tests,
incluyendo el caso de regresión específico -- un pixel opaco de 1px de
ancho que nearest-neighbor puede perder por completo -- y el caso de
fringing de color en un borde de transparencia).

## 9. Segunda animación de idle real: "groom", 70/30, intervalo de 10s (Block 04.3, corrección post-QA)

El owner exportó una segunda animación de idle real de Bunny
("acicalándose"/grooming) vía Ludo.ai, entregada en
`local_imports/bunny/bunny-idle-groom-left/` (25 frames, 446×602
nativo) y su spritesheet correspondiente
(`local_imports/bunny/bunny-idle-groom-left-spritesheet/`, 2230×3010 =
5×446 × 5×602, grilla 5×5 confirmada, no asumida) -- **instrucción
explícita del owner: usar `local_imports/` como staging en vez de
`~/Downloads`**, mismo cambio de convención que aplica a Nidir (ver
§19 de `docs/NIDIR_CONTENT.md`).

Copiado (nunca movido) a su ruta canónica,
`assets/source/nimvlets/bunny/animations/groom/left/{frames,spritesheet}/`
(Bunny es canónicamente "left", no "right" -- ver §2), verificado por
checksum MD5 contra el staging antes de considerar el import completo.
`tools/generate_bunny_pack.py` deriva "right" por el mismo espejado
horizontal determinista que ya usa para idle/click_reaction, con la
MISMA inversión canónica documentada en §2: la entrada canónica de
`groom` en el manifest usa los frames DERIVADOS ("right"), el override
"left" usa los frames REALES importados.

**Clipping:** el borde inferior se toca en la mayoría de los frames
(legítimo -- patas apoyadas en el piso, mismo patrón que idle/click).
Frames 19-22 tocan además el borde derecho (probablemente un brazo/
pata extendiéndose durante el acicalado) -- más leve que el hallazgo
de §3.1, no confirmado como un defecto real tras inspección visual
(a diferencia de la oreja cortada de idle/click_reaction).

**Canvas de trabajo compartido:** agregar `groom` como tercer grupo a
la normalización de contenido NO cambió el canvas de trabajo (sigue
siendo 428×563, el mismo que ya dominaba idle/click_reaction) --
`content_scale=0.8642` (groom viene dibujado a mayor escala nativa
que idle/click dentro de su propio frame; se reescala hacia abajo
para calzar con el tamaño de referencia, mismo mecanismo que ya
reescalaba `click_reaction` en §3).

**Selección ponderada 70/30 y 10 segundos** -- ver
`docs/ANIMATION_RUNTIME.md` para el mecanismo genérico. Para Bunny
específicamente: `passive_actions = [idle_breathing_right,
groom_right]`, `passive_action_weights = [0.7, 0.3]`,
`passive_interval_seconds = 10.0` (reemplaza el default de 300.0 que
§4 documentaba -- pedido explícito del owner para AMBOS Nimvlets).
`content_version` avanza a `"block04.3-bunny-2"`.

**Verificado** contra el binario real: carga sin errores ni
advertencias de cobertura de texturas; capturas de pantalla reales
(ver el informe de esta corrección) de `groom` en reproducción
muestran al personaje completo, orejas intactas, sin pérdida de
pixeles visible -- consistente con el fix de §8.

## 10. Tamaño visual global: +10% absoluto contra el baseline original (Block 04.3, corrección post-QA)

Mismo cambio genérico documentado en detalle en §18 de
`docs/NIDIR_CONTENT.md` -- `DISPLAY_SIZE_SCALE_FACTOR` pasa de `1.05`
(candidato ya confirmado por el owner) a `1.10`, absoluto contra
`REFERENCE_LOGICAL_SIZE` (160), no compuesto sobre el +5% anterior.
Para Bunny específicamente: canvas de trabajo 428×563 (sin cambios --
agregar `groom` no lo hizo crecer, ver §9), canvas lógico
122×160 (factor 1.0) → 128×168 (factor 1.05, primera pasada) →
**134×176** (factor 1.10, esta corrección).

## 11. Limitaciones honestas de esta corrección

- El fps de reproducción de `groom` (~8.33fps) usa la misma suposición
  de duración de generación de Ludo.ai (3 segundos) que idle/click ya
  documentaban en §7 -- no confirmada por separado para este export
  puntual.
- El clipping lateral de idle/click_reaction (§3.1) sigue sin
  corregirse -- documentado honestamente, requiere un nuevo export si
  el owner lo pide; no bloquea esta corrección porque el resto de las
  causas de "se ve mal" (calidad de downscale, §8) sí se corrigieron.
- El hallazgo de clipping de `groom` (frames 19-22, borde derecho) no
  se confirmó visualmente como un defecto real (a diferencia de la
  oreja de idle/click) -- documentado por transparencia, no porque se
  considere un problema pendiente.
