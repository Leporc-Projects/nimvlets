#include "content/AnimationController.h"

#include <cassert>

namespace nimvlets::content {

AnimationController::AnimationController(const PetDefinition& pet)
    : pet_(pet),
      currentAnimation_(&ResolveAnimation(
          pet_.states[currentStateIndex_].baseAnimation, pet_.states[currentStateIndex_].baseAnimationDirectionOverrides,
          direction_)) {}

std::size_t AnimationController::FindStateIndex(const std::string& stateId) const {
    for (std::size_t i = 0; i < pet_.states.size(); ++i) {
        if (pet_.states[i].id == stateId) {
            return i;
        }
    }
    // No debería alcanzarse -- PetPackLoader valida que todo
    // targetStateId referencie un estado real antes de aceptar un pack.
    assert(false && "AnimationController: targetStateId not found among pet.states");
    return 0;
}

bool AnimationController::Advance(double nowMs) {
    bool changed = false;
    // Se recalcula en CADA llamada -- nunca queda pegado de una
    // llamada anterior (ver ActionCompletedDuringLastAdvance()).
    lastActionCompletedMs_.reset();

    // Loop rather than step once: si el llamador estuvo dormido lo
    // bastante como para cruzar más de un frame boundary (o terminar un
    // one-shot completo), alcanza a `nowMs` en una sola llamada.
    while (true) {
        if (currentAnimation_->kind == PlaybackKind::kStatic) {
            return changed;
        }

        const double duration = currentAnimation_->FrameDurationMs(currentFrameIndex_);
        if (duration <= 0.0) {
            return changed;  // animación malformada/vacía -- tratar como estática, no spinear
        }
        if (nowMs < currentFrameStartMs_ + duration) {
            return changed;
        }

        currentFrameStartMs_ += duration;
        changed = true;
        ++currentFrameIndex_;

        if (currentFrameIndex_ < currentAnimation_->frames.size()) {
            continue;  // sigue dentro de esta animación
        }

        // Se acabaron los frames de esta animación.
        if (currentAnimation_->kind == PlaybackKind::kLoop) {
            currentFrameIndex_ = 0;
            continue;
        }

        // One-shot terminado.
        if (currentAnimation_->returnsToIdle) {
            // currentFrameStartMs_ es el instante exacto en que el
            // último frame terminó -- más preciso que el `nowMs` del
            // llamador, que puede llegar tarde si el loop estuvo
            // dormido. Se reporta ANTES de transicionar porque
            // TransitionToState() lo pisa.
            lastActionCompletedMs_ = currentFrameStartMs_;
            TransitionToState(pendingTargetStateIndex_, currentFrameStartMs_);
        } else {
            // Se mantiene en el último frame en vez de leer fuera de rango.
            currentFrameIndex_ = currentAnimation_->frames.size() - 1;
            return changed;
        }
    }
}

void AnimationController::StartAction(const WeightedAction& action, ControllerMode mode, double nowMs) {
    mode_ = mode;
    currentAnimation_ = &ResolveAnimation(action.animation, action.directionOverrides, direction_);
    currentFrameIndex_ = 0;
    currentFrameStartMs_ = nowMs;
    pendingTargetStateIndex_ = FindStateIndex(action.targetStateId);
}

bool AnimationController::TriggerClick(double uniformRandom01, double nowMs) {
    if (mode_ == ControllerMode::kClickAction) {
        return false;  // ya reproduciendo -- coalesce, no reiniciar (block brief: click coalesce)
    }
    const BehaviorState& state = pet_.states[currentStateIndex_];
    if (state.clickActions.empty()) {
        return false;  // ningún click definido para este estado
    }
    const std::size_t index = ChooseWeightedActionIndex(state.clickActions, uniformRandom01);
    StartAction(state.clickActions[index], ControllerMode::kClickAction, nowMs);
    return true;
}

bool AnimationController::TriggerAmbientAction(double uniformRandom01, double nowMs) {
    if (mode_ != ControllerMode::kBase) {
        return false;  // menor prioridad que click y que otra acción ya en curso
    }
    const BehaviorState& state = pet_.states[currentStateIndex_];
    if (state.ambientActions.empty()) {
        return false;
    }
    const std::size_t index = ChooseWeightedActionIndex(state.ambientActions, uniformRandom01);
    StartAction(state.ambientActions[index], ControllerMode::kAmbientOrHoverAction, nowMs);
    return true;
}

bool AnimationController::TriggerHoverAction(double uniformRandom01, double nowMs) {
    if (mode_ != ControllerMode::kBase) {
        return false;
    }
    const BehaviorState& state = pet_.states[currentStateIndex_];
    const std::vector<WeightedAction>& pool = EffectiveHoverActions(state);
    if (pool.empty()) {
        return false;  // este estado no define ninguna acción de hover (p. ej. Frin, hoy)
    }
    const std::size_t index = ChooseWeightedActionIndex(pool, uniformRandom01);
    StartAction(pool[index], ControllerMode::kAmbientOrHoverAction, nowMs);
    return true;
}

bool AnimationController::SetDirection(Direction direction, double nowMs) {
    if (direction == direction_) {
        return false;  // ya era la dirección activa -- nada que hacer
    }
    direction_ = direction;

    if (mode_ != ControllerMode::kBase) {
        // Guardado -- se aplica solo cuando TransitionToState() corra
        // la próxima vez. Nunca interrumpe un gesto en curso.
        return false;
    }

    const BehaviorState& state = pet_.states[currentStateIndex_];
    currentAnimation_ = &ResolveAnimation(state.baseAnimation, state.baseAnimationDirectionOverrides, direction_);
    currentFrameIndex_ = 0;
    currentFrameStartMs_ = nowMs;
    return true;
}

bool AnimationController::CancelActionToCurrentState(double nowMs) {
    if (mode_ == ControllerMode::kBase) {
        return false;  // no hay ninguna acción que cancelar
    }
    // Deliberadamente currentStateIndex_, NO pendingTargetStateIndex_:
    // una transición abortada a mitad de camino nunca completó, así
    // que el estado de origen sigue siendo el verdadero.
    TransitionToState(currentStateIndex_, nowMs);
    return true;
}

const FrameDefinition& AnimationController::CurrentFrame() const {
    return currentAnimation_->frames[currentFrameIndex_];
}

std::optional<double> AnimationController::NextFrameDeadlineMs() const {
    if (currentAnimation_->kind == PlaybackKind::kStatic) {
        return std::nullopt;
    }
    const double duration = currentAnimation_->FrameDurationMs(currentFrameIndex_);
    if (duration <= 0.0) {
        return std::nullopt;
    }
    return currentFrameStartMs_ + duration;
}

void AnimationController::TransitionToState(std::size_t stateIndex, double nowMs) {
    currentStateIndex_ = stateIndex;
    mode_ = ControllerMode::kBase;
    const BehaviorState& state = pet_.states[currentStateIndex_];
    currentAnimation_ = &ResolveAnimation(state.baseAnimation, state.baseAnimationDirectionOverrides, direction_);
    currentFrameIndex_ = 0;
    currentFrameStartMs_ = nowMs;
}

}  // namespace nimvlets::content
