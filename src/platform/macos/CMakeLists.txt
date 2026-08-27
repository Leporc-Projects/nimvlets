add_library(nimvlets_platform_macos STATIC
  TransparentWindowSupport.mm
  TextRasterizer.mm
  QuickMenu.mm
)

target_include_directories(nimvlets_platform_macos PUBLIC ${CMAKE_SOURCE_DIR}/src)

# CoreText/CoreGraphics: rasterizado de texto del sistema para el
# Product UI (Block 06) — ver src/platform/TextRasterizer.h. Son
# frameworks del OS, no dependencias de terceros (AGENTS.md §10), igual
# que AppKit.
target_link_libraries(nimvlets_platform_macos
  PUBLIC nimvlets_core SDL3::SDL3
  PRIVATE nimvlets_platform_policy nimvlets_warnings
          "-framework AppKit" "-framework CoreText" "-framework CoreGraphics"
)

# Verificación GUI nativa, opcional y NO por defecto -- ver el
# docstring de ClickThroughOwnershipCheck.mm. Necesita una ventana real
# y una sesión de WindowServer, así que jamás se agrega a la corrida
# normal de CTest (los jobs de CI de las cuatro plataformas quedan
# exactamente igual). Se enciende a mano en una máquina de escritorio
# real con -DNIMVLETS_ENABLE_GUI_CHECKS=ON.
option(NIMVLETS_ENABLE_GUI_CHECKS "Build (and register) the macOS native click-through GUI check" OFF)

if(NIMVLETS_ENABLE_GUI_CHECKS)
  # nimvlets_core llega transitivamente por nimvlets_platform_macos
  # (PUBLIC) -- listarlo otra vez solo produce el warning "duplicate
  # library".
  add_executable(nimvlets_macos_clickthrough_check ClickThroughOwnershipCheck.mm)
  target_link_libraries(nimvlets_macos_clickthrough_check
    PRIVATE nimvlets_platform_macos nimvlets_warnings "-framework AppKit"
  )
  add_test(NAME nimvlets_macos_clickthrough_check COMMAND nimvlets_macos_clickthrough_check)

  # Check del rasterizador de texto (Block 06). Core Text corre
  # headless, así que este NO necesita WindowServer -- pero sigue
  # requiriendo AppKit, por eso vive acá y no en tests/.
  add_executable(nimvlets_macos_text_check TextRasterizerCheck.mm)
  target_link_libraries(nimvlets_macos_text_check
    PRIVATE nimvlets_platform_macos nimvlets_warnings
            "-framework AppKit" "-framework CoreText" "-framework CoreGraphics"
  )
  add_test(NAME nimvlets_macos_text_check COMMAND nimvlets_macos_text_check)
endif()
