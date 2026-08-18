add_library(nimvlets_platform_linux STATIC
  TransparentWindowSupport.cpp
)

target_include_directories(nimvlets_platform_linux PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(nimvlets_platform_linux
  PUBLIC nimvlets_core SDL3::SDL3
  PRIVATE nimvlets_platform_policy nimvlets_warnings
)
