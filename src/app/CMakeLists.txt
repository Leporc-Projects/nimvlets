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
# no tiene ese camino transitivo (no depende de nada propio del
# proyecto — ver src/persistence/CMakeLists.txt), por eso se enlaza
# explícitamente aquí.
target_link_libraries(nimvlets_spike PRIVATE
  nimvlets_graphics
  nimvlets_persistence
  nimvlets_warnings
)

if(APPLE)
  target_link_libraries(nimvlets_spike PRIVATE nimvlets_platform_macos)
elseif(WIN32)
  target_link_libraries(nimvlets_spike PRIVATE nimvlets_platform_windows)
endif()
