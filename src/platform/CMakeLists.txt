# Librería pura, siempre compilada en cualquier plataforma (a
# diferencia de src/platform/macos, src/platform/windows y
# src/platform/linux, que el CMakeLists.txt raíz agrega solo en su OS
# correspondiente) -- ver LinuxBackendPolicy.h/RendererPolicy.h para
# por qué esta lógica necesita vivir afuera de un directorio por-OS:
# así tests/ puede ejercitarla en cualquier host, incluido este dev
# machine macOS. RendererPolicy.h además declara CurrentRendererPlatform(),
# implementada UNA vez por cada src/platform/<os>/TransparentWindowSupport.*
# -- esta librería solo provee su declaración/la parte pura, no esos
# tres cuerpos (ver el CMakeLists.txt de cada adapter, que ya linkea
# esta librería para poder definirla).
add_library(nimvlets_platform_policy STATIC
  LinuxBackendPolicy.h
  LinuxBackendPolicy.cpp
  RendererPolicy.h
  RendererPolicy.cpp
  SystemShellTypes.h
  GlobalClickTypes.h
  GlobalClickTypes.cpp
  GlobalClickMonitor.h
  QuickMenuModel.h
  QuickMenuModel.cpp
)

target_include_directories(nimvlets_platform_policy PUBLIC ${CMAKE_SOURCE_DIR}/src)

# nimvlets_core: QuickMenuModel reutiliza core::DisplayControls (política
# pura de tamaño/opacidad) como única fuente de verdad. core no arrastra
# SDL, así que esta librería sigue siendo compilable/testeable en
# cualquier host.
target_link_libraries(nimvlets_platform_policy PUBLIC nimvlets_core PRIVATE nimvlets_warnings)
