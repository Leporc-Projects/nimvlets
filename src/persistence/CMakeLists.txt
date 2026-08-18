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

# Standard library only (std::filesystem, std::optional, ...) — no
# dependency on nimvlets_core or SDL, by design: this library knows
# nothing about pets, windows, or rendering (see docs/PERSISTENCE.md).
target_link_libraries(nimvlets_persistence PRIVATE nimvlets_warnings)
