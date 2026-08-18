add_library(nimvlets_persistence STATIC
  AppState.h
  AppStateSerializer.h
  AppStateSerializer.cpp
  AppStateStore.h
  AppStateStore.cpp
  PersistenceScheduler.h
  PersistenceScheduler.cpp
)

target_include_directories(nimvlets_persistence PUBLIC ${CMAKE_SOURCE_DIR}/src)

# Solo standard library (std::filesystem, std::optional, ...) — sin
# dependencia de nimvlets_core ni de SDL, a propósito: esta librería no
# sabe nada de pets, ventanas, ni rendering (ver docs/PERSISTENCE.md).
target_link_libraries(nimvlets_persistence PRIVATE nimvlets_warnings)
