cmake_minimum_required(VERSION 3.25)

# Objective-C++ is only meaningful/available on Apple platforms; enabling it
# unconditionally would break configuration on Windows/Linux hosts.
if(APPLE)
  project(Nimvlets LANGUAGES CXX OBJCXX)
else()
  project(Nimvlets LANGUAGES CXX)
endif()

# ---------------------------------------------------------------------------
# Global project settings
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE)
  set(CMAKE_OBJCXX_STANDARD 20)
  set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
endif()

# Keep build output out of the source tree even if someone configures
# without a preset.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR
    "In-source builds are not allowed. Use a preset (see CMakePresets.json) "
    "or configure with -B build.")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

option(NIMVLETS_BUILD_TESTS "Build the Nimvlets test suite" ON)
option(NIMVLETS_ENABLE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(NimvletsWarnings)
include(FetchSDL3)

enable_testing()

add_subdirectory(src/core)
add_subdirectory(src/content)
add_subdirectory(src/persistence)
add_subdirectory(src/graphics)

if(APPLE)
  add_subdirectory(src/platform/macos)
elseif(WIN32)
  add_subdirectory(src/platform/windows)
endif()

add_subdirectory(src/app)

if(NIMVLETS_BUILD_TESTS)
  add_subdirectory(tests)
endif()

message(STATUS "Nimvlets: CMAKE_SYSTEM_NAME       = ${CMAKE_SYSTEM_NAME}")
message(STATUS "Nimvlets: CMAKE_OSX_ARCHITECTURES = ${CMAKE_OSX_ARCHITECTURES}")
message(STATUS "Nimvlets: CMAKE_BUILD_TYPE        = ${CMAKE_BUILD_TYPE}")
message(STATUS "Nimvlets: NIMVLETS_BUILD_TESTS    = ${NIMVLETS_BUILD_TESTS}")
