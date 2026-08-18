add_executable(nimvlets_spike
  main.cpp
  SpikeApp.h
  SpikeApp.cpp
)

target_include_directories(nimvlets_spike PRIVATE ${CMAKE_SOURCE_DIR}/src)

# nimvlets_core and SDL3::SDL3 are already PUBLIC dependencies of
# nimvlets_graphics (and of the platform adapter below), so they reach
# this target transitively — linking them again here would just produce
# harmless-but-noisy "duplicate library" warnings. nimvlets_persistence
# y nimvlets_catalog no tienen ese camino transitivo (ninguna depende
# de nimvlets_graphics — ver sus propios CMakeLists.txt), por eso se
# enlazan explícitamente aquí.
target_link_libraries(nimvlets_spike PRIVATE
  nimvlets_graphics
  nimvlets_persistence
  nimvlets_catalog
  nimvlets_warnings
)

if(APPLE)
  target_link_libraries(nimvlets_spike PRIVATE nimvlets_platform_macos)
elseif(WIN32)
  target_link_libraries(nimvlets_spike PRIVATE nimvlets_platform_windows)
elseif(UNIX)
  target_link_libraries(nimvlets_spike PRIVATE nimvlets_platform_linux)
endif()
