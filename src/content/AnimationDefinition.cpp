#include "content/AnimationDefinition.h"

#include <cmath>

namespace nimvlets::content {

const char* ToString(Direction direction) {
    switch (direction) {
        case Direction::kRight:
            return "right";
        case Direction::kLeft:
            return "left";
    }
    return "unknown";  // no debería alcanzarse -- Direction es un enum cerrado de 2 valores
}

const AnimationDefinition& ResolveAnimation(
    const AnimationDefinition& canonical, const std::vector<DirectionalAnimationOverride>& overrides, Direction direction) {
    if (direction != Direction::kRight) {
        for (const DirectionalAnimationOverride& override_ : overrides) {
            if (override_.direction == direction) {
                return override_.animation;
            }
        }
    }
    // kRight, o cualquier otra dirección sin variante dedicada: cae a
    // la animación canónica -- ver el comentario de la declaración.
    return canonical;
}

std::size_t ChooseWeightedActionIndex(const std::vector<WeightedAction>& actions, double uniformRandom01) {
    const std::size_t count = actions.size();
    if (count == 0) {
        return 0;  // el llamador garantiza count > 0 antes de disparar cualquier acción
    }
    if (uniformRandom01 < 0.0) {
        uniformRandom01 = 0.0;
    } else if (uniformRandom01 >= 1.0) {
        uniformRandom01 = std::nextafter(1.0, 0.0);
    }

    double totalWeight = 0.0;
    for (const WeightedAction& action : actions) {
        totalWeight += action.weight > 0.0 ? action.weight : 0.0;
    }
    if (totalWeight <= 0.0) {
        // Todos los pesos son 0/negativos -- no hay una distribución
        // válida; caer a la primera entrada en vez de dividir por cero.
        return 0;
    }

    const double target = uniformRandom01 * totalWeight;
    double cumulative = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const double weight = actions[i].weight > 0.0 ? actions[i].weight : 0.0;
        cumulative += weight;
        if (target < cumulative) {
            return i;
        }
    }
    return count - 1;  // margen de redondeo de punto flotante -- última entrada
}

const std::vector<WeightedAction>& EffectiveHoverActions(const BehaviorState& state) {
    return state.hoverUsesAmbientActions ? state.ambientActions : state.hoverActions;
}

double AnimationDefinition::FrameDurationMs(std::size_t frameIndex) const {
    if (frameIndex >= frames.size()) {
        return 0.0;
    }
    if (fps > 0.0) {
        return 1000.0 / fps;
    }
    const double duration = frames[frameIndex].durationMs;
    return duration > 0.0 ? duration : 0.0;
}

}  // namespace nimvlets::content
