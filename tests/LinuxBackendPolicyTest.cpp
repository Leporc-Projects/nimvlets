#include "LinuxBackendPolicyTest.h"

#include "platform/LinuxBackendPolicy.h"

// Tests puros -- sin SDL, sin X11, sin Wayland, sin display -- de la
// tabla de decisiones de capacidad por backend Linux (block brief
// §7: "backend capability decisions", "Wayland position-restore
// policy", "X11 position-restore policy", "Linux shape/hit-test
// selection logic"). Deliberadamente NO incluyen ninguna aserción
// dependiente de un compositor real (eso vive en el smoke de CI bajo
// Xvfb/headless Wayland -- ver .github/workflows/ci.yml) -- ver
// docs/LINUX_PLATFORM.md para la evidencia de código fuente de SDL
// 3.4.12 detrás de cada valor esperado acá. Corre en cualquier host,
// incluido este dev machine macOS, exactamente como
// tests/PetIdentityTest.cpp corre sin necesitar SDL.

using nimvlets::platform::LinuxBackendClickThroughPollingIsMeaningful;
using nimvlets::platform::LinuxBackendSupportsNativeShapeHitTest;
using nimvlets::platform::LinuxBackendSupportsPositionRestore;
using nimvlets::platform::LinuxVideoBackend;
using nimvlets::platform::ParseLinuxVideoBackend;

namespace nimvlets::tests {

namespace {

bool TestParseLinuxVideoBackendRecognizesX11() {
    NIMVLETS_CHECK(ParseLinuxVideoBackend("x11") == LinuxVideoBackend::kX11);
    return true;
}

bool TestParseLinuxVideoBackendRecognizesWayland() {
    NIMVLETS_CHECK(ParseLinuxVideoBackend("wayland") == LinuxVideoBackend::kWayland);
    return true;
}

// Comparación exacta, no case-insensitive ni parcial -- "X11"/"Wayland"
// (con mayúscula) o cualquier substring no son lo que
// SDL_GetCurrentVideoDriver() realmente retorna, así que deben caer en
// kOther en vez de adivinar.
bool TestParseLinuxVideoBackendIsCaseSensitiveAndExact() {
    NIMVLETS_CHECK(ParseLinuxVideoBackend("X11") == LinuxVideoBackend::kOther);
    NIMVLETS_CHECK(ParseLinuxVideoBackend("Wayland") == LinuxVideoBackend::kOther);
    NIMVLETS_CHECK(ParseLinuxVideoBackend("x11-extra") == LinuxVideoBackend::kOther);
    return true;
}

bool TestParseLinuxVideoBackendHandlesUnknownAndNull() {
    NIMVLETS_CHECK(ParseLinuxVideoBackend("dummy") == LinuxVideoBackend::kOther);
    NIMVLETS_CHECK(ParseLinuxVideoBackend(nullptr) == LinuxVideoBackend::kOther);
    return true;
}

// El path central de este bloque: X11 sí puede usar el mismo mecanismo
// de shape render-safe que macOS (XShapeCombineMask solo toca
// ShapeInput -- ver LinuxBackendPolicy.h); Wayland, con la SDL pineada
// de este bloque, no tiene ningún UpdateWindowShape wireado en
// absoluto.
bool TestNativeShapeHitTestOnlySupportedOnX11() {
    NIMVLETS_CHECK(LinuxBackendSupportsNativeShapeHitTest(LinuxVideoBackend::kX11) == true);
    NIMVLETS_CHECK(LinuxBackendSupportsNativeShapeHitTest(LinuxVideoBackend::kWayland) == false);
    NIMVLETS_CHECK(LinuxBackendSupportsNativeShapeHitTest(LinuxVideoBackend::kOther) == false);
    return true;
}

// Wayland: SDL_GetGlobalMouseState solo es útil con foco propio y no
// hay forma pública de tocar la input region -- pollear no cambiaría
// nada, así que SpikeApp no debe ni arrancar ese loop (ver
// usingPollDrivenClickThrough_ en src/app/SpikeApp.h).
bool TestClickThroughPollingOnlyMeaningfulOnX11() {
    NIMVLETS_CHECK(LinuxBackendClickThroughPollingIsMeaningful(LinuxVideoBackend::kX11) == true);
    NIMVLETS_CHECK(LinuxBackendClickThroughPollingIsMeaningful(LinuxVideoBackend::kWayland) == false);
    NIMVLETS_CHECK(LinuxBackendClickThroughPollingIsMeaningful(LinuxVideoBackend::kOther) == false);
    return true;
}

// La política de restauración de posición del block brief §3: X11
// puede reposicionar una toplevel (XMoveWindow); Wayland no puede
// (Wayland_SetWindowPosition retorna SDL_SetError para toda toplevel
// no-popup) -- una limitación real del protocolo xdg-shell, no un bug.
bool TestPositionRestoreOnlySupportedOnX11() {
    NIMVLETS_CHECK(LinuxBackendSupportsPositionRestore(LinuxVideoBackend::kX11) == true);
    NIMVLETS_CHECK(LinuxBackendSupportsPositionRestore(LinuxVideoBackend::kWayland) == false);
    NIMVLETS_CHECK(LinuxBackendSupportsPositionRestore(LinuxVideoBackend::kOther) == false);
    return true;
}

}  // namespace

void RegisterLinuxBackendPolicyTests(testing::TestRunner& runner) {
    runner.Add("LinuxBackendPolicy/ParseRecognizesX11", TestParseLinuxVideoBackendRecognizesX11);
    runner.Add("LinuxBackendPolicy/ParseRecognizesWayland", TestParseLinuxVideoBackendRecognizesWayland);
    runner.Add("LinuxBackendPolicy/ParseIsCaseSensitiveAndExact", TestParseLinuxVideoBackendIsCaseSensitiveAndExact);
    runner.Add("LinuxBackendPolicy/ParseHandlesUnknownAndNull", TestParseLinuxVideoBackendHandlesUnknownAndNull);
    runner.Add("LinuxBackendPolicy/NativeShapeHitTestOnlySupportedOnX11", TestNativeShapeHitTestOnlySupportedOnX11);
    runner.Add("LinuxBackendPolicy/ClickThroughPollingOnlyMeaningfulOnX11", TestClickThroughPollingOnlyMeaningfulOnX11);
    runner.Add("LinuxBackendPolicy/PositionRestoreOnlySupportedOnX11", TestPositionRestoreOnlySupportedOnX11);
}

}  // namespace nimvlets::tests
