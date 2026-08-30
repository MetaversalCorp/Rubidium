# Copyright 2026 Metaversal Corporation. All rights reserved.

cmake_minimum_required (VERSION 3.20)
project (RubidiumDeps LANGUAGES C CXX)

include (ExternalProject)

# ---------------------------------------------------------------------------
# This CMakeLists.txt knows only about files under deps/. It never looks
# into src/ and it never touches the Rubidium build tree. The scripts in
# scripts/ are the only glue between the deps branch and the Rubidium branch.
#
# Invocation modes:
#
#   cmake -S deps -B <build>                      Build all dependencies
#   cmake -S deps -B <build> -DDEP=<name>         Build a single dependency (CI)
#
# Optional in both modes:
#   -DRUBIDIUM_CONFIG=Debug|Release                (default Release)
#   -DRUBIDIUM_PLATFORM=<slug>                     (auto-detect if unset)
#   -DRUBIDIUM_DEP_REPO=<path>                     (default: deps/repos)
#   -DLIBS_DIR=<path>                             (default: deps/builds/<plat>/<cfg>/libs)
#   -DCMAKE_TOOLCHAIN_FILE=<path>                 (forwarded to each dep)
# ---------------------------------------------------------------------------

# RUBIDIUM_CONFIG ----------------------------------------------------------

if (NOT DEFINED RUBIDIUM_CONFIG OR RUBIDIUM_CONFIG STREQUAL "")
   set (RUBIDIUM_CONFIG "Release")
endif ()
if (NOT RUBIDIUM_CONFIG MATCHES "^(Debug|Release)$")
   message (FATAL_ERROR "RUBIDIUM_CONFIG must be Debug or Release, got '${RUBIDIUM_CONFIG}'")
endif ()

# RUBIDIUM_PLATFORM (auto-detect) -----------------------------------------

if (NOT DEFINED RUBIDIUM_PLATFORM OR RUBIDIUM_PLATFORM STREQUAL "")
   if (ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Android")
      set (_plat "android-arm64")
   elseif (CMAKE_SYSTEM_NAME STREQUAL "iOS")
      set (_plat "ios-arm64")
   elseif (WIN32)
      set (_plat "windows-x64")
   elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
      if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
         set (_plat "linux-arm64")
      else ()
         set (_plat "linux-x64")
      endif ()
   elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
      if (CMAKE_OSX_ARCHITECTURES MATCHES "arm64"
            AND CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
         set (_plat "macos-universal")
      elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64"
            OR CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64")
         set (_plat "macos-arm64")
      else ()
         set (_plat "macos-x64")
      endif ()
   else ()
      message (FATAL_ERROR "Unknown platform; set -DRUBIDIUM_PLATFORM=<slug> explicitly.")
   endif ()
   set (RUBIDIUM_PLATFORM "${_plat}" CACHE STRING
      "Platform slug (windows-x64, linux-x64, macos-arm64, ...)")
endif ()

string (TOLOWER "${RUBIDIUM_CONFIG}" _cfg_lower)

# Path defaults -----------------------------------------------------------

if (NOT DEFINED RUBIDIUM_DEP_REPO OR RUBIDIUM_DEP_REPO STREQUAL "")
   set (RUBIDIUM_DEP_REPO "${CMAKE_CURRENT_SOURCE_DIR}/repos" CACHE PATH
      "Shared dependency source clones (reused across platform+config).")
endif ()
if (NOT DEFINED LIBS_DIR OR LIBS_DIR STREQUAL "")
   set (LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/builds/${RUBIDIUM_PLATFORM}/${_cfg_lower}/libs"
      CACHE PATH "Per-dep build + install trees for this platform+config.")
endif ()

message (STATUS "Rubidium deps: platform = ${RUBIDIUM_PLATFORM}")
message (STATUS "Rubidium deps: config   = ${RUBIDIUM_CONFIG}")
message (STATUS "Rubidium deps: dep repo = ${RUBIDIUM_DEP_REPO}")
message (STATUS "Rubidium deps: libs dir = ${LIBS_DIR}")

# Cross-compilation variable forwarding -----------------------------------

set (CROSS_COMPILE_ARGS)

if (CMAKE_GENERATOR)
   list (APPEND CROSS_COMPILE_ARGS -G ${CMAKE_GENERATOR})
endif ()

if (CMAKE_TOOLCHAIN_FILE)
   list (APPEND CROSS_COMPILE_ARGS -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
endif ()

if (CMAKE_CROSSCOMPILING)
   if (CMAKE_SYSTEM_NAME)
      list (APPEND CROSS_COMPILE_ARGS -DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME})
   endif ()
   if (CMAKE_SYSTEM_PROCESSOR)
      list (APPEND CROSS_COMPILE_ARGS -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR})
   endif ()
   if (CMAKE_OSX_SYSROOT)
      list (APPEND CROSS_COMPILE_ARGS -DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT})
   endif ()
   if (CMAKE_ANDROID_NDK)
      list (APPEND CROSS_COMPILE_ARGS -DCMAKE_ANDROID_NDK=${CMAKE_ANDROID_NDK})
   endif ()
   if (ANDROID_ABI)
      list (APPEND CROSS_COMPILE_ARGS -DANDROID_ABI=${ANDROID_ABI})
   endif ()
   if (ANDROID_PLATFORM)
      list (APPEND CROSS_COMPILE_ARGS -DANDROID_PLATFORM=${ANDROID_PLATFORM})
   endif ()
   if (ANDROID_STL)
      list (APPEND CROSS_COMPILE_ARGS -DANDROID_STL=${ANDROID_STL})
   endif ()
   if (CMAKE_MAKE_PROGRAM)
      list (APPEND CROSS_COMPILE_ARGS -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM})
   endif ()
endif ()

if (CMAKE_OSX_ARCHITECTURES)
   # Replace the list separator so the multi-arch value (arm64;x86_64) survives
   # being forwarded into a dep ExternalProject's CMAKE_ARGS. ExternalProject's
   # LIST_SEPARATOR restores it to ';' for the inner CMake (see sdl3.cmake).
   string (REPLACE ";" "|" _osx_archs "${CMAKE_OSX_ARCHITECTURES}")
   list (APPEND CROSS_COMPILE_ARGS "-DCMAKE_OSX_ARCHITECTURES=${_osx_archs}")
endif ()
if (CMAKE_OSX_DEPLOYMENT_TARGET)
   list (APPEND CROSS_COMPILE_ARGS -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET})
endif ()

# ---------------------------------------------------------------------------
# Dependency list
# ---------------------------------------------------------------------------

set (RUBIDIUM_DEPS
   sdl3
   fonts
)

# ---------------------------------------------------------------------------
# Mode selection: single dep (CI) or all deps (scripts / dev).
# ---------------------------------------------------------------------------

if (DEP)
   list (FIND RUBIDIUM_DEPS "${DEP}" _dep_idx)
   if (_dep_idx LESS 0)
      message (FATAL_ERROR "DEP '${DEP}' is not in the known deps list (${RUBIDIUM_DEPS})")
   endif ()
   include ("${CMAKE_CURRENT_SOURCE_DIR}/${DEP}.cmake")
else ()
   foreach (dep IN LISTS RUBIDIUM_DEPS)
      include ("${CMAKE_CURRENT_SOURCE_DIR}/${dep}.cmake")
   endforeach ()
endif ()
