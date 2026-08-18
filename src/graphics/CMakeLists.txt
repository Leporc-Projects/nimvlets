add_library(nimvlets_graphics STATIC
  FrameTexture.h
  FrameTexture.cpp
)

target_include_directories(nimvlets_graphics PUBLIC ${CMAKE_SOURCE_DIR}/src)

# PUBLIC nimvlets_content: FrameTexture.h's API takes content::FrameDefinition&
# directly, and nimvlets_content already PUBLIC-links nimvlets_core, so both
# reach anything that links nimvlets_graphics transitively (see
# src/app/CMakeLists.txt's comment on avoiding duplicate-link warnings).
target_link_libraries(nimvlets_graphics
  PUBLIC nimvlets_content SDL3::SDL3
  PRIVATE nimvlets_warnings
)
