#include "ClickThroughPolicyTest.h"

#include "core/ClickThroughPolicy.h"
#include "platform/RendererPolicy.h"

#include <string>

// Tests puros -- sin SDL, sin ventana, sin display -- de la política de
// click-through que Block 05 hizo autoritativa en macOS (ver
// core/ClickThroughPolicy.h y DEC-086). El contrato de aceptación del
// brief se expresa acá como tabla de decisión; la parte que SÍ necesita
// una ventana real y eventos reales de Cocoa (que SDL ya no pueda
// deshacer nuestra decisión) vive en
// src/platform/macos/ClickThroughOwnershipCheck.mm, un ejecutable
// opcional -- ver su docstring.

using nimvlets::core::ClickThroughDecision;
using nimvlets::core::EvaluateClickThrough;

namespace nimvlets::tests {

namespace {

// -- Contrato de aceptación, pixel opaco vs. transparente -------------

bool OpaquePixelIsNeverClickThrough() {
    // "Opaque visible pet pixel: Nimvlets receives interaction."
    NIMVLETS_CHECK(!EvaluateClickThrough(true, true, false).clickThrough);
    return true;
}

bool TransparentPixelInsideTheWindowIsClickThrough() {
    // "Transparent pixel: the window/app underneath receives the click."
    NIMVLETS_CHECK(EvaluateClickThrough(true, false, false).clickThrough);
    return true;
}

bool RepeatedTransparentOpaqueTransitionsStayCorrect() {
    // "Repeated transitions transparent -> opaque -> transparent ->
    // opaque must remain correct." La política es sin estado a
    // propósito: no hay ningún latch que pueda quedar pegado tras N
    // ciclos, y este test lo fija como contrato.
    for (int cycle = 0; cycle < 8; ++cycle) {
        NIMVLETS_CHECK(EvaluateClickThrough(true, false, false).clickThrough);
        NIMVLETS_CHECK(!EvaluateClickThrough(true, true, false).clickThrough);
    }
    return true;
}

// -- Drag -------------------------------------------------------------

bool DragOverTransparentPixelKeepsTheWindowInteractive() {
    // "drag continues correctly once started" -- soltar el botón sobre
    // un pixel transparente a mitad de arrastre no debe tirar el gesto
    // al escritorio.
    NIMVLETS_CHECK(!EvaluateClickThrough(true, false, true).clickThrough);
    NIMVLETS_CHECK(!EvaluateClickThrough(false, false, true).clickThrough);
    return true;
}

bool ReleasingADragRestoresPerPixelBehavior() {
    // "releasing restores normal per-pixel behavior".
    NIMVLETS_CHECK(!EvaluateClickThrough(true, false, true).clickThrough);
    NIMVLETS_CHECK(EvaluateClickThrough(true, false, false).clickThrough);
    NIMVLETS_CHECK(!EvaluateClickThrough(true, true, false).clickThrough);
    return true;
}

// -- Política de muestreo (el presupuesto de CPU en reposo) ------------

bool CursorOutsideTheWindowNeedsNoSampling() {
    // La razón entera por la que el reposo es event-driven: con el
    // cursor fuera del rectángulo, el estado de click-through no puede
    // afectar a nadie, así que no hay nada que muestrear.
    const ClickThroughDecision outside = EvaluateClickThrough(false, false, false);
    NIMVLETS_CHECK(!outside.samplingRequired);
    NIMVLETS_CHECK(!outside.clickThrough);
    return true;
}

bool CursorInsideTheWindowArmsSampling() {
    // Dentro del rectángulo SÍ hace falta muestrear: sobre un pixel
    // transparente la ventana entra en click-through y deja de recibir
    // eventos de mouse, así que ningún evento puede avisar del próximo
    // cambio.
    NIMVLETS_CHECK(EvaluateClickThrough(true, false, false).samplingRequired);
    NIMVLETS_CHECK(EvaluateClickThrough(true, true, false).samplingRequired);
    return true;
}

bool DragNeedsNoSampling() {
    NIMVLETS_CHECK(!EvaluateClickThrough(true, true, true).samplingRequired);
    NIMVLETS_CHECK(!EvaluateClickThrough(true, false, true).samplingRequired);
    return true;
}

bool OpaqueImpliesInsideEvenIfTheCallerDisagrees() {
    // El hit-mask solo existe dentro de la ventana; se normaliza en la
    // política en vez de confiar en que todo call site lo mantenga
    // coherente.
    const ClickThroughDecision d = EvaluateClickThrough(false, true, false);
    NIMVLETS_CHECK(!d.clickThrough);
    NIMVLETS_CHECK(d.samplingRequired);
    return true;
}

// -- Contrato de plataforma -------------------------------------------

bool SoftwareRendererNeverGetsANativeWindowShape() {
    // La regla dura del brief: SDL_SetWindowShape() no debe llamarse
    // sobre la ventana visual renderizada por software (reproduce la
    // corrupción de silueta blanca -- ver
    // platform/TransparentWindowSupport.h para la causa raíz medida).
    // El adaptador de macOS deriva su respuesta EXCLUSIVAMENTE de esta
    // función pura, y src/app deriva de ese adaptador si llama o no a
    // SDL_SetWindowShape() -- así que fijarla acá es lo que impide que
    // la ruta de shape vuelva a activarse bajo el renderer de software.
    NIMVLETS_CHECK(!platform::MacOSNativeShapeIsRenderSafe(true));
    return true;
}

bool AcceleratedRendererKeepsTheNativeShapePathAvailable() {
    // El hallazgo es específico del driver de software, no "las formas
    // nativas están rotas en macOS" -- se fija el contraste para que
    // nadie lo generalice de más más adelante.
    NIMVLETS_CHECK(platform::MacOSNativeShapeIsRenderSafe(false));
    return true;
}

bool MacOSStillDefaultsToTheSoftwareRenderer() {
    // Amarra las dos mitades: mientras macOS use "software" por
    // defecto (DEC-083, el baseline visual que arregló Bunny/Frin), la
    // ruta de shape NO está disponible ahí -- que es exactamente la
    // razón por la que existe el mecanismo propio de click-through.
    NIMVLETS_CHECK(std::string(platform::PreferredRendererDriverName(platform::RendererPlatform::kMacOS, nullptr)) ==
                   "software");
    NIMVLETS_CHECK(!platform::MacOSNativeShapeIsRenderSafe(true));
    return true;
}

}  // namespace

void RegisterClickThroughPolicyTests(testing::TestRunner& runner) {
    runner.Add("ClickThroughPolicy.OpaquePixelIsNeverClickThrough", OpaquePixelIsNeverClickThrough);
    runner.Add("ClickThroughPolicy.TransparentPixelInsideTheWindowIsClickThrough", TransparentPixelInsideTheWindowIsClickThrough);
    runner.Add("ClickThroughPolicy.RepeatedTransparentOpaqueTransitionsStayCorrect", RepeatedTransparentOpaqueTransitionsStayCorrect);
    runner.Add("ClickThroughPolicy.DragOverTransparentPixelKeepsTheWindowInteractive", DragOverTransparentPixelKeepsTheWindowInteractive);
    runner.Add("ClickThroughPolicy.ReleasingADragRestoresPerPixelBehavior", ReleasingADragRestoresPerPixelBehavior);
    runner.Add("ClickThroughPolicy.CursorOutsideTheWindowNeedsNoSampling", CursorOutsideTheWindowNeedsNoSampling);
    runner.Add("ClickThroughPolicy.CursorInsideTheWindowArmsSampling", CursorInsideTheWindowArmsSampling);
    runner.Add("ClickThroughPolicy.DragNeedsNoSampling", DragNeedsNoSampling);
    runner.Add("ClickThroughPolicy.OpaqueImpliesInsideEvenIfTheCallerDisagrees", OpaqueImpliesInsideEvenIfTheCallerDisagrees);
    runner.Add("ClickThroughPolicy.SoftwareRendererNeverGetsANativeWindowShape", SoftwareRendererNeverGetsANativeWindowShape);
    runner.Add("ClickThroughPolicy.AcceleratedRendererKeepsTheNativeShapePathAvailable", AcceleratedRendererKeepsTheNativeShapePathAvailable);
    runner.Add("ClickThroughPolicy.MacOSStillDefaultsToTheSoftwareRenderer", MacOSStillDefaultsToTheSoftwareRenderer);
}

}  // namespace nimvlets::tests
