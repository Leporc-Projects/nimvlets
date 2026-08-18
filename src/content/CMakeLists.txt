# Pure C++20 content model + animation runtime — no SDL. See
# docs/ANIMATION_RUNTIME.md.

add_library(nimvlets_content STATIC
  AnimationDefinition.h
  AnimationDefinition.cpp
  AnimationController.h
  AnimationController.cpp
  PetPackLoader.h
  PetPackLoader.cpp
)

target_include_directories(nimvlets_content PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(nimvlets_content PUBLIC nimvlets_core PRIVATE nimvlets_warnings)
