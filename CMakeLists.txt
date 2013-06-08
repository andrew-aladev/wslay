cmake_minimum_required (VERSION 2.8)
project (wslay)

if (NOT DEFINED WSLAY_STATIC)
    set (WSLAY_STATIC true)
endif ()
if (NOT DEFINED WSLAY_SHARED)
    set (WSLAY_SHARED true)
endif ()

if (NOT DEFINED WSLAY_TARGET)
    set (WSLAY_TARGET ${PROJECT_NAME})
endif ()

set (CMAKE_BUILD_TYPE DEBUG)

enable_testing ()

set (TALLOC_TARGET wslay_talloc2)
add_subdirectory (talloc2)
include_directories ("talloc2/src/")

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Winline -std=gnu99")
set (CMAKE_C_FLAGS_DEBUG "-O0 -g")

add_subdirectory (src)

include_directories ("src/")

add_subdirectory (tests)