#include "RendererPolicyTest.h"

#include "platform/RendererPolicy.h"

#include <string>

// Tests puros -- sin SDL -- de platform::PreferredRendererDriverName()
// (Block 05, pasada de resolución de renderer: ver docs/DECISION_LOG.md
// DEC-083). Mismo patrón que tests/LinuxBackendPolicyTest.cpp: corre en
// cualquier host, incluido este dev machine macOS, sin necesitar una
// ventana ni un renderer real.

using nimvlets::platform::PreferredRendererDriverName;
using nimvlets::platform::RendererPlatform;

namespace nimvlets::tests {

namespace {

bool MacOSDefaultsToSoftware() {
    NIMVLETS_CHECK(std::string(PreferredRendererDriverName(RendererPlatform::kMacOS, nullptr)) == "software");
    return true;
}

bool MacOSDefaultsToSoftwareWithEmptyOverride() {
    // Una variable de entorno SETEADA PERO VACÍA se trata igual que
    // ausente -- nunca "un string vacío es el driver pedido".
    NIMVLETS_CHECK(std::string(PreferredRendererDriverName(RendererPlatform::kMacOS, "")) == "software");
    return true;
}

bool WindowsPolicyIsUnchangedByDefault() {
    // nullptr == "dejar que SDL elija su propio default" -- el
    // comportamiento histórico, sin cambios.
    NIMVLETS_CHECK(PreferredRendererDriverName(RendererPlatform::kWindows, nullptr) == nullptr);
    return true;
}

bool LinuxPolicyIsUnchangedByDefault() {
    NIMVLETS_CHECK(PreferredRendererDriverName(RendererPlatform::kLinux, nullptr) == nullptr);
    return true;
}

bool DevOverrideWinsOnMacOS() {
    NIMVLETS_CHECK(std::string(PreferredRendererDriverName(RendererPlatform::kMacOS, "metal")) == "metal");
    NIMVLETS_CHECK(std::string(PreferredRendererDriverName(RendererPlatform::kMacOS, "opengl")) == "opengl");
    return true;
}

bool DevOverrideAlsoWinsOnWindowsAndLinux() {
    // El override no es "solo para macOS" -- el owner puede comparar
    // drivers en CUALQUIER plataforma sin recompilar (block brief §2:
    // "owner should still be able to compare software/metal/opengl
    // without source edits").
    NIMVLETS_CHECK(std::string(PreferredRendererDriverName(RendererPlatform::kWindows, "direct3d11")) == "direct3d11");
    NIMVLETS_CHECK(std::string(PreferredRendererDriverName(RendererPlatform::kLinux, "opengl")) == "opengl");
    return true;
}

bool DevOverrideIsNotValidatedAgainstAnyDriverList() {
    // Esta función es pura -- no depende de SDL, así que no puede (ni
    // debe) validar contra la lista real de drivers compilados. Un
    // nombre inválido se retorna igual; SpikeApp::Init() es quien
    // maneja el fallo real de SDL_CreateRenderer() con un fallback
    // documentado.
    NIMVLETS_CHECK(std::string(PreferredRendererDriverName(RendererPlatform::kMacOS, "not_a_real_driver")) == "not_a_real_driver");
    return true;
}

}  // namespace

void RegisterRendererPolicyTests(testing::TestRunner& runner) {
    runner.Add("RendererPolicy.MacOSDefaultsToSoftware", MacOSDefaultsToSoftware);
    runner.Add("RendererPolicy.MacOSDefaultsToSoftwareWithEmptyOverride", MacOSDefaultsToSoftwareWithEmptyOverride);
    runner.Add("RendererPolicy.WindowsPolicyIsUnchangedByDefault", WindowsPolicyIsUnchangedByDefault);
    runner.Add("RendererPolicy.LinuxPolicyIsUnchangedByDefault", LinuxPolicyIsUnchangedByDefault);
    runner.Add("RendererPolicy.DevOverrideWinsOnMacOS", DevOverrideWinsOnMacOS);
    runner.Add("RendererPolicy.DevOverrideAlsoWinsOnWindowsAndLinux", DevOverrideAlsoWinsOnWindowsAndLinux);
    runner.Add("RendererPolicy.DevOverrideIsNotValidatedAgainstAnyDriverList", DevOverrideIsNotValidatedAgainstAnyDriverList);
}

}  // namespace nimvlets::tests
