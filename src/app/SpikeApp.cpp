#include "app/SpikeApp.h"

#include "catalog/PetCatalogLoader.h"
#include "core/DisplayControls.h"
#include "platform/QuickMenuModel.h"
#include "platform/RendererPolicy.h"
#include "platform/SystemShellTypes.h"
#include "platform/TransparentWindowSupport.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>

namespace nimvlets::app {

namespace {

// De dónde se carga el catálogo de pets. Ruta relativa, resuelta desde
// el directorio de trabajo del proceso — ver docs/CATALOG.md.
constexpr const char* kCatalogPath = "assets/dev/pet_catalog.nvcat";

// Catálogo SINTÉTICO solo-DEV para el harness de onboarding (Block 09A):
// artu_dev/rato_dev/rinrin_dev toman prestados packs existentes para
// ejercitar la máquina de estados/presentación ANTES de que exista
// contenido real de Artu/Rato/Rin Rin. Se carga EN LUGAR de kCatalogPath
// SOLO cuando NIMVLETS_DEV_ONBOARDING está seteada. NUNCA se envía. Ver
// docs/ONBOARDING.md.
constexpr const char* kOnboardingDevCatalogPath = "assets/dev/onboarding_dev_catalog.nvcat";

// NIMVLETS_DEV_ONBOARDING — harness solo-DEV: carga el catálogo
// sintético de arriba y fuerza el gate de onboarding (si el lifecycle es
// kPending). Ausente/vacía: no-op total, arranque de producción normal.
constexpr const char* kDevOnboardingEnvVar = "NIMVLETS_DEV_ONBOARDING";

// Reads NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS — see
// SpikeApp::ComputeEffectiveAmbientIntervalSeconds()'s doc comment.
constexpr const char* kDevPassiveIntervalEnvVar = "NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS";

// Lee NIMVLETS_DEV_SWITCH_TEST_COUNT — ver el comentario de
// SpikeApp::RunDevSwitchSmokeTestIfRequested().
constexpr const char* kDevSwitchTestCountEnvVar = "NIMVLETS_DEV_SWITCH_TEST_COUNT";

// Lee NIMVLETS_DEV_DIRECTION_TEST_COUNT — ver el comentario de
// SpikeApp::RunDevDirectionSmokeTestIfRequested().
constexpr const char* kDevDirectionTestCountEnvVar = "NIMVLETS_DEV_DIRECTION_TEST_COUNT";

// Lee NIMVLETS_DEV_CLICK_TEST_COUNT — ver el comentario de
// SpikeApp::RunDevClickSmokeTestIfRequested().
constexpr const char* kDevClickTestCountEnvVar = "NIMVLETS_DEV_CLICK_TEST_COUNT";

// Lee NIMVLETS_DEV_HOVER_TEST_COUNT — ver el comentario de
// SpikeApp::RunDevHoverSmokeTestIfRequested().
constexpr const char* kDevHoverTestCountEnvVar = "NIMVLETS_DEV_HOVER_TEST_COUNT";

// Lee NIMVLETS_DEV_RENDERER_DRIVER — mecanismo solo-DEV que fuerza un
// driver de SDL_Renderer específico (p. ej. "opengl", "metal",
// "software"), sin importar la plataforma o su default de producto
// (ver platform::PreferredRendererDriverName() y DEC-083 en
// docs/DECISION_LOG.md) -- así el owner puede comparar drivers en
// cualquier sistema operativo sin recompilar. Ausente/vacía: no-op,
// se usa el default de la plataforma actual.
constexpr const char* kDevRendererDriverEnvVar = "NIMVLETS_DEV_RENDERER_DRIVER";

// Lee NIMVLETS_DEV_SELECT_PET — mecanismo solo-DEV (Block 05, §10 del
// brief: "leave a simple DEV mechanism to manually launch/select"). Si
// está seteada a un "petId" o "petId/variantId" que exista en el
// catálogo, ese pet se activa en vez del default/persistido — sin
// tocar appState_.activePetId ni el archivo de estado real del owner
// (esto solo reemplaza QUÉ se carga, no lo que se persiste después:
// cualquier click/drag que ocurra en esta sesión SÍ se guarda contra
// el pet efectivamente activo, igual que un switch en runtime normal —
// ver TrySwitchActivePet()). Ausente o vacía: no-op total, mismo
// comportamiento que sin esta variable.
constexpr const char* kDevSelectPetEnvVar = "NIMVLETS_DEV_SELECT_PET";

// Mecanismo DEV genérico y reusado varias veces ya (no una sonda
// descartable de una sola investigación): si está seteada a un
// directorio, cada RenderFrame() real vuelca los pixeles YA COMPUESTOS
// por el renderer (SDL_RenderReadPixels, lo que de verdad se
// presentó) a un .rgba crudo -- evidencia directa de lo que el
// pipeline real produce, sin captura de pantalla (lee el backbuffer de
// NUESTRO propio renderer, nunca el escritorio). Usado originalmente
// para el diagnóstico de raíz de Bunny, y de nuevo para el A/B de
// renderer reutilizable-vs-por-frame (ver DEC-081) y para comparar
// software vs. acelerado en esta pasada (ver DEC-083) -- se conserva
// como herramienta permanente porque comparar "lo que realmente se
// presentó" entre dos configuraciones sigue siendo la forma más barata
// de verificar equivalencia visual sin depender de captura de pantalla.
constexpr const char* kDevDumpFramesDirEnvVar = "NIMVLETS_DEV_DUMP_FRAMES_DIR";

// Separación de wall-clock del redraw de confirmación (ver el
// comentario de confirmRedrawDeadlineMs_ en SpikeApp.h). 120ms es
// perceptualmente instantáneo pero suficiente separación real de
// tiempo como para que el compositor/GPU tenga una oportunidad genuina
// de asentarse entre presents.
constexpr double kConfirmRedrawDelayMs = 120.0;

constexpr const char* kPrefPathOrg = "Leporc Projects";
constexpr const char* kPrefPathApp = "Nimvlets";

constexpr const char* kDevAppDataDirEnvVar = "NIMVLETS_DEV_APPDATA_DIR";

std::atomic<bool> g_shutdownRequested{false};

extern "C" void HandleTerminationSignal(int /*signum*/) {
    g_shutdownRequested.store(true, std::memory_order_relaxed);
}

// Idioma inicial cuando el owner nunca eligió uno (Block 06.1, brief
// §5): "MAY follow OS language for English/Spanish. Otherwise default
// to English." SDL_GetPreferredLocales() ya está disponible (SDL está
// linkeado), así que no hace falta ninguna maquinaria de locale nueva
// — se mira el primer locale preferido y solo se distingue "es" de
// todo lo demás. NO se persiste el resultado: eso lo hace solo una
// elección explícita desde el menú.
core::Language ResolveInitialLanguageFromOS() {
    int count = 0;
    SDL_Locale** locales = SDL_GetPreferredLocales(&count);
    core::Language resolved = core::Language::kEn;
    if (locales != nullptr) {
        for (int i = 0; i < count && locales[i] != nullptr; ++i) {
            const char* lang = locales[i]->language;
            if (lang != nullptr && SDL_strncasecmp(lang, "es", 2) == 0) {
                resolved = core::Language::kEs;
                break;
            }
            if (lang != nullptr && SDL_strncasecmp(lang, "en", 2) == 0) {
                resolved = core::Language::kEn;
                break;
            }
            // Cualquier otro idioma preferido: seguir mirando; si
            // ninguno es en/es, queda el default kEn.
        }
        SDL_free(locales);
    }
    return resolved;
}

// Puente propiedad<->catálogo (Block 07). appState_ guarda la propiedad
// como persistence::OwnedEntitlement (datos planos, sin dependencia de
// src/catalog); src/catalog razona sobre catalog::PetEntitlement (con
// Covers()/canonicalización). Misma división que activePetId (string en
// persistence) vs. catalog::PetIdentity.
std::vector<catalog::PetEntitlement> ToCatalogEntitlements(
    const std::vector<persistence::OwnedEntitlement>& v) {
    std::vector<catalog::PetEntitlement> out;
    out.reserve(v.size());
    for (const persistence::OwnedEntitlement& e : v) {
        out.push_back(catalog::PetEntitlement{e.petId, e.variantId});
    }
    return out;
}

std::vector<persistence::OwnedEntitlement> ToPersistedEntitlements(
    const std::vector<catalog::PetEntitlement>& v) {
    std::vector<persistence::OwnedEntitlement> out;
    out.reserve(v.size());
    for (const catalog::PetEntitlement& e : v) {
        out.push_back(persistence::OwnedEntitlement{e.petId, e.variantId});
    }
    return out;
}

struct CursorSample {
    float globalX = 0.0f;
    float globalY = 0.0f;
    int windowX = 0;
    int windowY = 0;
    core::Point localPoint{};
};

CursorSample SampleCursor(SDL_Window* window) {
    CursorSample sample;
    SDL_GetGlobalMouseState(&sample.globalX, &sample.globalY);
    SDL_GetWindowPosition(window, &sample.windowX, &sample.windowY);
    sample.localPoint = core::Point{
        static_cast<double>(sample.globalX) - static_cast<double>(sample.windowX),
        static_cast<double>(sample.globalY) - static_cast<double>(sample.windowY),
    };
    return sample;
}

SDL_Surface* BuildHitTestShapeSurface(const core::AlphaMask& mask) {
    SDL_Surface* surface = SDL_CreateSurface(mask.Width(), mask.Height(), SDL_PIXELFORMAT_RGBA32);
    if (surface == nullptr) {
        SDL_Log("nimvlets: SDL_CreateSurface (hit-test shape) failed: %s", SDL_GetError());
        return nullptr;
    }

    for (int y = 0; y < mask.Height(); ++y) {
        for (int x = 0; x < mask.Width(); ++x) {
            const bool opaque = mask.Contains(core::Point{static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5});
            SDL_WriteSurfacePixel(surface, x, y, 255, 255, 255, opaque ? 255 : 0);
        }
    }
    return surface;
}

#ifndef NDEBUG
// Solo usado por el log de diagnóstico transition-only del loop
// principal (compilado fuera en Release, igual que el resto de la
// instrumentación #ifndef NDEBUG de este archivo).
const char* ControllerModeName(content::ControllerMode mode) {
    switch (mode) {
        case content::ControllerMode::kBase:
            return "Base";
        case content::ControllerMode::kAmbientOrHoverAction:
            return "AmbientOrHoverAction";
        case content::ControllerMode::kClickAction:
            return "ClickAction";
    }
    return "Unknown";
}
#endif  // NDEBUG

}  // namespace

bool ShutdownRequested() {
    return g_shutdownRequested.load(std::memory_order_relaxed);
}

int SpikeApp::EffectiveCanvasWidth() const {
    const double sizeFactor = core::PetSizeScaleFactor(core::ParsePetSizeChoice(appState_.sizeChoice));
    return std::max(1, static_cast<int>(std::lround(pet_.canvasWidth * pet_.visualScale * sizeFactor)));
}

int SpikeApp::EffectiveCanvasHeight() const {
    const double sizeFactor = core::PetSizeScaleFactor(core::ParsePetSizeChoice(appState_.sizeChoice));
    return std::max(1, static_cast<int>(std::lround(pet_.canvasHeight * pet_.visualScale * sizeFactor)));
}

double SpikeApp::ComputeEffectiveAmbientIntervalSeconds(const content::BehaviorState& state) const {
    const char* overrideEnv = std::getenv(kDevPassiveIntervalEnvVar);
    if (overrideEnv == nullptr) {
        return state.ambientIntervalSeconds;
    }

    char* end = nullptr;
    const double parsed = std::strtod(overrideEnv, &end);
    if (end == overrideEnv || parsed <= 0.0) {
        SDL_Log(
            "nimvlets: %s='%s' is not a valid positive number; ignoring (using state default %.1fs)",
            kDevPassiveIntervalEnvVar, overrideEnv, state.ambientIntervalSeconds);
        return state.ambientIntervalSeconds;
    }
    return parsed;
}

void SpikeApp::RearmAmbientDeadline(double nowMs) {
    const content::BehaviorState& state = animController_->CurrentState();
    if (state.ambientActions.empty()) {
        ambientDeadlineMs_.reset();
        return;
    }
    ambientDeadlineMs_ = nowMs + ComputeEffectiveAmbientIntervalSeconds(state) * 1000.0;
}

void SpikeApp::MarkNeedsRedraw(double nowMs) {
    needsRedraw_ = true;
    confirmRedrawDeadlineMs_ = nowMs + kConfirmRedrawDelayMs;
}

bool SpikeApp::Init() {
    std::signal(SIGINT, HandleTerminationSignal);
    std::signal(SIGTERM, HandleTerminationSignal);

    SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP, "1");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("nimvlets: SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    const bool devOnboarding =
        [] {
            const char* v = std::getenv(kDevOnboardingEnvVar);
            return v != nullptr && v[0] != '\0' && v[0] != '0';
        }();
    const char* catalogPath = devOnboarding ? kOnboardingDevCatalogPath : kCatalogPath;

    std::string catalogError;
    if (!catalog::LoadCatalogFromFile(catalogPath, catalog_, catalogError)) {
        SDL_Log("nimvlets: FATAL: could not load pet catalog '%s': %s", catalogPath, catalogError.c_str());
        SDL_Log("nimvlets: (run from the repository root, or regenerate it: python3 tools/compile_pet_catalog.py assets/dev/pet_catalog_manifest.json assets/dev/pet_catalog.nvcat)");
        return false;
    }
    if (devOnboarding) {
        SDL_Log(
            "nimvlets: DEV override active — %s (synthetic onboarding catalog '%s' — NOT production content)",
            kDevOnboardingEnvVar, catalogPath);
    }

    std::string appDataDir;
    if (const char* devDir = std::getenv(kDevAppDataDirEnvVar); devDir != nullptr && devDir[0] != '\0') {
        std::error_code ec;
        std::filesystem::create_directories(devDir, ec);
        if (ec) {
            SDL_Log(
                "nimvlets: could not create DEV app-data dir '%s' (%s) -- persistence disabled for this run",
                devDir, ec.message().c_str());
        } else {
            appDataDir = devDir;
            SDL_Log(
                "nimvlets: DEV override active — %s='%s' (production uses SDL_GetPrefPath instead)",
                kDevAppDataDirEnvVar, devDir);
        }
    } else {
        char* prefPathRaw = SDL_GetPrefPath(kPrefPathOrg, kPrefPathApp);
        if (prefPathRaw != nullptr) {
            appDataDir = prefPathRaw;
            SDL_free(prefPathRaw);
        } else {
            SDL_Log("nimvlets: SDL_GetPrefPath failed: %s -- persistence disabled for this run", SDL_GetError());
        }
    }

    // La versión de schema que traía el save EN DISCO (antes de
    // normalizarse a la actual). Gobierna si hay que correr la
    // reconciliación de propiedad legacy más abajo. Sin save / corrupto
    // -> queda en la actual: nada legacy que migrar (DEC-129).
    std::uint32_t loadedOnDiskSchema = persistence::AppState::kCurrentSchemaVersion;
    // ¿Había un archivo de estado en disco? (aunque no se pueda parsear).
    // Un usuario genuinamente NUEVO no tiene archivo — así se distingue
    // de una recuperación de un archivo corrupto, que NUNCA se manda a
    // onboarding (brief §4 / §27, DEC-131).
    bool saveFileExisted = false;
    if (!appDataDir.empty()) {
        appStateStore_.emplace(appDataDir);

        std::string loadWarning;
        appState_ = appStateStore_->Load(&loadWarning, &loadedOnDiskSchema, &saveFileExisted);
        if (!loadWarning.empty()) {
            SDL_Log("nimvlets: %s", loadWarning.c_str());
        }
    }

    // --- Idioma del Product UI (Block 06.1) ---
    // appState_.language vacío = el owner nunca eligió: se deriva del
    // locale del OS (solo en/es), SIN persistirlo. Una elección
    // explícita desde el menú gana siempre a partir de ahí (brief §5).
    if (appState_.language.empty()) {
        language_ = ResolveInitialLanguageFromOS();
        SDL_Log("nimvlets: UI language not set -- resolved from OS locale to '%s' (not persisted)",
                core::LanguageId(language_));
    } else {
        language_ = core::ParseLanguage(appState_.language);
        SDL_Log("nimvlets: UI language = '%s' (persisted choice)", core::LanguageId(language_));
    }
    // Override solo-DEV para QA / capturas: fuerza el idioma de esta
    // sesión sin persistir nada. "en" / "es". Ver README.md.
    if (const char* devLang = std::getenv("NIMVLETS_DEV_LANGUAGE"); devLang != nullptr && devLang[0] != '\0') {
        language_ = core::ParseLanguage(devLang);
        SDL_Log("nimvlets: DEV override active — NIMVLETS_DEV_LANGUAGE='%s' (not persisted)", devLang);
    }

    // Selección solo-DEV (ver kDevSelectPetEnvVar) -- resuelta ANTES
    // que la persistida, así que un valor válido siempre gana sobre lo
    // que appState_ diga para efectos de qué se carga esta sesión.
    catalog::PetIdentity requestedIdentity{appState_.activePetId, appState_.activeVariantId};
    bool haveDevSelection = false;
    if (const char* devSelect = std::getenv(kDevSelectPetEnvVar); devSelect != nullptr && devSelect[0] != '\0') {
        const std::string spec(devSelect);
        const std::size_t slash = spec.find('/');
        catalog::PetIdentity devIdentity{
            slash == std::string::npos ? spec : spec.substr(0, slash),
            slash == std::string::npos ? std::string() : spec.substr(slash + 1),
        };
        if (catalog_.Find(devIdentity) != nullptr) {
            requestedIdentity = devIdentity;
            haveDevSelection = true;
            SDL_Log(
                "nimvlets: DEV override active — %s='%s' (persisted selection ignored for this run only)",
                kDevSelectPetEnvVar, devSelect);
        } else {
            SDL_Log(
                "nimvlets: %s='%s' does not match any catalog entry; ignoring (using persisted/default selection)",
                kDevSelectPetEnvVar, devSelect);
        }
    }

    // --- Gate de onboarding de primer arranque (Block 09A) ---------
    //
    // Decide si ESTA sesión arranca en la selección de starter. Se
    // consulta ANTES de la reconciliación/siembra de propiedad: un
    // usuario genuinamente nuevo NO tiene propiedad legacy que migrar ni
    // se le siembra nada — su propiedad la fija la transacción de
    // completitud. Ver ResolveOnboarding / docs/ONBOARDING.md.
    ResolveOnboarding(saveFileExisted);

    // --- Estado de propiedad (Block 06 -> Block 07 autorizaciones) ---
    //
    // (1) Reconcilia la propiedad "por pet lógico" de un save v1/v2/v3.
    //     Un `ownedPetIds` "frin" se parseó PROVISIONALMENTE a
    //     `{frin, ""}` (el serializer no tiene catálogo — ver
    //     docs/PERSISTENCE.md §3); acá se expande al conjunto HISTÓRICO
    //     CONGELADO — `{frin, "male"} + {frin, "female"}`, las variantes
    //     que Block 06 exponía — mediante una tabla en src/catalog que
    //     NO consulta el catálogo actual (así una variante futura como
    //     `frin/spirit`, aunque ya exista en el catálogo, NO se otorga —
    //     DEC-129). Solo se corre si el save vino GENUINAMENTE de un
    //     schema legacy en disco; sobre un v4 nunca (no se puede
    //     fabricar propiedad reinterpretando un `{frin, ""}` de un save
    //     actual editado a mano).
    if (!onboardingActive_ &&
        loadedOnDiskSchema < persistence::AppState::kFirstExplicitEntitlementSchema) {
        std::vector<catalog::PetEntitlement> ents = CurrentEntitlements();
        if (catalog::ExpandHistoricalWholePetEntitlements(ents)) {
            appState_.ownedEntitlements = ToPersistedEntitlements(ents);
            persistenceScheduler_.MarkDirty(static_cast<double>(SDL_GetTicks()));
            SDL_Log(
                "nimvlets: migrated schema v%u ownership to explicit entitlements (%zu; frozen historical map)",
                loadedOnDiskSchema, appState_.ownedEntitlements.size());
        }
    }

    // (2) Siembra de desarrollo/default SOLO en el primer arranque (o si
    //     el conjunto quedó vacío por corrupción — un estado post-siembra
    //     nunca puede tener cero autorizaciones, no hay forma de "vender"
    //     un Nimvlet). La semilla otorga la autorización EXPLÍCITA de
    //     cada entrada `initiallyOwned` — Frin siembra sus dos variantes,
    //     no un `{frin, ""}`.
    //
    //     Block 09A: NO se siembra si esta sesión entró al gate de
    //     onboarding — un usuario nuevo no posee nada hasta que elige su
    //     starter, y esa elección (transacción de completitud) es la que
    //     escribe `ownedEntitlements` + `ownershipSeeded`. La siembra por
    //     catálogo sigue siendo el camino de dev/legacy hasta que Block
    //     09B habilite el onboarding de producción (brief §17).
    if (!onboardingActive_ && (!appState_.ownershipSeeded || appState_.ownedEntitlements.empty())) {
        appState_.ownedEntitlements =
            ToPersistedEntitlements(catalog::SeedEntitlementsFromCatalog(catalog_));
        appState_.ownershipSeeded = true;
        // La siembra por catálogo es el camino dev/legacy, no onboarding:
        // este estado se considera YA onboardeado (brief §17). Un usuario
        // que entró al gate no llega acá (onboardingActive_).
        if (appState_.onboardingLifecycle == persistence::OnboardingLifecycle::kPending) {
            appState_.onboardingLifecycle = persistence::OnboardingLifecycle::kLegacyComplete;
        }
        persistenceScheduler_.MarkDirty(static_cast<double>(SDL_GetTicks()));
        SDL_Log("nimvlets: ownership seeded from catalog (%zu entitlement(s))",
                appState_.ownedEntitlements.size());
    }


    // (3) Resolver el pet activo: primero contra el CATÁLOGO (identidad
    //     desconocida -> default), luego contra la PROPIEDAD (identidad
    //     no autorizada -> una que sí lo esté). ResolveOwnedActiveIdentity
    //     NUNCA otorga nada: un save corrupto con `active = nidir, owned
    //     = {bunny}` cae de vuelta a bunny, no se "compra" nidir gratis
    //     (brief §4, DEC-128). Se salta en una selección solo-DEV
    //     (NIMVLETS_DEV_SELECT_PET carga lo que se le pide sin tocar el
    //     estado real).
    // Durante el onboarding se carga el pet default SOLO para que la
    // ventana del pet tenga dimensiones válidas (queda oculta); no se
    // resuelve contra propiedad (no hay ninguna todavía) ni se repara /
    // persiste ninguna selección — la elección de starter la fija la
    // transacción de completitud.
    const bool bypassOwnershipResolution = haveDevSelection || onboardingActive_;

    const catalog::ResolvedSelection resolved = catalog::ResolveActiveSelection(catalog_, requestedIdentity);
    catalog::PetIdentity target = resolved.entry->identity;
    bool selectionRepaired = resolved.usedFallback;
    if (!bypassOwnershipResolution) {
        bool ownershipFellBack = false;
        target = catalog::ResolveOwnedActiveIdentity(CurrentEntitlements(), catalog_, target, ownershipFellBack);
        if (ownershipFellBack) {
            selectionRepaired = true;
            SDL_Log(
                "nimvlets: persisted active pet '%s'%s%s is not owned -- falling back to owned '%s'%s%s "
                "(no entitlement granted)",
                resolved.entry->identity.petId.c_str(),
                resolved.entry->identity.variantId.empty() ? "" : "/",
                resolved.entry->identity.variantId.c_str(), target.petId.c_str(),
                target.variantId.empty() ? "" : "/", target.variantId.c_str());
        }
    }

    // (4) Cargar el pack de `target` (con el fallback histórico al
    //     default si el pack no carga — nunca crashea solo porque un pet
    //     guardado dejó de estar disponible).
    content::PetDefinition loadedPet;
    std::string packError;
    catalog::PetIdentity loadedIdentity = target;
    if (!catalog::LoadPetForIdentity(catalog_, target, loadedPet, packError)) {
        SDL_Log("nimvlets: pack for '%s' failed to load (%s)", target.petId.c_str(), packError.c_str());
        if (target != catalog_.Default().identity) {
            SDL_Log("nimvlets: falling back to catalog default");
            loadedIdentity = catalog_.Default().identity;
            selectionRepaired = true;
            if (!catalog::LoadPetForIdentity(catalog_, loadedIdentity, loadedPet, packError)) {
                SDL_Log("nimvlets: FATAL: catalog default pack also failed to load: %s", packError.c_str());
                return false;
            }
        } else {
            SDL_Log("nimvlets: FATAL: catalog default pack failed to load; no further fallback possible");
            return false;
        }
    }
    pet_ = std::move(loadedPet);

    // (5) Repara la selección persistida en memoria si terminamos usando
    //     algo distinto de lo guardado -- NUNCA en una selección solo-DEV
    //     transitoria, ni durante el onboarding (el pet default es solo
    //     transitorio; la selección real la fija la completitud).
    if (selectionRepaired && !bypassOwnershipResolution) {
        appState_.activePetId = loadedIdentity.petId;
        appState_.activeVariantId = loadedIdentity.variantId;
        persistenceScheduler_.MarkDirty(static_cast<double>(SDL_GetTicks()));
        SDL_Log(
            "nimvlets: persisted pet selection repaired to '%s'%s%s", loadedIdentity.petId.c_str(),
            loadedIdentity.variantId.empty() ? "" : "/", loadedIdentity.variantId.c_str());
    }

    // Identidad de catálogo del pet realmente activo esta sesión (cubre
    // NIMVLETS_DEV_SELECT_PET, que no escribe appState_). Fuente de
    // verdad para la Collection, el invariante de propiedad, y el menú.
    activeCatalogIdentity_ = loadedIdentity;

    const SDL_WindowFlags flags =
        SDL_WINDOW_TRANSPARENT |
        SDL_WINDOW_BORDERLESS |
        SDL_WINDOW_ALWAYS_ON_TOP |
        SDL_WINDOW_UTILITY |
        SDL_WINDOW_NOT_FOCUSABLE |
        SDL_WINDOW_HIGH_PIXEL_DENSITY;

    window_ = SDL_CreateWindow(
        "Nimvlets Foundation Spike",
        EffectiveCanvasWidth(),
        EffectiveCanvasHeight(),
        flags);
    if (window_ == nullptr) {
        SDL_Log("nimvlets: SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    if (appState_.lastWindowPosition.has_value()) {
        if (!SDL_SetWindowPosition(window_, appState_.lastWindowPosition->x, appState_.lastWindowPosition->y)) {
            SDL_Log(
                "nimvlets: could not restore saved window position (%d, %d): %s -- this window system "
                "does not support positioning a normal window here; the saved position is kept in the "
                "state file for a session/platform where it does work",
                appState_.lastWindowPosition->x, appState_.lastWindowPosition->y, SDL_GetError());
        }
    } else {
        SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }

    {
        const char* devOverride = std::getenv(kDevRendererDriverEnvVar);
        const platform::RendererPlatform currentPlatform = platform::CurrentRendererPlatform();
        const char* preferredDriver = platform::PreferredRendererDriverName(currentPlatform, devOverride);

        renderer_ = SDL_CreateRenderer(window_, preferredDriver);
        bool preferredDriverHonored = renderer_ != nullptr;
        if (renderer_ == nullptr && preferredDriver != nullptr) {
            // Fallback documentado, nunca silencioso: si el driver
            // preferido (p. ej. "software" en macOS, ver DEC-083, o
            // cualquier NIMVLETS_DEV_RENDERER_DRIVER inválido) no pudo
            // crear un renderer en ESTA máquina -- SDL no lo tiene
            // compilado, o algún otro fallo real -- se cae al default
            // histórico de SDL en vez de fallar el arranque entero por
            // una preferencia que no es esencial para funcionar.
            SDL_Log(
                "nimvlets: preferred renderer driver '%s' failed (%s); falling back to SDL's own default driver",
                preferredDriver, SDL_GetError());
            renderer_ = SDL_CreateRenderer(window_, nullptr);
        }
        if (renderer_ == nullptr) {
            SDL_Log("nimvlets: SDL_CreateRenderer failed: %s", SDL_GetError());
            return false;
        }
        const bool devOverrideRequested = devOverride != nullptr && devOverride[0] != '\0';
        SDL_Log("nimvlets: renderer driver selected: '%s'%s", SDL_GetRendererName(renderer_),
                (devOverrideRequested && preferredDriverHonored) ? " (NIMVLETS_DEV_RENDERER_DRIVER override)" : "");
    }

    SDL_SetRenderLogicalPresentation(
        renderer_,
        EffectiveCanvasWidth(),
        EffectiveCanvasHeight(),
        SDL_LOGICAL_PRESENTATION_LETTERBOX);

    platform::ConfigureCompanionWindow(window_);


    animController_.emplace(pet_);
    lastKnownStateId_ = animController_->CurrentStateId();

    UpdateDirectionFromWindowPosition();

    // Hecho de runtime, no de plataforma: qué driver terminó eligiendo
    // SDL de verdad (ver el log "renderer driver selected" arriba), no
    // cuál pedimos. Ambas funciones de políticas de click-through abajo
    // lo necesitan porque Block 05 encontró que SDL_SetWindowShape()
    // corrompe el render bajo el driver "software" en macOS -- ver
    // NativeShapeHitTestIsRenderSafe() en platform/TransparentWindowSupport.h
    // para la evidencia completa.
    const bool usingSoftwareRenderer = SDL_strcmp(SDL_GetRendererName(renderer_), SDL_SOFTWARE_RENDERER) == 0;
    usingNativeShapeHitTest_ = platform::NativeShapeHitTestIsRenderSafe(usingSoftwareRenderer);
    usingPollDrivenClickThrough_ =
        !usingNativeShapeHitTest_ && platform::ClickThroughPollingIsMeaningful(usingSoftwareRenderer);

    // Antes de escribir el estado nativo por primera vez: hacer que la
    // política per-pixel de Nimvlets sea la ÚNICA escritora de esa
    // propiedad. En macOS bajo el renderer de software esto es lo que
    // convierte el mecanismo de "un poll que pelea contra el backend
    // Cocoa de SDL y pierde" en una decisión autoritativa -- ver
    // platform::MakeClickThroughAuthoritative() para la causa raíz
    // completa. En Windows/Linux es un no-op declarado (no hay otro
    // escritor), así que esta llamada es incondicional y sin #ifdef.
    if (usingPollDrivenClickThrough_) {
        clickThroughOwnershipInstalled_ = platform::MakeClickThroughAuthoritative(window_);
    }

    SDL_Log(
        "nimvlets: click-through mechanism = %s",
        usingNativeShapeHitTest_
            ? "native SDL_SetWindowShape (event-driven, no polling)"
            : (usingPollDrivenClickThrough_
                   ? (clickThroughOwnershipInstalled_
                          ? "Nimvlets-owned per-pixel state (authoritative; cursor sampled only while the cursor is inside the window)"
                          : "Nimvlets-driven per-pixel state (no ownership guard needed on this platform)")
                   : "none available (see docs/LINUX_PLATFORM.md) -- relying on IsPointInteractive() app-side gating only"));

    RenderFrame();
    ApplyCurrentHitMask();
    needsRedraw_ = false;

    if (usingPollDrivenClickThrough_) {
        // Estado inicial resuelto contra la posición REAL del cursor:
        // sin esto, el primer estado sería el default de
        // currentlyClickThrough_ (false) hasta el primer evento, y con
        // el muestreo apagado (arranca desarmado) ese primer evento
        // podría no llegar nunca si el cursor ya está quieto encima.
        const CursorSample startupSample = SampleCursor(window_);
        UpdateClickThrough(
            IsPointInsideWindow(startupSample.localPoint), IsPointInteractive(startupSample.localPoint));
    }

    RearmAmbientDeadline(static_cast<double>(SDL_GetTicks()));

    passiveActionRng_.seed(std::random_device{}());

    // --- Preferencia de opacidad de usuario (Block 06) ---
    // 0 = sin preferencia guardada -> 100% (totalmente opaco).
    {
        const int pct =
            appState_.opacityPercent == 0 ? 100 : core::NormalizeOpacityPercent(static_cast<int>(appState_.opacityPercent));
        SDL_SetWindowOpacity(window_, core::OpacityFraction(pct));
    }

    // --- Monitor de clics globales, OPT-IN (Block 11A) --------------
    //
    // El adapter se CREA siempre (Settings tiene que poder reportar la
    // capacidad real de esta plataforma), pero no instala absolutamente
    // nada hasta un Start() explícito. SyncGlobalClickMonitor() más
    // abajo solo arranca si el owner ya había pedido "Anywhere" Y el
    // permiso YA está concedido — solo preflight, jamás un prompt al
    // arrancar (brief §8).
    globalClickMonitor_ = platform::CreateGlobalClickMonitor();
    globalClickUserEventType_ = SDL_RegisterEvents(1);
    if (globalClickUserEventType_ == static_cast<std::uint32_t>(-1)) {
        globalClickUserEventType_ = 0;
        SDL_Log("nimvlets: SDL_RegisterEvents failed -- global click counting disabled this run");
    }
    // Reconcilia con lo que el owner haya dejado persistido. Solo
    // PREFLIGHT: si el permiso está, arranca; si no, cae a local y lo
    // reporta. Sin diálogo de permiso al arrancar, nunca (brief §8).
    SyncGlobalClickMonitor();

    // --- System Shell: menú rápido nativo (Block 06) ---
    shellUserEventType_ = SDL_RegisterEvents(1);
    if (shellUserEventType_ == 0 || shellUserEventType_ == static_cast<std::uint32_t>(-1)) {
        shellUserEventType_ = 0;
        SDL_Log("nimvlets: SDL_RegisterEvents failed -- quick menu actions disabled this run");
    } else {
        systemShell_ = platform::CreateSystemShell();
        if (systemShell_->Install(shellUserEventType_)) {
            PushShellState();
        } else {
            SDL_Log("nimvlets: no native system-shell menu on this platform (see docs/PRODUCT_UI.md)");
        }
    }

    int logicalW = 0;
    int logicalH = 0;
    int pixelW = 0;
    int pixelH = 0;
    SDL_GetWindowSize(window_, &logicalW, &logicalH);
    SDL_GetWindowSizeInPixels(window_, &pixelW, &pixelH);
    SDL_Log(
        "nimvlets: window size logical=%dx%d pixels=%dx%d (pixel density=%.2f, display scale=%.2f)",
        logicalW, logicalH, pixelW, pixelH,
        static_cast<double>(SDL_GetWindowPixelDensity(window_)),
        static_cast<double>(SDL_GetWindowDisplayScale(window_)));

    const content::BehaviorState& startupState = animController_->CurrentState();
    std::string ambientDescription = "no ambient action for this state";
    if (!startupState.ambientActions.empty()) {
        char ambientBuf[96];
        std::snprintf(
            ambientBuf, sizeof(ambientBuf), "ambient action every ~%.0fs", ComputeEffectiveAmbientIntervalSeconds(startupState));
        ambientDescription = ambientBuf;
    }
    SDL_Log(
        "nimvlets: pet '%s' (%s) ready — native canvas %dx%d x visualScale=%.3f = %dx%d on screen, "
        "alpha hit threshold=%d/255, state='%s' (%s). Click the shape; drag to move; close the window to "
        "quit. Click balance: %llu (%s).",
        pet_.id.c_str(), pet_.displayName.c_str(), pet_.canvasWidth, pet_.canvasHeight, pet_.visualScale,
        EffectiveCanvasWidth(), EffectiveCanvasHeight(), static_cast<int>(pet_.alphaHitThreshold),
        startupState.id.c_str(), ambientDescription.c_str(),
        static_cast<unsigned long long>(appState_.clickBalance),
        appStateStore_.has_value() ? "persisted locally" : "persistence unavailable this run");

    // --- Block 09A: arrancar en el gate de onboarding ---------------
    //
    // La ventana del pet queda OCULTA (el usuario nuevo no posee ningún
    // Nimvlet todavía; el pet default se cargó solo para dimensionar la
    // ventana). El Product UI se abre en modo onboarding y NO se puede
    // saltear (brief §19). El deadline de los 44 s se arma acá — cuando
    // la pantalla ya está activa y capaz de recibir input (brief §11).
    if (onboardingActive_) {
        petHidden_ = true;
        SDL_HideWindow(window_);

        if (productWindow_.Open(catalog_)) {
            productWindow_.SetLanguage(language_);
            productWindow_.SetPreferences(CurrentPreferences());
            productWindow_.EnterOnboarding(onboardingOffer_);
            productWindow_.FocusWindow();
        } else {
            SDL_Log("nimvlets: FATAL: could not open the Product UI for onboarding");
            return false;
        }

        double revealMs = catalog::kSecretRevealDwellMs;
        if (const char* env = std::getenv("NIMVLETS_DEV_ONBOARDING_REVEAL_MS");
            env != nullptr && env[0] != '\0') {
            char* end = nullptr;
            const double parsed = std::strtod(env, &end);
            if (end != env && parsed >= 0.0) {
                revealMs = parsed;
                SDL_Log("nimvlets: DEV override active — NIMVLETS_DEV_ONBOARDING_REVEAL_MS=%.0f", revealMs);
            }
        }
        onboardingRevealDeadlineMs_ = static_cast<double>(SDL_GetTicks()) + revealMs;

        // QA: revelar el secreto de una / forzar una etapa, para
        // capturas — sin correr el event loop.
        if (const char* rv = std::getenv("NIMVLETS_DEV_ONBOARDING_REVEAL");
            rv != nullptr && rv[0] != '\0' && rv[0] != '0') {
            RevealOnboardingSecret(static_cast<double>(SDL_GetTicks()));
        }
        if (const char* st = std::getenv("NIMVLETS_DEV_ONBOARDING_STAGE"); st != nullptr && st[0] != '\0') {
            const std::string spec(st);
            const std::size_t colon = spec.find(':');
            const std::string stageName = colon == std::string::npos ? spec : spec.substr(0, colon);
            const std::string focusId = colon == std::string::npos ? std::string() : spec.substr(colon + 1);
            const productui::OnboardingStage stage =
                stageName == "confirm"   ? productui::OnboardingStage::kConfirm
                : stageName == "variant" ? productui::OnboardingStage::kFrinVariant
                                         : productui::OnboardingStage::kBrowse;
            productWindow_.SetOnboardingStageForQA(stage, focusId);
        }

        SDL_Log(
            "nimvlets: onboarding ARMED — %zu normal starter(s), secret=%s; reveal in %.0f ms of session dwell",
            onboardingOffer_.normal.size(), onboardingOffer_.secret.has_value() ? "yes" : "no", revealMs);
    }

    return true;
}

void SpikeApp::ResolveOnboarding(bool saveFileExisted) {
    // Un estado que YA pasó por la inicialización de propiedad VIEJA
    // (`ownershipSeeded` — siembra por catálogo, no onboarding) pero
    // quedó en kPending es un usuario dev/legacy: se lo considera YA
    // onboardeado. Se normaliza a kLegacyComplete acá, así el schema v5
    // refleja la verdad y Block 09B no lo trata como usuario nuevo (brief
    // §3.A / §17). Un usuario genuinamente nuevo (sin archivo,
    // !ownershipSeeded) mantiene kPending hasta la transacción de
    // completitud. NO es "inferir completitud de la propiedad" (brief
    // §3): `ownershipSeeded` es un flag EXPLÍCITO de "hubo init".
    if (saveFileExisted &&
        appState_.onboardingLifecycle == persistence::OnboardingLifecycle::kPending &&
        appState_.ownershipSeeded) {
        appState_.onboardingLifecycle = persistence::OnboardingLifecycle::kLegacyComplete;
        persistenceScheduler_.MarkDirty(static_cast<double>(SDL_GetTicks()));
        SDL_Log("nimvlets: legacy/dev state (seeded, pending) normalized to onboarding=legacy-complete");
    }

    const bool devOnboarding = [] {
        const char* v = std::getenv(kDevOnboardingEnvVar);
        return v != nullptr && v[0] != '\0' && v[0] != '0';
    }();
    const catalog::OnboardingReadiness readiness = catalog::EvaluateCatalogOnboardingReadiness(catalog_);
    const bool pending =
        appState_.onboardingLifecycle == persistence::OnboardingLifecycle::kPending;

    if (devOnboarding) {
        // El harness DEV fuerza el gate SOLO si el catálogo cargado se
        // declaró SINTÉTICO-DEV al compilarse (packs/previews ALIAS de
        // otros Nimvlets — `dev_synthetic_onboarding` en el manifest) y
        // el lifecycle sigue en kPending (un dir de app-data aislado y
        // fresco -> kPending; un run previo incompleto se re-entra; un
        // onboarding COMPLETADO lo detiene). Exigir el marcador
        // sintético mantiene la separación limpia con el camino de
        // producción: un alias nunca llega ahí, y un catálogo REAL bajo
        // NIMVLETS_DEV_ONBOARDING no se fuerza a un onboarding cuyo
        // contenido no coincide con la identidad de los starters
        // (DEC-133).
        const bool syntheticCatalog = catalog_.DevSyntheticOnboarding();
        onboardingActive_ = syntheticCatalog && pending;
        if (!syntheticCatalog) {
            SDL_Log(
                "nimvlets: %s set but the loaded catalog is not a dev-synthetic onboarding "
                "catalog (dev_synthetic_onboarding=0); not forcing the onboarding gate",
                kDevOnboardingEnvVar);
        } else if (!onboardingActive_) {
            SDL_Log("nimvlets: %s set but onboarding already completed (lifecycle != pending); normal startup",
                    kDevOnboardingEnvVar);
        }
    } else {
        // Producción: onboarding ARMADO (contenido de starters listo) +
        // usuario genuinamente nuevo (kPending + sin archivo de estado
        // previo). Un archivo corrupto (`saveFileExisted`) se trata como
        // usuario existente, NUNCA se onboardea (brief §4/§27).
        onboardingActive_ = readiness.armed && pending && !saveFileExisted;
        if (readiness.armed && pending && saveFileExisted) {
            SDL_Log(
                "nimvlets: onboarding armed and lifecycle pending, but a state file exists "
                "(recovered) -- treating as an existing user; not onboarding");
        }
    }

    if (!onboardingActive_) {
        if (!readiness.armed && !devOnboarding) {
            SDL_Log("nimvlets: production onboarding not armed (%s)", readiness.reason.c_str());
        }
        return;
    }

    onboardingOffer_ = catalog::BuildOnboardingOffer(catalog_);
    onboardingOffer_.secretRevealed = false;
    SDL_Log(
        "nimvlets: entering ONBOARDING gate (lifecycle=pending%s)",
        devOnboarding ? ", DEV harness" : ", production content ready");
}

void SpikeApp::RevealOnboardingSecret(double nowMs) {
    if (!onboardingActive_ || onboardingOffer_.secretRevealed) {
        onboardingRevealDeadlineMs_.reset();
        return;
    }
    onboardingOffer_.secretRevealed = true;
    productWindow_.RevealOnboardingSecret();
    onboardingRevealDeadlineMs_.reset();
    SDL_Log("nimvlets: onboarding — secret candidate revealed after ~%.0f ms of session dwell",
            nowMs);
}

void SpikeApp::HandleOnboardingSelection(const catalog::PetIdentity& selection) {
    // Idempotencia (brief §15): una vez fuera del gate, cualquier pedido
    // que haya quedado encolado no hace nada.
    if (!onboardingActive_) {
        return;
    }

    const bool alreadyComplete =
        persistence::OnboardingConsideredComplete(appState_.onboardingLifecycle);
    const catalog::OnboardingGrant grant =
        catalog::EvaluateOnboardingSelection(onboardingOffer_, selection, alreadyComplete);

    if (grant.result != catalog::OnboardingSelectionResult::kOk) {
        SDL_Log(
            "nimvlets: onboarding selection '%s'%s%s not applied (%s)", selection.petId.c_str(),
            selection.variantId.empty() ? "" : "/", selection.variantId.c_str(),
            catalog::ToString(grant.result));
        return;
    }

    // --- Transacción de completitud (brief §14): balance 0 + grant
    //     EXACTO + activo + lifecycle + ownershipSeeded en el MISMO
    //     AppState, una sola escritura atómica. Un crash no puede dejar
    //     "completado sin starter" ni "starter sin completar".
    std::vector<catalog::PetEntitlement> owned = {grant.entitlement};
    catalog::CanonicalizePetEntitlements(owned);
    appState_.clickBalance = grant.newBalance;  // 0 — un usuario nuevo no recibe clics (brief §16)
    appState_.ownedEntitlements = ToPersistedEntitlements(owned);
    appState_.ownershipSeeded = true;
    appState_.activePetId = grant.activeIdentity.petId;
    appState_.activeVariantId = grant.activeIdentity.variantId;
    appState_.onboardingLifecycle = persistence::OnboardingLifecycle::kCompleted;
    persistenceScheduler_.MarkDirty(static_cast<double>(SDL_GetTicks()));
    FlushPersistedState();  // inmediato, atómico — igual que una compra del Shop (DEC-126)

    SDL_Log(
        "nimvlets: onboarding COMPLETE — starter '%s'%s%s granted, balance 0, active set, lifecycle=completed",
        grant.entitlement.petId.c_str(), grant.entitlement.variantId.empty() ? "" : "/",
        grant.entitlement.variantId.c_str());

    // Salir del gate y entrar a Nimvlets normal: cargar el starter,
    // mostrar la ventana del pet, cerrar el onboarding.
    onboardingActive_ = false;
    onboardingRevealDeadlineMs_.reset();
    productWindow_.ExitOnboarding();
    productWindow_.Close();

    if (TrySwitchActivePet(grant.activeIdentity)) {
        activeCatalogIdentity_ = grant.activeIdentity;
    } else {
        SDL_Log("nimvlets: onboarding: starter pack failed to load; the granted entitlement stands");
    }
    petHidden_ = false;
    SDL_ShowWindow(window_);
    MarkNeedsRedraw(static_cast<double>(SDL_GetTicks()));
    PushShellState();
}

void SpikeApp::FlushPersistedState() {
    if (!appStateStore_.has_value() || !persistenceScheduler_.IsDirty()) {
        return;
    }
    std::string error;
    if (appStateStore_->Save(appState_, error)) {
        persistenceScheduler_.OnFlushSucceeded();
    } else {
        SDL_Log("nimvlets: failed to save app state: %s", error.c_str());
        persistenceScheduler_.OnFlushFailed(static_cast<double>(SDL_GetTicks()));
    }
}

bool SpikeApp::TrySwitchActivePet(const catalog::PetIdentity& target) {
    // Gate de propiedad (defensa en profundidad): NUNCA se pone en el
    // escritorio una identidad que el owner no tiene autorizada, y este
    // switch JAMÁS otorga propiedad — establecer propiedad es cosa de la
    // siembra / migración / compra, no del switch de runtime (brief §4,
    // DEC-128). HandleActivateRequest ya gatea con CanActivate; esto
    // cubre además el smoke test solo-DEV (que itera todas las entradas
    // del catálogo, incluidas las no poseídas).
    if (!catalog::OwnsIdentity(CurrentEntitlements(), target)) {
        SDL_Log(
            "nimvlets: switch to '%s'%s%s refused: not owned (current pet unchanged)",
            target.petId.c_str(), target.variantId.empty() ? "" : "/", target.variantId.c_str());
        return false;
    }

    content::PetDefinition newPet;
    std::string error;
    if (!catalog::LoadPetForIdentity(catalog_, target, newPet, error)) {
        SDL_Log(
            "nimvlets: switch to '%s'%s%s failed: %s (current pet unchanged)",
            target.petId.c_str(), target.variantId.empty() ? "" : "/", target.variantId.c_str(), error.c_str());
        return false;
    }

    // Un pet nuevo tiene (potencialmente) otras dimensiones de frame:
    // se descarta la textura activa para que RenderFrame() la vuelva a
    // crear con el tamaño correcto en el próximo dibujo.
    activeFrameTexture_.Reset();
    animController_.reset();
    pet_ = std::move(newPet);
    animController_.emplace(pet_);  // arranca en states[0]/kBase del pet nuevo
    lastKnownStateId_ = animController_->CurrentStateId();

    UpdateDirectionFromWindowPosition();

    SDL_SetWindowSize(window_, EffectiveCanvasWidth(), EffectiveCanvasHeight());
    SDL_SetRenderLogicalPresentation(renderer_, EffectiveCanvasWidth(), EffectiveCanvasHeight(), SDL_LOGICAL_PRESENTATION_LETTERBOX);

    appState_.activePetId = target.petId;
    appState_.activeVariantId = target.variantId;
    activeCatalogIdentity_ = target;
    // La propiedad NO se toca acá: el gate de arriba ya garantizó que
    // `target` está autorizado, y el switch nunca otorga nada.
    persistenceScheduler_.MarkDirty(static_cast<double>(SDL_GetTicks()));
    RearmAmbientDeadline(static_cast<double>(SDL_GetTicks()));
    MarkNeedsRedraw(static_cast<double>(SDL_GetTicks()));

    SDL_Log(
        "nimvlets: switched active pet to '%s'%s%s ('%s')",
        pet_.id.c_str(), pet_.variantGroup.empty() ? "" : "/", pet_.variantGroup.c_str(), pet_.displayName.c_str());
    return true;
}

std::vector<catalog::PetEntitlement> SpikeApp::CurrentEntitlements() const {
    return ToCatalogEntitlements(appState_.ownedEntitlements);
}

catalog::CollectionModel SpikeApp::BuildCurrentCollectionModel() const {
    return catalog::BuildCollectionModel(catalog_, CurrentEntitlements(), activeCatalogIdentity_);
}

catalog::ShopModel SpikeApp::BuildCurrentShopModel() const {
    return catalog::BuildShopModel(catalog_, appState_.clickBalance, CurrentEntitlements());
}

bool SpikeApp::OnboardingLifecycleCompleted() const {
    // EXACTAMENTE kCompleted — un usuario que pasó por la elección real
    // de starter. NO kLegacyComplete (dev/legacy) ni kPending (brief §5).
    return appState_.onboardingLifecycle == persistence::OnboardingLifecycle::kCompleted;
}

catalog::StarterShopModel SpikeApp::BuildCurrentStarterShopModel() const {
    return catalog::BuildStarterShopModel(
        catalog_, OnboardingLifecycleCompleted(), CurrentEntitlements(), appState_.clickBalance);
}

content::FrameDefinition SpikeApp::CurrentRestFrame() const {
    if (pet_.states.empty()) {
        return content::FrameDefinition{};
    }
    const content::BehaviorState& state = pet_.states.front();
    const content::AnimationDefinition& anim = content::ResolveAnimation(
        state.baseAnimation, state.baseAnimationDirectionOverrides, content::Direction::kRight);
    if (anim.frames.empty()) {
        return content::FrameDefinition{};
    }
    return anim.frames.front();  // copia -- un solo frame, solo al abrir la Collection
}

void SpikeApp::PushModelsToProductWindow() {
    if (productWindow_.IsOpen()) {
        productWindow_.SetModels(
            BuildCurrentCollectionModel(), BuildCurrentShopModel(), BuildCurrentStarterShopModel(),
            appState_.clickBalance);
    }
}

void SpikeApp::PushShellState() {
    if (!systemShell_) {
        return;
    }
    platform::ShellState state;
    state.currentPetName = pet_.displayName;
    state.petHidden = petHidden_;
    state.lockPosition = appState_.lockPosition;
    state.sizeChoiceId = appState_.sizeChoice.empty() ? "medium" : appState_.sizeChoice;
    state.opacityPercent =
        appState_.opacityPercent == 0 ? 100 : static_cast<int>(appState_.opacityPercent);
    state.language = language_;
    systemShell_->SetState(state);
}

// --- Block 08: ruta canónica de preferencias -----------------------
//
// El menú rápido y la sección Settings llaman EXACTAMENTE a estos cuatro
// Apply*. No hay una segunda ruta con reglas propias de persistencia o
// runtime (brief §6, DEC-130).

core::Preferences SpikeApp::CurrentPreferences() const {
    core::Preferences p = core::PreferencesFromStored(
        appState_.sizeChoice, appState_.opacityPercent, appState_.lockPosition, appState_.language,
        appState_.clickCountingMode);
    // El idioma EFECTIVO de la sesión puede diferir de
    // appState_.language: vacío -> derivado del locale del OS sin
    // persistir; o un override solo-DEV. language_ manda para lo que se
    // MUESTRA.
    p.language = language_;
    return p;
}

void SpikeApp::PushPreferencesToProductWindow() {
    if (!productWindow_.IsOpen()) {
        return;
    }
    const core::Preferences prefs = CurrentPreferences();
    productWindow_.SetPreferences(prefs);
    // Block 11A: junto con las preferencias va el estado GENÉRICO del
    // conteo global (capacidad / permiso / actividad ya derivados a
    // platform::GlobalClickUiState). Settings nunca consulta el adapter
    // por su cuenta ni conoce la plataforma (brief §18).
    productWindow_.SetGlobalClick(
        platform::ResolveGlobalClickUiState(prefs.clickCounting, CurrentGlobalClickStatus()),
        globalClickExplanationVisible_);
}

// --- Block 11A: conteo de clics OPT-IN en todo el sistema -----------

platform::GlobalClickStatus SpikeApp::CurrentGlobalClickStatus() const {
    if (!globalClickMonitor_) {
        return platform::GlobalClickStatus{};  // kUnavailable
    }
    return globalClickMonitor_->QueryStatus();  // preflight: NUNCA muestra un diálogo
}

core::EffectiveClickCounting SpikeApp::CurrentEffectiveClickCounting() const {
    const bool monitorActive = globalClickMonitor_ && globalClickMonitor_->IsActive();
    return core::ResolveEffectiveClickCounting(
        core::ParseClickCountingMode(appState_.clickCountingMode), monitorActive);
}

void SpikeApp::HandleCountedClick(core::ClickSource source, double nowMs) {
    if (!core::CountedClickShouldIncrement(CurrentEffectiveClickCounting(), source)) {
        // En modo global efectivo un clic sobre el pet NO suma: el
        // monitor global ya vio ESE MISMO clic físico y lo va a contar
        // una sola vez (brief §4). La reacción del pet no pasa por acá.
        return;
    }
    // Misma mutación de siempre: uint64, mismo debounce de persistencia,
    // mismo refresco del wallet canónico del Product UI. NO existe un
    // segundo wallet ni un contador por fuente (brief §13).
    ++appState_.clickBalance;
    persistenceScheduler_.MarkDirty(nowMs);
    PushModelsToProductWindow();
}

void SpikeApp::OnGlobalPrimaryClick(void* userData) {
    // **Hilo del monitor.** Se hace lo mínimo indispensable: empujar un
    // evento vacío. Nada de AppState, nada de persistencia, nada de UI.
    auto* self = static_cast<SpikeApp*>(userData);
    if (self == nullptr || self->globalClickUserEventType_ == 0) {
        return;
    }
    SDL_Event event;
    SDL_zero(event);
    event.type = self->globalClickUserEventType_;
    event.user.type = self->globalClickUserEventType_;
    SDL_PushEvent(&event);
}

void SpikeApp::SyncGlobalClickMonitor() {
    if (!globalClickMonitor_) {
        return;
    }
    const bool wantGlobal =
        core::ParseClickCountingMode(appState_.clickCountingMode) == core::ClickCountingMode::kAnywhere;

    if (!wantGlobal) {
        if (globalClickMonitor_->IsActive()) {
            globalClickMonitor_->Stop();
            SDL_Log("nimvlets: global click monitor stopped (click counting -> Nimvlet only)");
        }
        return;
    }
    if (globalClickMonitor_->IsActive()) {
        return;
    }

    const platform::GlobalClickStatus status = globalClickMonitor_->QueryStatus();
    if (status.capability == platform::GlobalClickCapability::kUnavailable) {
        SDL_Log("nimvlets: global click counting is not available on this system (see docs/GLOBAL_CLICK_MODE.md)");
        return;
    }
    if (status.capability == platform::GlobalClickCapability::kSupportedNeedsPermission &&
        status.permission != platform::GlobalClickPermission::kGranted) {
        // PREFLIGHT solamente. Nunca se pide el permiso desde acá — por
        // eso arrancar la app con "Anywhere" persistido y el permiso
        // revocado NO muestra ningún prompt de TCC (brief §8): cae a
        // modo local y Settings lo dice.
        SDL_Log(
            "nimvlets: global click counting requested but '%s' permission is not granted — "
            "counting locally (see Settings)",
            status.permissionName.c_str());
        return;
    }

    if (globalClickUserEventType_ == 0) {
        SDL_Log("nimvlets: global click monitor cannot start — no SDL user event type registered");
        return;
    }
    if (globalClickMonitor_->Start(&SpikeApp::OnGlobalPrimaryClick, this)) {
        SDL_Log("nimvlets: global click monitor ACTIVE — primary mouse presses anywhere now count");
    } else {
        SDL_Log("nimvlets: global click monitor failed to start — counting locally");
    }
}

void SpikeApp::HandleGlobalClickAction(productui::GlobalClickAction action) {
    switch (action) {
        case productui::GlobalClickAction::kNone:
            return;
        case productui::GlobalClickAction::kNotNow:
            // Se cierra la explicación y NO se pide nada. La preferencia
            // sigue en "Nimvlet only" — nunca se cambió.
            globalClickExplanationVisible_ = false;
            PushPreferencesToProductWindow();
            return;
        case productui::GlobalClickAction::kContinue: {
            globalClickExplanationVisible_ = false;
            if (globalClickMonitor_) {
                // EL ÚNICO llamado de todo el programa que puede
                // provocar el diálogo de permiso del OS, y solo tras un
                // "Continue" explícito del owner sobre la explicación.
                globalClickMonitor_->RequestPermission();
            }
            // La elección del owner se PERSISTE aunque el permiso quede
            // pendiente: en macOS el prompt solo ofrece abrir Ajustes
            // del Sistema, así que "concedido ahora mismo" es la
            // excepción, no la regla. Si se revirtiera la preferencia
            // acá, el owner concedería el permiso y volvería a encontrar
            // el control de vuelta en "Nimvlet only". Ver
            // docs/GLOBAL_CLICK_MODE.md §4.
            ApplyClickCountingMode(core::ClickCountingMode::kAnywhere);
            return;
        }
        case productui::GlobalClickAction::kCheckAgain:
            // Re-preflight + reintento de arranque. No pide permiso.
            SyncGlobalClickMonitor();
            PushPreferencesToProductWindow();
            return;
    }
}

void SpikeApp::ApplySizeChoice(core::PetSizeChoice choice) {
    const double nowMs = static_cast<double>(SDL_GetTicks());
    appState_.sizeChoice = core::PetSizeChoiceId(choice);
    persistenceScheduler_.MarkDirty(nowMs);
    ApplyPetWindowMetrics();  // SDL_SetWindowSize + presentación lógica + hit-mask + redraw
    SDL_Log("nimvlets: pet size -> %s (%dx%d on screen)", appState_.sizeChoice.c_str(),
            EffectiveCanvasWidth(), EffectiveCanvasHeight());
    PushShellState();
    PushPreferencesToProductWindow();
}

void SpikeApp::ApplyOpacityChoice(int rawPercent) {
    const double nowMs = static_cast<double>(SDL_GetTicks());
    const int pct = core::NormalizeOpacityPercent(rawPercent);  // {100,85,70,55} — nunca un valor imposible
    appState_.opacityPercent = static_cast<std::uint32_t>(pct);
    persistenceScheduler_.MarkDirty(nowMs);
    SDL_SetWindowOpacity(window_, core::OpacityFraction(pct));
    SDL_Log("nimvlets: pet opacity -> %d%%", pct);
    PushShellState();
    PushPreferencesToProductWindow();
}

void SpikeApp::ApplyLockPosition(bool locked) {
    const double nowMs = static_cast<double>(SDL_GetTicks());
    appState_.lockPosition = locked;
    persistenceScheduler_.MarkDirty(nowMs);
    SDL_Log("nimvlets: pet position %s", appState_.lockPosition ? "LOCKED (drag disabled)" : "unlocked");
    PushShellState();
    PushPreferencesToProductWindow();
}

void SpikeApp::ApplyUiLanguage(core::Language language) {
    const double nowMs = static_cast<double>(SDL_GetTicks());
    if (language != language_ || appState_.language.empty()) {
        language_ = language;
        // Una elección EXPLÍCITA sí se persiste (a partir de acá gana
        // siempre sobre el locale del OS — block brief 06.1 §5).
        appState_.language = core::LanguageId(language_);
        persistenceScheduler_.MarkDirty(nowMs);
    }
    SDL_Log("nimvlets: UI language -> %s", core::LanguageId(language_));
    // Refresco inmediato, sin reiniciar: menú + las tres secciones del
    // Product UI (block brief 08 §17).
    PushShellState();
    productWindow_.SetLanguage(language_);
    PushPreferencesToProductWindow();
}

void SpikeApp::ApplyClickCountingMode(core::ClickCountingMode mode) {
    const double nowMs = static_cast<double>(SDL_GetTicks());
    appState_.clickCountingMode = core::ClickCountingModeId(mode);
    persistenceScheduler_.MarkDirty(nowMs);
    // Reconcilia el monitor con la preferencia recién escrita. Puede
    // quedar INACTIVO con la preferencia en "anywhere" (permiso
    // pendiente): eso es exactamente el modo pedido != modo efectivo, y
    // Settings lo muestra.
    SyncGlobalClickMonitor();
    SDL_Log(
        "nimvlets: click counting -> %s (effective: %s)", core::ClickCountingModeId(mode),
        CurrentEffectiveClickCounting() == core::EffectiveClickCounting::kGlobal ? "global" : "local");
    // A propósito SIN PushShellState(): el menú rápido no expone esta
    // preferencia y no debe crecer con ella (brief §10).
    PushPreferencesToProductWindow();
}

void SpikeApp::ApplyPreferenceChange(const productui::SettingsChange& change) {
    switch (change.field) {
        case core::PreferenceField::kSize:
            ApplySizeChoice(change.size);
            break;
        case core::PreferenceField::kOpacity:
            ApplyOpacityChoice(change.opacityPercent);
            break;
        case core::PreferenceField::kLockPosition:
            ApplyLockPosition(change.lockPosition);
            break;
        case core::PreferenceField::kLanguage:
            ApplyUiLanguage(change.language);
            break;
        case core::PreferenceField::kClickCounting:
            if (change.clickCounting == core::ClickCountingMode::kNimvletOnly) {
                // Volver a local es inmediato y nunca necesita permiso.
                globalClickExplanationVisible_ = false;
                ApplyClickCountingMode(core::ClickCountingMode::kNimvletOnly);
                break;
            }
            // Pedir "Anywhere" NO aplica nada todavía: primero se
            // consulta la política pura de si hace falta explicar
            // (brief §8). La preferencia solo cambia en kApplyDirectly o
            // tras un "Continue" explícito.
            switch (platform::EvaluateGlobalClickRequest(CurrentGlobalClickStatus())) {
                case platform::GlobalClickRequestOutcome::kUnavailable:
                    // No-op silencioso: Settings ya dibuja el segmento
                    // apagado y dice "Not available on this system".
                    break;
                case platform::GlobalClickRequestOutcome::kApplyDirectly:
                    globalClickExplanationVisible_ = false;
                    ApplyClickCountingMode(core::ClickCountingMode::kAnywhere);
                    break;
                case platform::GlobalClickRequestOutcome::kNeedsExplanation:
                    globalClickExplanationVisible_ = true;
                    PushPreferencesToProductWindow();
                    break;
            }
            break;
    }
}

void SpikeApp::ApplyPetWindowMetrics() {
    SDL_SetWindowSize(window_, EffectiveCanvasWidth(), EffectiveCanvasHeight());
    SDL_SetRenderLogicalPresentation(
        renderer_, EffectiveCanvasWidth(), EffectiveCanvasHeight(), SDL_LOGICAL_PRESENTATION_LETTERBOX);
    UpdateDirectionFromWindowPosition();
    MarkNeedsRedraw(static_cast<double>(SDL_GetTicks()));
}

void SpikeApp::OpenProductWindow() {
    // Durante el onboarding la ventana ya está abierta EN EL GATE:
    // "Collection…" del menú solo la re-enfoca, no cambia a la
    // Collection (brief §19 — no se puede saltear la selección).
    if (onboardingActive_) {
        productWindow_.FocusWindow();
        return;
    }
    if (!productWindow_.Open(catalog_)) {
        SDL_Log("nimvlets: could not open the Product UI window");
        return;
    }
    productWindow_.SetLanguage(language_);
    // Block 08 (preferencias) + Block 11A (estado del conteo global) —
    // el mismo punto único que usa cualquier cambio posterior.
    PushPreferencesToProductWindow();
    productWindow_.SetActivePreview(
        activeCatalogIdentity_.petId, activeCatalogIdentity_.variantId, CurrentRestFrame());
    productWindow_.SetModels(
        BuildCurrentCollectionModel(), BuildCurrentShopModel(), BuildCurrentStarterShopModel(),
        appState_.clickBalance);
    productWindow_.FocusWindow();
}

void SpikeApp::HandleActivateRequest(const productui::ActivateRequest& request) {
    const catalog::CollectionModel model = BuildCurrentCollectionModel();
    if (!catalog::CanActivate(model, request.petId, request.variantId)) {
        SDL_Log(
            "nimvlets: Collection: '%s'%s%s cannot be activated (locked / variant not owned / unknown) -- ignoring",
            request.petId.c_str(), request.variantId.empty() ? "" : "/", request.variantId.c_str());
        return;
    }
    const catalog::PetIdentity target{request.petId, request.variantId};
    if (target == activeCatalogIdentity_) {
        return;  // ya está en el escritorio -- no-op
    }
    if (TrySwitchActivePet(target)) {
        productWindow_.SetActivePreview(
            activeCatalogIdentity_.petId, activeCatalogIdentity_.variantId, CurrentRestFrame());
        PushModelsToProductWindow();
        PushShellState();
    }
}

void SpikeApp::HandlePurchaseRequest(const productui::PurchaseRequest& request) {
    // El objetivo es la IDENTIDAD de catálogo del ítem del Shop, no un
    // petId suelto (DEC-128). En Block 07 `variantId` siempre viene "".
    const catalog::PetIdentity target{request.petId, request.variantId};
    const std::string targetLabel =
        request.petId + (request.variantId.empty() ? "" : ("/" + request.variantId));
    const catalog::PurchaseOutcome outcome = catalog::EvaluatePurchase(
        catalog_, target, appState_.clickBalance, CurrentEntitlements());

    if (outcome.result != catalog::PurchaseResult::kSuccess) {
        SDL_Log("nimvlets: Shop: purchase of '%s' not completed (%s)", targetLabel.c_str(),
                catalog::ToString(outcome.result));
        // Igual se re-empujan los modelos: una confirmación que llegó
        // tarde (p. ej. el balance bajó) debe reflejar el estado real.
        PushModelsToProductWindow();
        return;
    }

    // Aplicación atómica compartida con el Starter Shop oculto (Block 10):
    // balance + propiedad en el MISMO AppState, flush inmediato.
    ApplyPurchasedState(outcome.newBalance, outcome.newEntitlements);

    SDL_Log(
        "nimvlets: Shop: purchased '%s' for %llu click(s) -- balance %llu -> %llu, ownership persisted",
        targetLabel.c_str(), static_cast<unsigned long long>(outcome.price),
        static_cast<unsigned long long>(outcome.price + outcome.newBalance),
        static_cast<unsigned long long>(outcome.newBalance));

    // Refresco inmediato de AMBAS secciones (brief §16). El runtime del
    // pet NO se toca; activar el pet recién comprado es una acción
    // aparte del owner (el botón "Use" de la Collection).
    PushModelsToProductWindow();
}

void SpikeApp::ApplyPurchasedState(
    std::uint64_t newBalance, const std::vector<catalog::PetEntitlement>& newEntitlements) {
    // El MISMO contrato atómico que una compra del Shop público (DEC-126):
    // balance y propiedad en el MISMO AppState, sin escrituras
    // intermedias, un solo SerializeAppState + un solo rename atómico.
    // Un crash no puede persistir "gasté el balance pero no tengo el pet".
    appState_.clickBalance = newBalance;
    appState_.ownedEntitlements = ToPersistedEntitlements(newEntitlements);
    persistenceScheduler_.MarkDirty(static_cast<double>(SDL_GetTicks()));
    // Persistencia INMEDIATA: una compra cambia PROPIEDAD (a diferencia
    // del per-click, que sigue con el debounce de ~2s).
    FlushPersistedState();
}

void SpikeApp::HandleStarterPurchaseRequest(const productui::PurchaseRequest& request) {
    // El objetivo es la IDENTIDAD EXACTA de la oferta del Starter Shop
    // ({petId, variantId}) — para Frin, la VARIANTE concreta. NUNCA un
    // {frin, ""} ni "todo Frin" (brief §13).
    const catalog::PetIdentity target{request.petId, request.variantId};
    const std::string targetLabel =
        request.petId + (request.variantId.empty() ? "" : ("/" + request.variantId));

    // Canal DISTINTO al del Shop público: EvaluateStarterPurchase re-checa
    // INDEPENDIENTEMENTE el lifecycle == kCompleted, el rol de starter, el
    // precio, la regla de no-divulgación del secreto, la propiedad y el
    // saldo (brief §15). El modelo/UI no es un límite de seguridad.
    const catalog::StarterPurchaseOutcome outcome = catalog::EvaluateStarterPurchase(
        catalog_, OnboardingLifecycleCompleted(), target, appState_.clickBalance,
        CurrentEntitlements());

    if (outcome.result != catalog::StarterPurchaseResult::kSuccess) {
        SDL_Log("nimvlets: Starter Shop: purchase of '%s' not completed (%s)", targetLabel.c_str(),
                catalog::ToString(outcome.result));
        PushModelsToProductWindow();  // reflejar el estado real si la confirmación llegó tarde
        return;
    }

    ApplyPurchasedState(outcome.newBalance, outcome.newEntitlements);

    SDL_Log(
        "nimvlets: Starter Shop: purchased '%s' for %llu click(s) -- balance %llu -> %llu, "
        "EXACT entitlement persisted (pet NOT auto-activated)",
        targetLabel.c_str(), static_cast<unsigned long long>(outcome.price),
        static_cast<unsigned long long>(outcome.price + outcome.newBalance),
        static_cast<unsigned long long>(outcome.newBalance));

    // Refresco inmediato: el Starter Shop (la oferta desaparece) y la
    // Collection (la nueva variante ya es usable) sin reiniciar. El pet
    // activo NO cambia (brief §17).
    PushModelsToProductWindow();
}

void SpikeApp::HandleShellAction(int shellActionCode, bool& running) {
    const auto action = static_cast<platform::ShellAction>(shellActionCode);
    const double nowMs = static_cast<double>(SDL_GetTicks());
    switch (action) {
        case platform::ShellAction::kTogglePetVisibility:
            if (onboardingActive_) {
                // El usuario nuevo no posee ningún Nimvlet todavía — no
                // hay pet real que mostrar hasta que elija su starter.
                SDL_Log("nimvlets: Show/Hide ignored during onboarding (no Nimvlet chosen yet)");
                break;
            }
            petHidden_ = !petHidden_;
            if (petHidden_) {
                SDL_HideWindow(window_);
            } else {
                SDL_ShowWindow(window_);
                MarkNeedsRedraw(nowMs);
            }
            SDL_Log("nimvlets: pet window %s (application still running)", petHidden_ ? "hidden" : "shown");
            PushShellState();
            break;

        case platform::ShellAction::kOpenCollection:
            OpenProductWindow();
            break;

        case platform::ShellAction::kToggleLockPosition:
            // El menú rápido es un toggle; Settings manda un bool
            // explícito. Ambos entran por la MISMA ruta canónica.
            ApplyLockPosition(!appState_.lockPosition);
            break;

        case platform::ShellAction::kSetSizeSmall:
        case platform::ShellAction::kSetSizeMedium:
        case platform::ShellAction::kSetSizeLarge:
            ApplySizeChoice(action == platform::ShellAction::kSetSizeSmall
                                ? core::PetSizeChoice::kSmall
                                : (action == platform::ShellAction::kSetSizeLarge
                                       ? core::PetSizeChoice::kLarge
                                       : core::PetSizeChoice::kMedium));
            break;

        case platform::ShellAction::kSetOpacity100:
        case platform::ShellAction::kSetOpacity85:
        case platform::ShellAction::kSetOpacity70:
        case platform::ShellAction::kSetOpacity55:
            ApplyOpacityChoice(action == platform::ShellAction::kSetOpacity100
                                   ? 100
                                   : (action == platform::ShellAction::kSetOpacity85
                                          ? 85
                                          : (action == platform::ShellAction::kSetOpacity70 ? 70 : 55)));
            break;

        case platform::ShellAction::kSetLanguageEn:
        case platform::ShellAction::kSetLanguageEs:
            ApplyUiLanguage(action == platform::ShellAction::kSetLanguageEs ? core::Language::kEs
                                                                           : core::Language::kEn);
            break;

        case platform::ShellAction::kQuit:
            SDL_Log("nimvlets: quit requested from the quick menu");
            running = false;
            break;
    }
}

void SpikeApp::RunDevSwitchSmokeTestIfRequested() {
    const char* countEnv = std::getenv(kDevSwitchTestCountEnvVar);
    if (countEnv == nullptr || countEnv[0] == '\0') {
        return;
    }

    char* end = nullptr;
    const long parsed = std::strtol(countEnv, &end, 10);
    if (end == countEnv || parsed <= 0) {
        SDL_Log("nimvlets: %s='%s' is not a valid positive integer; ignoring", kDevSwitchTestCountEnvVar, countEnv);
        return;
    }
    const auto count = static_cast<std::size_t>(parsed);

    SDL_Log(
        "nimvlets: DEV switch smoke test active — %s=%zu (%zu catalog entr%s available)",
        kDevSwitchTestCountEnvVar, count, catalog_.Entries().size(), catalog_.Entries().size() == 1 ? "y" : "ies");

    std::size_t successCount = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const catalog::CatalogEntry& target = catalog_.Entries()[i % catalog_.Entries().size()];
        if (TrySwitchActivePet(target.identity)) {
            ++successCount;
        }
    }

    SDL_Log("nimvlets: DEV switch smoke test complete — %zu/%zu switches succeeded", successCount, count);
}

void SpikeApp::SetActiveDirection(content::Direction direction) {
    const double nowMs = static_cast<double>(SDL_GetTicks());
    const bool changed = animController_->SetDirection(direction, nowMs);
    SDL_Log(
        "nimvlets: active direction -> %s%s",
        content::ToString(direction), changed ? "" : " (no visual change -- already active, or a gesture is in progress)");
    if (changed) {
        MarkNeedsRedraw(nowMs);
        // Un cambio de dirección real (incluyendo el causado por un
        // drag que cruza la mitad de pantalla -- ver
        // UpdateDirectionFromWindowPosition()) es una interacción
        // visible del owner con el pet (Block 05 segunda pasada).
        RearmAmbientDeadline(nowMs);
    }
}

void SpikeApp::UpdateDirectionFromWindowPosition() {
    int winX = 0;
    int winY = 0;
    int winW = 0;
    int winH = 0;
    SDL_GetWindowPosition(window_, &winX, &winY);
    SDL_GetWindowSize(window_, &winW, &winH);
    const double windowCenterX = static_cast<double>(winX) + static_cast<double>(winW) / 2.0;

    const SDL_DisplayID displayId = SDL_GetDisplayForWindow(window_);
    if (displayId == 0) {
        SDL_Log("nimvlets: UpdateDirectionFromWindowPosition: SDL_GetDisplayForWindow failed: %s", SDL_GetError());
        return;
    }
    SDL_Rect displayBounds{};
    if (!SDL_GetDisplayBounds(displayId, &displayBounds)) {
        SDL_Log("nimvlets: UpdateDirectionFromWindowPosition: SDL_GetDisplayBounds failed: %s", SDL_GetError());
        return;
    }

    const double displayCenterX = static_cast<double>(displayBounds.x) + static_cast<double>(displayBounds.w) / 2.0;
    const content::Direction target =
        (windowCenterX < displayCenterX) ? content::Direction::kLeft : content::Direction::kRight;
    SDL_Log(
        "nimvlets: screen-half direction check: windowCenterX=%.1f displayBounds=(%d,%d,%dx%d) displayCenterX=%.1f -> %s",
        windowCenterX, displayBounds.x, displayBounds.y, displayBounds.w, displayBounds.h, displayCenterX,
        content::ToString(target));
    SetActiveDirection(target);
}

void SpikeApp::RunDevDirectionSmokeTestIfRequested() {
    const char* countEnv = std::getenv(kDevDirectionTestCountEnvVar);
    if (countEnv == nullptr || countEnv[0] == '\0') {
        return;
    }

    char* end = nullptr;
    const long parsed = std::strtol(countEnv, &end, 10);
    if (end == countEnv || parsed <= 0) {
        SDL_Log("nimvlets: %s='%s' is not a valid positive integer; ignoring", kDevDirectionTestCountEnvVar, countEnv);
        return;
    }
    const auto count = static_cast<std::size_t>(parsed);

    SDL_Log("nimvlets: DEV direction smoke test active — %s=%zu", kDevDirectionTestCountEnvVar, count);

    for (std::size_t i = 0; i < count; ++i) {
        const content::Direction target = (i % 2 == 0) ? content::Direction::kLeft : content::Direction::kRight;
        SetActiveDirection(target);
    }

    SDL_Log("nimvlets: DEV direction smoke test complete — %zu direction change(s) requested", count);
}

void SpikeApp::RunDevClickSmokeTestIfRequested() {
    const char* countEnv = std::getenv(kDevClickTestCountEnvVar);
    if (countEnv == nullptr || countEnv[0] == '\0') {
        return;
    }

    char* end = nullptr;
    const long parsed = std::strtol(countEnv, &end, 10);
    if (end == countEnv || parsed <= 0) {
        SDL_Log("nimvlets: %s='%s' is not a valid positive integer; ignoring", kDevClickTestCountEnvVar, countEnv);
        return;
    }
    const auto count = static_cast<std::size_t>(parsed);

    SDL_Log("nimvlets: DEV click smoke test active — %s=%zu", kDevClickTestCountEnvVar, count);

    const double nowMs = static_cast<double>(SDL_GetTicks());
    for (std::size_t i = 0; i < count; ++i) {
        ++clickCount_;
        // Mismo camino canónico que un clic real del owner sobre el pet.
        HandleCountedClick(core::ClickSource::kLocalPet, nowMs);
        animController_->TriggerClick(NextUniformRandom01(), nowMs);
    }
    MarkNeedsRedraw(nowMs);

    SDL_Log("nimvlets: DEV click smoke test complete — %zu click(s) requested", count);
}

double SpikeApp::NextUniformRandom01() {
    return std::uniform_real_distribution<double>(0.0, 1.0)(passiveActionRng_);
}

void SpikeApp::MaybeTriggerHoverAction(bool cursorOverOpaque, double nowMs) {
    if (dragClassifier_.IsActive()) {
        // Un gesto en curso resetea el dwell -- "arrastrando" no
        // cuenta como reposo tranquilo, y el owner pidió
        // explícitamente que un drag reinicie el contador.
        ResetHoverDwell();
        return;
    }

    const bool wasDwelling = hoverDwellTracker_.IsDwelling();
    const bool crossedThreshold = hoverDwellTracker_.Update(cursorOverOpaque, nowMs);

    if (!wasDwelling && hoverDwellTracker_.IsDwelling()) {
        // El cursor ACABA de entrar a la región interactiva -- un
        // dwell nuevo recién arrancó. "Visible pointer enter" cuenta
        // como interacción real por sí solo (owner: "the pet should
        // not perform an ambient action immediately after the owner
        // just interacted with it" -- pasar el mouse por encima
        // también cuenta, incluso si nunca llega a mantenerse quieto
        // lo suficiente como para disparar una acción de hover).
        RearmAmbientDeadline(nowMs);
    }

    // Mantiene armado (o limpia) hoverDwellDeadlineMs_ para que el
    // loop principal pueda despertar exactamente cuando el dwell EN
    // CURSO cruzaría el umbral, incluso sin más eventos de motion --
    // ver el comentario del campo en SpikeApp.h.
    if (hoverDwellTracker_.IsDwelling()) {
        if (!hoverDwellDeadlineMs_.has_value()) {
            hoverDwellDeadlineMs_ = nowMs + kHoverDwellSeconds * 1000.0;
        }
    } else {
        hoverDwellDeadlineMs_.reset();
    }

    if (!crossedThreshold) {
        return;
    }

    if (animController_->TriggerHoverAction(NextUniformRandom01(), nowMs)) {
        MarkNeedsRedraw(nowMs);
        // Un disparo de hover completo es, con más razón, una
        // interacción real -- el próximo ambient debe volver a quedar
        // a ~15s de distancia desde ESTE momento, no desde el enter
        // previo (ya reiniciado arriba, pero una acción real
        // completada lo confirma/reafirma).
        RearmAmbientDeadline(nowMs);
        // %.1f, no %.0f -- con kHoverDwellSeconds=0.2 (DEC-090) un
        // formato de cero decimales redondearía a "0s", un log
        // engañoso que sugeriría que no hubo dwell alguno.
        SDL_Log("nimvlets: hover-triggered action after %.1fs dwell (state='%s')", kHoverDwellSeconds, animController_->CurrentStateId().c_str());
    }
}

void SpikeApp::ResetHoverDwell() {
    hoverDwellTracker_.Reset();
    hoverDwellDeadlineMs_.reset();
}

void SpikeApp::RunDevHoverSmokeTestIfRequested() {
    const char* countEnv = std::getenv(kDevHoverTestCountEnvVar);
    if (countEnv == nullptr || countEnv[0] == '\0') {
        return;
    }

    char* end = nullptr;
    const long parsed = std::strtol(countEnv, &end, 10);
    if (end == countEnv || parsed <= 0) {
        SDL_Log("nimvlets: %s='%s' is not a valid positive integer; ignoring", kDevHoverTestCountEnvVar, countEnv);
        return;
    }
    const auto count = static_cast<std::size_t>(parsed);

    SDL_Log("nimvlets: DEV hover smoke test active — %s=%zu", kDevHoverTestCountEnvVar, count);

    // Tiempo SIMULADO, no wall-clock real. Cada ciclo: el cursor
    // "entra" (arranca el dwell, no dispara todavía), permanece
    // encima kHoverDwellSeconds completos (cruza el umbral -- dispara,
    // solo en el primer ciclo real: los siguientes quedan en no-op
    // porque el controller pasa a modo AmbientOrHoverAction y nada lo
    // avanza de vuelta a Base dentro de esta corrida sincrónica --
    // mismo patrón honesto que antes: "dispara una vez, después se
    // queda callado", prueba que nunca hace spam), y luego "sale"
    // (resetea el dwell) antes del próximo ciclo.
    double simulatedNowMs = static_cast<double>(SDL_GetTicks());
    for (std::size_t i = 0; i < count; ++i) {
        simulatedNowMs += 1.0;
        MaybeTriggerHoverAction(true, simulatedNowMs);  // entra -- arranca el dwell
        simulatedNowMs += kHoverDwellSeconds * 1000.0 + 1.0;
        MaybeTriggerHoverAction(true, simulatedNowMs);  // cruza el umbral de dwell
        simulatedNowMs += 1.0;
        MaybeTriggerHoverAction(false, simulatedNowMs);  // sale -- resetea
    }

    // Deja un estado limpio de wall-clock antes de devolver el control
    // -- el tiempo simulado de arriba no debe dejar ningún deadline de
    // dwell pendiente que dispare inesperadamente en el loop real.
    ResetHoverDwell();

    SDL_Log("nimvlets: DEV hover smoke test complete — %zu hover cycle(s) requested", count);
}

void SpikeApp::Shutdown() {
    // PRIMERO de todo: el monitor de clics globales (Block 11A). Stop()
    // hace join del hilo del tap, así que al volver está GARANTIZADO que
    // no puede llegar ningún callback nativo más — ni un SDL_PushEvent
    // sobre un event loop que ya no corre, ni un HandleCountedClick
    // sobre un AppState que se está por escribir (brief §19). Recién
    // después se flushea el estado.
    if (globalClickMonitor_) {
        globalClickMonitor_->Stop();
        globalClickMonitor_.reset();
    }

    FlushPersistedState();

    // Product UI + System Shell primero: cada uno con su propio
    // renderer/recursos nativos, independientes de la ventana del pet.
    productWindow_.Close();
    if (systemShell_) {
        systemShell_->Shutdown();
        systemShell_.reset();
    }

    // Antes de destruir el renderer: la textura activa debe morir
    // primero (SDL exige que ninguna textura sobreviva a su renderer).
    activeFrameTexture_.Reset();
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Log(
        "nimvlets: clean shutdown, %d click(s) this session, click balance %llu",
        clickCount_, static_cast<unsigned long long>(appState_.clickBalance));
    SDL_Quit();
}

bool SpikeApp::IsPointInteractive(core::Point localPoint) const {
    return activeHitMask_.Contains(localPoint);
}

bool SpikeApp::IsPointInsideWindow(core::Point localPoint) const {
    return localPoint.x >= 0.0 && localPoint.y >= 0.0 &&
           localPoint.x < static_cast<double>(EffectiveCanvasWidth()) &&
           localPoint.y < static_cast<double>(EffectiveCanvasHeight());
}

void SpikeApp::RenderFrame() {
    const content::FrameDefinition& frame = animController_->CurrentFrame();

    // UNA textura reutilizable por pet, actualizada en el lugar (ver
    // graphics::ActiveFrameTexture / DEC-081): el objeto SDL_Texture NO
    // cambia entre frames de una animación -- solo su contenido. Se
    // crea perezosamente en el primer dibujo de cada pet.
    SDL_Texture* texture = nullptr;
    if (activeFrameTexture_.SetFrame(renderer_, frame)) {
        texture = activeFrameTexture_.Get();
    }

    // Se presenta el mismo contenido DOS veces seguidas -- ver el
    // comentario histórico de Block 04.3 que sigue aplicando sin
    // cambios: un SDL_RenderPresent() "frío" tras un período largo sin
    // presentar nada puede mostrar un drawable todavía no asentado en
    // Metal. Block 05 amplía CUÁNDO se arma un redraw de confirmación
    // adicional 120ms más tarde (ver MarkNeedsRedraw()) a cualquier
    // transición de animación, no solo cambios de dirección -- ver el
    // informe final, sección de diagnóstico de Bunny, para la evidencia
    // real (un dump de frames vía SDL_RenderReadPixels) que motivó
    // ampliar esto, y sus límites honestos.
    for (int presentPass = 0; presentPass < 2; ++presentPass) {
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
        SDL_RenderClear(renderer_);
        if (texture != nullptr) {
            const SDL_FRect dst{
                0.0f, 0.0f, static_cast<float>(EffectiveCanvasWidth()), static_cast<float>(EffectiveCanvasHeight())};
            SDL_RenderTexture(renderer_, texture, nullptr, &dst);
        } else if (!frame.pixels.empty() && presentPass == 0) {
            SDL_Log(
                "nimvlets: RenderFrame: current frame (%dx%d) has pixel data but no attached texture -- "
                "rendering fully transparent; check ActiveFrameTexture::SetFrame() failures above",
                frame.width, frame.height);
        }
        SDL_RenderPresent(renderer_);
    }

    if (const char* dumpDir = std::getenv(kDevDumpFramesDirEnvVar); dumpDir != nullptr && dumpDir[0] != '\0') {
        DumpCurrentRenderedFrame(dumpDir);
    }
}

void SpikeApp::DumpCurrentRenderedFrame(const std::string& dir) {
    SDL_Surface* raw = SDL_RenderReadPixels(renderer_, nullptr);
    if (raw == nullptr) {
        SDL_Log("nimvlets: [dev-dump] SDL_RenderReadPixels failed: %s", SDL_GetError());
        return;
    }
    SDL_Surface* rgba = raw;
    bool ownsRgba = false;
    if (raw->format != SDL_PIXELFORMAT_RGBA32) {
        rgba = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_RGBA32);
        ownsRgba = true;
        if (rgba == nullptr) {
            SDL_Log("nimvlets: [dev-dump] SDL_ConvertSurface failed: %s", SDL_GetError());
            SDL_DestroySurface(raw);
            return;
        }
    }

    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const std::string path = dir + "/frame_" + std::to_string(devDumpFrameCount_) + ".rgba";
    std::ofstream out(path, std::ios::binary);
    const std::uint32_t w = static_cast<std::uint32_t>(rgba->w);
    const std::uint32_t h = static_cast<std::uint32_t>(rgba->h);
    out.write(reinterpret_cast<const char*>(&w), sizeof(w));
    out.write(reinterpret_cast<const char*>(&h), sizeof(h));
    for (int y = 0; y < rgba->h; ++y) {
        const auto* row = static_cast<const char*>(rgba->pixels) + static_cast<std::size_t>(y) * static_cast<std::size_t>(rgba->pitch);
        out.write(row, static_cast<std::streamsize>(w) * 4);
    }
    SDL_Log("nimvlets: [dev-dump] wrote %s (%ux%u)", path.c_str(), w, h);
    ++devDumpFrameCount_;

    if (ownsRgba) {
        SDL_DestroySurface(rgba);
    }
    SDL_DestroySurface(raw);
}

void SpikeApp::ApplyCurrentHitMask() {
    const content::FrameDefinition& frame = animController_->CurrentFrame();
    activeHitMask_ = core::AlphaMask::FromAlphaChannel(
        frame.pixels.empty() ? nullptr : frame.pixels.data(),
        frame.width, frame.height,
        EffectiveCanvasWidth(), EffectiveCanvasHeight(),
        pet_.alphaHitThreshold);

    if (usingNativeShapeHitTest_) {
        SDL_Surface* shape = BuildHitTestShapeSurface(activeHitMask_);
        if (shape != nullptr) {
            if (!SDL_SetWindowShape(window_, shape)) {
                SDL_Log("nimvlets: SDL_SetWindowShape (frame update) failed: %s", SDL_GetError());
            }
            SDL_DestroySurface(shape);
        }
    }
}

void SpikeApp::PollHover() {
    const double nowMs = static_cast<double>(SDL_GetTicks());

    const CursorSample sample = SampleCursor(window_);
    const bool cursorOverOpaque = IsPointInteractive(sample.localPoint);

#ifndef NDEBUG
    if (!diagHasValue_ || sample.globalX != diagGlobalX_ || sample.globalY != diagGlobalY_) {
        SDL_Log("nimvlets: [diag] global cursor position: (%.1f, %.1f)", sample.globalX, sample.globalY);
        diagGlobalX_ = sample.globalX;
        diagGlobalY_ = sample.globalY;
    }
    if (!diagHasValue_ || sample.windowX != diagWindowX_ || sample.windowY != diagWindowY_) {
        SDL_Log("nimvlets: [diag] window position: (%d, %d)", sample.windowX, sample.windowY);
        diagWindowX_ = sample.windowX;
        diagWindowY_ = sample.windowY;
    }
    if (!diagHasValue_ || sample.localPoint.x != diagLocalPoint_.x || sample.localPoint.y != diagLocalPoint_.y) {
        SDL_Log("nimvlets: [diag] local coordinate (global - window): (%.2f, %.2f)", sample.localPoint.x, sample.localPoint.y);
        diagLocalPoint_ = sample.localPoint;
    }
    if (!diagHasValue_ || cursorOverOpaque != diagContains_) {
        SDL_Log("nimvlets: [diag] IsPointInteractive(local)=%s", cursorOverOpaque ? "true" : "false");
        diagContains_ = cursorOverOpaque;
    }
    diagHasValue_ = true;
#endif  // NDEBUG

    UpdateClickThrough(IsPointInsideWindow(sample.localPoint), cursorOverOpaque);
    MaybeTriggerHoverAction(cursorOverOpaque, nowMs);

    hoverScheduler_.OnFramePresented(nowMs);
}

void SpikeApp::UpdateClickThrough(bool cursorInsideWindow, bool cursorOverOpaque) {
    if (!usingPollDrivenClickThrough_) {
        // Esta plataforma/driver NO usa el mecanismo propio de
        // Nimvlets: o bien la ruta nativa de shape ya resuelve el
        // hit-test por su cuenta (macOS con driver acelerado,
        // Linux/X11 -- ahí escribir el estado a mano PELEARÍA contra
        // SDL, que es exactamente el error que este bloque acaba de
        // corregir en la dirección contraria), o bien no hay ningún
        // mecanismo disponible (Linux/Wayland). En los dos casos esto
        // es un no-op deliberado, y el muestreo queda desarmado.
        //
        // El guard vive acá y no en cada call site a propósito:
        // HandleEvent() llama a esta función desde cinco lugares
        // (motion/enter/leave/button-down/button-up) y repetir la
        // condición en todos era la forma segura de que algún día uno
        // quedara sin guardar.
        clickThroughSamplingActive_ = false;
        return;
    }

    const core::ClickThroughDecision decision =
        core::EvaluateClickThrough(cursorInsideWindow, cursorOverOpaque, dragClassifier_.IsActive());

    // Arma/desarma el muestreo periódico. Esto es lo que hace que el
    // costo en reposo sea CERO despertares con el cursor en cualquier
    // otro lugar de la pantalla -- ver core/ClickThroughPolicy.h.
    clickThroughSamplingActive_ = decision.samplingRequired;

#ifndef NDEBUG
    if (decision.clickThrough != diagRequestedClickThrough_ || !diagHasValue_) {
        SDL_Log("nimvlets: [diag] requested click-through=%s (sampling=%s)",
                decision.clickThrough ? "true" : "false", decision.samplingRequired ? "on" : "off");
        diagRequestedClickThrough_ = decision.clickThrough;
    }
#endif  // NDEBUG

    if (decision.clickThrough != currentlyClickThrough_) {
        const bool actual = platform::SetWindowClickThrough(window_, decision.clickThrough);
        currentlyClickThrough_ = decision.clickThrough;

#ifndef NDEBUG
        if (actual != diagActualIgnoresMouseEvents_ || !diagHasValue_) {
            SDL_Log(
                "nimvlets: [diag] native click-through actual=%s%s",
                actual ? "true" : "false",
                actual == decision.clickThrough ? "" : "  <-- MISMATCH vs. requested value!");
            diagActualIgnoresMouseEvents_ = actual;
        }
#else
        (void)actual;
#endif  // NDEBUG
        return;
    }

#ifndef NDEBUG
    // Diagnóstico que el brief de este bloque pide explícitamente:
    // comparar "lo que pedimos" contra "lo que el OS realmente tiene"
    // DESPUÉS de actividad real de mouse de Cocoa/SDL, no solo en el
    // instante de la asignación. Sin esto, el modo de falla histórico
    // (SDL pisando NSWindow.ignoresMouseEvents por la espalda) era
    // literalmente invisible desde el lado de la app: currentlyClickThrough_
    // decía "ya está en el valor correcto" y nunca se volvía a mirar.
    // ReadWindowClickThrough() LEE sin escribir, así que este chequeo no
    // puede enmascarar el bug arreglándolo de casualidad.
    const bool actualNow = platform::ReadWindowClickThrough(window_);
    if (actualNow != currentlyClickThrough_) {
        SDL_Log(
            "nimvlets: [diag] click-through DRIFT: requested=%s but native state=%s "
            "(foreign writes intercepted so far: %llu)",
            currentlyClickThrough_ ? "true" : "false", actualNow ? "true" : "false",
            platform::ForeignClickThroughWriteCount());
    }
#endif  // NDEBUG
}

void SpikeApp::HandleEvent(const SDL_Event& event, bool& running) {
    // Ruteo del Product UI: los eventos de ESA ventana se manejan
    // aparte y NUNCA terminan la app (block brief §4/§18). Cerrar la
    // Collection solo libera sus recursos; el runtime del pet sigue.
    if (productWindow_.IsOpen()) {
        const productui::ProductWindowEvent pe = productWindow_.HandleEvent(event);
        if (pe.consumed) {
            if (pe.hasActivate) {
                HandleActivateRequest(pe.activate);
            }
            if (pe.hasPurchase) {
                HandlePurchaseRequest(pe.purchase);
            }
            if (pe.hasStarterPurchase) {
                HandleStarterPurchaseRequest(pe.starterPurchase);
            }
            if (pe.hasPreferenceChange) {
                // Misma ruta canónica que la acción equivalente del menú
                // rápido (block brief 08 §6).
                ApplyPreferenceChange(pe.preferenceChange);
            }
            if (pe.hasGlobalClickAction) {
                HandleGlobalClickAction(pe.globalClickAction);
            }
            if (pe.hasOnboardingSelection) {
                HandleOnboardingSelection(pe.onboardingSelection);
            }
            if (pe.closeRequested) {
                if (onboardingActive_) {
                    // El onboarding es OBLIGATORIO: no se puede cerrar la
                    // ventana para saltearlo (brief §19/§25). Se
                    // re-enfoca en vez de cerrar.
                    productWindow_.FocusWindow();
                } else {
                    productWindow_.Close();
                }
            }
            return;
        }
    }

    // Acción del menú rápido nativo, llegada como SDL_EVENT_USER en el
    // hilo principal (ver platform::SystemShell).
    if (shellUserEventType_ != 0 && event.type == shellUserEventType_) {
        HandleShellAction(event.user.code, running);
        return;
    }

    // Clic primario GLOBAL, reenviado por el monitor nativo desde su
    // propio hilo (Block 11A). El evento no lleva NADA: solo su tipo. La
    // mutación canónica del wallet ocurre acá, en el hilo principal.
    if (globalClickUserEventType_ != 0 && event.type == globalClickUserEventType_) {
        HandleCountedClick(core::ClickSource::kGlobalMonitor, static_cast<double>(SDL_GetTicks()));
        return;
    }

    switch (event.type) {
        case SDL_EVENT_QUIT:
            running = false;
            break;

        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            // Solo la ventana del pet llega acá (la del Product UI ya se
            // filtró arriba). Es borderless y sin botón de cerrar, pero
            // si el window system igual manda un close, se respeta.
            running = false;
            break;

        case SDL_EVENT_WINDOW_EXPOSED:
            needsRedraw_ = true;
            break;

        case SDL_EVENT_WINDOW_MOVED:
            // Durante un drag REAL no se resuelve dirección: cada
            // SetActiveDirection() que cambia algo dispara un redraw
            // (y con él un swap de textura) JUSTO mientras la ventana
            // se está moviendo -- exactamente el escenario que el owner
            // reportó como corrupción visual al arrastrar durante una
            // animación. La dirección final se resuelve UNA vez al
            // soltar (ver SDL_EVENT_MOUSE_BUTTON_UP).
            if (!dragClassifier_.IsDragging()) {
                UpdateDirectionFromWindowPosition();
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            if (event.button.button != SDL_BUTTON_LEFT) {
                break;
            }
            const core::Point localOrigin{
                static_cast<double>(event.button.x),
                static_cast<double>(event.button.y),
            };

            if (!IsPointInteractive(localOrigin)) {
                break;
            }

            dragClassifier_.Begin(localOrigin);
            // Un gesto que arranca fija el estado en "no click-through"
            // y apaga el muestreo -- durante el arrastre los eventos
            // reales alcanzan.
            UpdateClickThrough(true, true);
            // Un click/drag que arranca reinicia cualquier dwell de
            // hover en curso -- el owner lo pidió explícitamente,
            // incluso si termina siendo un click corto (no un drag) y
            // el cursor nunca sale de la región interactiva.
            ResetHoverDwell();
            // "Drag start" es también una interacción real que debe
            // reiniciar el conteo ambient (owner, Block 05 segunda
            // pasada) -- así un drag largo no deja que el ambient
            // dispare mientras el owner sigue con el botón apretado.
            RearmAmbientDeadline(static_cast<double>(SDL_GetTicks()));

            int winX = 0;
            int winY = 0;
            SDL_GetWindowPosition(window_, &winX, &winY);
            float globalX = 0.0f;
            float globalY = 0.0f;
            SDL_GetGlobalMouseState(&globalX, &globalY);
            dragGrabOffsetX_ = static_cast<double>(globalX) - static_cast<double>(winX);
            dragGrabOffsetY_ = static_cast<double>(globalY) - static_cast<double>(winY);
            break;
        }

        case SDL_EVENT_MOUSE_MOTION: {
            const core::Point localCurrent{
                static_cast<double>(event.motion.x),
                static_cast<double>(event.motion.y),
            };

            if (dragClassifier_.IsActive()) {
                const bool wasDragging = dragClassifier_.IsDragging();
                dragClassifier_.Update(localCurrent);

                if (!wasDragging && dragClassifier_.IsDragging()) {
                    // El gesto acaba de calificar como DRAG real (cruzó
                    // el umbral de DragClassifier). Prioridad
                    // DRAG > CLICK > HOVER/AMBIENT > BASE: se aborta
                    // cualquier acción one-shot en curso y se vuelve a
                    // la pose base ESTABLE del estado ACTUAL, para que
                    // el arrastre mueva una imagen quieta en vez de una
                    // animación reproduciéndose (ver DEC-080).
                    const double dragStartMs = static_cast<double>(SDL_GetTicks());
                    if (animController_->CancelActionToCurrentState(dragStartMs)) {
                        MarkNeedsRedraw(dragStartMs);
                        SDL_Log(
                            "nimvlets: drag started -- in-progress action cancelled, holding base pose of state='%s'",
                            animController_->CurrentStateId().c_str());
                    }
                }

                // Block 06 §16: con la posición BLOQUEADA el gesto sigue
                // clasificándose (un click corto sigue contando) pero la
                // ventana no se mueve. Hover, click-through y animaciones
                // quedan intactos porque nada más de esta rama cambia.
                if (dragClassifier_.IsDragging() && core::PetDragAllowed(appState_.lockPosition)) {
                    float globalX = 0.0f;
                    float globalY = 0.0f;
                    SDL_GetGlobalMouseState(&globalX, &globalY);
                    const int newWinX = static_cast<int>(std::lround(static_cast<double>(globalX) - dragGrabOffsetX_));
                    const int newWinY = static_cast<int>(std::lround(static_cast<double>(globalY) - dragGrabOffsetY_));
                    SDL_SetWindowPosition(window_, newWinX, newWinY);
                }
                break;
            }

            // Camino EVENT-DRIVEN del click-through: mientras la
            // ventana NO está en modo click-through recibe eventos de
            // mouse reales, así que cada motion resuelve el estado con
            // latencia cero y sin ningún despertar periódico. El
            // muestreo de PollHover() solo hace falta para el caso
            // inverso (ya en click-through, la ventana deja de recibir
            // eventos) -- ver core/ClickThroughPolicy.h.
            UpdateClickThrough(IsPointInsideWindow(localCurrent), IsPointInteractive(localCurrent));
            MaybeTriggerHoverAction(IsPointInteractive(localCurrent), static_cast<double>(SDL_GetTicks()));
            break;
        }

        case SDL_EVENT_WINDOW_MOUSE_ENTER: {
            const CursorSample sample = SampleCursor(window_);
            UpdateClickThrough(IsPointInsideWindow(sample.localPoint), IsPointInteractive(sample.localPoint));
            break;
        }

        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            // El cursor salió del rectángulo: el estado de
            // click-through deja de ser observable (ningún click de ahí
            // puede llegar a esta ventana). Se elige explícitamente NO
            // click-through para recuperar la entrega normal de eventos
            // y poder detectar el próximo ingreso sin encuestar.
            UpdateClickThrough(false, false);
            ResetHoverDwell();
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP: {
            if (event.button.button != SDL_BUTTON_LEFT || !dragClassifier_.IsActive()) {
                break;
            }
            const core::Point localEnd{
                static_cast<double>(event.button.x),
                static_cast<double>(event.button.y),
            };
            const core::PointerGesture gesture = dragClassifier_.End(localEnd);
            const double nowMs = static_cast<double>(SDL_GetTicks());
            // El gesto terminó: la política vuelve a depender solo de
            // dónde está el cursor, así que se re-evalúa de inmediato
            // en vez de esperar al próximo evento/muestra ("releasing
            // restores normal per-pixel behavior", brief §5).
            UpdateClickThrough(IsPointInsideWindow(localEnd), IsPointInteractive(localEnd));
            if (gesture == core::PointerGesture::kClick) {
                ++clickCount_;  // interacciones con el pet en esta sesión (diagnóstico, no moneda)
                // La MONEDA pasa por el único punto canónico. En modo
                // global efectivo esto NO suma: el monitor global ya vio
                // este mismo clic físico (brief §4). La reacción de
                // personalidad de abajo se dispara igual en los dos
                // modos (brief §22).
                HandleCountedClick(core::ClickSource::kLocalPet, nowMs);
                animController_->TriggerClick(NextUniformRandom01(), nowMs);
                MarkNeedsRedraw(nowMs);
                RearmAmbientDeadline(nowMs);  // un click es una interacción real -- ver el comentario del campo
                SDL_Log(
                    "nimvlets: pet click #%d this session (balance: %llu, counting: %s)",
                    clickCount_, static_cast<unsigned long long>(appState_.clickBalance),
                    CurrentEffectiveClickCounting() == core::EffectiveClickCounting::kGlobal
                        ? "global monitor"
                        : "local");
            } else {
                int endX = 0;
                int endY = 0;
                SDL_GetWindowPosition(window_, &endX, &endY);
                appState_.lastWindowPosition = persistence::WindowPosition{endX, endY};
                persistenceScheduler_.MarkDirty(nowMs);
                // Al soltar: se resuelve la dirección UNA sola vez
                // contra la posición FINAL (durante el arrastre se
                // suprime -- ver SDL_EVENT_WINDOW_MOVED), se reinicia
                // el dwell de hover desde cero, y el conteo ambient
                // arranca de nuevo completo desde acá.
                UpdateDirectionFromWindowPosition();
                ResetHoverDwell();
                RearmAmbientDeadline(nowMs);
                SDL_Log("nimvlets: drag ended at (%d, %d) (correctly not counted as a click)", endX, endY);
            }
            break;
        }

        default:
            break;
    }
}

int SpikeApp::Run() {
    if (!Init()) {
        Shutdown();
        return 1;
    }

    RunDevSwitchSmokeTestIfRequested();
    RunDevDirectionSmokeTestIfRequested();
    RunDevClickSmokeTestIfRequested();
    RunDevHoverSmokeTestIfRequested();

    // Mecanismo solo-DEV (Block 07): confirma una compra del Shop de
    // forma no interactiva ("<petId>"), para smoke-testear la
    // transacción de wallet completa (misma ruta que "Confirmar"):
    // EvaluatePurchase -> mutación atómica de balance + propiedad ->
    // flush INMEDIATO. Corre ANTES de abrir el Product UI, así que si
    // además se abre, la Collection y el Shop ya reflejan la compra.
    // Combinar con NIMVLETS_DEV_CLICK_TEST_COUNT para tener saldo, y con
    // NIMVLETS_DEV_APPDATA_DIR para no tocar el estado real del owner.
    // Ausente/vacía: no-op. Ver README.md.
    if (const char* buy = std::getenv("NIMVLETS_DEV_BUY"); buy != nullptr && buy[0] != '\0') {
        productui::PurchaseRequest req;
        req.petId = buy;
        SDL_Log("nimvlets: DEV override active — NIMVLETS_DEV_BUY='%s'", buy);
        HandlePurchaseRequest(req);
    }

    // Solo-DEV (Block 09A): confirma una selección de starter sin
    // interacción ("petId" o "petId/variantId", p. ej. "artu_dev" o
    // "frin/male") — misma ruta que "Choose <name>": evalúa la política,
    // aplica la transacción de completitud atómica, sale del gate. Para
    // smoke-testear completitud + reinicio. Requiere NIMVLETS_DEV_ONBOARDING.
    // Corre ANTES del bloque NIMVLETS_DEV_OPEN_COLLECTION: así una captura
    // de QA compuesta (CHOOSE + OPEN_COLLECTION + SECTION + PRODUCT_SHOT)
    // fotografía el Product UI NORMAL de post-onboarding — el flujo del
    // brief §15 "reopen Product UI -> Settings".
    if (onboardingActive_) {
        if (const char* ch = std::getenv("NIMVLETS_DEV_ONBOARDING_CHOOSE");
            ch != nullptr && ch[0] != '\0') {
            const std::string spec(ch);
            const std::size_t slash = spec.find('/');
            const catalog::PetIdentity sel{
                slash == std::string::npos ? spec : spec.substr(0, slash),
                slash == std::string::npos ? std::string() : spec.substr(slash + 1)};
            SDL_Log("nimvlets: DEV override active — NIMVLETS_DEV_ONBOARDING_CHOOSE='%s'", ch);
            HandleOnboardingSelection(sel);
        }
    }

    // Solo-DEV (Block 10 QA): SUMA n clics al wallet sin disparar
    // animaciones — para tener saldo tras el onboarding (que deja el
    // balance en 0) y ejercitar una compra del Starter Shop. Corre
    // DESPUÉS de NIMVLETS_DEV_ONBOARDING_CHOOSE (que pone el balance en 0)
    // y ANTES de NIMVLETS_DEV_STARTER_BUY / NIMVLETS_DEV_OPEN_COLLECTION,
    // así una invocación de un solo tiro (CHOOSE + GRANT_CLICKS +
    // STARTER_BUY | OPEN_COLLECTION + PRODUCT_SHOT) funciona. Combinar con
    // NIMVLETS_DEV_APPDATA_DIR aislado. Ausente/vacía: no-op. Ver README.md.
    if (const char* grant = std::getenv("NIMVLETS_DEV_GRANT_CLICKS");
        grant != nullptr && grant[0] != '\0') {
        char* end = nullptr;
        const long long n = std::strtoll(grant, &end, 10);
        if (end != grant && n > 0) {
            appState_.clickBalance += static_cast<std::uint64_t>(n);
            persistenceScheduler_.MarkDirty(static_cast<double>(SDL_GetTicks()));
            FlushPersistedState();
            SDL_Log("nimvlets: DEV override active — NIMVLETS_DEV_GRANT_CLICKS=%lld (balance now %llu)",
                    n, static_cast<unsigned long long>(appState_.clickBalance));
        }
    }

    // Solo-DEV (Block 11A): empuja n eventos de clic primario GLOBAL por
    // el MISMO camino que el monitor nativo (HandleCountedClick con
    // ClickSource::kGlobalMonitor). NO finge que el OS concedió ningún
    // permiso (brief §25): si el modo efectivo no es global, estos
    // eventos se IGNORAN — que es justamente la mitad interesante del
    // test. Sirve para verificar, sin permiso de TCC, que
    //   - en modo local un evento global no suma nada;
    //   - con el modo global REALMENTE activo cada evento suma 1.
    // Ausente/vacía: no-op. Ver README.md.
    if (const char* gc = std::getenv("NIMVLETS_DEV_GLOBAL_CLICKS"); gc != nullptr && gc[0] != '\0') {
        char* end = nullptr;
        const long long n = std::strtoll(gc, &end, 10);
        if (end != gc && n > 0) {
            const std::uint64_t before = appState_.clickBalance;
            const double nowMs = static_cast<double>(SDL_GetTicks());
            for (long long i = 0; i < n; ++i) {
                HandleCountedClick(core::ClickSource::kGlobalMonitor, nowMs);
            }
            FlushPersistedState();
            SDL_Log(
                "nimvlets: DEV override active — NIMVLETS_DEV_GLOBAL_CLICKS=%lld (effective mode: %s, "
                "balance %llu -> %llu, counted %llu)",
                n,
                CurrentEffectiveClickCounting() == core::EffectiveClickCounting::kGlobal ? "global"
                                                                                         : "local",
                static_cast<unsigned long long>(before),
                static_cast<unsigned long long>(appState_.clickBalance),
                static_cast<unsigned long long>(appState_.clickBalance - before));
        }
    }

    // Solo-DEV (Block 10): confirma una compra del SHOP OCULTO DE STARTERS
    // sin interacción ("<petId>" o "<petId>/<variant>", p. ej.
    // "frin/male") — misma ruta que "Confirmar" en el submodo:
    // EvaluateStarterPurchase -> ApplyPurchasedState (atómico) -> flush
    // inmediato -> refresco de Collection y del propio Starter Shop.
    // Requiere el lifecycle en kCompleted (p. ej. tras
    // NIMVLETS_DEV_ONBOARDING_CHOOSE en el mismo run, o un run previo).
    // Ausente/vacía: no-op. Ver README.md.
    if (const char* sbuy = std::getenv("NIMVLETS_DEV_STARTER_BUY");
        sbuy != nullptr && sbuy[0] != '\0') {
        const std::string sbSpec(sbuy);
        const std::size_t sbSlash = sbSpec.find('/');
        productui::PurchaseRequest req;
        req.petId = sbSlash == std::string::npos ? sbSpec : sbSpec.substr(0, sbSlash);
        req.variantId = sbSlash == std::string::npos ? std::string() : sbSpec.substr(sbSlash + 1);
        SDL_Log("nimvlets: DEV override active — NIMVLETS_DEV_STARTER_BUY='%s'", sbuy);
        HandleStarterPurchaseRequest(req);
    }

    // Mecanismo solo-DEV (Block 06): abre la Collection al arrancar,
    // para QA / capturas de pantalla sin tener que clickear el menú de
    // la barra. Ausente/vacía: no-op — la Collection solo se abre desde
    // el menú rápido, exactamente como en producción. Ver README.md.
    if (const char* openCol = std::getenv("NIMVLETS_DEV_OPEN_COLLECTION");
        openCol != nullptr && openCol[0] != '\0' && openCol[0] != '0') {
        SDL_Log("nimvlets: DEV override active — NIMVLETS_DEV_OPEN_COLLECTION (opening Collection at startup)");
        OpenProductWindow();
        // "petId" o "petId/variantId" (p. ej. "frin/female"): además de
        // abrir la Collection, elige ese Nimvlet como hero (y su
        // variante), para capturas de QA de los estados del hero.
        const std::string spec(openCol);
        if (spec != "1") {
            const std::size_t slash = spec.find('/');
            const std::string petId = slash == std::string::npos ? spec : spec.substr(0, slash);
            const std::string variantId = slash == std::string::npos ? std::string() : spec.substr(slash + 1);
            for (const catalog::CatalogEntry& entry : catalog_.Entries()) {
                if (entry.identity.petId == petId) {
                    productWindow_.SelectHeroForQA(petId);
                    if (!variantId.empty()) {
                        productWindow_.SetHeroVariantForQA(variantId);
                    }
                    break;
                }
            }
        }
        // Solo-DEV: fuerza el estado de hover/foco sobre una entrada de
        // la gallery para la captura de "hover/focus state".
        if (const char* hov = std::getenv("NIMVLETS_DEV_HERO_HOVER"); hov != nullptr && hov[0] != '\0') {
            productWindow_.SetGalleryHoverForQA(hov);
        }
        // Solo-DEV: pone el foco de TECLADO sobre un chip de variante del
        // hero (captura de "keyboard-focused Frin variant", brief §29).
        if (const char* vf = std::getenv("NIMVLETS_DEV_VARIANT_FOCUS"); vf != nullptr && vf[0] != '\0') {
            productWindow_.SetVariantKeyboardFocusForQA(vf);
        }
        // --- Block 07/08: hooks de QA del Shop y Settings -----------
        // NIMVLETS_DEV_SECTION=shop|settings|collection — sección visible.
        if (const char* sec = std::getenv("NIMVLETS_DEV_SECTION"); sec != nullptr && sec[0] != '\0') {
            const std::string s(sec);
            productWindow_.ShowSectionForQA(s == "shop"       ? productui::ProductSection::kShop
                                            : s == "settings" ? productui::ProductSection::kSettings
                                                              : productui::ProductSection::kCollection);
        }
        // NIMVLETS_DEV_PREFS=small,70,lock,es — aplica preferencias por la
        // MISMA ruta canónica que el menú rápido (SpikeApp::Apply*), para
        // capturas de un estado no-default de Settings y como smoke en
        // vivo de que esa ruta produce el AppState/runtime esperado
        // (brief §27/§28). Tokens: small|medium|large, 100|85|70|55,
        // lock|unlock, en|es. Ausente/vacía: no-op.
        if (const char* prefs = std::getenv("NIMVLETS_DEV_PREFS"); prefs != nullptr && prefs[0] != '\0') {
            SDL_Log("nimvlets: DEV override active — NIMVLETS_DEV_PREFS='%s' (via canonical Apply* path)", prefs);
            std::string tok;
            std::string all(prefs);
            all.push_back(',');
            for (const char c : all) {
                if (c != ',') {
                    tok.push_back(c);
                    continue;
                }
                if (tok == "small") {
                    ApplySizeChoice(core::PetSizeChoice::kSmall);
                } else if (tok == "medium") {
                    ApplySizeChoice(core::PetSizeChoice::kMedium);
                } else if (tok == "large") {
                    ApplySizeChoice(core::PetSizeChoice::kLarge);
                } else if (tok == "100" || tok == "85" || tok == "70" || tok == "55") {
                    ApplyOpacityChoice(std::atoi(tok.c_str()));
                } else if (tok == "lock") {
                    ApplyLockPosition(true);
                } else if (tok == "unlock") {
                    ApplyLockPosition(false);
                } else if (tok == "en") {
                    ApplyUiLanguage(core::Language::kEn);
                } else if (tok == "es") {
                    ApplyUiLanguage(core::Language::kEs);
                } else if (!tok.empty()) {
                    SDL_Log("nimvlets: NIMVLETS_DEV_PREFS: ignoring unknown token '%s'", tok.c_str());
                }
                tok.clear();
            }
        }
        // NIMVLETS_DEV_SHOP_PET=<petId> — browse-first (DEC-135): abre el
        // Shop directamente con ESE personaje SELECCIONADO (hero grande +
        // detalle). Sin la variable, el Shop abre en modo BROWSE normal.
        if (const char* sp = std::getenv("NIMVLETS_DEV_SHOP_PET"); sp != nullptr && sp[0] != '\0') {
            productWindow_.SelectShopHeroForQA(sp);
        }
        // NIMVLETS_DEV_SHOP_HOVER=<petId> — hover sobre una tarjeta del
        // Shop (rejilla de browse, o rail si hay un personaje
        // seleccionado): revela su info liviana (precio / propiedad).
        if (const char* sh = std::getenv("NIMVLETS_DEV_SHOP_HOVER"); sh != nullptr && sh[0] != '\0') {
            productWindow_.SetShopGalleryHoverForQA(sh);
        }
        // NIMVLETS_DEV_SHOP_FOCUS=<petId> — foco de TECLADO sobre una
        // tarjeta del Shop (captura de "keyboard-focused candidate").
        if (const char* sk = std::getenv("NIMVLETS_DEV_SHOP_FOCUS"); sk != nullptr && sk[0] != '\0') {
            productWindow_.SetShopTileKeyboardFocusForQA(sk);
        }
        // NIMVLETS_DEV_SHOP_CONFIRM=1 — abre la confirmación de compra
        // inline (necesita NIMVLETS_DEV_SHOP_PET: la confirmación solo
        // existe con un personaje seleccionado y asequible).
        if (const char* sc = std::getenv("NIMVLETS_DEV_SHOP_CONFIRM");
            sc != nullptr && sc[0] != '\0' && sc[0] != '0') {
            productWindow_.SetShopConfirmingForQA(true);
        }
        // --- Block 10: hooks de QA del SHOP OCULTO DE STARTERS ------
        // NIMVLETS_DEV_STARTER_SHOP=1 — entra al submodo del Starter Shop
        // DIRECTAMENTE (necesita NIMVLETS_DEV_SECTION=shop). Sin la
        // variable, el Shop público normal (el acceso de producción es un
        // HOTSPOT INVISIBLE en la esquina inf-der — corrección de QA del
        // owner).
        if (const char* ss = std::getenv("NIMVLETS_DEV_STARTER_SHOP");
            ss != nullptr && ss[0] != '\0' && ss[0] != '0') {
            productWindow_.EnterStarterShopSubmodeForQA();
        }
        // NIMVLETS_DEV_STARTER_HOTSPOT=1 — sintetiza un click REAL en la
        // esquina inf-der del Shop público (el MISMO camino que un click
        // del owner). Prueba el hotspot INVISIBLE de verdad: se abre sii
        // hay ofertas legítimas (lifecycle kCompleted + regla del
        // secreto). Necesita NIMVLETS_DEV_SECTION=shop.
        if (const char* sht = std::getenv("NIMVLETS_DEV_STARTER_HOTSPOT");
            sht != nullptr && sht[0] != '\0' && sht[0] != '0') {
            const bool opened = productWindow_.ClickStarterHotspotForQA();
            SDL_Log("nimvlets: DEV — NIMVLETS_DEV_STARTER_HOTSPOT: invisible corner click %s",
                    opened ? "OPENED the hidden Starter Shop"
                           : "was a NO-OP (hotspot not armed — no eligible offers)");
        }
        // NIMVLETS_DEV_STARTER_OFFER=<petId>[/<variant>] — selecciona esa
        // oferta EXACTA como hero dentro del submodo.
        if (const char* so = std::getenv("NIMVLETS_DEV_STARTER_OFFER");
            so != nullptr && so[0] != '\0') {
            const std::string oSpec(so);
            const std::size_t oSlash = oSpec.find('/');
            productWindow_.SelectStarterOfferForQA(
                oSlash == std::string::npos ? oSpec : oSpec.substr(0, oSlash),
                oSlash == std::string::npos ? std::string() : oSpec.substr(oSlash + 1));
        }
        // NIMVLETS_DEV_STARTER_HOVER=<petId>[/<variant>] — hover sobre una
        // tarjeta de oferta (revela su precio).
        if (const char* sh2 = std::getenv("NIMVLETS_DEV_STARTER_HOVER");
            sh2 != nullptr && sh2[0] != '\0') {
            const std::string hSpec(sh2);
            const std::size_t hSlash = hSpec.find('/');
            productWindow_.SetStarterHoverForQA(
                hSlash == std::string::npos ? hSpec : hSpec.substr(0, hSlash),
                hSlash == std::string::npos ? std::string() : hSpec.substr(hSlash + 1));
        }
        // NIMVLETS_DEV_STARTER_FOCUS=<focusId> — foco de teclado sobre un
        // widget del submodo ("starter:back", "starteritem:frin/male", …).
        if (const char* sfk = std::getenv("NIMVLETS_DEV_STARTER_FOCUS");
            sfk != nullptr && sfk[0] != '\0') {
            productWindow_.SetStarterKeyboardFocusForQA(sfk);
        }
        // NIMVLETS_DEV_STARTER_CONFIRM=1 — abre la confirmación inline
        // (necesita NIMVLETS_DEV_STARTER_OFFER con una oferta asequible).
        if (const char* scc = std::getenv("NIMVLETS_DEV_STARTER_CONFIRM");
            scc != nullptr && scc[0] != '\0' && scc[0] != '0') {
            productWindow_.SetStarterConfirmingForQA(true);
        }
        // --- Block 11A: hooks de QA del conteo de clics global ------
        // NIMVLETS_DEV_CLICK_COUNTING=nimvlet_only|anywhere — pide ese
        // modo por el MISMO camino que un click del owner en Settings
        // (ApplyPreferenceChange -> EvaluateGlobalClickRequest). Con
        // "anywhere" y el permiso ausente, deja la explicación de primera
        // parte VISIBLE — sin pedir nada todavía (brief §8).
        if (const char* cc = std::getenv("NIMVLETS_DEV_CLICK_COUNTING");
            cc != nullptr && cc[0] != '\0') {
            productui::SettingsChange change;
            change.field = core::PreferenceField::kClickCounting;
            change.clickCounting = core::ParseClickCountingMode(cc);
            SDL_Log("nimvlets: DEV override active — NIMVLETS_DEV_CLICK_COUNTING='%s'", cc);
            ApplyPreferenceChange(change);
        }
        // NIMVLETS_DEV_GLOBAL_CLICK_ACTION=continue|notnow|recheck —
        // acciona un botón del flujo de permiso por su camino real.
        // OJO: "continue" SÍ llama al pedido nativo (puede mostrar el
        // diálogo del OS) — es exactamente lo que hace el botón.
        if (const char* ga = std::getenv("NIMVLETS_DEV_GLOBAL_CLICK_ACTION");
            ga != nullptr && ga[0] != '\0') {
            const std::string actionSpec(ga);
            const productui::GlobalClickAction action =
                actionSpec == "continue"  ? productui::GlobalClickAction::kContinue
                : actionSpec == "notnow"  ? productui::GlobalClickAction::kNotNow
                : actionSpec == "recheck" ? productui::GlobalClickAction::kCheckAgain
                                          : productui::GlobalClickAction::kNone;
            SDL_Log("nimvlets: DEV override active — NIMVLETS_DEV_GLOBAL_CLICK_ACTION='%s'", ga);
            HandleGlobalClickAction(action);
        }
        // NIMVLETS_DEV_SETTINGS_FOCUS=row:opacity — foco de teclado sobre
        // una fila de Settings (captura del anillo de foco).
        if (const char* sf = std::getenv("NIMVLETS_DEV_SETTINGS_FOCUS"); sf != nullptr && sf[0] != '\0') {
            productWindow_.SetSettingsKeyboardFocusForQA(sf);
        }
        // Solo-DEV: vuelca el framebuffer de la Collection a un BMP y
        // sale (captura de QA a densidad nativa, sin captura de pantalla
        // del SO — brief §29). Ausente: no-op.
        if (const char* shot = std::getenv("NIMVLETS_DEV_PRODUCT_SHOT"); shot != nullptr && shot[0] != '\0') {
            productWindow_.CaptureToBmpForQA(shot);
            productWindow_.Close();
            Shutdown();
            return 0;
        }
    }

    // Solo-DEV (Block 09A-QA): smoke NO interactivo de que las tres
    // pestañas de la cabecera compartida (Collection · Shop · Settings)
    // son ALCANZABLES con un click desde cualquier sección — el mismo
    // camino que un click del owner (ProductWindow::ClickNavTabForQA ->
    // HandleEvent -> View::OnMouseDown -> ActivateWidget ->
    // NavTargetSection). El bug de este pase: CollectionView / ShopView
    // solo ruteaban Collection/Shop, así que "Settings" no hacía nada
    // desde las dos secciones donde el owner arranca. Requiere
    // NIMVLETS_DEV_APPDATA_DIR aislado. Ver README.md.
    if (const char* nav = std::getenv("NIMVLETS_DEV_UI_NAV_SMOKE");
        nav != nullptr && nav[0] != '\0' && nav[0] != '0') {
        OpenProductWindow();
        struct NavHop {
            productui::ProductSection from;
            productui::ProductSection to;
            const char* label;
        };
        static constexpr NavHop kHops[] = {
            {productui::ProductSection::kCollection, productui::ProductSection::kSettings, "Collection -> Settings"},
            {productui::ProductSection::kShop, productui::ProductSection::kSettings, "Shop -> Settings"},
            {productui::ProductSection::kSettings, productui::ProductSection::kCollection, "Settings -> Collection"},
            {productui::ProductSection::kSettings, productui::ProductSection::kShop, "Settings -> Shop"},
            {productui::ProductSection::kCollection, productui::ProductSection::kShop, "Collection -> Shop"},
            {productui::ProductSection::kShop, productui::ProductSection::kCollection, "Shop -> Collection"},
        };
        int failures = 0;
        for (const NavHop& hop : kHops) {
            productWindow_.ShowSectionForQA(hop.from);
            const productui::ProductSection got = productWindow_.ClickNavTabForQA(hop.to);
            const bool ok = got == hop.to;
            failures += ok ? 0 : 1;
            SDL_Log("nimvlets: [ui-nav-smoke] %-24s %s", hop.label, ok ? "PASS" : "FAIL");
        }
        SDL_Log("nimvlets: [ui-nav-smoke] %s (%d/%zu hop(s) reachable)",
                failures == 0 ? "ALL SECTIONS REACHABLE" : "UNREACHABLE SECTION(S) FOUND",
                static_cast<int>(sizeof(kHops) / sizeof(kHops[0])) - failures,
                sizeof(kHops) / sizeof(kHops[0]));
        productWindow_.Close();
        Shutdown();
        return failures == 0 ? 0 : 2;
    }

    // Mecanismo solo-DEV (Block 06): arranca con el pet oculto, para
    // capturar la Collection sin la ventana always-on-top del pet en el
    // cuadro. Equivale a elegir "Hide Nimvlet" en el menú. Ver README.md.
    if (const char* hidePet = std::getenv("NIMVLETS_DEV_HIDE_PET");
        hidePet != nullptr && hidePet[0] != '\0' && hidePet[0] != '0') {
        petHidden_ = true;
        SDL_HideWindow(window_);
        PushShellState();
        SDL_Log("nimvlets: DEV override active — NIMVLETS_DEV_HIDE_PET (pet window hidden at startup)");
    }

    // Mecanismo solo-DEV (Block 06): dispara una activación desde la
    // Collection sin un click real ("petId" o "petId/variantId"), para
    // smoke-testear el switch en vivo (misma ruta que el botón "Use"):
    // CanActivate -> TrySwitchActivePet -> refresco de preview/modelo/
    // menú. Ausente/vacía: no-op. Ver README.md y docs/PRODUCT_UI.md.
    // Mecanismo solo-DEV (Block 06): abre y cierra la ventana de
    // Collection N veces seguidas, para smoke-testear que el ciclo de
    // vida (crear/soltar todos los recursos de GPU, reabrir) es correcto
    // y no filtra ni crashea, y que el runtime del pet lo sobrevive
    // (block brief §18/§27). Ausente/vacía: no-op.
    if (const char* cyclesEnv = std::getenv("NIMVLETS_DEV_COLLECTION_CYCLES");
        cyclesEnv != nullptr && cyclesEnv[0] != '\0') {
        char* end = nullptr;
        const long n = std::strtol(cyclesEnv, &end, 10);
        if (end != cyclesEnv && n > 0) {
            SDL_Log("nimvlets: DEV override active — NIMVLETS_DEV_COLLECTION_CYCLES=%ld", n);
            for (long i = 0; i < n; ++i) {
                OpenProductWindow();
                productWindow_.RenderIfNeeded();
                productWindow_.Close();
            }
            SDL_Log(
                "nimvlets: DEV collection cycles complete — %ld open/close pair(s), pet '%s' still active, "
                "renderer alive=%s",
                n, pet_.id.c_str(), renderer_ != nullptr ? "yes" : "no");
        }
    }

    if (const char* act = std::getenv("NIMVLETS_DEV_ACTIVATE"); act != nullptr && act[0] != '\0') {
        const std::string spec(act);
        const std::size_t slash = spec.find('/');
        productui::ActivateRequest req;
        req.petId = slash == std::string::npos ? spec : spec.substr(0, slash);
        req.variantId = slash == std::string::npos ? std::string() : spec.substr(slash + 1);
        SDL_Log("nimvlets: DEV override active — NIMVLETS_DEV_ACTIVATE='%s'", act);
        HandleActivateRequest(req);
    }

    // Solo-DEV (Block 09A): captura la pantalla de onboarding a un BMP y
    // sale — para QA de la presentación sin correr el event loop (ni
    // esperar 44 s). Requiere NIMVLETS_DEV_ONBOARDING; combinable con
    // NIMVLETS_DEV_ONBOARDING_REVEAL / _STAGE / NIMVLETS_DEV_LANGUAGE.
    if (onboardingActive_) {
        if (const char* shot = std::getenv("NIMVLETS_DEV_PRODUCT_SHOT");
            shot != nullptr && shot[0] != '\0') {
            productWindow_.CaptureToBmpForQA(shot);
            productWindow_.Close();
            Shutdown();
            return 0;
        }
    }

    bool running = true;

    SDL_Event event;
    while (running && !ShutdownRequested()) {
        const double nowMs = static_cast<double>(SDL_GetTicks());

        double waitMs = 1000.0;  // ver kMaxWaitMs más abajo -- valor inicial, se recorta enseguida
        if (ambientDeadlineMs_) {
            waitMs = *ambientDeadlineMs_ - nowMs;
        }
        if (const std::optional<double> frameDeadline = animController_->NextFrameDeadlineMs()) {
            waitMs = std::min(waitMs, *frameDeadline - nowMs);
        }
        if (const std::optional<double> flushDeadline = persistenceScheduler_.NextFlushDeadlineMs()) {
            waitMs = std::min(waitMs, *flushDeadline - nowMs);
        }
        if (confirmRedrawDeadlineMs_) {
            waitMs = std::min(waitMs, *confirmRedrawDeadlineMs_ - nowMs);
        }
        if (onboardingRevealDeadlineMs_) {
            // El reveal secreto de los 44 s se integra al MISMO cálculo
            // de next-deadline (brief §12): antes del deadline el loop
            // puede dormir hasta el próximo evento o deadline, lo que
            // venga primero; no hay ningún timer thread ni polling.
            waitMs = std::min(waitMs, *onboardingRevealDeadlineMs_ - nowMs);
        }
        if (hoverDwellDeadlineMs_) {
            // Ver el comentario del campo en SpikeApp.h -- despierta el
            // loop exactamente cuando un dwell en curso cruzaría el
            // umbral de dwell, incluso sin más eventos de motion reales
            // (el caso de un cursor perfectamente quieto).
            waitMs = std::min(waitMs, *hoverDwellDeadlineMs_ - nowMs);
        }
        if (usingPollDrivenClickThrough_ && clickThroughSamplingActive_) {
            // Solo se acorta la espera mientras el muestreo está
            // realmente armado (cursor DENTRO del rectángulo de la
            // ventana). En reposo -- el caso dominante -- esta rama no
            // corre, así que el loop puede dormir hasta el próximo
            // evento real o deadline de animación, sin ningún despertar
            // a 60Hz. Ver core/ClickThroughPolicy.h y el informe de
            // este bloque para la medición de CPU.
            waitMs = std::min(waitMs, hoverScheduler_.MillisUntilNextFrame(nowMs));
        }
        if (waitMs < 0.0) {
            waitMs = 0.0;
        }
        // Acotado para que un SIGINT/SIGTERM pendiente siempre se note
        // dentro de aproximadamente un segundo -- ver el comentario
        // histórico de Block 02 en docs/ANIMATION_RUNTIME.md.
        constexpr double kMaxWaitMs = 1000.0;
        waitMs = std::min(waitMs, kMaxWaitMs);
        const Sint32 timeoutMs = static_cast<Sint32>(waitMs);

        if (SDL_WaitEventTimeout(&event, timeoutMs)) {
            HandleEvent(event, running);
            while (running && SDL_PollEvent(&event)) {
                HandleEvent(event, running);
            }
        }
        if (!running || ShutdownRequested()) {
            break;
        }

        const double afterMs = static_cast<double>(SDL_GetTicks());

        // Ninguna animación avanza mientras hay un drag REAL en curso
        // (Block 05, corrección de ciclo de vida): el drag tiene la
        // prioridad más alta y arrastra una pose base ESTABLE -- ver
        // MaybeCancelActionForDragStart(). En la práctica esto ya sería
        // un no-op (cancelar deja una pose estática, y Advance() sobre
        // kStatic no hace nada), pero se explicita para que el
        // invariante "cero avance de frame durante el arrastre" no
        // dependa de que el contenido resulte ser estático.
        if (!dragClassifier_.IsDragging() && animController_->Advance(afterMs)) {
            needsRedraw_ = true;
        }

        // Una acción one-shot que TERMINA reinicia el contador ambient
        // completo desde su instante exacto de terminación (Block 05,
        // corrección de ciclo de vida -- ver DEC-079). Esto reemplaza
        // al viejo disparador basado en "cambió CurrentStateId()", que
        // se perdía TODA terminación de self-loop (el click de Bunny/
        // Nidir, howl/tail_greet de Frin: empiezan y terminan en el
        // mismo estado, el id nunca cambia), dejando que la duración de
        // la animación se comiera parte del intervalo ambient.
        // RearmAmbientDeadline() lee el estado ACTUAL del controller,
        // que en este punto ya es el estado post-transición -- así que
        // esto cubre por igual el self-loop y el cambio de estado real.
        if (const std::optional<double> completedMs = animController_->ActionCompletedDuringLastAdvance()) {
            RearmAmbientDeadline(*completedMs);
        }

        // Una transición de BehaviorState real (p. ej. Frin seated ->
        // lying) invalida cualquier dwell de hover acumulado contra el
        // estado ANTERIOR. El rearme del timer ambient ya lo cubre el
        // bloque de terminación de arriba (más preciso: usa el instante
        // real de terminación, no `afterMs`), así que acá solo queda el
        // reset de hover.
        if (animController_->CurrentStateId() != lastKnownStateId_) {
            lastKnownStateId_ = animController_->CurrentStateId();
            ResetHoverDwell();
        }

        if (ambientDeadlineMs_ && afterMs >= *ambientDeadlineMs_) {
            // Solo dispara de verdad si el controller está en modo Base
            // (TriggerAmbientAction() ya es un no-op en cualquier otro
            // caso) -- pero el deadline SIEMPRE se reprograma, para que
            // un deadline que cae durante un click se salte
            // coherentemente en vez de encolarse.
            if (animController_->TriggerAmbientAction(NextUniformRandom01(), afterMs)) {
                MarkNeedsRedraw(afterMs);
            }
            RearmAmbientDeadline(afterMs);
        }

        if (hoverDwellDeadlineMs_ && afterMs >= *hoverDwellDeadlineMs_) {
            // El dwell en curso (si sigue vigente) cruza el umbral
            // justo ahora -- pero no asumimos que el cursor SIGUE
            // encima solo porque nadie mandó un evento de motion
            // nuevo (podría haberse quedado perfectamente quieto
            // encima, o podría haberse ido sin que llegara ningún
            // evento adicional en este camino). Se vuelve a muestrear
            // la posición real -- misma función que ya usa el
            // fallback poll-driven de Windows, ver SampleCursor() -- y
            // se alimenta por el mismo camino que cualquier otra
            // muestra de hover.
            const CursorSample sample = SampleCursor(window_);
            MaybeTriggerHoverAction(IsPointInteractive(sample.localPoint), afterMs);
        }

        if (const std::optional<double> flushDeadline = persistenceScheduler_.NextFlushDeadlineMs();
            flushDeadline && afterMs >= *flushDeadline) {
            FlushPersistedState();
        }

        if (confirmRedrawDeadlineMs_ && afterMs >= *confirmRedrawDeadlineMs_) {
            needsRedraw_ = true;
            confirmRedrawDeadlineMs_.reset();
        }

        // Deadline del reveal secreto de onboarding: transición UNA vez,
        // un redibujo, y se acabó (brief §12: "after reveal: no ongoing
        // secret timer work" — RevealOnboardingSecret limpia el deadline).
        if (onboardingRevealDeadlineMs_ && afterMs >= *onboardingRevealDeadlineMs_) {
            RevealOnboardingSecret(afterMs);
        }

        if (needsRedraw_) {
#ifndef NDEBUG
            SDL_Log(
                "nimvlets: [diag] animation redraw: mode=%s state='%s' frame=%p t=%.0fms",
                ControllerModeName(animController_->Mode()), animController_->CurrentStateId().c_str(),
                static_cast<const void*>(&animController_->CurrentFrame()), afterMs);
#endif  // NDEBUG
            RenderFrame();
            ApplyCurrentHitMask();
            needsRedraw_ = false;
        }

        if (usingPollDrivenClickThrough_ && clickThroughSamplingActive_ &&
            afterMs >= hoverScheduler_.NextFrameDeadline(afterMs)) {
            PollHover();
        }

        // Product UI: redibuja SOLO si su vista está sucia o hubo un
        // EXPOSED (event-driven, sin loop ni polling — block brief §19).
        // No-op barato cuando la Collection está cerrada, y no toca el
        // cálculo de waitMs de arriba, así que el idle del pet con la
        // Collection cerrada es idéntico al de antes de este bloque.
        if (productWindow_.IsOpen()) {
            productWindow_.RenderIfNeeded();
        }
    }

    Shutdown();
    return 0;
}

}  // namespace nimvlets::app
