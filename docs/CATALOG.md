# Nimvlets — Catálogo de pets + switching en runtime (Block 04)

Esto describe la capa genérica y data-driven, construida en Block 04,
que le permite al runtime saber qué Nimvlets existen, resolver cuál
mostrar al arrancar, y cambiar de pet activo mientras corre — sin
ninguna rama de C++ específica de un pet. Se apoya en el runtime de
contenido de Block 02 (`docs/ANIMATION_RUNTIME.md`) y en el
`activePetId`/`activeVariantId` persistidos de Block 03
(`docs/PERSISTENCE.md`). Ver `docs/DECISION_LOG.md` DEC-029 en
adelante para por qué se tomó cada decisión de abajo.

## 1. Alcance

Este bloque es backend/runtime únicamente — no hay selector ni menú
visible para el usuario (block brief §4). Bunny sigue siendo el único
fixture de arte real requerido; el catálogo real de dev tiene
exactamente una entrada. El resultado deja todo lo necesario para que
un bloque futuro agregue más pets reales y construya una UI de
selección sin tocar `src/catalog` ni `src/app`.

**Actualización (Block 04.2):** el catálogo real de dev ahora tiene
**dos** entradas — Bunny (sigue siendo el default) y Nidir, el primer
pet con arte real de producción (ver `docs/NIDIR_CONTENT.md`).
Agregarlo fue exactamente la promesa de este párrafo: una fila nueva
en `assets/dev/pet_catalog_manifest.json` + recompilar, cero cambios
en `src/catalog` ni `src/app`.

## 2. Identidad de pet

`catalog::PetIdentity` (`src/catalog/PetIdentity.h`): un `petId` más un
`variantId` opcional (vacío = sin variante), ambos strings simples —
sin ningún enum de Nimvlets específicos. Provee `operator==`,
`operator<` (orden estable, sin significado de producto) y
`PetIdentityHash`, para poder usarse tanto en comparaciones directas
como en contenedores ordenados/hash si algún día hace falta.

El caso central de variante (ver `docs/PET_CONTENT_SPEC.md` sobre
Frin): dos entradas del catálogo pueden compartir el mismo `petId` con
`variantId` distinto (`{"frin", "male"}` vs. `{"frin", "female"}`) y
son identidades completamente separadas — cada una apunta a su propio
pack.

`PetIdentity` es el esquema de identidad a nivel de *catálogo*
(selección/persistencia), distinto de
`content::PetDefinition::id`/`variantGroup` (lo que un pack ya cargado
sabe de sí mismo). Este bloque no exige ninguna relación fija entre
ambos — en la práctica, para un catálogo bien formado, coincidirán,
pero nada en el código lo verifica ni lo necesita.

## 3. Formato del catálogo ("NVCATLG1")

Productor: `tools/compile_pet_catalog.py`. Consumidor:
`catalog::LoadCatalogFromMemory`/`LoadCatalogFromFile`
(`src/catalog/PetCatalogLoader.cpp`) — parseo puro, sin I/O de
archivos en la lógica central, directamente testeable con buffers
sintéticos (`tests/PetCatalogLoaderTest.cpp`), el mismo patrón que
`content::PetPackLoader` y `persistence::AppStateSerializer` ya
establecieron. Todos los enteros little-endian (mismo supuesto de
plataforma que el resto de formatos de este repositorio).

```
magic       : 8 bytes, "NVCATLG1"
schemaVersion: uint32
entryCount  : uint32
entries[entryCount]:
  petId       : string  (uint32 byte-length + UTF-8 bytes)
  variantId   : string
  displayName : string
  packPath    : string
  isDefault   : uint8 (0/1)
```

**Determinista:** orden de campos fijo, sin iteración de map/set en
ningún lugar del formato — cargar el mismo catálogo siempre produce el
mismo resultado.

**Validación estricta, falla ruidosamente** (nunca inventa ni adivina
datos):

- `entryCount == 0` se rechaza — un catálogo necesita al menos una
  entrada.
- `petId` o `packPath` vacíos en cualquier entrada se rechazan.
- una identidad `(petId, variantId)` duplicada se rechaza — dos
  entradas con el mismo `petId` pero `variantId` distinto están
  permitidas (no es un duplicado, es el caso de variante).
- la cantidad de entradas con `isDefault=true` debe ser exactamente
  una — cero o más de una se rechazan.

**Deliberadamente NO valida** que cada `packPath` apunte a un archivo
que realmente existe: eso requeriría tocar el filesystem desde un
parser que de otro modo es puro, y verificarlo cargando cada pack
violaría "el runtime no debe cargar todos los packs al arranque". Un
pack faltante o corrupto se descubre y reporta claramente recién
cuando algo intenta cargarlo de verdad — ver §5.

`tools/compile_pet_catalog.py` sí valida `pack_path` en tiempo de
*compilación* del catálogo (chequeo de cordura para quien autora el
manifest, no parte del formato binario en sí ni del loader en C++).

## 4. Manifest de autoría (JSON) + pipeline

```
manifest JSON -> tools/compile_pet_catalog.py -> assets/dev/pet_catalog.nvcat
```

Mismo patrón que `tools/compile_pet_pack.py` (manifest -> binario
determinista, sin dependencias de terceros). Esquema:

```json
{
  "schema_version": 1,
  "entries": [
    {
      "pet_id": "bunny_dev",
      "variant_id": "",
      "display_name": "Bunny (dev fixture)",
      "pack_path": "assets/dev/bunny_pack.nvpack",
      "is_default": true
    }
  ]
}
```

`pack_path` se guarda tal cual en el binario compilado — a diferencia
de los `source` de frames en `compile_pet_pack.py` (resueltos relativo
al directorio del manifest, porque ese script necesita leer los PNG en
el momento de compilar), `pack_path` es una referencia que el C++ en
runtime resolverá más tarde desde el directorio de trabajo del
proceso, exactamente igual que `kPetPackPath` se resolvía antes de
este bloque.

El catálogo de dev real (`assets/dev/pet_catalog_manifest.json` ->
`assets/dev/pet_catalog.nvcat`) tiene una única entrada: Bunny, marcada
default — porque Bunny es el único pack real que existe (block brief:
"Bunny remains the only real runtime art fixture required"). Agregar
un Nimvlet real más adelante es: agregar su pack compilado + una nueva
entrada al manifest + recompilar el catálogo — cero cambios en
`src/catalog` o `src/app`.

## 5. Resolución de la selección activa al arranque

`catalog::ResolveActiveSelection(catalog, persistedIdentity)`
(`src/catalog/ActivePetResolution.h`, puro): busca la identidad
persistida en el catálogo; si calza exactamente, la usa; si no —
incluyendo el caso de una identidad vacía (primera ejecución, sin save
aún) — cae al default del catálogo. `PetCatalog::Default()` nunca es
null en un catálogo cargado con éxito (invariante garantizada por el
loader: siempre hay al menos una entrada y exactamente un default).

`catalog::LoadPetForIdentity(catalog, identity, outPet, outError)`
busca `identity` y, si existe, intenta cargar su pack vía
`content::LoadPetPackFromFile()`. Falla ruidosamente tanto si la
identidad no está en el catálogo como si su pack no carga — y en
cualquiera de los dos casos `outPet` queda completamente intacto (la
carga ocurre sobre un `PetDefinition` local; solo se mueve a `outPet`
tras confirmar éxito). Esta es la misma función que usan tanto la
resolución de arranque como el switching en runtime (§6) — ver §7.

**Secuencia de arranque** (`SpikeApp::Init()`):

1. Cargar el catálogo (`assets/dev/pet_catalog.nvcat`) — fail loud si
   no carga, igual que el pack de un pet individual en Block 02/03: sin
   catálogo no hay forma de saber qué mostrar.
2. Cargar el `AppState` persistido (Block 03, sin cambios).
3. `ResolveActiveSelection()` contra `activePetId`/`activeVariantId`
   persistidos.
4. Intentar cargar el pack de la entrada resuelta. Si falla y esa
   entrada no era ya el default, reintentar una vez con el default —
   **nunca debe crashear solo porque un pet guardado dejó de estar
   disponible** (block brief §3). Si ambos intentos fallan, es un
   fallo de arranque genuino (sin más fallback posible), reportado
   igual de ruidosamente que un catálogo ausente.
5. Si se terminó usando algo distinto de lo persistido (identidad
   desconocida, vacía, o el pack guardado ya no cargaba): reparar
   `appState_.activePetId`/`activeVariantId` en memoria para que
   reflejen la verdad, y marcar el scheduler de persistencia dirty —
   así el próximo flush (debounced, o el shutdown limpio) corrige el
   archivo en disco.

## 6. Switching en runtime

`SpikeApp::TrySwitchActivePet(target)` — la API reutilizable de
switching (block brief §4):

- **Valida** `target` implícitamente: si no está en el catálogo,
  `LoadPetForIdentity` falla y no se toca nada.
- **Carga antes de descartar**: el pack nuevo se carga en un
  `PetDefinition` local; solo si esa carga tiene éxito se sueltan las
  texturas del pet anterior y se reemplaza `pet_`.
- **Al tener éxito**: suelta las texturas del pet anterior
  (`ReleaseAllTextures()`), reemplaza `pet_`, reatacha texturas del
  nuevo (`AttachAllTextures()`), reconstruye `animController_` (que
  arranca en Idle del pet nuevo — exactamente lo requerido), reaplica
  tamaño de ventana/presentación lógica (por si el canvas cambió de
  tamaño entre pets), actualiza
  `appState_.activePetId`/`activeVariantId`, marca el scheduler de
  persistencia dirty, y pide un redraw (el loop principal reconstruye
  textura/hit-mask/forma de ventana en su siguiente vuelta, el mismo
  mecanismo `needsRedraw_` que ya existía — ver
  `docs/ANIMATION_RUNTIME.md` §6).
- **Al fallar**: `pet_`/`animController_`/`appState_` quedan
  completamente intactos — el pet activo anterior sigue usable — y se
  loguea claramente por qué.

`animController_.reset()` ocurre *antes* de reemplazar `pet_` y
*antes* de que se le vuelva a hacer `emplace()`: esto evita que el
puntero interno del controller a la animación activa quede colgando si
`pet_.passiveActions` cambia de tamaño entre el pet viejo y el nuevo
(reasignar un `std::vector` puede reubicar su buffer) — nunca existe un
`AnimationController` vivo mientras el contenido de `pet_` está siendo
reemplazado.

No hay switching automático de producto en este bloque: nada llama a
`TrySwitchActivePet()` salvo el mecanismo solo-DEV de abajo.

## 7. Mecanismo solo-DEV para smoke-testear el switching

`NIMVLETS_DEV_SWITCH_TEST_COUNT` (entero positivo): si está seteada,
inmediatamente después de que `Init()` termina y antes de entrar al
loop principal, `SpikeApp` ejecuta esa cantidad de llamadas a
`TrySwitchActivePet()` — una detrás de otra, sincrónicamente, cicladas
por `catalog_.Entries()` — logueando cada resultado. Sin la variable de
entorno, esto es un no-op total: cero cambio de comportamiento en
producción (block brief §4: "no debe convertirse en comportamiento de
producto"). No agrega ningún polling: es un lote síncrono, no un
schedule recurrente.

Con el catálogo de dev real (una sola entrada), esto ejercita
repetidamente el switch-a-sí-mismo — suficiente para probar el
*mecanismo* (soltar/recargar/reatachar, sin crecimiento de recursos —
ver §8) contra el binario real, aun sin un segundo pack real con el
que alternar. El switching entre pets genuinamente distintos ya está
cubierto por los tests puros con fixtures sintéticas (§9).

```bash
NIMVLETS_DEV_SWITCH_TEST_COUNT=5 ./build/macos-debug/src/app/nimvlets_spike
```

## 8. Recursos / performance

- Solo el pack del pet activo está cargado en memoria en cualquier
  momento — el catálogo es puro metadato (id/nombre/ruta/marca de
  default), nunca dispara la carga de otras entradas.
- Cada switch exitoso suelta las texturas del pet anterior *antes* de
  adjuntar las del nuevo — nunca se acumulan texturas de packs viejos.
- Medido: 500 switches automatizados (vía el mecanismo DEV) contra el
  binario Release real no muestran crecimiento de RSS frente a 5
  switches (~76.4 MB en ambos casos, diferencia dentro del ruido de
  medición) — ver `docs/PERFORMANCE_BUDGETS.md`.
- No se agrega ningún polling: ni la resolución de arranque ni el
  switching en runtime introducen un nuevo tick — el mecanismo DEV
  corre una sola vez, sincrónicamente, antes del loop principal.
- El comportamiento de idle estático (Block 02) y el debounce de
  persistencia (Block 03) quedan sin cambios — el catálogo no toca el
  cálculo de `waitMs` del event loop en absoluto.

## 9. Testeabilidad

Todo `src/catalog` es puro (sin SDL) y depende solo de
`nimvlets_content` — cada pieza es testeable sin display:

- `tests/PetIdentityTest.cpp` — semántica de igualdad/orden/hash,
  puro, sin archivos (6 casos).
- `tests/PetCatalogLoaderTest.cpp` — buffers sintéticos en memoria,
  mismo estilo que `PetPackLoaderTest.cpp`: carga válida, variantes
  compartiendo `petId`, orden determinista, magic/truncamiento/schema
  version inválidos, cero entradas, campos vacíos, identidad
  duplicada, default ausente o múltiple (13 casos).
- `tests/ActivePetResolutionTest.cpp` — un `PetCatalog` construido
  directamente en memoria (sin pasar por el formato binario, ya
  cubierto arriba): selección persistida válida, desconocida, vacía, y
  resolución exacta de variante (5 casos).
- `tests/PetSwitchingTest.cpp` — directorios temporales reales con
  packs "NVPACK1" sintéticos (mismo nivel de realismo que
  `tests/AppStateStoreTest.cpp`, nunca datos reales): switch exitoso,
  switch fallido por identidad desconocida, switch fallido por pack
  faltante, switching repetido sin acumular estado, y persistencia
  marcada dirty solo tras un cambio exitoso — este último conecta
  `persistence::AppState` + `PersistenceScheduler` de la misma forma
  que `tests/PersistenceIntegrationTest.cpp` ya hizo para click/drag
  (5 casos).

Los 4 archivos corren por la misma invocación de `ctest` que el resto
del repositorio; ninguno toca el directorio real de app-data del
usuario ni requiere display.

## 10. Intencionalmente no implementado

- **Sin UI ni menú de selección.** El switching es una API de backend
  reutilizable, sin ningún punto de entrada visible al usuario en este
  bloque.
- **Sin migración de schema del catálogo.** Exactamente un
  `schemaVersion` soportado, igual que `persistence::AppState`.
- **Sin arte real para ningún Nimvlet más allá de Nidir** (Block 04.2
  — ver `docs/NIDIR_CONTENT.md`). Bunny sigue siendo un fixture de QA,
  no un Nimvlet real.
- **Sin verificación de existencia de `packPath` en el loader C++.**
  Ver §3/§5 para por qué, y dónde sí se descubre un pack faltante.
- **Sin variantes reales cargadas.** El concepto de variante está
  soportado estructuralmente (ver §2 y los tests) pero no hay ningún
  pack de variante real en este bloque.
