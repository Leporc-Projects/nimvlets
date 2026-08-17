# NimvletsWarnings.cmake
#
# Defines an INTERFACE target `nimvlets_warnings` that every first-party
# target links against PRIVATE to get a reasonable, non-exotic warning
# baseline. Third-party code (SDL3) never links against this, so its
# warnings never fail our build.

add_library(nimvlets_warnings INTERFACE)

if(MSVC)
  target_compile_options(nimvlets_warnings INTERFACE /W4 /permissive-)
  if(NIMVLETS_ENABLE_WARNINGS_AS_ERRORS)
    target_compile_options(nimvlets_warnings INTERFACE /WX)
  endif()
else()
  target_compile_options(nimvlets_warnings INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
    -Wsign-conversion
  )
  if(NIMVLETS_ENABLE_WARNINGS_AS_ERRORS)
    target_compile_options(nimvlets_warnings INTERFACE -Werror)
  endif()
endif()
