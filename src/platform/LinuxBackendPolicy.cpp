#include "platform/LinuxBackendPolicy.h"

#include <cstring>

namespace nimvlets::platform {

LinuxVideoBackend ParseLinuxVideoBackend(const char* sdlVideoDriverName) {
    if (sdlVideoDriverName == nullptr) {
        return LinuxVideoBackend::kOther;
    }
    // Comparación exacta contra los nombres literales que
    // SDL_GetCurrentVideoDriver() retorna para estos dos drivers en la
    // fuente pineada (X11_bootstrap.name / Wayland_bootstrap.name) --
    // nunca una comparación parcial/case-insensitive, para no
    // adivinar sobre un backend que este bloque no soporta.
    if (std::strcmp(sdlVideoDriverName, "x11") == 0) {
        return LinuxVideoBackend::kX11;
    }
    if (std::strcmp(sdlVideoDriverName, "wayland") == 0) {
        return LinuxVideoBackend::kWayland;
    }
    return LinuxVideoBackend::kOther;
}

bool LinuxBackendSupportsNativeShapeHitTest(LinuxVideoBackend backend) {
    return backend == LinuxVideoBackend::kX11;
}

bool LinuxBackendClickThroughPollingIsMeaningful(LinuxVideoBackend backend) {
    return backend == LinuxVideoBackend::kX11;
}

bool LinuxBackendSupportsPositionRestore(LinuxVideoBackend backend) {
    return backend == LinuxVideoBackend::kX11;
}

}  // namespace nimvlets::platform
