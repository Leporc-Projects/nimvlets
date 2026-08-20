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

const AnimationDefinition& ResolveIdleAnimation(const PetDefinition& pet, Direction direction) {
    if (direction != Direction::kRight) {
        for (const DirectionalAnimationOverride& override_ : pet.idleDirectionOverrides) {
            if (override_.direction == direction) {
                return override_.animation;
            }
        }
    }
    // kRight, o cualquier otra dirección sin variante dedicada: cae al
    // idle canónico del pet -- ver el comentario de la declaración en
    // AnimationDefinition.h.
    return pet.idle;
}

const AnimationDefinition& ResolveClickReaction(const PetDefinition& pet, Direction direction) {
    if (direction != Direction::kRight) {
        for (const DirectionalAnimationOverride& override_ : pet.clickReactionDirectionOverrides) {
            if (override_.direction == direction) {
                return override_.animation;
            }
        }
    }
    return pet.clickReaction;
}

const AnimationDefinition& ResolvePassiveAction(const PetDefinition& pet, std::size_t passiveActionIndex, Direction direction) {
    if (direction != Direction::kRight) {
        for (const PassiveActionDirectionalOverride& override_ : pet.passiveActionDirectionOverrides) {
            if (override_.passiveActionIndex == passiveActionIndex && override_.direction == direction) {
                return override_.animation;
            }
        }
    }
    return pet.passiveActions[passiveActionIndex];
}

std::size_t ChooseWeightedPassiveActionIndex(const PetDefinition& pet, double uniformRandom01) {
    const std::size_t count = pet.passiveActions.size();
    // El llamador garantiza count > 0 (ver el comentario de la
    // declaración) -- pero clampear el random a [0, count) de forma
    // segura de todos modos si algo llega fuera de rango en vez de
    // asumir ciegamente.
    if (count == 0) {
        return 0;
    }
    if (uniformRandom01 < 0.0) {
        uniformRandom01 = 0.0;
    } else if (uniformRandom01 >= 1.0) {
        uniformRandom01 = std::nextafter(1.0, 0.0);
    }

    if (pet.passiveActionWeights.empty() || pet.passiveActionWeights.size() != count) {
        // Sin pesos explícitos (o un tamaño inconsistente, que
        // PetPackLoader ya debería haber rechazado al cargar -- este
        // fallback es solo defensivo) -- selección uniforme, un
        // muestreo directo del rango de índices.
        const std::size_t index = static_cast<std::size_t>(uniformRandom01 * static_cast<double>(count));
        return index < count ? index : count - 1;
    }

    double totalWeight = 0.0;
    for (const double weight : pet.passiveActionWeights) {
        totalWeight += weight > 0.0 ? weight : 0.0;
    }
    if (totalWeight <= 0.0) {
        // Todos los pesos son 0/negativos -- no hay una distribución
        // válida; caer a la primera entrada en vez de dividir por
        // cero.
        return 0;
    }

    const double target = uniformRandom01 * totalWeight;
    double cumulative = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const double weight = pet.passiveActionWeights[i] > 0.0 ? pet.passiveActionWeights[i] : 0.0;
        cumulative += weight;
        if (target < cumulative) {
            return i;
        }
    }
    return count - 1;  // margen de redondeo de punto flotante -- última entrada
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
