# High-value, SDL-free tests only (see AGENTS.md and block brief §10:
# avoid snapshot tests and redundant matrices). These exercise
# nimvlets_core and nimvlets_content in isolation and run identically on
# every platform, including CI runners with no display — see
# docs/PLATFORM_SPIKE.md for the distinction between "tests pass" and
# "GUI verified".

add_executable(nimvlets_tests
  TestRunner.h
  TestPetFixtures.h
  NvPack2TestBuilder.h
  StatefulBehaviorTest.h
  StatefulBehaviorTest.cpp
  DragClassifierTest.h
  DragClassifierTest.cpp
  HoverPassiveGateTest.h
  HoverPassiveGateTest.cpp
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
  AppStateSerializerTest.h
  AppStateSerializerTest.cpp
  AppStateStoreTest.h
  AppStateStoreTest.cpp
  PersistenceSchedulerTest.h
  PersistenceSchedulerTest.cpp
  PersistenceIntegrationTest.h
  PersistenceIntegrationTest.cpp
  PetIdentityTest.h
  PetIdentityTest.cpp
  PetCatalogLoaderTest.h
  PetCatalogLoaderTest.cpp
  ActivePetResolutionTest.h
  ActivePetResolutionTest.cpp
  PetSwitchingTest.h
  PetSwitchingTest.cpp
  LinuxBackendPolicyTest.h
  LinuxBackendPolicyTest.cpp
  DirectionTest.h
  DirectionTest.cpp
  main.cpp
)

target_include_directories(nimvlets_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
# nimvlets_core and nimvlets_content reach this target transitively via
# nimvlets_catalog's PUBLIC link — linking them again here just
# produces a harmless-but-noisy "duplicate library" warning (same
# lesson as Block 01's src/app/CMakeLists.txt). nimvlets_platform_policy
# es pura (sin SDL) y se compila en cualquier OS — ver
# src/platform/CMakeLists.txt y src/platform/LinuxBackendPolicy.h — así
# que LinuxBackendPolicyTest.cpp corre en cualquier host, incluido uno
# sin Linux real.
target_link_libraries(nimvlets_tests PRIVATE nimvlets_persistence nimvlets_catalog nimvlets_platform_policy nimvlets_warnings)

add_test(NAME nimvlets_unit_tests COMMAND nimvlets_tests)
