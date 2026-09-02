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
  HoverDwellTrackerTest.h
  HoverDwellTrackerTest.cpp
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
  ClickCountingPolicyTest.h
  ClickCountingPolicyTest.cpp
  FakeGlobalClickMonitor.h
  GlobalClickMonitorTest.h
  GlobalClickMonitorTest.cpp
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
  RendererPolicyTest.h
  RendererPolicyTest.cpp
  ClickThroughPolicyTest.h
  ClickThroughPolicyTest.cpp
  PetEntitlementTest.h
  PetEntitlementTest.cpp
  EntitlementMigrationTest.h
  EntitlementMigrationTest.cpp
  CollectionModelTest.h
  CollectionModelTest.cpp
  CollectionLayoutTest.h
  CollectionLayoutTest.cpp
  ShopModelTest.h
  ShopModelTest.cpp
  PurchasePolicyTest.h
  PurchasePolicyTest.cpp
  StarterShopModelTest.h
  StarterShopModelTest.cpp
  StarterPurchasePolicyTest.h
  StarterPurchasePolicyTest.cpp
  ShopLayoutTest.h
  ShopLayoutTest.cpp
  StarterShopLayoutTest.h
  StarterShopLayoutTest.cpp
  SectionNavTest.h
  SectionNavTest.cpp
  ProductWindowStateTest.h
  ProductWindowStateTest.cpp
  SettingsLayoutTest.h
  SettingsLayoutTest.cpp
  SettingsCompanionTest.h
  SettingsCompanionTest.cpp
  OnboardingPolicyTest.h
  OnboardingPolicyTest.cpp
  OnboardingLayoutTest.h
  OnboardingLayoutTest.cpp
  FocusListTest.h
  FocusListTest.cpp
  TextLayoutTest.h
  TextLayoutTest.cpp
  FormatTest.h
  FormatTest.cpp
  DisplayControlsTest.h
  DisplayControlsTest.cpp
  PreferencesTest.h
  PreferencesTest.cpp
  LocalizationTest.h
  LocalizationTest.cpp
  PetAccentTest.h
  PetAccentTest.cpp
  PetEditorialTest.h
  PetEditorialTest.cpp
  PreviewArtifactTest.h
  PreviewArtifactTest.cpp
  QuickMenuModelTest.h
  QuickMenuModelTest.cpp
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
# nimvlets_catalog llega transitivamente por nimvlets_productui_core
# (PUBLIC), y nimvlets_content/core por debajo de ese — enlistarlos otra
# vez solo produce el warning "duplicate library" (misma lección que
# src/app/CMakeLists.txt). nimvlets_persistence y nimvlets_platform_policy
# NO tienen ese camino transitivo, así que se enlazan explícitamente.
target_link_libraries(nimvlets_tests PRIVATE nimvlets_persistence nimvlets_productui_core nimvlets_platform_policy nimvlets_warnings)

add_test(NAME nimvlets_unit_tests COMMAND nimvlets_tests)
