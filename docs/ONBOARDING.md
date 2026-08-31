# Onboarding de primer arranque (Block 09A)

Este documento describe la ARQUITECTURA de onboarding introducida en
Block 09A: el lifecycle persistido, la migración de usuarios existentes,
la política pura de selección de starter, el secreto de los 44 segundos,
la transacción de completitud, el gate de contenido de producción, y el
harness solo-DEV. **El onboarding de producción NO está habilitado al
terminar Block 09A** — falta el contenido de Artu/Rato/Rin Rin (ver §9).
Block 09B lo liga a contenido real y lo activa; Block 10 agrega el shop
oculto para comprar la variante de Frin no elegida.

Fuentes: `src/persistence/AppState.h` (v5), `src/catalog/OnboardingPolicy.{h,cpp}`,
`src/catalog/PetCatalog.h` (StarterRole), `src/productui/OnboardingLayout.{h,cpp}`
+ `OnboardingView.{h,cpp}`, `src/app/SpikeApp.cpp` (`ResolveOnboarding` /
`HandleOnboardingSelection`), `tools/compile_pet_catalog.py`
(`_validate_starter_content`), `tools/read_pet_pack.py` /
`tools/read_pet_preview.py`. Decisiones: DEC-131 (lifecycle / schema v5),
DEC-132 (metadata de starter + gate + 44 s + harness), y **DEC-133**
(el gate de producción exige contenido COINCIDENTE con la identidad del
starter, no solo existente; catálogo "NVCATLG1" v5 con
`devSyntheticOnboarding`).

## 1. La distinción crítica: dos poblaciones

Con exactamente **un Nimvlet** poseído, un estado puede ser un usuario
nuevo que recién eligió su starter, o un usuario viejo al que le queda
uno. **No se puede inferir de la propiedad.** Por eso hay un campo
persistido explícito.

| Población | Qué debe pasar |
|---|---|
| **A. Usuario existente / instalación migrada** (Blocks 01–08) | Conserva TODA la propiedad, el balance, el pet/variante activo, las preferencias y la posición. NUNCA se lo manda a selección de starter. NUNCA se le resetea la propiedad a un starter. NUNCA se le pone el balance en 0. |
| **B. Usuario genuinamente nuevo** (con onboarding de producción habilitado) | Arranca con 0 clics, sin poseer ningún starter. Ve el onboarding. Elige exactamente uno gratis. Recibe EXACTAMENTE esa autorización. Ese starter queda activo. Termina el onboarding. Entra a Nimvlets normal. |

## 2. `persistence::OnboardingLifecycle` — schema v5

AppState sube a **schema v5** agregando un solo byte, `onboardingLifecycle`:

| Valor | Significado |
|---|---|
| `kPending` (0) | El estado nunca pasó por selección de starter. **Es el default de un `AppState{}` recién construido** — es decir, lo que representa la AUSENCIA de archivo de estado. Con onboarding ARMADO significa "usuario nuevo → mostrar onboarding". |
| `kLegacyComplete` (1) | Un estado que precede al onboarding (schema v1..v4) **o** un estado dev/legacy que ya pasó por la siembra de propiedad por catálogo. Se considera ya onboardeado; se preserva todo lo demás exacto. |
| `kCompleted` (2) | Un usuario nuevo completó la selección en el producto con onboarding armado. Lo fija la transacción de completitud. |

`OnboardingConsideredComplete(l)` = `l != kPending`. A efectos de runtime
`kLegacyComplete` y `kCompleted` se tratan igual ("no mostrar onboarding");
se distinguen solo para trazabilidad.

**NUNCA se persiste ningún detalle transitorio del UI de onboarding**
(hover, foco de teclado, ms de dwell, highlight de selección, estado de
animación de confirmación) — solo este único enum (brief §4).

### Migración (ver `docs/PERSISTENCE.md` §3)

- **v1 / v2 / v3 / v4 → v5**: `DeserializeAppState` fija
  `onboardingLifecycle = kLegacyComplete` para CUALQUIER archivo que
  precede a v5. Todo lo demás (balance, propiedad, preferencias,
  posición, pet activo) se preserva EXACTO. `schemaVersion` se normaliza
  a 5, así el próximo `Save()` lo reescribe.
- **Ausencia de archivo** → `AppState{}` → `kPending`. Distinguible de un
  migrado (`kLegacyComplete`): así "usuario nuevo" ≠ "usuario viejo".
- **Archivo v5 con un byte de lifecycle fuera de {0,1,2}** (editado a
  mano / build futura) → se trata como `kLegacyComplete`: la opción NO
  DESTRUCTIVA. NO se rechaza el archivo entero (no se pierde la propiedad
  del usuario).
- **v5 truncado antes del byte** → se rechaza (no se adivina); el loader
  cae a defaults seguros como con cualquier corrupción.
- El umbral de la reconciliación de propiedad legacy de Block 07
  (`ExpandHistoricalWholePetEntitlements`) es
  `AppState::kFirstExplicitEntitlementSchema` (== 4), **no**
  `kCurrentSchemaVersion`: subir el schema por el onboarding (v5) NO
  empieza a "migrar" la propiedad de un v4. La **migración histórica de
  Frin de Block 07 no se toca** (DEC-129 sigue vigente): legacy `frin`
  significa exactamente `frin/male` + `frin/female`, nunca variantes
  futuras. El grant de onboarding de Frin es una política DISTINTA (§7).

### `AppStateStore::Load` — `outSaveFileExisted`

`Load` gana un out-param `outSaveFileExisted`: `true` si había un archivo
(aunque no se pueda parsear), `false` solo si genuinamente no existía.
`src/app` distingue así un **usuario nuevo** (sin archivo → onboarding,
cuando esté armado) de una **recuperación de un archivo corrupto**
(existía → se trata como usuario existente, NUNCA se lo onboardea —
brief §27).

## 3. Metadata de starter — `catalog::StarterRole` (catálogo v4)

El roster de starters es **DATO**, nunca `if (pet == "artu")` en el
runtime/UI (brief §7). El catálogo sube a schema **"NVCATLG1" v4**:

- Por entrada: `starter_role` (u8) — `"none"` (0) / `"normal"` (1) /
  `"secret"` (2). `kNormal` = la tríada Artu / Rato / Rin Rin; `kSecret`
  = Frin (un pet lógico con variantes macho / hembra).
- A nivel de catálogo: `production_onboarding_ready` (u8) — el datum
  EXPLÍCITO que arma el onboarding de producción (ver §8).

**Sin metadata especulativa de economía** (brief §7): un starter NO es
`publicly_purchasable` por serlo, y el shop oculto de starters es Block
10.

`catalog::BuildOnboardingOffer(catalog)` agrupa las entradas por rol:
los normales en orden de catálogo, y las variantes del secreto
colapsadas bajo su petId lógico (`{frin,""}` + `variants`).

## 4. Política pura — `catalog::OnboardingPolicy`

Sin SDL, sin AppKit, sin I/O. `src/app` la consulta y aplica su
resultado; la vista la usa para saber qué dibujar. Todo testeable sin
GUI (`tests/OnboardingPolicyTest.cpp`).

`EvaluateOnboardingSelection(offer, selected, alreadyCompleted) -> OnboardingGrant`:

| `selected` | Resultado |
|---|---|
| onboarding ya completo (`alreadyCompleted`) | `kAlreadyCompleted`, **cero mutación** (idempotencia — §15) |
| una identidad normal ofrecida | `kOk`, grant `{esa identidad}` |
| `{secretPetId, ""}` sin reveal | `kSecretNotYetRevealed` |
| `{secretPetId, ""}` con reveal | `kSecretNeedsVariant` (el UI debe pedir macho/hembra) |
| una variante ofrecida del secreto, con reveal | `kOk`, grant esa variante **EXACTA** |
| una variante del secreto sin reveal | `kSecretNotYetRevealed` |
| cualquier otra cosa | `kUnknownStarter` |

Un `OnboardingGrant` con `result == kOk` trae: `entitlement` (la
identidad EXACTA elegida), `activeIdentity` (== entitlement) y
`newBalance` = **0 siempre** (§16). Cualquier `result != kOk` deja el
grant en su default → **cero mutación** aguas arriba (§28).

## 5. El secreto de los 44 segundos

Cuando la pantalla de selección está activa y capaz de recibir input, un
**dwell de sesión** de 44 s sin selección completada revela a **Frin**
discretamente. Sin popup, sin toast, sin cuenta regresiva, sin "¡secreto
desbloqueado!", sin flash, sin sonido, sin notificación (brief §10).

**Composición del reveal (DEC-134, corrección de QA del owner).** La
primera fila —las 3 tarjetas normales— NO se toca: se dibuja EXACTAMENTE
igual (misma caja / arte / nombre / especie / posición) esté o no
revelado el secreto. Frin NO entra a esa fila: aparece en una SEGUNDA
fila debajo, horizontalmente centrada, en el espacio en blanco inferior,
con una tarjeta más compacta (armoniosa con las normales, no idéntica —
`kRevealCardW` / `kRevealArt` en `OnboardingLayout.cpp`). La pantalla
revelada entra en 800×560 sin scroll (EN y ES). El reveal se siente
"apareció algo más abajo", no "la grilla de selección ganó otro slot".
Frin entra SIEMPRE al final de `candidates` / `focusOrder` (orden
[normal 0..2, Frin]); `FocusList::SetItems` preserva el id enfocado, así
el reveal no reordena ni arrebata el foco, y el `HitTest` (que itera
`candidates`) cae automáticamente en la nueva caja BAJA de Frin.

### Semántica del timer (brief §11/§12)

- **Reloj MONOTÓNICO** (`SDL_GetTicks()`), nunca wall-clock, nunca
  timestamps.
- **NO se acumula progreso en AppState.** Si la app sale antes del reveal
  con el onboarding aún incompleto, la próxima sesión arranca un dwell
  FRESCO.
- **Event-driven, sin timer thread ni polling.** `catalog::SecretRevealDeadlineMs(activadaEn)`
  = `activadaEn + 44000`. `src/app` guarda `onboardingRevealDeadlineMs_`
  (`std::optional<double>`) y lo integra al MISMO cálculo de
  `SDL_WaitEventTimeout` / next-deadline que `ambientDeadlineMs_` &
  co. Antes del deadline el loop duerme hasta el próximo evento o
  deadline, lo que venga primero. Al llegar: transición del estado UNA
  vez + un redibujo, y se limpia el deadline (`RevealOnboardingSecret`).
  Después del reveal no queda ningún trabajo de timer del secreto.
- `SecretRevealedAfterDwell(dwellMs)` = `dwellMs >= 44000` (frontera
  EXACTA: 44000 → revelado). Testeable con tiempo inyectado
  (`tests/OnboardingPolicyTest.cpp` — no hay ningún `sleep` de 44 s).

## 6. La transacción de completitud (`HandleOnboardingSelection`)

En una selección CONFIRMADA que evalúa a `kOk`, `src/app` establece la
verdad de usuario-nuevo **atómicamente** en el MISMO `AppState`, con UNA
escritura (`FlushPersistedState()` inmediato — mismo contrato atómico
temp+rename que una compra del Shop, DEC-126):

```
clickBalance      = 0
ownedEntitlements = { grant.entitlement }   (canonicalizado)
ownershipSeeded   = true
activePetId/Var   = grant.activeIdentity
onboardingLifecycle = kCompleted
```

Las preferencias (tamaño/opacidad/lock/idioma) y la posición NO se
tocan. Un crash no puede producir "completado sin starter", "starter sin
completar", "activo equivocado" ni "grant parcial de Frin": los cinco
campos se serializan y renombran juntos.

**Idempotencia** (brief §15): `HandleOnboardingSelection` sale temprano
si `!onboardingActive_`; y `EvaluateOnboardingSelection` devuelve
`kAlreadyCompleted` (cero mutación) para un lifecycle `!= kPending`. Un
doble click / Enter repetido / callback duplicado no otorga múltiples
autorizaciones, no resetea el balance, no reemplaza el starter, no
completa dos veces, no otorga ambas variantes de Frin.

Tras completar: `src/app` carga el pack del starter
(`TrySwitchActivePet`, que gatea propiedad — y acaba de otorgarla),
muestra la ventana del pet, y saca al Product UI del modo onboarding.

## 7. Grant EXACTO de la variante de Frin

Frin es **UN Nimvlet lógico** con variantes macho / hembra. En el
onboarding:

- Elegir **Frin Macho** otorga `{frin, "male"}` — y NADA más.
- Elegir **Frin Hembra** otorga `{frin, "female"}` — y NADA más.
- La variante NO elegida queda **sin otorgar**. Comprarla es Block 10.
- **NO se reutiliza la regla de migración histórica** (`ExpandHistorical…`):
  esa expande `frin` legacy a macho + hembra; el onboarding es una
  política de grant NUEVA que otorga exactamente una.
- No existe una autorización de "todo Frin".

## 8. Gate de contenido listo para producción

**El arranque normal de producción NUNCA debe mostrar un onboarding roto
cuyos assets de starter no existen — ni uno cuyos assets son ALIAS del
contenido de otro Nimvlet** (brief §8/§31; endurecido por DEC-133). Que
un archivo EXISTA no significa "el contenido del starter está listo para
producción": el gate exige contenido REAL Y COINCIDENTE CON LA IDENTIDAD
del starter.

### Señales de identidad / procedencia (ya en los formatos en disco)

- **`.nvpack` ("NVPACK2")** lleva embebidos `id` y `variantGroup` del
  pet (`src/content/PetPackLoader.cpp`).
- **`.nvprev` ("NVPREV1")** lleva embebidos `pet_id`, `variant_id`, y
  `source_pack` (el basename del `.nvpack` del que se derivó —
  `src/productui/PreviewArtifact.cpp`).

### El gate, en capas

1. **Compilador** (`tools/compile_pet_catalog.py`,
   `_validate_starter_content`) — **prueba primaria**.
   `production_onboarding_ready: true` solo COMPILA si:
   - el manifest declara ≥ 3 **identidades lógicas distintas** con
     `starter_role: "normal"` (un `pet_id` distinto cada una; un starter
     normal no lleva `variant_id`, así dos variantes de un mismo pet no
     cuentan como dos — brief de endurecimiento §6);
   - para cada starter normal, su `.nvpack` existe, **parsea** (lector
     real `read_pet_pack`), y su identidad embebida coincide:
     `id == pet_id` **y** `variantGroup` vacío;
   - su `.nvprev` hermano existe, **parsea**
     (`read_pet_preview`), y su identidad embebida coincide:
     `pet_id`/`variant_id` iguales a los de la entrada **y**
     `source_pack == basename(pack_path)` (prueba que la preview se
     derivó de ESE pack).
   Un `pet_id: "artu"` que apunta al pack/preview de Bunny se RECHAZA
   aunque los archivos de Bunny existan. La metadata del secreto (Frin)
   sola nunca cuenta.
2. **Loader** (`PetCatalogLoader`) — defensa barata en runtime (brief de
   endurecimiento §9). Rechaza un `.nvcat` hecho a mano con
   `productionOnboardingReady` (o `devSyntheticOnboarding`) y < 3
   identidades lógicas distintas de starter normal; rechaza un
   `starterRole` normal con `variantId`; rechaza ambos flags de
   onboarding en `true`. **NO** reabre los `.nvpack` de decenas de MB
   para re-verificar identidad embebida — el `.nvcat` no lleva esa
   metadata y "el runtime no debe cargar todos los packs al arranque".
   La coincidencia de identidad pack/preview es una **garantía de tiempo
   de compilación**; el loader confía en el `productionOnboardingReady`
   ya validado y re-checa solo lo estructural.
3. **Runtime** (`catalog::EvaluateOnboardingReadiness`): `armed` sii
   `catalog.ProductionOnboardingReady()` **y** el conteo de identidades
   lógicas distintas de starter normal ≥ `kRequiredNormalStarterCount`
   (== 3). `CountNormalStarters` dedupe por `petId`.

`src/app` (`ResolveOnboarding`) entra al gate de onboarding SOLO si:

```
producción:  readiness.armed
             && appState_.onboardingLifecycle == kPending
             && !saveFileExisted            (un archivo corrupto no se onboardea)

DEV harness: catalog_.DevSyntheticOnboarding()
             && NIMVLETS_DEV_ONBOARDING seteada
             && appState_.onboardingLifecycle == kPending
```

Las dos ramas son simétricas y disjuntas: un catálogo sintético-DEV
tiene `ProductionOnboardingReady() == false` (nunca arma producción), y
un catálogo de producción real tiene `DevSyntheticOnboarding() == false`
(el env var solo no fuerza el gate). El compilador impone que los dos
bytes sean mutuamente excluyentes.

## 9. Por qué el onboarding de producción sigue DESHABILITADO tras Block 09A

El repositorio **no contiene todavía** packs/previews de producción para
Artu / Rato / Rin Rin (`assets/dev/` tiene solo bunny, nidir, frin).
Por lo tanto:

- `assets/dev/pet_catalog_manifest.json` tiene
  `production_onboarding_ready: false` y **cero** entradas con
  `starter_role`. `EvaluateOnboardingReadiness` devuelve `armed = false`
  con la razón "catalog does not mark production onboarding ready".
- Un `AppState{}` (usuario nuevo hoy) → `kPending`, pero onboarding NO
  armado → cae al camino de siembra dev/legacy EXISTENTE, sin ningún
  cambio de comportamiento. **El arranque normal es idéntico al de antes
  de Block 09A** (brief §31).
- La siembra por catálogo, además, normaliza el lifecycle de `kPending` a
  `kLegacyComplete` cuando corre — así un usuario dev/legacy queda
  marcado como "ya onboardeado" y Block 09B nunca lo trata como nuevo.

## 10. El harness solo-DEV (`NIMVLETS_DEV_ONBOARDING`)

Para ejercitar la máquina de estados / presentación REAL antes de que
exista contenido de producción:

- `NIMVLETS_DEV_ONBOARDING=1` — carga `assets/dev/onboarding_dev_catalog.nvcat`
  EN LUGAR de `pet_catalog.nvcat`, y fuerza el gate mientras el lifecycle
  sea `kPending` **y** ese catálogo tenga `dev_synthetic_onboarding: true`
  (si no, `src/app` loguea "not a dev-synthetic onboarding catalog" y NO
  fuerza el gate — DEC-133). El catálogo DEV declara `artu_dev` /
  `rato_dev` / `rinrin_dev` (`starter_role: normal`, prestando
  packs/previews existentes) + `frin` male/female (`starter_role: secret`,
  packs reales) + **`dev_synthetic_onboarding: true`** (NO
  `production_onboarding_ready` — son mutuamente excluyentes). **`artu_dev`
  etc. NO son Artu / Rato / Rin Rin**: son descriptores sintéticos para
  QA del flujo (selección → confirmación → grant → completitud → reinicio
  → activación). El compilador NO exige coincidencia de identidad bajo
  `dev_synthetic_onboarding` (los alias son el punto), pero SÍ exige la
  tríada de identidades lógicas distintas y que cada pack/preview exista
  y parsee. NUNCA se envía. El `(dev)` en el display name lo hace
  inconfundible.
- `NIMVLETS_DEV_ONBOARDING_REVEAL_MS=<n>` — usa `<n>` ms en vez de 44000
  para el deadline (smoke sin dormir 44 s).
- `NIMVLETS_DEV_ONBOARDING_REVEAL=1` — revela el secreto al arranque (QA
  del estado revelado sin correr el loop).
- `NIMVLETS_DEV_ONBOARDING_STAGE=browse|variant|confirm[:<focusId>]` —
  fuerza una etapa (p. ej. `confirm:cand:artu_dev` o `confirm:var:female`).
- `NIMVLETS_DEV_ONBOARDING_CHOOSE=<petId>[/<variant>]` — confirma una
  selección sin interacción (misma ruta que "Choose <name>"): evalúa la
  política, aplica la transacción de completitud, sale del gate.
- Combinable con `NIMVLETS_DEV_LANGUAGE`, `NIMVLETS_DEV_APPDATA_DIR`
  (usar SIEMPRE un dir aislado y fresco), `NIMVLETS_DEV_PRODUCT_SHOT`.

## 11. Presentación (`productui::OnboardingLayout` / `OnboardingView`)

Onboarding **NO es una sección del Product UI** (brief §19): es un GATE
de primer arranque. NO se agrega "Onboarding" a
`Collection · Shop · Settings`, NO hay cabecera de navegación en la
pantalla, y mientras está activo la ventana ignora por completo la
navegación de secciones — no se puede saltear la selección. Cerrar la
ventana la re-enfoca en vez de cerrarla; `Esc` solo cancela una
confirmación / vuelve del sub-menú de Frin, nunca saltea (brief §25).
Tras completar, el gate desaparece permanentemente para ese usuario; un
usuario existente nunca lo ve.

Composición cálida / quieta / espaciosa (brief §18/§20): un encabezado
("Choose your first Nimvlet" / "Elige tu primer Nimvlet") y una fila de
TRES tarjetas normales centrada (Frin, si el secreto fue revelado, va en
una segunda fila compacta debajo — ver §5), el arte manda cuando se ligue
contenido real. **Sin** precios, lenguaje de Shop, rareza, stats,
habilidades, slots bloqueados, ni "Paso 1 de 3". Información mínima por
tarjeta: nombre propio + una etiqueta corta de identidad si hay
editorial.

Etapas (una sola pantalla; `stage` cambia qué se dibuja):
`kBrowse` → `kFrinVariant` (si se toca Frin) → `kConfirm`. La
confirmación es inline (no un modal gigante): copy del brief §23 —
"Make Artu your first Nimvlet?" / "¿Quieres que Artu sea tu primer
Nimvlet?", botones "Cancel" / "Choose Artu" (para Frin la identidad
confirmada incluye la variante: "Choose Frin (Male)"). **El foco arranca
en "Cancel"** — un click perdido nunca completa el onboarding.

**Contenido**: la pantalla usa el bundle `.nvprev` liviano (mismo que la
Collection) para el arte de los candidatos — **NUNCA abre un `.nvpack`**
(brief §26). El pack completo se carga recién al activar el starter
elegido.

### Teclado / foco (brief §25)

Mouse; `Tab` / `Shift+Tab`; flechas (recorren el anillo de foco); `Enter`
/ `Space` (activan); `Esc` (solo cancela una confirmación / vuelve del
sub-menú). Foco de teclado visible (`focus-visible por modalidad`, como
el resto del Product UI); un click de mouse apaga el chrome de foco.
Orden de foco predecible: candidatos de izquierda a derecha (Frin al
final), luego Cancel/Choose en la confirmación.

## 12. Localización

Se extiende `core::StringKey`: `kOnboardingChooseFirst`,
`kOnboardingConfirmStarter` (plantilla con `{pet}`, formateada por
`productui::FormatOnboardingConfirmPrompt`),
`kOnboardingConfirmChoosePrefix`, `kOnboardingWhichVariant`. "Male" /
"Female" reusan `kMale` / `kFemale`; "Cancel" reusa `kCancel`. Los
nombres propios (Nimvlet, Artu, Rato, Rin Rin, Frin) NUNCA se traducen.
Cambiar el idioma desde el menú rápido relabela la pantalla de
onboarding de inmediato (mismo camino `ProductWindow::SetLanguage`).

## 13. Privacidad / performance

Ningún permiso nuevo, ninguna red, ningún account / cloud / telemetría /
analytics, ninguna API global del SO. El timer del secreto es un solo
deadline monotónico local integrado al event loop existente — sin
polling, sin thread permanente, sin redraw continuo. La pantalla de
onboarding abierta y en reposo es efectivamente ociosa.

## 14. Fronteras

### Block 09B — habilitar el onboarding de producción

1. Agregar el contenido de producción de **Artu**, **Rato** y **Rin Rin**
   (`assets/source/nimvlets/…`, generadores, `.nvpack` + `.nvprev` en
   `assets/dev/`) — mismo contrato que Nidir/Frin
   (`docs/PET_CONTENT_SPEC.md` / `docs/NIDIR_CONTENT.md`). Cada `.nvpack`
   debe llevar su `id` embebido == su `pet_id` de catálogo (lo hace el
   generador al setear `"id"` en el manifest del pack), y su `.nvprev`
   se genera con `tools/compile_pet_previews.py` (que sella el `pet_id` /
   `variant_id` / `source_pack` correctos).
2. En `assets/dev/pet_catalog_manifest.json`: agregar las 3 entradas con
   `starter_role: "normal"` (sin `variant_id`), y poner
   `production_onboarding_ready: true`. El compilador valida que los 6
   archivos existen, **parsean, y su identidad embebida coincide** con
   cada entrada (DEC-133); si algo no calza, falla ruidosamente. No hace
   falta tocar `dev_synthetic_onboarding` (queda `false`).
3. Recompilar `pet_catalog.nvcat`
   (`python3 tools/compile_pet_catalog.py assets/dev/pet_catalog_manifest.json assets/dev/pet_catalog.nvcat`).
4. (Opcional) retirar / ajustar la siembra de propiedad por catálogo
   (`initially_owned` / `SeedEntitlementsFromCatalog`) una vez que todo
   usuario nuevo pasa por onboarding (brief §17). Los usuarios dev/legacy
   de Block 09A ya quedaron marcados `kLegacyComplete`, así que no se
   ven afectados.
5. Refinar la presentación de la pantalla cuando el arte real esté
   ligado (composición, el momento de "bienvenida" tras completar, etc.).

Nada más de persistencia, grants, timing ni política de entitlements
necesita rediseñarse — esa es la razón de ser de Block 09A.

### Block 10 — shop oculto de starters

Comprar la **variante de Frin no elegida** en el onboarding (y,
eventualmente, otros starters). Reusa los primitivos de entitlement y la
transacción atómica del Shop; NO se implementa ni se insinúa acá.
