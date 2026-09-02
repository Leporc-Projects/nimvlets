#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nimvlets::persistence {

// Última posición en pantalla de la ventana, en el mismo espacio de
// coordenadas que usan SDL_GetWindowPosition()/SDL_SetWindowPosition()
// (int, puntos lógicos de pantalla) — ver el manejo de fin-de-drag en
// src/app/SpikeApp.cpp.
struct WindowPosition {
    int x = 0;
    int y = 0;

    friend bool operator==(WindowPosition a, WindowPosition b) {
        return a.x == b.x && a.y == b.y;
    }
};

// Una AUTORIZACIÓN de propiedad persistida (Block 07, schema v4).
// Reemplaza al `ownedPetIds` (un simple conjunto de petId) de Block 06.
//
//   petId no vacío + variantId vacío   -> el Nimvlet entero.
//   petId no vacío + variantId no vacío -> solo esa variante.
//
// src/persistence la almacena como DATO plano; la semántica (¿cubre
// esta identidad?, canonicalización con subsunción) vive en
// src/catalog (catalog::PetEntitlement), que src/app puentea — la misma
// división que activePetId (string acá) vs. catalog::PetIdentity, para
// que src/persistence siga sin depender de src/catalog ni de SDL.
struct OwnedEntitlement {
    std::string petId;
    std::string variantId;  // "" = el pet entero (cualquier variante)

    friend bool operator==(const OwnedEntitlement& a, const OwnedEntitlement& b) {
        return a.petId == b.petId && a.variantId == b.variantId;
    }
};

// Lifecycle de PRIMER ARRANQUE (Block 09A, schema v5). Distingue tres
// poblaciones de usuario que NO se pueden inferir de la propiedad
// actual (brief §3): un usuario con exactamente un Nimvlet puede ser un
// nuevo que recién eligió su starter, o un viejo al que le queda uno.
enum class OnboardingLifecycle : std::uint8_t {
    // El estado nunca pasó por selección de starter. Es el DEFAULT de un
    // `AppState{}` recién construido — es decir, lo que representa la
    // AUSENCIA de archivo de estado (`AppStateStore::Load` sin save
    // devuelve `AppState{}`). En el producto con onboarding ARMADO
    // significa "usuario genuinamente nuevo -> mostrar onboarding". Hoy,
    // con onboarding NO armado (no hay contenido de Artu/Rato/Rin Rin —
    // ver docs/ONBOARDING.md), cae al camino de siembra dev existente
    // sin ningún cambio de comportamiento.
    kPending = 0,
    // El estado viene de un schema v1..v4 que PRECEDE al onboarding — un
    // usuario existente de Blocks 01-08. Se considera YA onboardeado: su
    // propiedad / balance / preferencias / posición se preservan EXACTOS
    // y NUNCA se lo manda a selección de starter, NUNCA se le resetea la
    // propiedad a un starter, NUNCA se le pone el balance en 0 (brief
    // §3.A / §4). `DeserializeAppState` lo fija al migrar cualquier
    // v1/v2/v3/v4.
    kLegacyComplete = 1,
    // Un usuario nuevo COMPLETÓ la selección de starter en el producto
    // con onboarding armado. Lo fija la transacción de completitud
    // (balance 0 + grant del starter + activo + este flag, atómico —
    // brief §14). A efectos de runtime `kLegacyComplete` y `kCompleted`
    // se tratan igual ("no mostrar onboarding"); se distinguen solo para
    // trazabilidad.
    kCompleted = 2,
};

// true si el runtime debe considerar el onboarding YA resuelto para
// este estado — es decir, cualquier cosa MENOS `kPending`.
inline bool OnboardingConsideredComplete(OnboardingLifecycle lifecycle) {
    return lifecycle != OnboardingLifecycle::kPending;
}

// El conjunto completo de estado de aplicación local, en disco, que
// persiste Block 03. Datos puros — sin SDL, sin I/O de archivos, sin
// código de plataforma (ver AppStateSerializer.h para la
// (de)serialización y AppStateStore.h para la política de
// almacenamiento que lo lee/escribe). Deliberadamente mínimo: solo
// campos con significado real en el runtime de este bloque — ver
// docs/PERSISTENCE.md para qué se dejó fuera a propósito y por qué.
//
// Genérico por construcción: activePetId/activeVariantId son simples
// strings, no un enum de Nimvlets conocidos — agregar un nuevo pet id
// o variante más adelante nunca requiere tocar este struct, ni
// AppStateSerializer, ni AppStateStore.
struct AppState {
    // Solo se incrementa cuando la forma en disco de este struct
    // cambia de manera no retrocompatible. Ver AppStateSerializer.cpp
    // para cómo se maneja un desajuste.
    //
    // v1 (Block 03): clickBalance + activePetId/Variant + windowPos.
    // v2 (Block 06): agrega el estado de propiedad (ownedPetIds +
    //   ownershipSeeded) y las preferencias del menú rápido
    //   (lockPosition, sizeChoice, opacityPercent).
    // v3 (Block 06.1): agrega `language` ("en"/"es").
    // v4 (Block 07): la propiedad pasa de `ownedPetIds` (petIds sueltos)
    //   a `ownedEntitlements` (autorizaciones {petId, variantId}) —
    //   capaz de expresar "posee solo Frin macho". La migración desde
    //   v1/v2/v3 mapea cada petId viejo a una autorización de PET ENTERO
    //   ({petId, ""}), así un Frin poseído sigue dando macho + hembra
    //   (brief §5). Ver docs/PERSISTENCE.md §3 y DEC-124.
    // v5 (Block 09A): agrega `onboardingLifecycle` — el lifecycle de
    //   primer arranque (kPending / kLegacyComplete / kCompleted). Todo
    //   v1/v2/v3/v4 migra a `kLegacyComplete` (usuario existente, ya
    //   onboardeado — se preserva TODO lo demás exacto). Ver
    //   docs/ONBOARDING.md y DEC-131.
    // v6 (Block 11A): agrega `clickCountingMode` — la preferencia OPT-IN
    //   de conteo de clics ("" / "nimvlet_only" / "anywhere"). Todo
    //   v1..v5 migra al DEFAULT LOCAL: el campo llega vacío y
    //   core::ParseClickCountingMode lo lee como kNimvletOnly. Un
    //   usuario existente NUNCA queda con conteo global habilitado por
    //   una actualización, y la app nunca pide un permiso de input por
    //   haber subido de schema. Ver docs/GLOBAL_CLICK_MODE.md y DEC-139.
    //
    // Cada subida trae una migración hacia adelante mínima: un archivo
    // más viejo se lee con su layout, los campos nuevos quedan en su
    // default (o se derivan del viejo, como la propiedad en v4 o el
    // lifecycle en v5), y `schemaVersion` se marca como el actual para
    // que el próximo Save() lo reescriba — así el click balance, la
    // posición, la propiedad y las preferencias del owner sobreviven
    // cada actualización. Ver docs/PERSISTENCE.md §3 y
    // DEC-109/DEC-116/DEC-124/DEC-131/DEC-139.
    static constexpr std::uint32_t kCurrentSchemaVersion = 6;

    // El primer schema que guardó la propiedad como pares
    // {petId, variantId} explícitos (v4), en vez de `ownedPetIds`
    // sueltos que el serializer parsea PROVISIONALMENTE a `{petId, ""}`.
    // src/app corre la reconciliación de propiedad legacy
    // (`catalog::ExpandHistoricalWholePetEntitlements`) SOLO cuando la
    // versión EN DISCO es < esto — nunca sobre un v4+ (DEC-129). Es un
    // umbral SEMÁNTICO fijo, no `kCurrentSchemaVersion`: subir el schema
    // por una razón no relacionada (v5, onboarding; v6, modo de conteo
    // de clics) no debe empezar a "migrar" propiedad de un v4. Esta
    // constante NO se toca al subir de schema — es la frontera histórica
    // CONGELADA de DEC-129, y `4` es su valor permanente. Ver
    // tests/EntitlementMigrationTest.cpp, que fija v4->v6 y v5->v6 como
    // regresión explícita.
    static constexpr std::uint32_t kFirstExplicitEntitlementSchema = 4;

    std::uint32_t schemaVersion = kCurrentSchemaVersion;

    // La única moneda — ver AGENTS.md §2. Empieza en 0; se incrementa
    // por un click real y, desde Block 07, se DECREMENTA por una compra
    // en el Shop (nunca por debajo de 0 — la política de compra verifica
    // balance >= precio antes de restar). uint64 para que nunca desborde
    // de forma realista.
    std::uint64_t clickBalance = 0;

    // Qué pet está activo actualmente. String vacío = sin save aún /
    // sin definir. Este bloque no implementa *selección* de pet — ver
    // docs/PERSISTENCE.md — solo mantiene este campo sincronizado con
    // la verdad: cualquier pet que el runtime haya cargado realmente.
    std::string activePetId;

    // Qué variante de activePetId está activa, si ese pet tiene
    // variantes (ver content::PetDefinition::variantGroup). String
    // vacío = sin variante / no aplica. Nada en este bloque escribe un
    // valor no vacío aquí (todavía no existe selección de variante) —
    // el campo se conserva a través de load/save para que un bloque
    // futuro pueda poblarlo sin un cambio de schema.
    std::string activeVariantId;

    // Posición de la ventana la última vez que el usuario la movió,
    // para que la app pueda reabrir donde la dejaron. std::nullopt =
    // sin save aún / nunca se arrastró (usa el default existente de
    // centrado al iniciar).
    std::optional<WindowPosition> lastWindowPosition;

    // --- Estado de propiedad (Block 06 -> Block 07 schema v4) --------
    //
    // Qué tiene derecho a usar el owner, como AUTORIZACIONES
    // {petId, variantId} (ver OwnedEntitlement arriba). Reemplaza al
    // `ownedPetIds` de Block 06: ahora se puede expresar "posee solo
    // Frin macho". Orden canónico: ordenado, sin duplicados, sin petId
    // vacío, y con subsunción ((p,"") descarta (p,<var>)) — lo impone
    // catalog::CanonicalizePetEntitlements en src/app al mutar, y el
    // serializer (orden + dedup) sobre una copia, así el archivo es
    // determinista y dos AppState equivalentes comparan igual.
    //
    // Autoridad una vez escrito; NO se deriva del catálogo en cada
    // arranque. El catálogo solo aporta la SEMILLA de desarrollo (ver
    // catalog::CatalogEntry::initiallyOwned) y solo cuando
    // `ownershipSeeded` todavía es false — ver más abajo.
    std::vector<OwnedEntitlement> ownedEntitlements;

    // false = este estado nunca pasó por la inicialización de
    // propiedad. En ese caso src/app siembra `ownedEntitlements` desde
    // las entradas `initiallyOwned` del catálogo y pone esto en true.
    // Un bloque futuro de onboarding (Block 09) reemplaza esa siembra
    // por la elección real de starter del jugador SIN un cambio de
    // schema: solo escribe `ownedEntitlements` + `ownershipSeeded =
    // true` con su propia lógica. "Poseer cero Nimvlets" (todo
    // bloqueado) y "nunca se inicializó" son estados distintos
    // precisamente por este flag.
    bool ownershipSeeded = false;

    // --- Preferencias del menú rápido (Block 06) -------------------
    //
    // Ver docs/PRODUCT_UI.md §7 y core::DisplayControls, que traduce
    // estos a comportamiento genérico de runtime (ninguna rama por
    // pet).

    // Si true, la ventana del pet no se puede arrastrar (click / hover
    // / click-through / animaciones siguen intactos — block brief §16).
    bool lockPosition = false;

    // Tamaño de usuario, MULTIPLICADOR encima de visualScale. String
    // legible ("small"/"medium"/"large"); un valor desconocido se
    // interpreta como "medium" al leer (core::ParsePetSizeChoice).
    // Vacío se trata igual que "medium".
    std::string sizeChoice;

    // Opacidad de la ventana del pet, porcentaje. 0 = "sin preferencia
    // guardada" -> se trata como 100 (totalmente opaco) al aplicar;
    // cualquier otro valor se ajusta al conjunto finito del menú
    // (core::NormalizeOpacityPercent).
    std::uint32_t opacityPercent = 0;

    // --- Idioma del Product UI (Block 06.1) -------------------------
    //
    // "" = el owner nunca eligió idioma explícitamente. En ese caso
    // src/app resuelve el inicial desde el locale del OS (en/es), pero
    // NO lo persiste — así "el owner eligió inglés" y "adivinamos
    // inglés" siguen siendo distinguibles. Una vez que elige desde el
    // menú Language, se escribe "en"/"es" acá y su preferencia gana
    // siempre (brief §5). Un valor desconocido se interpreta como "en"
    // al leer (core::ParseLanguage). Ver docs/PRODUCT_UI.md §16.
    std::string language;

    // --- Lifecycle de primer arranque (Block 09A, schema v5) ---------
    //
    // Ver OnboardingLifecycle arriba. DEFAULT `kPending` a propósito:
    // un `AppState{}` recién construido = "ningún save" = usuario
    // potencialmente nuevo. `DeserializeAppState` lo pone en
    // `kLegacyComplete` para CUALQUIER archivo v1..v4 (usuario
    // existente). NUNCA se persiste ningún detalle transitorio del UI de
    // onboarding acá (hover, foco, ms de dwell, highlight — brief §4);
    // solo este único enum de lifecycle.
    OnboardingLifecycle onboardingLifecycle = OnboardingLifecycle::kPending;

    // --- Modo de conteo de clics (Block 11A, schema v6) --------------
    //
    // La preferencia OPT-IN de dónde cuentan los clics: "" (el owner
    // nunca eligió) / "nimvlet_only" / "anywhere". Se guarda como STRING
    // por la misma razón que `sizeChoice` y `language` — es una
    // preferencia, y `core::PreferencesFromStored` ya tiene el idioma de
    // "parsear un campo crudo y normalizar lo desconocido a un default
    // seguro" (core::ParseClickCountingMode: cualquier cosa que no sea
    // "anywhere" -> kNimvletOnly).
    //
    // Este campo guarda el modo PEDIDO, no el efectivo. Que el conteo
    // global esté REALMENTE activo depende de la capacidad de la
    // plataforma y del permiso del OS, que no son estado persistible: se
    // consultan en cada arranque (ver platform::GlobalClickMonitor). Un
    // "anywhere" persistido cuyo permiso ya no está NUNCA se auto-
    // degrada en disco — el owner sigue viendo lo que eligió, y Settings
    // le dice que no está activo (docs/GLOBAL_CLICK_MODE.md §4).
    //
    // NUNCA se persiste nada más de esta feature: ni coordenadas, ni
    // timestamps, ni historial de clics, ni contadores por fuente, ni el
    // estado del permiso (AGENTS.md §5, docs/PRIVACY_SECURITY.md §H).
    std::string clickCountingMode;

    friend bool operator==(const AppState& a, const AppState& b) {
        return a.schemaVersion == b.schemaVersion &&
               a.clickBalance == b.clickBalance &&
               a.activePetId == b.activePetId &&
               a.activeVariantId == b.activeVariantId &&
               a.lastWindowPosition == b.lastWindowPosition &&
               a.ownedEntitlements == b.ownedEntitlements &&
               a.ownershipSeeded == b.ownershipSeeded &&
               a.lockPosition == b.lockPosition &&
               a.sizeChoice == b.sizeChoice &&
               a.opacityPercent == b.opacityPercent &&
               a.language == b.language &&
               a.onboardingLifecycle == b.onboardingLifecycle &&
               a.clickCountingMode == b.clickCountingMode;
    }
};

}  // namespace nimvlets::persistence
