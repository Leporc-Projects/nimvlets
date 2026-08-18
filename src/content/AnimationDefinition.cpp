#include "content/AnimationDefinition.h"

namespace nimvlets::content {

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
