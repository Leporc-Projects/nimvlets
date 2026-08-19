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
  ready — 128x168 canvas...` (con el factor de tamaño +5%, ver
  `docs/NIDIR_CONTENT.md` §"tamaño visual global" -- 128x168 es el
  resultado real derivado del canvas de trabajo 428x563 de Bunny, no
  un valor hardcodeado).
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
