# FetchSDL3.cmake
#
# Pulls SDL3 as source via CMake's FetchContent and builds it as part of
# this project's build graph. This is the spike's chosen dependency
# strategy (see docs/DECISION_LOG.md, DEC-004):
#   - reproducible: pinned to an exact release tag, not a branch/latest;
#   - no manual "install SDL3 first" step for the primary dev workflow;
#   - SDL3 source lands under the build directory (CMake FetchContent
#     default: <binaryDir>/_deps), never inside the versioned source tree.
#
# SDL3 is zlib-licensed. See docs/PLATFORM_SPIKE.md for the license note
# required by AGENTS.md's dependency rules.

include(FetchContent)

set(NIMVLETS_SDL3_GIT_TAG "release-3.4.12" CACHE STRING
  "SDL3 git tag pinned for this block. Do not float to a branch or 'latest'.")

# Keep SDL3's own build minimal: we only need the core library for a
# windowed, software/GPU-agnostic 2D spike. Disabling the pieces we don't
# use keeps configure/build time and binary size down and avoids pulling
# in extra system dependencies transitively.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG        ${NIMVLETS_SDL3_GIT_TAG}
  GIT_SHALLOW    TRUE
  GIT_PROGRESS   TRUE
)

FetchContent_MakeAvailable(SDL3)
