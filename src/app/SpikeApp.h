#pragma once

#include "catalog/ActivePetResolution.h"
#include "catalog/PetCatalog.h"
#include "catalog/PetIdentity.h"
#include "content/AnimationController.h"
#include "content/AnimationDefinition.h"
#include "core/AlphaMask.h"
#include "core/DragClassifier.h"
#include "core/FrameScheduler.h"
#include "core/HoverDwellTracker.h"
#include "persistence/AppState.h"
#include "persistence/AppStateStore.h"
#include "persistence/PersistenceScheduler.h"

#include <SDL3/SDL.h>

#include <optional>
#include <random>
#include <string>

namespace nimvlets::app {

// Dueña del ciclo de vida de ventana/renderer de SDL y del event loop
// principal del pequeño runtime de contenido+animación+persistencia+
// catálogo, data-driven, construido a través de Block 01-04 y
// generalizado en Block 05 a un grafo de comportamiento por-estado
// (ver docs/ANIMATION_RUNTIME.md) — el mismo mecanismo sirve tanto a un
// pet "normal" de un solo estado (Bunny, Nidir) como a un pet con
// postura/transiciones reales (Frin: seated/lying).
//
// This is still explicitly a foundation/spike executable, not the
// product — see docs/PLATFORM_SPIKE.md, docs/ANIMATION_RUNTIME.md, and
// the block briefs' NON-SCOPE lists.
//
// El pet activo es enteramente datos: cuál pack cargar se resuelve
// contra catalog_ — esta clase no contiene ninguna rama específica de
// un pet. Agregar un pet nuevo al catálogo nunca requiere tocar este
// archivo.
class SpikeApp {
public:
    // Runs until the window is closed. Returns a process exit code (0 on
    // clean shutdown, non-zero if SDL initialization or content loading
    // failed).
    int Run();

    // Segundos de dwell continuo requeridos para que hover dispare
    // (Block 05, corrección de comportamiento — ver
    // MaybeTriggerHoverAction()). Bajado de 5.0 a 1.0 en la segunda
    // pasada de corrección post-QA del owner ("current 5-second dwell
    // is too long") -- el mecanismo en sí (dwell continuo, resets en
    // salida/click/drag/cambio de estado, desacoplado del timer
    // ambient) no cambia, solo este único umbral. `static constexpr`
    // (no un anonymous-namespace constant en el .cpp) porque tanto el
    // inicializador de hoverDwellTracker_ como el cálculo de
    // hoverDwellDeadlineMs_ en SpikeApp.cpp lo necesitan.
    static constexpr double kHoverDwellSeconds = 1.0;

private:
    bool Init();
    void Shutdown();

    void HandleEvent(const SDL_Event& event, bool& running);

    // Draws content::AnimationController::CurrentFrame()'s texture,
    // scaled into the pet's EFFECTIVE (visual-scale-adjusted) canvas
    // rect. Called only when needsRedraw_ is set.
    void RenderFrame();

    // Rebuilds activeHitMask_ from the controller's current frame at
    // the pet's EFFECTIVE canvas size and, on platforms where it's
    // render-safe, pushes it to the OS via SDL_SetWindowShape.
    void ApplyCurrentHitMask();

    void PollHover();
    void UpdateClickThrough(bool cursorOverOpaque);

    // TEMP diagnostic-only (Block 05 Bunny root-cause investigation) —
    // ver el comentario de kDevDumpFramesDirEnvVar en SpikeApp.cpp.
    void DumpCurrentRenderedFrame(const std::string& dir);

    // Marca needsRedraw_ Y arma confirmRedrawDeadlineMs_ (Block 05 —
    // ver el comentario de ese campo en este header): el punto único
    // que toda transición de animación/estado real (click, disparo
    // ambient/hover, cambio de dirección, switch de pet) usa para
    // pedir un redraw, en vez de asignar needsRedraw_ = true a mano en
    // cada call site. Un simple frame-advance DENTRO de una animación
    // ya en reproducción (AnimationController::Advance() devolviendo
    // true sin que el estado/modo haya cambiado) NO pasa por acá —
    // sigue usando needsRedraw_ = true directo, para no rearmar el
    // redraw de confirmación en cada frame de una animación ya
    // "caliente".
    void MarkNeedsRedraw(double nowMs);

    // True if `localPoint` (window-local, logical coordinates) is over
    // the currently active pet frame's real alpha-derived hit region.
    bool IsPointInteractive(core::Point localPoint) const;

    // El tamaño EFECTIVO en pantalla (puntos lógicos) del pet activo —
    // pet_.canvasWidth/Height multiplicado por pet_.visualScale y
    // redondeado (Block 05, escala visual por-pet — ver el comentario
    // de PetDefinition::visualScale). Éste, no pet_.canvasWidth/Height
    // directamente, es lo que gobierna el tamaño de ventana, el
    // render-target lógico, y las dimensiones del hit-mask — así los
    // tres se mantienen alineados entre sí y con el alto-DPI sin
    // importar la escala. El arte fuente y los bytes del pack
    // compilado nunca se tocan: esto es puramente cuánto se estira al
    // dibujar.
    int EffectiveCanvasWidth() const;
    int EffectiveCanvasHeight() const;

    // Creates/releases one SDL_Texture per frame across TODAS las
    // colecciones de animación de pet_ (cada BehaviorState::
    // baseAnimation + sus overrides, y cada WeightedAction de
    // ambient/hover/click + sus propios overrides) — ver
    // graphics::AttachFrameTexture()/ReleaseFrameTexture() y el aviso
    // junto a content::PetDefinition sobre por qué esta cobertura
    // importa.
    void AttachAllTextures();
    void ReleaseAllTextures();

    // Segundos objetivo de intervalo ambient para `state`, aplicando
    // NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS si está seteada a un valor
    // positivo válido (mecanismo solo-DEV, ver su comentario en
    // SpikeApp.cpp) — nunca muta state.ambientIntervalSeconds en sí.
    double ComputeEffectiveAmbientIntervalSeconds(const content::BehaviorState& state) const;

    // (Re)arma ambientDeadlineMs_ a partir del BehaviorState ACTUAL de
    // animController_: nullopt si ese estado no define ambientActions
    // (p. ej. Frin "lying" — sin timer, nunca se despierta el loop para
    // esto), o `nowMs + intervalo*1000` si sí. Se llama:
    //   - una vez tras cargar/cambiar de pet;
    //   - cada vez que el loop principal detecta que el estado activo
    //     cambió (ver lastKnownStateId_) — así un pet con estados
    //     (Frin) rearma correctamente el timer al entrar/salir de un
    //     estado con/sin comportamiento ambient propio;
    //   - cuando el timer ambient dispara de verdad (o se salta por
    //     estar ocupado — ver el loop principal en SpikeApp::Run());
    //   - y, Block 05 segunda pasada de corrección post-QA ("el
    //     ambient no debe dispararse recién después de que el owner
    //     acaba de interactuar"): en CUALQUIER interacción real del
    //     owner con el pet -- el cursor entrando a la región
    //     interactiva, un disparo de hover completo, un click, el
    //     inicio y el fin de un drag. Esto reinicia el conteo de
    //     ~15s desde ESE momento, en vez de dejar que el ambient
    //     dispare inmediatamente después de una interacción que
    //     casualmente terminó justo cuando el deadline anterior ya
    //     había vencido.
    void RearmAmbientDeadline(double nowMs);

    // Escribe appState_ vía appStateStore_ si — y solo si —
    // persistenceScheduler_ está dirty en ese momento.
    void FlushPersistedState();

    // La API de switching en runtime reutilizable: intenta cargar el
    // pack de `target` según catalog_. Si falla, retorna false y no
    // toca pet_/animController_/appState_ en absoluto. Si carga con
    // éxito: suelta las texturas del pet anterior, reemplaza pet_,
    // reatacha texturas del nuevo, reconstruye animController_ (que
    // arranca en el estado[0]/kBase del pet nuevo), actualiza
    // appState_.activePetId/activeVariantId, marca
    // persistenceScheduler_ dirty, y pide un redraw.
    bool TrySwitchActivePet(const catalog::PetIdentity& target);

    // Mecanismo solo-DEV para smoke-testear el switching de forma no
    // interactiva contra el binario real. Ver
    // NIMVLETS_DEV_SWITCH_TEST_COUNT.
    void RunDevSwitchSmokeTestIfRequested();

    // El "runtime method to change direction" — delega en
    // content::AnimationController::SetDirection(); si el frame
    // mostrado cambió de verdad, pide un redraw.
    void SetActiveDirection(content::Direction direction);

    // Política automática de dirección por mitad de pantalla: calcula
    // en qué mitad del display que contiene la ventana cae el CENTRO
    // de la ventana y llama a SetActiveDirection() con el resultado.
    void UpdateDirectionFromWindowPosition();

    // Mecanismo solo-DEV para smoke-testear cambios de dirección. Ver
    // NIMVLETS_DEV_DIRECTION_TEST_COUNT.
    void RunDevDirectionSmokeTestIfRequested();

    // Mecanismo solo-DEV para smoke-testear el click de forma no
    // interactiva. Ver NIMVLETS_DEV_CLICK_TEST_COUNT.
    void RunDevClickSmokeTestIfRequested();

    // Política de hover (Block 05, corrección de comportamiento real
    // #2 — reemplaza el diseño de flanco+cooldown de la corrección
    // anterior). El owner pidió explícitamente: "no quiero que se
    // dispare una animación apenas el mouse entra" — hover ahora exige
    // DWELL: el cursor debe permanecer CONTINUAMENTE sobre la región
    // interactiva durante kHoverDwellSeconds (1s, segunda pasada de
    // corrección post-QA -- bajado de 5s) antes de disparar. Si sale
    // antes, el contador se reinicia por completo (ver
    // core::HoverDwellTracker). Sigue disparando la MISMA
    // TriggerHoverAction() que el timer ambient puede disparar (vía el
    // pool efectivo de hover del estado activo — ver
    // content::EffectiveHoverActions()); nunca toca
    // clickCount_/appState_.clickBalance/persistencia; sigue
    // completamente desacoplado de ambientDeadlineMs_ en el sentido de
    // que NINGÚN reloj es compartido entre ambos -- pero un dwell que
    // recién arranca (el cursor entrando a la región interactiva) SÍ
    // reinicia el timer ambient por separado (ver
    // RearmAmbientDeadline(), llamado acá mismo) porque "el owner
    // recién interactuó con el pet" también aplica a un simple pasar
    // el mouse por encima, no solo a un disparo de hover completo.
    //
    // Prioridad click/drag > hover/pasiva > estática: un click, un
    // drag, o un cambio de BehaviorState reinician el dwell (ver
    // ResetHoverDwell()) incluso si el cursor sigue físicamente
    // encima — el owner lo pidió explícitamente ("si hay click, drag o
    // cambio de estado, el dwell-hover debe resetearse").
    void MaybeTriggerHoverAction(bool cursorOverOpaque, double nowMs);

    // Reinicia hoverDwellTracker_ Y limpia hoverDwellDeadlineMs_ — el
    // único punto que un click, el inicio de un drag, o una transición
    // de BehaviorState real usan para invalidar un dwell en curso.
    void ResetHoverDwell();

    // Devuelve un double uniforme en [0, 1) de passiveActionRng_ — la
    // única fuente de aleatoriedad real de todo este archivo.
    double NextUniformRandom01();

    // Mecanismo solo-DEV para smoke-testear el disparo por hover. Ver
    // NIMVLETS_DEV_HOVER_TEST_COUNT.
    void RunDevHoverSmokeTestIfRequested();

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;

    catalog::PetCatalog catalog_;

    // El pet activo actual, con todo su contenido data-driven —
    // reemplazado por completo en cada switch exitoso.
    content::PetDefinition pet_;

    std::optional<content::AnimationController> animController_;

    // The rasterized hit-test region for animController_'s *current*
    // frame, at EffectiveCanvasWidth()/Height() — rebuilt by
    // ApplyCurrentHitMask() only when the frame changes.
    core::AlphaMask activeHitMask_{1, 1};

    core::DragClassifier dragClassifier_;
    int clickCount_ = 0;

    // Set whenever something that affects the displayed picture
    // happens. Cleared right after RenderFrame()+ApplyCurrentHitMask()
    // run for it. Starts true so the very first frame renders.
    bool needsRedraw_ = true;

    // Redraw de confirmación programado, con separación real de
    // wall-clock (Block 04.3 originalmente solo para cambios de
    // dirección; Block 05 lo generaliza a CUALQUIER transición de
    // animación/estado — ver MarkNeedsRedraw() — tras encontrar, con
    // un dump real de frames renderizados, que el primer render "frío"
    // de una sesión puede leerse de vuelta vacío; ver el informe final
    // para el detalle y sus límites honestos). `nullopt` la mayor
    // parte del tiempo.
    std::optional<double> confirmRedrawDeadlineMs_;

    // Último BehaviorState::id conocido de animController_ — usado
    // solo para detectar una transición de estado real en el loop
    // principal y (re)armar ambientDeadlineMs_ en consecuencia (ver
    // RearmAmbientDeadline()). No tiene ningún otro uso.
    std::string lastKnownStateId_;

    // Deadline del disparo ambient (timer) para el BehaviorState
    // activo — nullopt si ese estado no define ambientActions (p. ej.
    // Frin "lying"). Reemplaza al viejo nextPassiveDeadlineMs_ (un
    // único valor por-pet); ahora es por-estado, re-armado por
    // RearmAmbientDeadline().
    std::optional<double> ambientDeadlineMs_;

    // Tracker de dwell continuo (Block 05 — ver el comentario de
    // MaybeTriggerHoverAction()). Reemplaza al viejo par
    // hoverPassiveGate_/hoverCooldownUntilMs_ (flanco + cooldown
    // corto) por un requisito real de permanencia continua.
    core::HoverDwellTracker hoverDwellTracker_{kHoverDwellSeconds};

    // Deadline de wall-clock en el que el dwell EN CURSO cruzaría el
    // umbral de kHoverDwellSeconds -- nullopt si no hay ningún dwell
    // activo ahora mismo. Necesario porque, a diferencia del timer
    // ambient, hover en el camino nativo (macOS/Linux-X11) solo se
    // alimenta de eventos SDL_EVENT_MOUSE_MOTION reales -- si el
    // cursor se queda PERFECTAMENTE quieto sobre el pet (el caso
    // exacto que "dwell" debe detectar), ningún evento de motion
    // nuevo llega para volver a chequear el umbral. El loop principal
    // arma un wakeup en este deadline (igual que ambientDeadlineMs_/
    // confirmRedrawDeadlineMs_) y, al llegar, vuelve a MUESTREAR la
    // posición real del cursor (SampleCursor(), la misma función que
    // ya usa el fallback poll-driven de Windows) en vez de asumir que
    // sigue encima -- así el mecanismo es correcto sin importar si el
    // mouse se movió o no durante esos kHoverDwellSeconds.
    std::optional<double> hoverDwellDeadlineMs_;

    // Fuente real de aleatoriedad para elegir qué WeightedAction
    // disparar (ambient/hover/click, política ponderada). Sembrado una
    // sola vez en Init() desde std::random_device.
    std::mt19937 passiveActionRng_;

    persistence::AppState appState_;
    std::optional<persistence::AppStateStore> appStateStore_;
    persistence::PersistenceScheduler persistenceScheduler_;

    bool usingNativeShapeHitTest_ = false;
    bool usingPollDrivenClickThrough_ = false;

    core::FrameScheduler hoverScheduler_{1000.0 / 60.0};
    bool currentlyClickThrough_ = false;

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

    double dragGrabOffsetX_ = 0.0;
    double dragGrabOffsetY_ = 0.0;

    // TEMP diagnostic-only — ver DumpCurrentRenderedFrame().
    int devDumpFrameCount_ = 0;
};

// Returns true once a SIGINT/SIGTERM has been received.
bool ShutdownRequested();

}  // namespace nimvlets::app
