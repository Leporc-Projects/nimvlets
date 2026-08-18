# Librería pura, siempre compilada en cualquier plataforma (a
# diferencia de src/platform/macos, src/platform/windows y
# src/platform/linux, que el CMakeLists.txt raíz agrega solo en su OS
# correspondiente) -- ver LinuxBackendPolicy.h para por qué esta lógica
# necesita vivir afuera de src/platform/linux/: así tests/ puede
# ejercitarla en cualquier host, incluido este dev machine macOS.
add_library(nimvlets_platform_policy STATIC
  LinuxBackendPolicy.h
  LinuxBackendPolicy.cpp
)

target_include_directories(nimvlets_platform_policy PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(nimvlets_platform_policy PRIVATE nimvlets_warnings)
