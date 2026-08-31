add_library(nimvlets_core STATIC
  Geometry.h
  DragClassifier.h
  DragClassifier.cpp
  HoverDwellTracker.h
  ClickThroughPolicy.h
  ClickThroughPolicy.cpp
  FrameScheduler.h
  FrameScheduler.cpp
  DisplayControls.h
  DisplayControls.cpp
  Preferences.h
  Preferences.cpp
  Localization.h
  Localization.cpp
  Silhouette.h
  Silhouette.cpp
  AlphaMask.h
  AlphaMask.cpp
)

target_include_directories(nimvlets_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(nimvlets_core PRIVATE nimvlets_warnings)
