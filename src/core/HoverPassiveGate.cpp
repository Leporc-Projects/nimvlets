#include "core/HoverPassiveGate.h"

namespace nimvlets::core {

bool HoverPassiveGate::Update(bool isHoveringNow) {
    const bool risingEdge = isHoveringNow && !isHovering_;
    isHovering_ = isHoveringNow;
    return risingEdge;
}

}  // namespace nimvlets::core
