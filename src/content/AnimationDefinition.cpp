#include "content/AnimationDefinition.h"

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
