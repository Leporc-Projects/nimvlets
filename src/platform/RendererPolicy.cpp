#include "platform/RendererPolicy.h"

namespace nimvlets::platform {

const char* PreferredRendererDriverName(RendererPlatform platform, const char* devOverride) {
    if (devOverride != nullptr && devOverride[0] != '\0') {
        return devOverride;
    }
    if (platform == RendererPlatform::kMacOS) {
        return "software";
    }
    return nullptr;
}

bool MacOSNativeShapeIsRenderSafe(bool usingSoftwareRenderer) {
    return !usingSoftwareRenderer;
}

}  // namespace nimvlets::platform
