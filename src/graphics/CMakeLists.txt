add_library(nimvlets_graphics STATIC
  BlobRenderer.h
  BlobRenderer.cpp
)

target_include_directories(nimvlets_graphics PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(nimvlets_graphics
  PUBLIC nimvlets_core SDL3::SDL3
  PRIVATE nimvlets_warnings
)
