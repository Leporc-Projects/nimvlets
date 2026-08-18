# FetchSDL3.cmake
#
# Pulls SDL3 as source via CMake's FetchContent and builds it as part of
# this project's build graph. This is the spike's chosen dependency
# strategy (see docs/DECISION_LOG.md, DEC-004):
#   - reproducible: pinned to an exact release tag, not a branch/latest;
#   - no manual "install SDL3 first" step for the primary dev workflow;
#   - SDL3 source lands under the build directory (CMake FetchContent
#     default: <binaryDir>/_deps), never inside the versioned source tree.
#
# SDL3 is zlib-licensed. See docs/PLATFORM_SPIKE.md for the license note
# required by AGENTS.md's dependency rules.

include(FetchContent)

set(NIMVLETS_SDL3_GIT_TAG "release-3.4.12" CACHE STRING
  "SDL3 git tag pinned for this block. Do not float to a branch or 'latest'.")

# Keep SDL3's own build minimal: we only need the core library for a
# windowed, software/GPU-agnostic 2D spike. Disabling the pieces we don't
# use keeps configure/build time and binary size down and avoids pulling
# in extra system dependencies transitively.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)

# Linux (Block 04.1): mantener X11 y Wayland activos -- ambos son
# ${UNIX_SYS}-default-ON en el propio CMakeLists.txt de SDL, así que no
# hace falta tocarlos -- pero cada sub-feature X11 que SDL deja en ON
# por defecto hace FATAL_ERROR el configure de SDL si falta el paquete
# de desarrollo correspondiente (ver cmake/sdlchecks.cmake,
# SDL_missing_dependency() en la fuente pineada de SDL) en vez de
# saltearla en silencio -- así que en vez de instalar en CI el listado
# "todas las features" de docs/README-linux.md de SDL, se apaga acá
# explícitamente lo que este proyecto no usa (sin audio/joystick/
# haptics: SpikeApp solo llama SDL_Init(SDL_INIT_VIDEO)), y solo se
# deja lo que docs/LINUX_PLATFORM.md documenta como necesario:
#   - SDL_X11_XSHAPE: mecanismo real de click-through (ver
#     src/platform/linux/TransparentWindowSupport.cpp) -- confirmado
#     por lectura directa de la fuente pineada 3.4.12
#     (src/video/x11/SDL_x11shape.c): XShapeCombineMask/Region solo
#     toca la región de hit-test (ShapeInput), nunca compone ni
#     reemplaza pixeles renderizados -- misma familia que
#     Cocoa_UpdateWindowShape en macOS.
#   - SDL_X11_XINPUT (XInput2): ruta de entrada de mouse/teclado
#     moderna que SDL documenta como la esperada en un desktop Linux
#     típico -- se deja ON en vez de apostar a que el protocolo core
#     X11 alcanza, ya que no hay forma de verificar eso en este host
#     macOS (sin hardware Linux real -- ver §11 del brief de este
#     bloque).
#   - SDL_X11_XRANDR: necesario para que SDL consulte escala/DPI por
#     monitor con precisión (requisito "high-DPI correctness" del
#     brief), no solo el tamaño de pantalla core de Xlib.
# El resto de sub-features X11 (Xcursor, Xdbe, Xfixes, Xscrnsaver,
# Xsync, Xtest) no las usa nada de src/ -- se apagan para reducir tanto
# los paquetes apt de CI como el riesgo de un configure fallido por un
# header ausente.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(SDL_X11_XSHAPE ON CACHE BOOL "" FORCE)
  set(SDL_X11_XINPUT ON CACHE BOOL "" FORCE)
  set(SDL_X11_XRANDR ON CACHE BOOL "" FORCE)
  set(SDL_X11_XCURSOR OFF CACHE BOOL "" FORCE)
  set(SDL_X11_XDBE OFF CACHE BOOL "" FORCE)
  set(SDL_X11_XFIXES OFF CACHE BOOL "" FORCE)
  set(SDL_X11_XSCRNSAVER OFF CACHE BOOL "" FORCE)
  set(SDL_X11_XSYNC OFF CACHE BOOL "" FORCE)
  set(SDL_X11_XTEST OFF CACHE BOOL "" FORCE)

  # libdecor dibuja la decoración cliente-side de una toplevel Wayland
  # quien la pida -- nuestra ventana siempre se crea con
  # SDL_WINDOW_BORDERLESS (ver src/app/SpikeApp.cpp), así que esa ruta
  # (WAYLAND_SHELL_SURFACE_TYPE_LIBDECOR en la fuente pineada de SDL)
  # nunca se selecciona en tiempo de ejecución -- apagarla evita
  # depender de libdecor-0-dev sin cambiar ningún comportamiento real.
  set(SDL_WAYLAND_LIBDECOR OFF CACHE BOOL "" FORCE)
endif()

FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG        ${NIMVLETS_SDL3_GIT_TAG}
  GIT_SHALLOW    TRUE
  GIT_PROGRESS   TRUE
)

FetchContent_MakeAvailable(SDL3)
