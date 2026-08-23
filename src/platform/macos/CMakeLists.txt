add_library(nimvlets_platform_macos STATIC
  TransparentWindowSupport.mm
)

target_include_directories(nimvlets_platform_macos PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(nimvlets_platform_macos
  PUBLIC nimvlets_core SDL3::SDL3
  PRIVATE nimvlets_platform_policy nimvlets_warnings "-framework AppKit"
)

# Verificación GUI nativa, opcional y NO por defecto -- ver el
# docstring de ClickThroughOwnershipCheck.mm. Necesita una ventana real
# y una sesión de WindowServer, así que jamás se agrega a la corrida
# normal de CTest (los jobs de CI de las cuatro plataformas quedan
# exactamente igual). Se enciende a mano en una máquina de escritorio
# real con -DNIMVLETS_ENABLE_GUI_CHECKS=ON.
option(NIMVLETS_ENABLE_GUI_CHECKS "Build (and register) the macOS native click-through GUI check" OFF)

if(NIMVLETS_ENABLE_GUI_CHECKS)
  add_executable(nimvlets_macos_clickthrough_check ClickThroughOwnershipCheck.mm)
  target_link_libraries(nimvlets_macos_clickthrough_check
    PRIVATE nimvlets_platform_macos nimvlets_core nimvlets_warnings "-framework AppKit"
  )
  add_test(NAME nimvlets_macos_clickthrough_check COMMAND nimvlets_macos_clickthrough_check)
endif()
