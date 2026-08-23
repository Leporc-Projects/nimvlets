#pragma once

// Lógica pura (sin SDL, sin AppKit/Win32/X11) detrás de qué driver de
// SDL_Renderer pedir en cada plataforma -- mismo patrón que
// LinuxBackendPolicy.h: una decisión que necesita ser explícita,
// documentada y testeable en cualquier host, separada del único punto
// real donde se llama a SDL (SpikeApp::Init()).
//
// Por qué existe (Block 05, pasada de resolución de renderer -- ver
// docs/DECISION_LOG.md DEC-083): QA manual real e interactiva del
// owner, en su propia máquina macOS, aisló el bug de larga data de
// "pixeles/partes del cuerpo que desaparecen durante animaciones"
// (Bunny, Frin) a la etapa de PRESENTACIÓN, no al pipeline de
// contenido: el driver de software de SDL renderiza los MISMOS assets
// correctamente; los drivers acelerados (Metal/OpenGL, lo que sea que
// SDL elija por default en macOS) no. Nidir -- el control ya
// establecido -- se ve bien en ambos, así que esto no es "el software
// siempre es más seguro" en general, es una discrepancia real y
// reproducible de un driver acelerado específico contra assets
// específicos en esta máquina. Esta política trata eso como un hecho
// de producto a corregir (default a software en macOS), no como una
// afirmación de que el renderizado acelerado nunca podrá funcionar --
// ver el aviso al final de este archivo.
//
// No se aplica NADA de esto fuera de macOS: Windows/Linux conservan el
// comportamiento histórico (SDL_CreateRenderer(window, nullptr) -- que
// SDL elija su propio default), porque nunca se demostró el mismo bug
// ahí.

namespace nimvlets::platform {

// Qué plataforma compiló este binario -- un hecho de compilación, no
// una decisión. Implementada UNA VEZ en cada
// src/platform/{macos,windows,linux}/TransparentWindowSupport.*, cada
// una devolviendo su propio valor fijo (exactamente el mismo patrón que
// NativeShapeHitTestIsRenderSafe()/ClickThroughPollingIsMeaningful() en
// TransparentWindowSupport.h) -- así SpikeApp nunca necesita su propio
// #ifdef de plataforma (ver AGENTS.md §3, "si te encontrás escribiendo
// #ifdef _WIN32/__APPLE__ dentro de src/app... movelo a src/platform").
enum class RendererPlatform {
    kMacOS,
    kWindows,
    kLinux,
};

RendererPlatform CurrentRendererPlatform();

// Resuelve el nombre de driver a pedirle a SDL_CreateRenderer() (su
// parámetro `name`), o nullptr para pedir el comportamiento histórico
// ("dejar que SDL elija su propio default", sin cambios en absoluto).
//
// `devOverride`: el valor crudo de NIMVLETS_DEV_RENDERER_DRIVER (o
// nullptr/string vacío si la variable no está seteada o está vacía).
// Si es un string no vacío, GANA sobre cualquier default de
// plataforma, en CUALQUIER plataforma -- así el owner puede comparar
// software/metal/opengl/direct3d/lo-que-sea sin recompilar ni tocar
// código, incluso en Windows/Linux donde el default de producto sigue
// siendo "no forzar nada". Esta función no valida el nombre contra la
// lista real de drivers que SDL tiene compilados -- no depende de SDL
// en absoluto, por diseño (ver el docstring del módulo) -- eso lo hace
// SDL_CreateRenderer() en tiempo real, con el fallback documentado que
// SpikeApp::Init() implementa si el nombre pedido no puede crear un
// renderer.
//
// Default por plataforma (sin override):
//   - macOS:  "software" -- ver el docstring del módulo para la
//     evidencia de QA real detrás de esto. `SDL_SOFTWARE_RENDERER`
//     (SDL3/SDL_render.h) es literalmente el string "software" --
//     confirmado contra la fuente pineada
//     (src/render/software/SDL_render_sw.c: `SW_RenderDriver = {
//     SW_CreateRenderer, SDL_SOFTWARE_RENDERER }`).
//   - Windows/Linux: nullptr -- comportamiento histórico sin cambios;
//     nunca se demostró el mismo bug ahí (no hay máquina Windows en
//     este bloque, y Linux nunca reportó el problema).
const char* PreferredRendererDriverName(RendererPlatform platform, const char* devOverride);

}  // namespace nimvlets::platform
