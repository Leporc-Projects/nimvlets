# High-value, SDL-free tests only (see AGENTS.md and block brief §10:
# avoid snapshot tests and redundant matrices). These exercise
# nimvlets_core and nimvlets_content in isolation and run identically on
# every platform, including CI runners with no display — see
# docs/PLATFORM_SPIKE.md for the distinction between "tests pass" and
# "GUI verified".

add_executable(nimvlets_tests
  TestRunner.h
  DragClassifierTest.h
  DragClassifierTest.cpp
  FrameSchedulerTest.h
  FrameSchedulerTest.cpp
  SilhouetteTest.h
  SilhouetteTest.cpp
  AlphaMaskTest.h
  AlphaMaskTest.cpp
  AnimationControllerTest.h
  AnimationControllerTest.cpp
  PetPackLoaderTest.h
  PetPackLoaderTest.cpp
  ClickAccountingTest.h
  ClickAccountingTest.cpp
  main.cpp
)

target_include_directories(nimvlets_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
# nimvlets_core reaches this target transitively via nimvlets_content's
# PUBLIC link — linking it again here just produces a harmless-but-noisy
# "duplicate library" warning (same lesson as Block 01's src/app/CMakeLists.txt).
target_link_libraries(nimvlets_tests PRIVATE nimvlets_content nimvlets_warnings)

add_test(NAME nimvlets_unit_tests COMMAND nimvlets_tests)
