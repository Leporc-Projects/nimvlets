#pragma once

#include "catalog/ActivePetResolution.h"
#include "catalog/PetCatalog.h"
#include "catalog/PetIdentity.h"
#include "content/AnimationController.h"
#include "content/AnimationDefinition.h"
#include "core/AlphaMask.h"
#include "core/DragClassifier.h"
#include "core/FrameScheduler.h"
#include "persistence/AppState.h"
#include "persistence/AppStateStore.h"
#include "persistence/PersistenceScheduler.h"

#include <SDL3/SDL.h>

#include <optional>

namespace nimvlets::app {

// Dueña del ciclo de vida de ventana/renderer de SDL y del event loop
// principal del pequeño runtime de contenido+animación+persistencia+
// catálogo, data-driven, construido a través de Block 01 (spike de
// plataforma), Block 02 (contenido/animación), Block 03 (persistencia
// local de estado — ver docs/PERSISTENCE.md) y Block 04 (catálogo de
// pets + switching en runtime — ver docs/CATALOG.md).
//
// This is still explicitly a foundation/spike executable, not the
// product — see docs/PLATFORM_SPIKE.md, docs/ANIMATION_RUNTIME.md, and
// the block briefs' NON-SCOPE lists for what it deliberately does not do
// (Shop, Collection, onboarding, pet-selector UI, audio, global click
// mode, final content for all 8 Nimvlets, ...).
//
// El pet activo es enteramente datos: cuál pack cargar se resuelve
// contra catalog_ (ver Init() y TrySwitchActivePet()) — esta clase no
// contiene ninguna rama específica de un pet, ninguna forma
// hardcodeada, y ningún conocimiento de "Bunny" más allá de ser la
// única entrada real del catálogo de dev de este bloque. Agregar un
// pet nuevo al catálogo nunca requiere tocar este archivo.
class SpikeApp {
public:
    // Runs until the window is closed. Returns a process exit code (0 on
    // clean shutdown, non-zero if SDL initialization or content loading
    // failed).
    int Run();

private:
    bool Init();
    void Shutdown();

    void HandleEvent(const SDL_Event& event, bool& running);

    // Draws content::AnimationController::CurrentFrame()'s texture.
    // Called only when needsRedraw_ is set — see that field's doc
    // comment — never on a fixed per-frame tick.
    void RenderFrame();

    // Rebuilds activeHitMask_ from the controller's current frame (see
    // core::AlphaMask::FromAlphaChannel) and, on platforms where it's
    // render-safe (see platform::NativeShapeHitTestIsRenderSafe()),
    // pushes it to the OS via SDL_SetWindowShape. Called alongside
    // RenderFrame() only when the displayed frame actually changed —
    // never every tick — so the window shape surface is rebuilt exactly
    // as often as the picture it hit-tests against.
    void ApplyCurrentHitMask();

    void PollHover();
    void UpdateClickThrough(bool cursorOverOpaque);

    // True if `localPoint` (window-local, logical coordinates) is over
    // the currently active pet frame's real alpha-derived hit region.
    // Every hit-test in this file (the MOUSE_BUTTON_DOWN defense-in-
    // depth check, the poll-driven fallback's hover check, and the
    // SDL_SetWindowShape surface built in ApplyCurrentHitMask()) goes
    // through this one predicate.
    bool IsPointInteractive(core::Point localPoint) const;

    // Creates/releases one SDL_Texture per frame across pet_'s idle,
    // every idle direction override (Block 04.2 — see
    // docs/NIDIR_CONTENT.md), click-reaction, and every passive action
    // — see graphics::AttachFrameTexture()/ReleaseFrameTexture().
    void AttachAllTextures();
    void ReleaseAllTextures();

    // Reads NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS (if set to a valid
    // positive number) to shorten the passive-action wait for manual QA,
    // without ever touching pet_.passiveIntervalSeconds itself — the
    // pack's authored ~300s default stays the production value in every
    // build; this is purely an opt-in override for *this run*. See
    // docs/ANIMATION_RUNTIME.md, "DEV passive-interval override".
    double ComputeEffectivePassiveIntervalSeconds() const;

    // Escribe appState_ vía appStateStore_ si — y solo si —
    // persistenceScheduler_ está dirty en ese momento; no hace nada en
    // caso contrario, y no hace nada si appStateStore_ nunca se
    // inicializó (ver el comentario de Init() sobre el fallo de
    // SDL_GetPrefPath()). Se llama tanto desde el event loop, cuando se
    // alcanza el deadline de persistenceScheduler_, como
    // incondicionalmente desde Shutdown() (el shutdown limpio siempre
    // flushea lo que quede dirty, sin importar el deadline de
    // debounce — ver docs/PERSISTENCE.md).
    void FlushPersistedState();

    // La API de switching en runtime reutilizable (block brief §4):
    // intenta cargar el pack de `target` según catalog_. Si `target` no
    // está en el catálogo, o su pack no carga, retorna false, deja un
    // log claro, y NO toca pet_/animController_/appState_ en absoluto —
    // el pet activo anterior sigue completamente usable. Si carga con
    // éxito: suelta las texturas del pet anterior, reemplaza pet_,
    // reatacha texturas del nuevo, reconstruye animController_ (que
    // arranca en Idle del pet nuevo — exactamente lo requerido),
    // actualiza appState_.activePetId/activeVariantId, marca
    // persistenceScheduler_ dirty, y pide un redraw. Ver
    // docs/CATALOG.md.
    bool TrySwitchActivePet(const catalog::PetIdentity& target);

    // Mecanismo solo-DEV para smoke-testear el switching de forma no
    // interactiva contra el binario real (block brief §4: "no debe
    // convertirse en comportamiento de producto"). Si
    // NIMVLETS_DEV_SWITCH_TEST_COUNT es un entero positivo válido,
    // ejecuta esa cantidad de llamadas a TrySwitchActivePet() -- una
    // detrás de otra, cicladas por catalog_.Entries() -- inmediatamente
    // después de que Init() termina y antes de entrar al loop
    // principal, logueando cada resultado. Sin la variable de entorno,
    // esto es un no-op total: cero cambio de comportamiento en
    // producción. Ver docs/CATALOG.md.
    void RunDevSwitchSmokeTestIfRequested();

    // El "runtime method to change direction" que pide el block brief
    // 04.2 §7 — todavía sin ningún control de UI que lo dispare
    // (explícitamente fuera de alcance de este bloque). Delega en
    // content::AnimationController::SetDirection(); si el frame
    // mostrado cambió de verdad, pide un redraw (el loop principal se
    // encarga de RenderFrame()+ApplyCurrentHitMask() en su próxima
    // vuelta, igual que TrySwitchActivePet() ya hace). No toca
    // appState_ — ver el comentario de appState_ sobre por qué la
    // dirección no se persiste en este bloque.
    void SetActiveDirection(content::Direction direction);

    // Mecanismo solo-DEV para smoke-testear cambios de dirección de
    // forma no interactiva contra el binario real, reflejando
    // exactamente el patrón ya establecido de
    // RunDevSwitchSmokeTestIfRequested() (block brief §9: "repeated
    // direction changes do not accumulate logical resources" necesita
    // poder ejercitarse sin QA manual). Si
    // NIMVLETS_DEV_DIRECTION_TEST_COUNT es un entero positivo válido,
    // alterna esa cantidad de veces entre Direction::kRight/kLeft
    // -- sincrónicamente, antes del loop principal -- logueando cada
    // cambio. Sin la variable de entorno, no-op total.
    void RunDevDirectionSmokeTestIfRequested();

    // Mecanismo solo-DEV para smoke-testear el click reaction de forma
    // no interactiva contra el binario real -- este bloque no tiene
    // ninguna forma de sintetizar un click de mouse real de forma
    // segura/no invasiva (ver AGENTS.md §5), así que esto llama
    // directamente a animController_->TriggerClick(), el mismo método
    // que SDL_EVENT_MOUSE_BUTTON_UP ya llama para un click real -- ver
    // HandleEvent(). Si NIMVLETS_DEV_CLICK_TEST_COUNT es un entero
    // positivo válido, ejecuta esa cantidad de clicks sintéticos --
    // uno detrás de otro, sincrónicamente, antes del loop principal --
    // exactamente igual que RunDevSwitchSmokeTestIfRequested(). El
    // último click deja al controller efectivamente en
    // ControllerState::kClickReaction al momento en que el loop
    // principal arranca, así que su reproducción real (frame a frame,
    // deadline-driven) se puede observar/medir contra el binario
    // corriendo de verdad. Sin la variable de entorno, no-op total.
    void RunDevClickSmokeTestIfRequested();

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;

    // --- Block 04: catálogo de pets (ver docs/CATALOG.md) ---
    // Cargado una única vez en Init() desde kCatalogPath (ver
    // SpikeApp.cpp) vía catalog::LoadCatalogFromFile(). Puro metadato
    // -- nunca carga por adelantado los packs de otras entradas; el
    // único pack efectivamente en memoria en todo momento es el de
    // pet_ (ver TrySwitchActivePet()).
    catalog::PetCatalog catalog_;

    // El pet activo actual, con todo su contenido data-driven --
    // reemplazado por completo en cada switch exitoso (ver
    // TrySwitchActivePet()), nunca mutado incrementalmente. Declarado
    // antes de animController_ para que exista (default-construido, si
    // aún no se cargó nada) en el momento en que el member-initializer
    // de animController_ le enlaza una referencia -- ver el comentario
    // de animController_ para por qué ese orden importa y por qué
    // sigue siendo seguro incluso después de reemplazar pet_ en un
    // switch (ver TrySwitchActivePet()).
    content::PetDefinition pet_;

    // Constructed only after pet_ is successfully loaded (Init() calls
    // animController_.emplace(pet_)) — std::optional rather than binding
    // a reference to pet_ before it holds real content, so there is
    // never a window where a reference exists to not-yet-loaded data.
    std::optional<content::AnimationController> animController_;

    // The rasterized hit-test region for animController_'s *current*
    // frame — rebuilt by ApplyCurrentHitMask() only when the frame
    // changes. 1x1 placeholder until Init() populates real content
    // (never used as such: Init() always calls ApplyCurrentHitMask()
    // before the window becomes interactive).
    core::AlphaMask activeHitMask_{1, 1};

    core::DragClassifier dragClassifier_;
    int clickCount_ = 0;

    // Set whenever something that affects the displayed picture happens
    // (a frame-advance from animController_->Advance(), a
    // TriggerClick()/TriggerPassiveAction() call, or an
    // SDL_EVENT_WINDOW_EXPOSED asking us to repaint) and cleared right
    // after RenderFrame()+ApplyCurrentHitMask() run for it. This is the
    // mechanism that makes static idle render/redraw *nothing* — no
    // fixed tick exists anymore (contrast Block 01's frameScheduler_).
    // Starts true so the very first frame renders.
    bool needsRedraw_ = true;

    // Effective seconds between sparse passive actions for this run —
    // see ComputeEffectivePassiveIntervalSeconds(). Computed once in
    // Init(); pet_.passiveIntervalSeconds (the pack's authored default)
    // is never mutated.
    double passiveIntervalSecondsEffective_ = 300.0;
    double nextPassiveDeadlineMs_ = 0.0;
    std::size_t nextPassiveActionIndex_ = 0;

    // --- Block 03: persistencia local (ver docs/PERSISTENCE.md) ---
    // Se carga una vez en Init() (o queda en defaults seguros si aún
    // no existe ningún save, o el save no se puede leer) y se muta
    // mientras la app corre; nunca se relee desde disco hasta el
    // *siguiente* arranque del proceso.
    //
    // La dirección activa (Block 04.2 — ver SetActiveDirection()) NO
    // vive acá ni en ningún lado persistido: el block brief §7 es
    // explícito ("Persistence of direction is not required unless
    // essentially free and clearly justified") y agregar un campo acá
    // exigiría tocar el formato NVSTATE1 (bump de schema, migración)
    // por un beneficio que nadie pidió — no calza con "esencialmente
    // gratis". animController_ arranca en Direction::kRight en cada
    // Init()/TrySwitchActivePet(), sin excepción.
    persistence::AppState appState_;

    // Se construye en Init() una vez que se resuelve el directorio de
    // app-data por usuario vía SDL_GetPrefPath(). std::nullopt solo si
    // esa resolución falla (raro — p. ej. un home directory sin
    // permisos de escritura); en ese caso la app corre completamente
    // normal, simplemente no carga ni guarda nada en esta sesión — ver
    // el comentario de Init(). std::optional en vez de un miembro
    // plano por la misma razón que animController_ arriba: no se
    // conoce ninguna ruta hasta que Init() corre.
    std::optional<persistence::AppStateStore> appStateStore_;

    // Aplica debounce a las escrituras a disco para que los clicks
    // rápidos se coalescan en una sola escritura en vez de una por
    // click — ver docs/PERSISTENCE.md y el comentario de
    // FlushPersistedState().
    persistence::PersistenceScheduler persistenceScheduler_;

    // True once Init() has successfully handed hit-testing to the
    // platform's own native mechanism (SDL_SetWindowShape on macOS —
    // see platform::NativeShapeHitTestIsRenderSafe()). When true, the
    // poll-driven fallback below (hoverScheduler_, PollHover(),
    // UpdateClickThrough()) is never invoked — the OS handles
    // click-through on every real mouse event, with zero polling.
    bool usingNativeShapeHitTest_ = false;

    // True once Init() has determined that the poll-driven fallback
    // below is actually worth running — i.e. usingNativeShapeHitTest_
    // is false AND platform::ClickThroughPollingIsMeaningful() says a
    // poll-triggered SetWindowClickThrough() call could really change
    // OS-level input delivery here. Added in Block 04.1: on Windows
    // this always matches !usingNativeShapeHitTest_ (unchanged
    // behavior — the poll fallback genuinely works there), but on
    // Linux/Wayland neither the native shape path nor the poll
    // fallback can do anything (see
    // src/platform/linux/TransparentWindowSupport.cpp and
    // docs/LINUX_PLATFORM.md), so this stays false there and the event
    // loop never starts the ~60Hz wakeup loop at all — running it
    // anyway, knowing in advance it could never change anything, would
    // be exactly the permanent polling loop AGENTS.md §2 forbids.
    bool usingPollDrivenClickThrough_ = false;

    // --- poll-driven click-through FALLBACK, used only when
    // usingPollDrivenClickThrough_ is true (currently: Windows, and
    // X11 in practice never reaches here either — see
    // platform::NativeShapeHitTestIsRenderSafe()'s doc comment for why
    // macOS/Linux-X11 don't need this). Unchanged in spirit from
    // Block 01 — still a bounded SDL_WaitEventTimeout wakeup at ~60 Hz,
    // never a busy-wait, still a cursor-*position* poll
    // (SDL_GetGlobalMouseState), not a global input hook. See
    // AGENTS.md's privacy rules. ---
    core::FrameScheduler hoverScheduler_{1000.0 / 60.0};
    bool currentlyClickThrough_ = false;

    // Click-through pipeline instrumentation (fallback path only — see
    // PollHover()'s and UpdateClickThrough()'s doc comments, and
    // docs/PLATFORM_SPIKE.md's "click-through instrumentation"
    // section). Tracks the last-logged value of each stage of the
    // pipeline so PollHover()/UpdateClickThrough() can log a line ONLY
    // when a stage's value actually changes — never once per poll/frame
    // regardless of change, which would flood the log at 60 Hz.
    // `diagHasValue_` guards the very first sample so it's always
    // logged once, establishing a baseline. The fields themselves (not
    // just the logging that uses them) are compiled out entirely in
    // Release builds (#ifndef NDEBUG) — matching the call sites exactly
    // avoids "unused private field" warnings there instead of just
    // silencing them.
#ifndef NDEBUG
    bool diagHasValue_ = false;
    float diagGlobalX_ = 0.0f;
    float diagGlobalY_ = 0.0f;
    int diagWindowX_ = 0;
    int diagWindowY_ = 0;
    core::Point diagLocalPoint_{};
    bool diagContains_ = false;
    bool diagRequestedClickThrough_ = false;
    bool diagActualIgnoresMouseEvents_ = false;
#endif  // NDEBUG

    // Manual window drag state. We deliberately do not use
    // SDL_HITTEST_DRAGGABLE / OS-level window dragging: that would hand
    // the whole gesture to the window manager and we would never see the
    // button-down/motion/button-up sequence core::DragClassifier needs
    // to tell a click from a drag.
    double dragGrabOffsetX_ = 0.0;
    double dragGrabOffsetY_ = 0.0;
};

// Returns true once a SIGINT/SIGTERM has been received.
//
// The spike window is intentionally borderless, NOT_FOCUSABLE, and
// excluded from the Dock (see Init()) — that's correct for a desktop
// companion, but it also means there is no window-chrome close button
// and no way for it to ever receive Cmd+Q. This is the dev/QA exit path
// (`kill -TERM <pid>` or Ctrl+C in the launching terminal) used to
// verify "clean shutdown, no hung process" without adding any real UI.
// It is not a product feature — see docs/PLATFORM_SPIKE.md.
bool ShutdownRequested();

}  // namespace nimvlets::app
