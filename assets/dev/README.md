# assets/dev

Artefactos de runtime **compilados** (packs `.nvpack`/catálogo `.nvcat`)
generados por `tools/` a partir de la fuente real en
`assets/source/nimvlets/` (ver ese directorio's README). El nombre
"dev" describe el pipeline de generación (herramientas Python, dev
tooling — ver `docs/PET_CONTENT_SPEC.md`), no que el contenido sea
sintético: desde Block 04.2/04.3/05 todo lo que vive acá es arte real
compilado, nunca placeholders. Se mantiene el nombre del directorio
por continuidad — ver `docs/DECISION_LOG.md` para la decisión
explícita de Block 05 de NO renombrarlo (el riesgo de tocar una ruta
hardcodeada en `src/app/SpikeApp.cpp` y en cada `generate_<pet>_pack.py`
no se justificaba frente a ningún beneficio real).

## Contenido actual

- **`bunny_pack.nvpack`** — pack compilado de Bunny (arte real, ver
  `docs/BUNNY_CONTENT.md`), generado por `tools/generate_bunny_pack.py`
  desde `assets/source/nimvlets/bunny/`.
- **`nidir_pack.nvpack`** — pack compilado de Nidir (arte real, ver
  `docs/NIDIR_CONTENT.md`), generado por `tools/generate_nidir_pack.py`
  desde `assets/source/nimvlets/nidir/`.
- **`frin_male_pack.nvpack`** / **`frin_female_pack.nvpack`** — los dos
  packs compilados de Frin (Block 05, arte real, ver el informe de ese
  bloque), generados por `tools/generate_frin_pack.py` desde
  `assets/source/nimvlets/frin/{male,female}/`. Frin es UN Nimvlet
  lógico con dos variantes — dos packs porque cada variante tiene su
  propio contenido, pero el catálogo los agrupa bajo el mismo
  `pet_id: "frin"` (`variant_id: "male"`/`"female"`) — ver
  `docs/CATALOG.md`.
- **`pet_catalog_manifest.json`** — el manifest del catálogo (ver
  `docs/CATALOG.md`): hand-written, no generado — no hay paso de
  derivación para un catálogo como sí lo hay para frames de pixeles.
- **`pet_catalog.nvcat`** — el catálogo compilado que `src/app` carga
  de verdad al arrancar (`catalog::LoadCatalogFromFile`), construido
  desde `pet_catalog_manifest.json` por `tools/compile_pet_catalog.py`.
  Binario, no pensado para leerse directamente. Regenerar tras editar
  el manifest:
  `python3 tools/compile_pet_catalog.py assets/dev/pet_catalog_manifest.json assets/dev/pet_catalog.nvcat`.

Todos los `.nvpack` de arriba son binarios grandes (arte real a
resolución de hasta 320px por lado, ver `docs/NIDIR_CONTENT.md` §8) —
no pensados para leerse directamente; regenerar corriendo el
`generate_<pet>_pack.py` correspondiente después de tocar su fuente en
`assets/source/nimvlets/`.

`src/app/SpikeApp` carga `pet_catalog.nvcat` al arrancar y resuelve
contra él qué pack cargar (ver `docs/CATALOG.md`); deriva el hit-test
de click-through de cada frame desde su propio canal alpha real. Si el
catálogo o el pack resuelto no cargan (p. ej. el proceso no se lanzó
desde la raíz del repo, donde estas rutas relativas se resuelven), la
app falla ruidosamente — loguea un error fatal específico y sale con
código no-cero — en vez de caer a ninguna forma hardcodeada.

## Superseded / removido (Block 05)

El fixture sintético original de Bunny (Block 01/02 — `bunny_source.png`
+ `bunny_pack/` + `tools/generate_bunny_dev_pack.py`, un pack derivado
por transformaciones de pixeles simples, nunca arte real) se **eliminó**
en este bloque: Bunny tiene arte real de producción desde Block 04.3, y
el generador sintético ya no podía producir un pack válido de todos
modos (el formato de runtime cambió a "NVPACK2" — ver
`docs/ANIMATION_RUNTIME.md` — y ese script nunca se actualizó a la
nueva forma de manifest, por diseño: era explícitamente un artefacto
histórico peligroso de re-ejecutar, según su propio docstring). Su
valor histórico queda preservado en `git log`/DEC-018/DEC-023 sin
necesidad de mantener el código corriendo.
