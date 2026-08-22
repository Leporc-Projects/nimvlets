#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "content/AnimationDefinition.h"

namespace nimvlets::content {

// Qué está reproduciendo el controller ahora mismo. "Base" reemplaza al
// viejo "Idle" (Block 02-04.3): la pose de reposo del BehaviorState
// activo, sea la única pose estática de un pet normal o "seated"/
// "lying" de un pet con estados. "AmbientOrHoverAction" y "ClickAction"
// reemplazan a "PassiveAction"/"ClickReaction" — genéricos sobre CUÁL
// WeightedAction se está reproduciendo, no solo "el" passive/click de
// un pet de un solo estado.
enum class ControllerMode {
    kBase,
    kAmbientOrHoverAction,
    kClickAction,
};

// A small, deadline-driven, SDL-free animation+behavior state machine
// for one active PetDefinition. Deliberately pure/testable — see
// tests/AnimationControllerTest.cpp — so its timing logic can be
// verified with fabricated timestamps.
//
// Behavioral contract (see docs/ANIMATION_RUNTIME.md for the full
// writeup):
// - El controller siempre está "en" un BehaviorState (pet.states[i]) y
//   en un ControllerMode dentro de él. Al reposo (kBase) muestra
//   ResolveAnimation(state.baseAnimation, overrides, direction). Si es
//   PlaybackKind::kStatic, NextFrameDeadlineMs() retorna nullopt para
//   siempre.
// - TriggerClick(rand, now) elige (ponderado) una entrada de
//   state.clickActions y la reproduce, A MENOS que ya haya una
//   ClickAction en curso (coalesce, no reinicia el gesto) — pero SÍ
//   interrumpe una AmbientOrHoverAction en curso (click > passive/hover
//   > static). No-op si state.clickActions está vacío.
// - TriggerAmbientAction(rand, now)/TriggerHoverAction(rand, now) solo
//   tienen efecto si el controller está en kBase ahora mismo — llegar
//   mientras hay un ClickAction u otra AmbientOrHoverAction en curso es
//   un no-op silencioso (nunca interrumpen nada). TriggerHoverAction
//   consulta EffectiveHoverActions(state) — el mismo pool que ambient
//   si el estado no define uno propio. Ambas retornan true si de
//   verdad dispararon algo (para que el llamador sepa si reprogramar un
//   deadline/marcar redraw), false si fue un no-op.
// - Cuando el WeightedAction en curso (ambient/hover/click) termina
//   (returnsToIdle == true en su animación), el controller transiciona
//   al BehaviorState nombrado por su targetStateId (puede ser el MISMO
//   estado — un self-loop, lo normal para un pet sin estados reales) y
//   vuelve a kBase mostrando la pose base de ESE estado. Ese instante se
//   reporta vía ActionCompletedDuringLastAdvance() — ver su comentario:
//   un self-loop NO cambia CurrentStateId(), así que el id de estado NO
//   sirve como señal de "terminó una acción".
// - CancelActionToCurrentState() aborta una acción en curso y vuelve a
//   la pose base del estado ACTUAL (nunca al targetStateId pendiente) —
//   el mecanismo genérico que le da al drag la prioridad más alta.
class AnimationController {
public:
    explicit AnimationController(const PetDefinition& pet);

    // Advances the current animation according to elapsed time,
    // possibly stepping through more than one frame boundary (or
    // completing a one-shot and transitioning state) if `nowMs` is far
    // enough past the last call. Returns true if the *displayed frame*
    // actually changed. Si durante esta llamada una acción one-shot
    // terminó, ActionCompletedDuringLastAdvance() lo reporta.
    bool Advance(double nowMs);

    // El instante EXACTO (tiempo de contenido, no wall-clock del
    // llamador) en que una acción one-shot terminó durante la ÚLTIMA
    // llamada a Advance() — nullopt si esa llamada no completó ninguna.
    // Se recalcula en cada Advance(); nunca queda "pegado" de una
    // llamada anterior.
    //
    // Por qué existe (Block 05, corrección de ciclo de vida): el
    // llamador necesita saber cuándo una acción TERMINÓ para reiniciar
    // el contador ambient desde ese momento. Observar CurrentStateId()
    // NO alcanza: la mayoría de las acciones son self-loops (click de
    // Bunny/Nidir, howl/tail_greet de Frin) que terminan en el MISMO
    // estado en el que empezaron, así que el id nunca cambia y la
    // duración de la animación se comía parte del intervalo ambient.
    // Esto reporta las DOS clases de terminación (self-loop y
    // cambio-de-estado real) por el mismo camino.
    std::optional<double> ActionCompletedDuringLastAdvance() const { return lastActionCompletedMs_; }

    // Aborta la acción one-shot en curso (si la hay) y vuelve a la pose
    // base del estado ACTUAL — deliberadamente NO al targetStateId
    // pendiente: una transición de postura interrumpida a mitad de
    // camino nunca "completó", así que el pet se queda donde estaba
    // (Frin: sit_to_lie interrumpido -> sigue seated; lie_to_sit
    // interrumpido -> sigue lying). No-op (retorna false) si ya estaba
    // en kBase. Retorna true si el frame mostrado cambió.
    //
    // Genérico, sin ninguna rama por pet: "volver al estado actual" es
    // la respuesta correcta para cualquier grafo de comportamiento.
    bool CancelActionToCurrentState(double nowMs);

    // Ver el comentario de clase para el contrato completo de cada uno.
    bool TriggerClick(double uniformRandom01, double nowMs);
    bool TriggerAmbientAction(double uniformRandom01, double nowMs);
    bool TriggerHoverAction(double uniformRandom01, double nowMs);

    // Cambia la dirección activa. Dirección es metadata genérica, nunca
    // interrumpe un gesto en curso. Si el controller está en kBase, el
    // frame mostrado se actualiza de inmediato (frame 0 de la nueva
    // variante); si no, el cambio queda guardado y se aplica recién en
    // la próxima transición a un estado base. Retorna true si el frame
    // mostrado cambió de verdad.
    bool SetDirection(Direction direction, double nowMs);

    Direction CurrentDirection() const { return direction_; }

    ControllerMode Mode() const { return mode_; }
    const std::string& CurrentStateId() const { return pet_.states[currentStateIndex_].id; }
    const BehaviorState& CurrentState() const { return pet_.states[currentStateIndex_]; }
    const FrameDefinition& CurrentFrame() const;

    // The timestamp at which Advance() next has real work to do —
    // nullopt when the currently playing animation is
    // PlaybackKind::kStatic.
    std::optional<double> NextFrameDeadlineMs() const;

private:
    // Arranca `action` (de cualquiera de las tres listas de triggers)
    // en `mode`, desde frame 0. Guarda action.targetStateId para
    // cuando el one-shot termine.
    void StartAction(const WeightedAction& action, ControllerMode mode, double nowMs);

    // Busca el índice de `stateId` en pet_.states — precondición:
    // stateId existe (impuesto por PetPackLoader al validar
    // targetStateId contra la lista de estados). Lineal: la cantidad
    // de estados de cualquier pet real es pequeñísima (1-3), no
    // justifica un std::unordered_map.
    std::size_t FindStateIndex(const std::string& stateId) const;

    void TransitionToState(std::size_t stateIndex, double nowMs);

    const PetDefinition& pet_;

    // Dirección activa. Default kRight, igual que el runtime entero.
    // Declarado ANTES que currentStateIndex_/currentAnimation_ a
    // propósito -- el orden real de inicialización de miembros sigue
    // el orden de declaración.
    Direction direction_ = Direction::kRight;

    std::size_t currentStateIndex_ = 0;
    ControllerMode mode_ = ControllerMode::kBase;
    const AnimationDefinition* currentAnimation_;
    std::size_t currentFrameIndex_ = 0;
    double currentFrameStartMs_ = 0.0;

    // Estado destino cuando el WeightedAction actualmente en curso
    // termine (solo significativo si mode_ != kBase).
    std::size_t pendingTargetStateIndex_ = 0;

    // Ver ActionCompletedDuringLastAdvance(). Reseteado al principio de
    // cada Advance(), seteado solo si una acción terminó ahí.
    std::optional<double> lastActionCompletedMs_;
};

}  // namespace nimvlets::content
