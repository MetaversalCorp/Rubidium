# Copyright 2026 Metaversal Corporation. All rights reserved.

cmake_minimum_required (VERSION 3.20)
file (STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION" RUBIDIUM_VER)
set_property (DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION")

# Product identity. Artemis overlays branding/Product.cmake; this tree's
# CMake target names, namespace, and file layout stay Rubidium.
set (RUBIDIUM_BRANDING_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../branding")
include ("${RUBIDIUM_BRANDING_DIR}/Product.cmake")
set_property (DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
   "${RUBIDIUM_BRANDING_DIR}/Product.cmake")
set (RUBIDIUM_CDN_URL "${PRODUCT_CDN_URL}" CACHE STRING
   "CDN base URL for manifest and installers")
# Command-line -DRUBIDIUM_CDN_URL=... wins via the cache; feed Brand.h from it.
set (PRODUCT_CDN_URL "${RUBIDIUM_CDN_URL}")

# Limit multi-config generators (VS, Xcode, Ninja Multi-Config) to the two
# configs we actually support. Dropping MinSizeRel + RelWithDebInfo is
# required because the deps trees are only built for debug and release;
# paths with $<LOWER_CASE:$<CONFIG>> would resolve to non-existent dirs
# (e.g. .../minsizerel/libs/...) and CMake validates those during generate.
# Must be set BEFORE project() so it takes effect for this build.
set (CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING
   "Semicolon separated list of supported configuration types" FORCE)

project (Rubidium VERSION ${RUBIDIUM_VER} LANGUAGES CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

# The chrome window needs one Objective-C++ translation unit on macOS desktop
# (shell/MacChrome.mm) to give its NSWindow the transparent full-size-content
# title bar that keeps the native traffic-light buttons. Enable the language so
# CMake knows how to compile .mm files.
if (APPLE)
   enable_language (OBJCXX)
endif ()

# ---------------------------------------------------------------------------
# Release PDBs (MSVC)
# ---------------------------------------------------------------------------
# Generate PDB symbol files for Release builds so crash dumps and attach-to-
# process debugging can resolve symbols against optimized binaries. /Zi emits
# debug info into .obj; /DEBUG tells the linker to produce a .pdb next to the
# binary. /DEBUG alone silently disables /OPT:REF and /OPT:ICF -- we force them
# back on so codegen and binary size stay identical to a no-PDB Release build.
# Applied before add_subdirectory(Sneeze) so the flags propagate to Sneeze's
# targets when it is built inline as part of the Rubidium tree.
if (MSVC)
   add_compile_options ($<$<CONFIG:Release>:/Zi>)
   add_link_options    (
      $<$<CONFIG:Release>:/DEBUG>
      $<$<CONFIG:Release>:/OPT:REF>
      $<$<CONFIG:Release>:/OPT:ICF>
   )
endif ()

# Version components for resource files
set (RUBIDIUM_VER_MAJOR ${PROJECT_VERSION_MAJOR})
set (RUBIDIUM_VER_MINOR ${PROJECT_VERSION_MINOR})
set (RUBIDIUM_VER_PATCH ${PROJECT_VERSION_PATCH})
set (RUBIDIUM_PKG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../pkg")

configure_file (
   "${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
   "${CMAKE_CURRENT_BINARY_DIR}/version.h"
   @ONLY
)
configure_file (
   "${CMAKE_CURRENT_SOURCE_DIR}/../pkg/setup.rc.in"
   "${CMAKE_CURRENT_BINARY_DIR}/setup.rc"
   @ONLY
)
configure_file (
   "${RUBIDIUM_BRANDING_DIR}/Brand.h.in"
   "${CMAKE_CURRENT_BINARY_DIR}/Brand.h"
   @ONLY
)

# ---------------------------------------------------------------------------
# Force output directories
# ---------------------------------------------------------------------------
# The build scripts pass RUBIDIUM_BUILD_ROOT = <repo>/builds/<platform>.
# Outputs land in <root>/install/<config>/bin where <config> (debug/release)
# is resolved at BUILD time so a single multi-config CMake tree can emit both
# Debug and Release binaries into distinct install trees via the VS/Xcode/
# Ninja-MC dropdown.

if (NOT DEFINED RUBIDIUM_BUILD_ROOT)
   set (RUBIDIUM_BUILD_ROOT "${CMAKE_BINARY_DIR}")
endif ()

# RUBIDIUM_CONFIG names the config used to resolve find_package during THIS
# configure (the deps tree is still per-config). It is NOT the config binaries
# are emitted for -- that is decided per `cmake --build --config`.
if (NOT DEFINED RUBIDIUM_CONFIG OR RUBIDIUM_CONFIG STREQUAL "")
   if (CMAKE_BUILD_TYPE)
      set (RUBIDIUM_CONFIG "${CMAKE_BUILD_TYPE}")
   else ()
      set (RUBIDIUM_CONFIG "Release")
   endif ()
endif ()
if (NOT DEFINED RUBIDIUM_PLATFORM)
   set (RUBIDIUM_PLATFORM "unknown")
endif ()

# Auto-detect platform tag if the build script / CI didn't pass it.
# This MUST happen before add_subdirectory(Sneeze) below, since Sneeze's
# CMakeLists reads SNEEZE_PLATFORM (which we propagate from RUBIDIUM_PLATFORM)
# and uses it to drive find_package paths. Auto-detect happens later in the
# file too — but by then it's too late for the inlined Sneeze build.
if (RUBIDIUM_PLATFORM STREQUAL "unknown")
   if (WIN32)
      if (CMAKE_SIZEOF_VOID_P EQUAL 8)
         set (RUBIDIUM_PLATFORM "windows-x64" CACHE STRING "Platform identifier" FORCE)
      else ()
         set (RUBIDIUM_PLATFORM "windows-x86" CACHE STRING "Platform identifier" FORCE)
      endif ()
   elseif (ANDROID)
      set (RUBIDIUM_PLATFORM "android-${ANDROID_ABI}" CACHE STRING "Platform identifier" FORCE)
   elseif (CMAKE_SYSTEM_NAME STREQUAL "iOS")
      set (RUBIDIUM_PLATFORM "ios-arm64" CACHE STRING "Platform identifier" FORCE)
   elseif (APPLE)
      if (CMAKE_OSX_ARCHITECTURES MATCHES "arm64" AND CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
         set (RUBIDIUM_PLATFORM "macos-universal" CACHE STRING "Platform identifier" FORCE)
      elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
         set (RUBIDIUM_PLATFORM "macos-arm64" CACHE STRING "Platform identifier" FORCE)
      else ()
         set (RUBIDIUM_PLATFORM "macos-x64" CACHE STRING "Platform identifier" FORCE)
      endif ()
   else ()
      if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
         set (RUBIDIUM_PLATFORM "linux-arm64" CACHE STRING "Platform identifier" FORCE)
      else ()
         set (RUBIDIUM_PLATFORM "linux-x64" CACHE STRING "Platform identifier" FORCE)
      endif ()
   endif ()
endif ()

string (TOLOWER "${RUBIDIUM_CONFIG}" RUBIDIUM_CONFIG_LC)

message (STATUS "Rubidium: platform     = ${RUBIDIUM_PLATFORM}")
message (STATUS "Rubidium: find config  = ${RUBIDIUM_CONFIG}")
message (STATUS "Rubidium: build root   = ${RUBIDIUM_BUILD_ROOT}")

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${RUBIDIUM_BUILD_ROOT}/install/$<LOWER_CASE:$<CONFIG>>/bin")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${RUBIDIUM_BUILD_ROOT}/install/$<LOWER_CASE:$<CONFIG>>/bin")
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${RUBIDIUM_BUILD_ROOT}/install/$<LOWER_CASE:$<CONFIG>>/lib")
foreach (_cfg IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
   string (TOLOWER "${_cfg}" _cfg_lc)
   set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfg} "${RUBIDIUM_BUILD_ROOT}/install/${_cfg_lc}/bin")
   set (CMAKE_LIBRARY_OUTPUT_DIRECTORY_${_cfg} "${RUBIDIUM_BUILD_ROOT}/install/${_cfg_lc}/bin")
   set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${_cfg} "${RUBIDIUM_BUILD_ROOT}/install/${_cfg_lc}/lib")
endforeach ()

# ---------------------------------------------------------------------------
# Sneeze engine (compiled inline via add_subdirectory)
# ---------------------------------------------------------------------------

if (NOT DEFINED SNEEZE_DIR)
   set (SNEEZE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../Sneeze" CACHE PATH "Path to Sneeze engine repo")
endif ()

# Sneeze's src/CMakeLists.txt reads LIBS_DIR to find its own deps. For
# multi-config consumption, LIBS_DIR must resolve to the configure-time
# config (for find_package) while the actual link paths are later rewritten
# by Sneeze's dual-config block to use $<LOWER_CASE:$<CONFIG>> at build time.
#
# The build scripts pass SNEEZE_LIBS_DIR as a config-specific absolute path
# (e.g. .../sneeze/deps/builds/windows-x64/release/libs). That's what Sneeze's
# find_package calls resolve against.
if (NOT DEFINED SNEEZE_LIBS_DIR OR SNEEZE_LIBS_DIR STREQUAL "")
   # CI and a handful of legacy callers pass LIBS_DIR instead of SNEEZE_LIBS_DIR.
   # Accept it for backward compat before falling through to the default.
   if (DEFINED LIBS_DIR AND NOT LIBS_DIR STREQUAL "")
      set (SNEEZE_LIBS_DIR "${LIBS_DIR}" CACHE PATH
         "Path to Sneeze installed deps (computed by build scripts)")
   else ()
      set (SNEEZE_LIBS_DIR "${SNEEZE_DIR}/deps/builds/windows-x64/${RUBIDIUM_CONFIG_LC}/libs" CACHE PATH
         "Path to Sneeze installed deps (computed by build scripts)")
   endif ()
endif ()
set (LIBS_DIR "${SNEEZE_LIBS_DIR}")

# Base form of the Sneeze libs dir with the config segment replaced by a
# generator expression. Used by POST_BUILD / install rules that copy DLLs
# out of the deps tree.
#
# The Windows build scripts pass SNEEZE_LIBS_DIR as a config-partitioned
# path (.../<win-host>/<config>/libs) so we can flip the config segment
# for multi-config generators. The CI workflows for macOS / Linux / iOS /
# Android / Quest pass a flat per-platform path (Sneeze/libs-macos etc.)
# with no config segment — for those, just reuse SNEEZE_LIBS_DIR as-is
# since they don't multi-config the runtime deps.
get_filename_component (_sneeze_libs_parent "${SNEEZE_LIBS_DIR}" DIRECTORY)
get_filename_component (_sneeze_libs_grandparent "${_sneeze_libs_parent}" DIRECTORY)
get_filename_component (_sneeze_libs_cfgseg "${_sneeze_libs_parent}" NAME)
string (TOLOWER "${_sneeze_libs_cfgseg}" _sneeze_libs_cfgseg_lc)
# Multi-config generators (Ninja Multi-Config on macOS/Linux, VS on Windows)
# configure once but can build either config via --build --config. When the
# deps tree is config-partitioned (.../<platform>/<debug|release>/libs) we must
# track the BUILD config, not the configure-time one — otherwise a Release
# build copies the Debug runtime dylibs (or vice-versa) that were frozen into
# SNEEZE_LIBS_DIR at configure time. Detect the config segment by name and
# substitute a generator expression. CI passes a flat per-platform path with no
# config segment; that falls through to using SNEEZE_LIBS_DIR verbatim.
if ((_sneeze_libs_cfgseg_lc STREQUAL "debug" OR _sneeze_libs_cfgseg_lc STREQUAL "release")
      AND EXISTS "${_sneeze_libs_grandparent}")
   set (SNEEZE_LIBS_BASE_GEN "${_sneeze_libs_grandparent}/$<LOWER_CASE:$<CONFIG>>/libs")
else ()
   set (SNEEZE_LIBS_BASE_GEN "${SNEEZE_LIBS_DIR}")
endif ()

message (STATUS "Rubidium: sneeze dir      = ${SNEEZE_DIR}")
message (STATUS "Rubidium: sneeze libs     = ${SNEEZE_LIBS_DIR}")
message (STATUS "Rubidium: sneeze libs gen = ${SNEEZE_LIBS_BASE_GEN}")

# Cross-compile: Android/iOS toolchains default CMAKE_FIND_ROOT_PATH_MODE_*
# to ONLY for PACKAGE/LIBRARY/INCLUDE, which blocks find_package from
# resolving paths outside the sysroot. Set BOTH so find_package(SDL3)
# below, and Sneeze's find_package(anari) / find_library calls in the
# add_subdirectory'd project, can resolve LIBS_DIR-backed installs.
if (ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
   set (CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
   set (CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
   set (CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
endif ()

# All Sneeze build activity stays in Sneeze's own build folder — intermediates
# (.vcxproj, .obj) and final binaries (Sneeze.lib, test/tool exes) alike.
# Without these two lines, Sneeze falls back to CMAKE_BINARY_DIR (Rubidium's
# top-level build dir) and creates a stray install tree under Rubidium.
set (SNEEZE_BUILD_ROOT "${SNEEZE_DIR}/builds/${RUBIDIUM_PLATFORM}")

# Propagate config + platform explicitly. Sneeze's src/CMakeLists.txt also
# auto-derives SNEEZE_CONFIG from LIBS_DIR's debug/release segment, but the
# regex it uses requires forward slashes; on Windows, Rubidium passes a
# backslash-separated SNEEZE_LIBS_DIR (D:\github\Sneeze\deps\...\debug\libs)
# that the regex misses, leaving Sneeze defaulting to "Release" even when
# Rubidium is building Debug. That mismatch silently disables Sneeze's
# dual-config dispatch (its `_sneeze_cfg_seg = "/release/"` doesn't match
# the actual `\debug\` paths in IMPORTED_LOCATION), so vox::vox keeps raw
# debug paths in INTERFACE_LINK_LIBRARIES and the Release build links Debug
# libs (LNK2038 _ITERATOR_DEBUG_LEVEL / RuntimeLibrary mismatches against
# openxr_loaderd.lib, spirv-cross-cored.lib, etc.). Setting it explicitly
# makes the intent unambiguous and bypasses Sneeze's path-format-sensitive
# derivation entirely.
set (SNEEZE_CONFIG   "${RUBIDIUM_CONFIG}")
set (SNEEZE_PLATFORM "${RUBIDIUM_PLATFORM}")

# Rubidium only needs Sneeze.lib — skip SneezeTest/SignMsf in the same solution.
# Parallel MSBuild can otherwise link those targets before Sneeze.lib is written (LNK1104).
set (SNEEZE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set (SNEEZE_BUILD_TOOLS OFF CACHE BOOL "" FORCE)

add_subdirectory ("${SNEEZE_DIR}/src" "${SNEEZE_DIR}/builds/${RUBIDIUM_PLATFORM}/build")

# ---------------------------------------------------------------------------
# SDL3
# ---------------------------------------------------------------------------

if (NOT DEFINED RUBIDIUM_LIBS_DIR OR RUBIDIUM_LIBS_DIR STREQUAL "")
   set (RUBIDIUM_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../deps/builds/windows-x64/${RUBIDIUM_CONFIG_LC}/libs"
      CACHE PATH "Path to Rubidium installed deps (computed by build scripts)")
endif ()

get_filename_component (_rubidium_libs_parent "${RUBIDIUM_LIBS_DIR}" DIRECTORY)
get_filename_component (_rubidium_libs_parent "${_rubidium_libs_parent}" DIRECTORY)
set (RUBIDIUM_LIBS_BASE_GEN "${_rubidium_libs_parent}/$<LOWER_CASE:$<CONFIG>>/libs")

message (STATUS "Rubidium: rubidium libs     = ${RUBIDIUM_LIBS_DIR}")
message (STATUS "Rubidium: rubidium libs gen = ${RUBIDIUM_LIBS_BASE_GEN}")

if (NOT SDL3_ROOT)
   set (SDL3_ROOT "${RUBIDIUM_LIBS_DIR}/SDL3/install")
endif ()
list (APPEND CMAKE_PREFIX_PATH "${SDL3_ROOT}")
find_package (SDL3 REQUIRED CONFIG)

# Rewrite SDL3's imported target for dual-config, same pattern as Sneeze's
# LOCAL_DEPS block. At configure time the SDL3 config package pointed every
# IMPORTED_LOCATION_* at the configure-time config tree; we dualize them
# so the VS/Xcode/Ninja-MC dropdown can pick the right config at build time.
set (_rubidium_cfg_seg     "/${RUBIDIUM_CONFIG_LC}/")
set (_rubidium_cfg_gen     "/$<LOWER_CASE:$<CONFIG>>/")
set (_rubidium_cfg_debug   "/debug/")
set (_rubidium_cfg_release "/release/")

function (_rubidium_dualize_target tgt)
   if (NOT TARGET ${tgt})
      return ()
   endif ()
   get_target_property (_aliased ${tgt} ALIASED_TARGET)
   if (_aliased)
      set (tgt "${_aliased}")
   endif ()

   get_property (_already GLOBAL PROPERTY _RUBIDIUM_DUALIZED_${tgt} SET)
   if (_already)
      return ()
   endif ()
   set_property (GLOBAL PROPERTY _RUBIDIUM_DUALIZED_${tgt} TRUE)

   foreach (_prop IN ITEMS INTERFACE_LINK_LIBRARIES IMPORTED_LINK_DEPENDENT_LIBRARIES)
      get_target_property (_deps ${tgt} ${_prop})
      if (_deps)
         foreach (_d IN LISTS _deps)
            string (REGEX REPLACE "^\\\$<[^:]+:(.*)>$" "\\1" _d_clean "${_d}")
            if (TARGET ${_d_clean})
               _rubidium_dualize_target (${_d_clean})
            endif ()
         endforeach ()
      endif ()
   endforeach ()

   get_target_property (_type ${tgt} TYPE)
   if (_type MATCHES "LIBRARY")
      set (_ref "")
      foreach (_prop IN ITEMS
         IMPORTED_LOCATION_RELEASE IMPORTED_LOCATION_DEBUG
         IMPORTED_LOCATION_RELWITHDEBINFO IMPORTED_LOCATION_MINSIZEREL
         IMPORTED_LOCATION_NOCONFIG IMPORTED_LOCATION)
         get_target_property (_v ${tgt} ${_prop})
         if (_v)
            set (_ref "${_v}")
            break ()
         endif ()
      endforeach ()
      if (_ref AND _ref MATCHES "${_rubidium_cfg_seg}")
         string (REPLACE "${_rubidium_cfg_seg}" "${_rubidium_cfg_debug}"   _d "${_ref}")
         string (REPLACE "${_rubidium_cfg_seg}" "${_rubidium_cfg_release}" _r "${_ref}")
         set_target_properties (${tgt} PROPERTIES
            IMPORTED_LOCATION_DEBUG   "${_d}"
            IMPORTED_LOCATION_RELEASE "${_r}"
            IMPORTED_CONFIGURATIONS   "Debug;Release")
      endif ()

      set (_ref "")
      foreach (_prop IN ITEMS
         IMPORTED_IMPLIB_RELEASE IMPORTED_IMPLIB_DEBUG
         IMPORTED_IMPLIB_RELWITHDEBINFO IMPORTED_IMPLIB_MINSIZEREL
         IMPORTED_IMPLIB_NOCONFIG IMPORTED_IMPLIB)
         get_target_property (_v ${tgt} ${_prop})
         if (_v)
            set (_ref "${_v}")
            break ()
         endif ()
      endforeach ()
      if (_ref AND _ref MATCHES "${_rubidium_cfg_seg}")
         string (REPLACE "${_rubidium_cfg_seg}" "${_rubidium_cfg_debug}"   _d "${_ref}")
         string (REPLACE "${_rubidium_cfg_seg}" "${_rubidium_cfg_release}" _r "${_ref}")
         set_target_properties (${tgt} PROPERTIES
            IMPORTED_IMPLIB_DEBUG   "${_d}"
            IMPORTED_IMPLIB_RELEASE "${_r}")
      endif ()
   endif ()

   # CMake validates INTERFACE_INCLUDE_DIRECTORIES on imported targets at
   # generate time, evaluating any genex per entry in CMAKE_CONFIGURATION_TYPES.
   # If we unconditionally rewrite "/release/" to "/$<LOWER_CASE:$<CONFIG>>/",
   # configuring against a tree that has only one config's deps on disk fails
   # with "Imported target ... includes non-existent path .../debug/...".
   # Apply the genex only when BOTH config-resolved paths exist on disk.
   # Otherwise leave the include path concrete to whatever find_package just
   # resolved -- always valid for the configure-time config, and headers are
   # config-invariant for the deps Rubidium ships (just SDL3 today).
   get_target_property (_inc ${tgt} INTERFACE_INCLUDE_DIRECTORIES)
   if (_inc)
      set (_inc_new "")
      foreach (_p IN LISTS _inc)
         if (_p MATCHES "${_rubidium_cfg_seg}")
            string (REPLACE "${_rubidium_cfg_seg}" "${_rubidium_cfg_debug}"   _p_dbg "${_p}")
            string (REPLACE "${_rubidium_cfg_seg}" "${_rubidium_cfg_release}" _p_rel "${_p}")
            if (EXISTS "${_p_dbg}" AND EXISTS "${_p_rel}")
               string (REPLACE "${_rubidium_cfg_seg}" "${_rubidium_cfg_gen}" _p_new "${_p}")
               list (APPEND _inc_new "${_p_new}")
            else ()
               list (APPEND _inc_new "${_p}")
            endif ()
         else ()
            list (APPEND _inc_new "${_p}")
         endif ()
      endforeach ()
      set_target_properties (${tgt} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${_inc_new}")
   endif ()
endfunction ()

foreach (_tgt IN ITEMS SDL3::SDL3-static SDL3::SDL3 SDL3::Headers)
   if (TARGET ${_tgt})
      _rubidium_dualize_target (${_tgt})
   endif ()
endforeach ()

# Generalize Sneeze-exported scalar path variables for RubidiumSetup.
# Sneeze's find modules set these as cache variables pointing into the
# configure-time config tree. Sneeze's own generalization rewrites them
# as locals in its directory scope, but those don't propagate up here.
function (_rubidium_resolve_variant in_path out_var)
   if (EXISTS "${in_path}")
      set (${out_var} "${in_path}" PARENT_SCOPE)
      return ()
   endif ()
   foreach (_suffix IN ITEMS "d" "-d")
      string (REGEX REPLACE "\\.(lib|a)$" "${_suffix}.\\1" _alt "${in_path}")
      if (EXISTS "${_alt}")
         set (${out_var} "${_alt}" PARENT_SCOPE)
         return ()
      endif ()
   endforeach ()
   string (REGEX REPLACE "([^/\\\\])-d\\.(lib|a)$" "\\1.\\2" _alt "${in_path}")
   if (EXISTS "${_alt}")
      set (${out_var} "${_alt}" PARENT_SCOPE)
      return ()
   endif ()
   set (${out_var} "${in_path}" PARENT_SCOPE)
endfunction ()

function (_rubidium_make_dual_lib in_path out_var)
   string (REPLACE "${_rubidium_cfg_seg}" "${_rubidium_cfg_debug}"   _d_path "${in_path}")
   string (REPLACE "${_rubidium_cfg_seg}" "${_rubidium_cfg_release}" _r_path "${in_path}")
   _rubidium_resolve_variant ("${_d_path}" _d)
   _rubidium_resolve_variant ("${_r_path}" _r)
   set (${out_var} "$<IF:$<CONFIG:Debug>,${_d},${_r}>" PARENT_SCOPE)
endfunction ()

foreach (_v IN ITEMS CURL_LIB CURL_INCLUDE NLOHMANN_JSON_INCLUDE)
   if (DEFINED ${_v})
      set (_val "${${_v}}")
      if (_val MATCHES "\\.(lib|a|so|dylib)$" AND _val MATCHES "${_rubidium_cfg_seg}")
         _rubidium_make_dual_lib ("${_val}" _new)
         set (${_v} "${_new}")
      elseif (_val MATCHES "${_rubidium_cfg_seg}")
         string (REPLACE "${_rubidium_cfg_seg}" "${_rubidium_cfg_gen}" _new "${_val}")
         set (${_v} "${_new}")
      endif ()
   endif ()
endforeach ()

# ---------------------------------------------------------------------------
# Source files by module
# ---------------------------------------------------------------------------

# Shell + entry point.
#
# SHELL_SOURCES lists every platform .cpp so VS / Xcode / Qt Creator project
# trees show them on every host (a Windows dev can ctrl-click into
# AppFrame_Mac.cpp etc.). Only the platform's own .cpp files actually
# compile; the rest stay HEADER_FILE_ONLY — applied via list-diff against
# SHELL_SOURCES_PLATFORM, no per-platform repetition.
set (SHELL_HEADERS
   shell/App.h
   shell/App_SDL.h
   shell/AppFrame.h
   shell/AppFrameTab.h
   shell/AppFrameTab_SDL.h
   shell/AppFrame_SDL.h
   shell/ChromeRml.h
   shell/AppFrame_Win32.h
   shell/AppFrame_iOS.h
   shell/AppFrame_Mac.h
   shell/AppFrame_Android.h
   shell/AppFrame_Linux.h
   shell/UrlBarRml.h
   shell/WinUtils.h
   shell/MacChrome.h
)

set (SHELL_SOURCES
   shell/App.cpp
   shell/AppFrame.cpp
   shell/MacChrome.mm
   shell/App_Win32.cpp        shell/AppFrame_Win32.cpp  shell/AppFrameTab.cpp  shell/WinUtils.cpp
   shell/App_SDL.cpp          shell/AppFrame_SDL.cpp    shell/AppFrameTab_SDL.cpp  shell/ChromeRml.cpp  shell/UrlBarRml.cpp
   shell/AppFrame_iOS.cpp     shell/AppFrame_Mac.cpp
   shell/AppFrame_Android.cpp shell/AppFrame_Linux.cpp
)
set (MAIN_SOURCES_ALL main_Win32.cpp main_SDL.cpp)

# Pick the per-target subset that actually compiles. Everything else in the
# matching ALL list above stays in the source tree but is marked
# HEADER_FILE_ONLY further down.
if (WIN32)
   set (SHELL_SOURCES_PLATFORM
      shell/App.cpp
      shell/AppFrame.cpp
      shell/App_Win32.cpp  shell/AppFrame_Win32.cpp  shell/AppFrameTab.cpp  shell/WinUtils.cpp
   )
   set (MAIN_SOURCES main_Win32.cpp)
else ()
   set (SHELL_SOURCES_PLATFORM
      shell/App.cpp
      shell/AppFrame.cpp
      shell/App_SDL.cpp  shell/AppFrame_SDL.cpp  shell/AppFrameTab_SDL.cpp  shell/ChromeRml.cpp  shell/UrlBarRml.cpp
   )
   set (MAIN_SOURCES main_SDL.cpp)
   if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
      list (APPEND SHELL_SOURCES_PLATFORM shell/AppFrame_iOS.cpp)
   elseif (APPLE)
      list (APPEND SHELL_SOURCES_PLATFORM shell/AppFrame_Mac.cpp shell/MacChrome.mm)
   elseif (ANDROID)
      list (APPEND SHELL_SOURCES_PLATFORM shell/AppFrame_Android.cpp)
   else ()
      list (APPEND SHELL_SOURCES_PLATFORM shell/AppFrame_Linux.cpp)
   endif ()
endif ()

set (_foreign ${SHELL_SOURCES} ${MAIN_SOURCES_ALL})
list (REMOVE_ITEM _foreign ${SHELL_SOURCES_PLATFORM} ${MAIN_SOURCES})
set_source_files_properties (${_foreign} PROPERTIES HEADER_FILE_ONLY TRUE)
unset (_foreign)

set (CANVAS_SOURCES  canvas/Canvas.cpp  canvas/Canvas_Win32.cpp  canvas/Canvas_Linux.cpp  canvas/Canvas_macOS.cpp  canvas/Canvas_Metal.mm  canvas/Canvas_Android.cpp)
set (CANVAS_HEADERS  canvas/Canvas.h)

# Platform-specific CANVAS_NATIVE implementations. Each stays in the source
# list on every host so IDE project trees show it, but is marked
# HEADER_FILE_ONLY off-platform so it is not actually built there.
if (NOT WIN32)
   set_source_files_properties (canvas/Canvas_Win32.cpp PROPERTIES HEADER_FILE_ONLY TRUE)
endif ()
if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
   set_source_files_properties (canvas/Canvas_Linux.cpp PROPERTIES HEADER_FILE_ONLY TRUE)
endif ()
if (NOT APPLE)
   set_source_files_properties (canvas/Canvas_macOS.cpp PROPERTIES HEADER_FILE_ONLY TRUE)
endif ()
# Canvas_Metal.mm (CAMetalLayer colour-space fix) is macOS-only: built on
# macOS, header-only everywhere else (including iOS, which has no chrome).
if (NOT APPLE OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
   set_source_files_properties (canvas/Canvas_Metal.mm PROPERTIES HEADER_FILE_ONLY TRUE)
endif ()
if (NOT ANDROID)
   set_source_files_properties (canvas/Canvas_Android.cpp PROPERTIES HEADER_FILE_ONLY TRUE)
endif ()

# Win32 uses MessageBox + ShellExecute; all other platforms use the generic
# implementation (SDL_ShowMessageBox on desktop, no-op RunCheck on mobile).
set (UPDATER_HEADERS)
set (UPDATER_SOURCES)
if (WIN32)
   list (APPEND UPDATER_HEADERS updater/Updater.h  updater/UpdaterWnd.h)
   list (APPEND UPDATER_SOURCES updater/Updater.cpp  updater/UpdaterWnd.cpp)
else ()
   list (APPEND UPDATER_HEADERS updater/Updater.h  updater/UpdaterGeneric.h)
   list (APPEND UPDATER_SOURCES updater/Updater.cpp  updater/UpdaterGeneric.cpp)
endif ()

set (LOGGER_SOURCES  logger/Logger.cpp  logger/ILogger.cpp)
set (LOGGER_HEADERS  logger/Logger.h  logger/ILogger.h)

set (UTIL_SOURCES  Utils.cpp)
set (UTIL_HEADERS  Utils.h)

set (RMLUI_SDL_SOURCES
   rmlui_sdl/RmlUi_SDL.cpp
   rmlui_sdl/RmlUi_SDL_Renderer.cpp
   rmlui_sdl/RmlUi_SDL_Platform.cpp
)
set (RMLUI_SDL_HEADERS
   rmlui_sdl/RmlUi_SDL.h
   rmlui_sdl/RmlUi_SDL_Renderer.h
   rmlui_sdl/RmlUi_SDL_Platform.h
)
set (INSPECTOR_SOURCES
   inspector/InspectorRml.cpp
   inspector/preview3d/Preview3D.cpp
   inspector/common/Button.cpp
   inspector/elements/Elements_Frame.cpp
   inspector/elements/Elements_Containers.cpp
   inspector/elements/Elements_Tree.cpp
   inspector/elements/Elements_Details.cpp
   inspector/elements/Elements_Detail_Computed.cpp
   inspector/elements/Elements_Detail_Details.cpp
   inspector/elements/Elements_Detail_Events.cpp
   inspector/console/Console_Frame.cpp
   inspector/console/Console_Toolbar.cpp
   inspector/console/Console_Body.cpp
   inspector/console/Console_Containers.cpp
   inspector/console/Console_Entries.cpp
   inspector/sources/Sources_Frame.cpp
   inspector/network/Network_Frame.cpp
   inspector/network/Network_Toolbar.cpp
   inspector/network/Network_Statusbar.cpp
   inspector/network/Network_Waterfall.cpp
   inspector/network/Network_Body.cpp
   inspector/network/Network_Containers.cpp
   inspector/network/Network_Files.cpp
   inspector/network/Network_Details.cpp
   inspector/network/Network_Detail_Headers.cpp
   inspector/network/Network_Detail_Preview.cpp
   inspector/network/Network_Detail_Response.cpp
   inspector/network/Network_Detail_Timing.cpp
   inspector/performance/Performance_Frame.cpp
   inspector/performance/Performance_Toolbar.cpp
   inspector/performance/Performance_Metrics.cpp
   inspector/performance/Performance_Chart.cpp
   inspector/performance/Performance_Log.cpp
   inspector/memory/Memory_Frame.cpp
   inspector/storage/Storage_Frame.cpp
   inspector/storage/Storage_Containers.cpp
   inspector/storage/Storage_Preview.cpp
   inspector/security/Security_Frame.cpp
   inspector/security/Security_Containers.cpp
   inspector/security/Security_Preview.cpp
)
set (INSPECTOR_HEADERS
   inspector/InspectorRml.h
   inspector/preview3d/Preview3D.h
   inspector/common/Button.h
   inspector/common/Collapse.h
   inspector/elements/Elements.h
   inspector/console/Console.h
   inspector/sources/Sources.h
   inspector/network/Network.h
   inspector/performance/Performance.h
   inspector/memory/Memory.h
   inspector/storage/Storage.h
   inspector/security/Security.h
)
set (SETTINGS_UI_SOURCES
   settings_ui/SettingsRml.cpp
)
set (SETTINGS_UI_HEADERS
   settings_ui/SettingsRml.h
)
set (RELEASE_NOTES_SOURCES
   release_notes/Markdown.cpp
   release_notes/ReleaseNotesRml.cpp
)
set (RELEASE_NOTES_HEADERS
   release_notes/Markdown.h
   release_notes/ReleaseNotesRml.h
)

set (CLI_SOURCES
   cli/MsfCli.cpp
)
set (CLI_HEADERS
   cli/MsfCli.h
)


set (PCH_HEADERS     pch.h  resource.h  Native.h)

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------

if (WIN32)
   set (RESOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/rubidium.rc")
endif ()

if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
   set (IOS_LAUNCH_STORYBOARD "${CMAKE_CURRENT_SOURCE_DIR}/../pkg/ios/LaunchScreen.storyboard")
   set (IOS_RESOURCES "${IOS_LAUNCH_STORYBOARD}")
   set_source_files_properties ("${IOS_LAUNCH_STORYBOARD}" PROPERTIES
      MACOSX_PACKAGE_LOCATION "Resources"
   )
endif ()

# macOS: build AppIcon.icns from pkg/logo.png using sips + iconutil (both
# native to macOS runners). Skipped on iOS — iOS uses PNG icons direct.
set (APP_ICON_SOURCE)
if (APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
   set (ICON_PNG    "${CMAKE_CURRENT_SOURCE_DIR}/../pkg/logo.png")
   set (ICONSET_DIR "${CMAKE_CURRENT_BINARY_DIR}/AppIcon.iconset")
   set (ICON_ICNS   "${CMAKE_CURRENT_BINARY_DIR}/AppIcon.icns")
   add_custom_command (
      OUTPUT "${ICON_ICNS}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${ICONSET_DIR}"
      COMMAND sips -z   16   16 "${ICON_PNG}" --out "${ICONSET_DIR}/icon_16x16.png"     > /dev/null
      COMMAND sips -z   32   32 "${ICON_PNG}" --out "${ICONSET_DIR}/icon_16x16@2x.png"  > /dev/null
      COMMAND sips -z   32   32 "${ICON_PNG}" --out "${ICONSET_DIR}/icon_32x32.png"     > /dev/null
      COMMAND sips -z   64   64 "${ICON_PNG}" --out "${ICONSET_DIR}/icon_32x32@2x.png"  > /dev/null
      COMMAND sips -z  128  128 "${ICON_PNG}" --out "${ICONSET_DIR}/icon_128x128.png"   > /dev/null
      COMMAND sips -z  256  256 "${ICON_PNG}" --out "${ICONSET_DIR}/icon_128x128@2x.png" > /dev/null
      COMMAND sips -z  256  256 "${ICON_PNG}" --out "${ICONSET_DIR}/icon_256x256.png"   > /dev/null
      COMMAND sips -z  512  512 "${ICON_PNG}" --out "${ICONSET_DIR}/icon_256x256@2x.png" > /dev/null
      COMMAND sips -z  512  512 "${ICON_PNG}" --out "${ICONSET_DIR}/icon_512x512.png"   > /dev/null
      COMMAND sips -z 1024 1024 "${ICON_PNG}" --out "${ICONSET_DIR}/icon_512x512@2x.png" > /dev/null
      COMMAND iconutil -c icns "${ICONSET_DIR}" -o "${ICON_ICNS}"
      DEPENDS "${ICON_PNG}"
      COMMENT "Generating AppIcon.icns"
      VERBATIM
   )
   set_source_files_properties ("${ICON_ICNS}" PROPERTIES
      MACOSX_PACKAGE_LOCATION "Resources")
   set (APP_ICON_SOURCE "${ICON_ICNS}")
endif ()

# iOS: generate PNG icon sizes from pkg/logo.png (Asset Catalog would be
# cleaner but requires actool + .xcassets; raw PNG + CFBundleIcons works
# for sideloaded / dev builds).
set (IOS_ICONS)
if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
   set (ICON_PNG "${CMAKE_CURRENT_SOURCE_DIR}/../pkg/logo.png")
   foreach (_size 40 58 60 80 87 120 152 167 180 1024)
      set (_out "${CMAKE_CURRENT_BINARY_DIR}/AppIcon-${_size}.png")
      add_custom_command (
         OUTPUT "${_out}"
         COMMAND sips -z ${_size} ${_size} "${ICON_PNG}" --out "${_out}" > /dev/null
         DEPENDS "${ICON_PNG}"
         COMMENT "Generating ${_size}x${_size} iOS icon"
         VERBATIM
      )
      set_source_files_properties ("${_out}" PROPERTIES
         MACOSX_PACKAGE_LOCATION "Resources")
      list (APPEND IOS_ICONS "${_out}")
   endforeach ()
endif ()

# Android: shared library loaded by SDLActivity. All others: executable.
if (ANDROID)
   add_library (Rubidium SHARED
      ${MAIN_SOURCES_ALL}
      ${SHELL_SOURCES}      ${SHELL_HEADERS}
      ${CANVAS_SOURCES}     ${CANVAS_HEADERS}
      ${UPDATER_SOURCES}    ${UPDATER_HEADERS}
      ${LOGGER_SOURCES}     ${LOGGER_HEADERS}
      ${UTIL_SOURCES}       ${UTIL_HEADERS}
      ${RMLUI_SDL_SOURCES}  ${RMLUI_SDL_HEADERS}
      ${INSPECTOR_SOURCES}  ${INSPECTOR_HEADERS}
      ${SETTINGS_UI_SOURCES} ${SETTINGS_UI_HEADERS}
      ${RELEASE_NOTES_SOURCES} ${RELEASE_NOTES_HEADERS}
      ${CLI_SOURCES}        ${CLI_HEADERS}
      ${PCH_HEADERS}
   )
else ()
   add_executable (Rubidium
      ${MAIN_SOURCES_ALL}
      ${SHELL_SOURCES}      ${SHELL_HEADERS}
      ${CANVAS_SOURCES}     ${CANVAS_HEADERS}
      ${UPDATER_SOURCES}    ${UPDATER_HEADERS}
      ${LOGGER_SOURCES}     ${LOGGER_HEADERS}
      ${UTIL_SOURCES}       ${UTIL_HEADERS}
      ${RMLUI_SDL_SOURCES}  ${RMLUI_SDL_HEADERS}
      ${INSPECTOR_SOURCES}  ${INSPECTOR_HEADERS}
      ${SETTINGS_UI_SOURCES} ${SETTINGS_UI_HEADERS}
      ${RELEASE_NOTES_SOURCES} ${RELEASE_NOTES_HEADERS}
      ${CLI_SOURCES}        ${CLI_HEADERS}
      ${PCH_HEADERS}
      ${RESOURCE_FILES}
      ${IOS_RESOURCES}
      ${APP_ICON_SOURCE}
      ${IOS_ICONS}
   )
endif ()

set_target_properties (Rubidium PROPERTIES OUTPUT_NAME "${PRODUCT_NAME}")

if (WIN32)
   set_target_properties (Rubidium PROPERTIES WIN32_EXECUTABLE TRUE)
endif ()

if (APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
   set_target_properties (Rubidium PROPERTIES
      MACOSX_BUNDLE TRUE
      MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/../pkg/macos/Info.plist"
      MACOSX_BUNDLE_GUI_IDENTIFIER "${PRODUCT_BUNDLE_ID}"
      MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
      MACOSX_BUNDLE_BUNDLE_NAME "${PRODUCT_NAME}"
      MACOSX_BUNDLE_ICON_FILE "AppIcon"
      XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${PRODUCT_BUNDLE_ID}"
   )
endif ()

if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
   set_target_properties (Rubidium PROPERTIES
      MACOSX_BUNDLE TRUE
      MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/../pkg/ios/Info.plist"
      MACOSX_BUNDLE_GUI_IDENTIFIER "${PRODUCT_BUNDLE_ID}"
      MACOSX_BUNDLE_BUNDLE_VERSION "1"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
      XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${PRODUCT_BUNDLE_ID}"
      RESOURCE "${IOS_RESOURCES}"
   )
endif ()

target_include_directories (Rubidium PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}
   ${CMAKE_CURRENT_BINARY_DIR}
)

target_precompile_headers (Rubidium PRIVATE pch.h)

target_link_libraries (Rubidium PRIVATE Sneeze)
if (ANDROID)
   target_link_libraries (Rubidium PRIVATE SDL3::SDL3)
elseif (TARGET SDL3::SDL3-static)
   target_link_libraries (Rubidium PRIVATE SDL3::SDL3-static)
else ()
   target_link_libraries (Rubidium PRIVATE SDL3::SDL3)
endif ()

if (WIN32)
   target_link_libraries (Rubidium PRIVATE dwmapi comctl32 uxtheme)
elseif (ANDROID)
   # Android: Sneeze's libcurl links against BoringSSL (replaced OpenSSL).
   target_link_libraries (Rubidium PRIVATE
      "${SNEEZE_LIBS_DIR}/boringssl/install/lib/libssl.a"
      "${SNEEZE_LIBS_DIR}/boringssl/install/lib/libcrypto.a")
elseif (APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
   # Do not link Homebrew OpenSSL on universal macOS builds — CI runners
   # often provide arm64-only dylibs, which makes the x86_64 slice crash at
   # launch on Intel Macs and under Rosetta. Use Sneeze's static BoringSSL.
   target_link_libraries (Rubidium PRIVATE
      "${SNEEZE_LIBS_DIR}/boringssl/install/lib/libssl.a"
      "${SNEEZE_LIBS_DIR}/boringssl/install/lib/libcrypto.a")
   # Cocoa for shell/MacChrome.mm (NSWindow title-bar configuration).
   find_library (COCOA_FW Cocoa REQUIRED)
   target_link_libraries (Rubidium PRIVATE ${COCOA_FW})
   # QuartzCore (CAMetalLayer) + CoreGraphics (CGColorSpace) for
   # canvas/Canvas_Metal.mm (Metal layer colour-space fix).
   find_library (QUARTZCORE_FW QuartzCore REQUIRED)
   find_library (COREGRAPHICS_FW CoreGraphics REQUIRED)
   target_link_libraries (Rubidium PRIVATE ${QUARTZCORE_FW} ${COREGRAPHICS_FW})
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
   # curl links Sneeze's static BoringSSL (propagated from the Sneeze target).
   # Do NOT link system OpenSSL: libssl.so/libcrypto.so export the same symbol
   # names as BoringSSL and get cross-wired at runtime (system TLS_method()
   # passed into BoringSSL SSL_CTX_new), crashing on the first HTTPS fetch.
   find_package (ZLIB QUIET)
   if (ZLIB_FOUND)
      target_link_libraries (Rubidium PRIVATE ZLIB::ZLIB)
   endif ()
   # X11 is required to reparent each tab's Filament canvas window as a child of
   # the chrome frame (XReparentWindow / XMoveResizeWindow in canvas/Canvas.cpp).
   find_package (X11 REQUIRED)
   target_link_libraries (Rubidium PRIVATE X11::X11)
elseif (NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
   find_package (OpenSSL QUIET)
   find_package (ZLIB QUIET)
   if (OpenSSL_FOUND)
      target_link_libraries (Rubidium PRIVATE OpenSSL::SSL OpenSSL::Crypto)
   endif ()
   if (ZLIB_FOUND)
      target_link_libraries (Rubidium PRIVATE ZLIB::ZLIB)
   endif ()
endif ()

target_compile_definitions (Rubidium PRIVATE
   RUBIDIUM_PLATFORM="${RUBIDIUM_PLATFORM}"
   RMLUI_SDL_VERSION_MAJOR=3
   $<$<BOOL:${WIN32}>:RUBIDIUM_PLATFORM_WINDOWS>
   $<$<AND:$<BOOL:${APPLE}>,$<NOT:$<STREQUAL:${CMAKE_SYSTEM_NAME},iOS>>>:RUBIDIUM_PLATFORM_MACOS>
   $<$<STREQUAL:${CMAKE_SYSTEM_NAME},iOS>:RUBIDIUM_PLATFORM_IOS>
   $<$<BOOL:${ANDROID}>:RUBIDIUM_PLATFORM_ANDROID>
   $<$<AND:$<NOT:$<BOOL:${WIN32}>>,$<NOT:$<BOOL:${APPLE}>>,$<NOT:$<BOOL:${ANDROID}>>>:RUBIDIUM_PLATFORM_LINUX>
   $<$<STREQUAL:${CMAKE_SYSTEM_NAME},iOS>:RUBIDIUM_IOS>
)

# ---------------------------------------------------------------------------
# IDE source grouping
# ---------------------------------------------------------------------------

source_group ("shell"      FILES ${SHELL_SOURCES} ${SHELL_HEADERS})
source_group ("canvas"     FILES ${CANVAS_SOURCES} ${CANVAS_HEADERS})
source_group ("updater"    FILES ${UPDATER_SOURCES} ${UPDATER_HEADERS})
source_group ("logger"     FILES ${LOGGER_SOURCES} ${LOGGER_HEADERS})
source_group ("inspector"  FILES ${INSPECTOR_SOURCES} ${INSPECTOR_HEADERS})
source_group ("settings_ui" FILES ${SETTINGS_UI_SOURCES} ${SETTINGS_UI_HEADERS})
source_group ("release_notes" FILES ${RELEASE_NOTES_SOURCES} ${RELEASE_NOTES_HEADERS})
source_group ("cli"        FILES ${CLI_SOURCES} ${CLI_HEADERS})
source_group ("Source Files" FILES ${PCH_HEADERS})

# ---------------------------------------------------------------------------
# RubidiumSetup - standalone setup/updater executable
# ---------------------------------------------------------------------------

if (NOT ANDROID AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")

add_executable (RubidiumSetup
   "${CMAKE_CURRENT_SOURCE_DIR}/../pkg/RubidiumSetup.cpp"
   "${CMAKE_CURRENT_BINARY_DIR}/setup.rc"
)

target_include_directories (RubidiumSetup PRIVATE
   ${CURL_INCLUDE}
   ${NLOHMANN_JSON_INCLUDE}
   ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_definitions (RubidiumSetup PRIVATE
   CURL_STATICLIB
   RUBIDIUM_PLATFORM="${RUBIDIUM_PLATFORM}"
)

target_link_libraries (RubidiumSetup PRIVATE
   ${CURL_LIB}
)

if (WIN32)
   target_link_libraries (RubidiumSetup PRIVATE
      bcrypt wldap32 crypt32 normaliz ws2_32 comctl32
   )
   set_target_properties (RubidiumSetup PROPERTIES
      WIN32_EXECUTABLE TRUE
      OUTPUT_NAME "${PRODUCT_NAME_SETUP}"
   )
elseif (APPLE)
   find_library (SECURITY_FW Security REQUIRED)
   find_library (COREFOUNDATION_FW CoreFoundation REQUIRED)
   find_library (SYSTEMCONFIGURATION_FW SystemConfiguration REQUIRED)
   # macOS curl is BoringSSL-backed (same as Linux). libcurl.a does not pull
   # libssl/libcrypto transitively, so RubidiumSetup must link them explicitly.
   target_link_libraries (RubidiumSetup PRIVATE
      "${SNEEZE_LIBS_DIR}/boringssl/install/lib/libssl.a"
      "${SNEEZE_LIBS_DIR}/boringssl/install/lib/libcrypto.a"
      pthread
      ${SECURITY_FW} ${COREFOUNDATION_FW} ${SYSTEMCONFIGURATION_FW})
else ()
   target_link_libraries (RubidiumSetup PRIVATE
      ${BORINGSSL_SSL_LIB} ${BORINGSSL_CRYPTO_LIB} pthread dl)
endif ()

set_target_properties (RubidiumSetup PROPERTIES OUTPUT_NAME "${PRODUCT_NAME_SETUP}")

# BoringSSL curl (macOS/Linux) has no OS trust store. Compile Sneeze's
# generated CA bundle directly into RubidiumSetup (it does not link Sneeze.lib).
# Windows uses Schannel + the Windows store and needs neither.
if (NOT WIN32)
   target_sources (RubidiumSetup PRIVATE
      "${SNEEZE_BUILD_ROOT}/build/cacert_data.cpp")
   target_compile_definitions (RubidiumSetup PRIVATE RUBIDIUM_SETUP_EMBED_CACERT)
endif ()

add_custom_command (TARGET RubidiumSetup POST_BUILD
   COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:RubidiumSetup>
      $<TARGET_FILE_DIR:Rubidium>
)

# RubidiumSetup's POST_BUILD steps copy the binary into the Rubidium .app bundle
# ($<TARGET_FILE_DIR:Rubidium>). The Rubidium target is what creates that bundle
# directory, so it must build first — otherwise a clean build (e.g. after
# --rebuild wipes the install tree) runs RubidiumSetup's copy before
# Rubidium.app/Contents/MacOS exists and fails with "Not a directory".
add_dependencies (RubidiumSetup Rubidium)

# Copying RubidiumSetup into Contents/MacOS/ must be followed by a fresh ad-hoc
# sign or codesign --verify fails ("sealed resource is missing or invalid").
if (APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
   add_custom_command (TARGET RubidiumSetup POST_BUILD
      COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:Rubidium>"
   )
endif ()

endif () # NOT mobile

# ---------------------------------------------------------------------------
# Copy runtime shared libraries to output directory (desktop dev builds)
# ---------------------------------------------------------------------------

if (WIN32)
   add_custom_command (TARGET Rubidium POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
         "${SNEEZE_LIBS_BASE_GEN}/Wasmtime/install/lib/wasmtime.dll"
         $<TARGET_FILE_DIR:Rubidium>
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
         "${SNEEZE_LIBS_BASE_GEN}/Halogen/install/bin/anari_library_halogen.dll"
         $<TARGET_FILE_DIR:Rubidium>
      COMMAND ${CMAKE_COMMAND} -E copy_directory
         "${CMAKE_CURRENT_SOURCE_DIR}/../deps/fonts"
         "$<TARGET_FILE_DIR:Rubidium>/fonts"
      COMMAND ${CMAKE_COMMAND} -E make_directory
         "$<TARGET_FILE_DIR:Rubidium>/images"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
         "${CMAKE_CURRENT_SOURCE_DIR}/../pkg/logo.png"
         "$<TARGET_FILE_DIR:Rubidium>/images/logo.png"
   )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
   set (_rt_libs
      "${SNEEZE_LIBS_BASE_GEN}/Wasmtime/install/lib/libwasmtime.so"
      "${SNEEZE_LIBS_BASE_GEN}/ANARI-SDK/install/lib/libanari.so"
      "${SNEEZE_LIBS_BASE_GEN}/Halogen/install/lib/libanari_library_halogen.so"
   )

   # Bundle the clang/libc++ runtime next to the binary so Rubidium runs on
   # machines without libc++ installed. Rubidium is built with clang + libc++
   # (to match Sneeze's static deps); a stock distro ships libstdc++, not
   # libc++. Ask the compiler for the concrete file (don't hardcode a multiarch
   # path), then copy it under its canonical soname so the loader -- which
   # searches $ORIGIN via the rpath set below -- finds the expected name.
   # libunwind is pulled in by libc++abi on most clang setups. The resolved
   # real paths are stashed in RUBIDIUM_CXX_RUNTIME_SONAMES / _PATHS so the
   # install(CODE) rule below ships the same files in the package.
   set (RUBIDIUM_CXX_RUNTIME_SONAMES "")
   set (RUBIDIUM_CXX_RUNTIME_PATHS   "")
   foreach (_soname libc++.so.1 libc++abi.so.1 libunwind.so.1)
      execute_process (
         COMMAND "${CMAKE_CXX_COMPILER}" -print-file-name=${_soname}
         OUTPUT_VARIABLE _so_path
         OUTPUT_STRIP_TRAILING_WHITESPACE)
      if (_so_path AND EXISTS "${_so_path}")
         get_filename_component (_so_real "${_so_path}" REALPATH)
         list (APPEND RUBIDIUM_CXX_RUNTIME_SONAMES "${_soname}")
         list (APPEND RUBIDIUM_CXX_RUNTIME_PATHS   "${_so_real}")
         add_custom_command (TARGET Rubidium POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
               "${_so_real}" "$<TARGET_FILE_DIR:Rubidium>/${_soname}"
         )
      else ()
         message (WARNING "Rubidium: ${_soname} not found via ${CMAKE_CXX_COMPILER} -print-file-name; release binary may not run on machines without it installed.")
      endif ()
   endforeach ()

   add_custom_command (TARGET Rubidium POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
         "${CMAKE_CURRENT_SOURCE_DIR}/../deps/fonts"
         "$<TARGET_FILE_DIR:Rubidium>/fonts"
      COMMAND ${CMAKE_COMMAND} -E make_directory
         "$<TARGET_FILE_DIR:Rubidium>/images"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
         "${CMAKE_CURRENT_SOURCE_DIR}/../pkg/logo.png"
         "$<TARGET_FILE_DIR:Rubidium>/images/logo.png"
   )
elseif (APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
   # libanari_library_halogen.dylib links against libanari.dylib as a shared
   # dependency (@rpath/libanari.0.dylib). Ship the anari runtime alongside
   # so dyld can resolve it via the install_name_tool @executable_path
   # rewrite below.
   #
   # libMoltenVK.dylib is bundled inside the .app for Vulkan/Metal on end-user
   # machines without brew. CI passes -DMOLTENVK_DYLIB=... from
   # scripts/build-moltenvk-universal.sh (fat arm64+x86_64). Local dev can set
   # the same cache variable or fall back to Homebrew molten-vk.
   set (MOLTENVK_DYLIB "" CACHE FILEPATH "Universal libMoltenVK.dylib for macOS bundle")
   set (_mvk_lib "")
   if (MOLTENVK_DYLIB AND EXISTS "${MOLTENVK_DYLIB}")
      set (_mvk_lib "${MOLTENVK_DYLIB}")
   else ()
      execute_process (
         COMMAND brew --prefix molten-vk
         OUTPUT_VARIABLE _mvk_prefix
         OUTPUT_STRIP_TRAILING_WHITESPACE
         RESULT_VARIABLE _mvk_rc)
      if (_mvk_rc EQUAL 0 AND EXISTS "${_mvk_prefix}")
         file (GLOB_RECURSE _mvk_candidates
            "${_mvk_prefix}/lib/libMoltenVK.dylib"
            "${_mvk_prefix}/libexec/MoltenVK.framework/MoltenVK")
         if (_mvk_candidates)
            list (GET _mvk_candidates 0 _mvk_lib)
         endif ()
      endif ()
   endif ()
   if (NOT _mvk_lib)
      message (FATAL_ERROR
         "MoltenVK not found. Set -DMOLTENVK_DYLIB= to a universal libMoltenVK.dylib "
         "(see scripts/build-moltenvk-universal.sh) or install with `brew install molten-vk`.")
   endif ()
   message (STATUS "Rubidium: MoltenVK dylib = ${_mvk_lib}")

   set (_rt_libs
      "${SNEEZE_LIBS_BASE_GEN}/Wasmtime/install/lib/libwasmtime.dylib"
      "${SNEEZE_LIBS_BASE_GEN}/ANARI-SDK/install/lib/libanari.0.dylib"
      "${SNEEZE_LIBS_BASE_GEN}/Halogen/install/lib/libanari_library_halogen.dylib"
      "${_mvk_lib}"
   )
else ()
   set (_rt_libs)
endif ()

if (_rt_libs)
   foreach (_lib ${_rt_libs})
      add_custom_command (TARGET Rubidium POST_BUILD
         COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_lib}" $<TARGET_FILE_DIR:Rubidium>
      )
   endforeach ()
endif ()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
   # The halogen plugin has a NEEDED entry on the versioned soname
   # 'libanari.so.0'. Ship that name too — the unversioned libanari.so alone
   # leaves dlopen("halogen") unable to resolve it.
   add_custom_command (TARGET Rubidium POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
         "${SNEEZE_LIBS_BASE_GEN}/ANARI-SDK/install/lib/libanari.so"
         "$<TARGET_FILE_DIR:Rubidium>/libanari.so.0"
   )
endif ()

# Linux: runtime .so files are copied next to Rubidium; teach the loader to search $ORIGIN.
if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ANDROID)
   set_target_properties (Rubidium PROPERTIES
      BUILD_RPATH   "\$ORIGIN"
      INSTALL_RPATH "\$ORIGIN"
   )
   # Use DT_RPATH (not DT_RUNPATH) so $ORIGIN is searched for transitive deps
   # of dlopen'd plugins (halogen -> libanari.so.0), not just the exe's direct
   # dependencies.
   target_link_options (Rubidium PRIVATE "-Wl,--disable-new-dtags")
   if (TARGET RubidiumSetup)
      set_target_properties (RubidiumSetup PROPERTIES
         BUILD_RPATH   "\$ORIGIN"
         INSTALL_RPATH "\$ORIGIN"
      )
   endif ()
endif ()

# On macOS the dylibs Sneeze / Halogen / Wasmtime produce are linked with
# absolute install_name paths that point back at the build machine
# (e.g. /Users/runner/work/Sneeze/Sneeze/deps/repos/Wasmtime/target/release/
# deps/libwasmtime.dylib). After copying them next to the Rubidium binary,
# rewrite both the dylib's own install_name and the corresponding
# LC_LOAD_DYLIB entry in Rubidium to @executable_path so dyld can resolve
# them from inside the .app bundle on any machine.
if (APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS" AND _rt_libs)
   # The dylib's install_name (LC_ID_DYLIB) is the string baked into
   # LC_LOAD_DYLIB of every binary that linked against it — but it's
   # NOT the filesystem path of the dylib the linker read. For dylibs
   # produced by other repos' CI (Wasmtime, Halogen) the install_name
   # refers to the upstream build machine's absolute path. A helper
   # cmake -P script resolves the original install_name dynamically
   # so we can feed the right "from" to install_name_tool -change.
   set (_fix_macos_rpaths_script "${CMAKE_CURRENT_BINARY_DIR}/fix_macos_rpaths.cmake")
   file (WRITE "${_fix_macos_rpaths_script}"
      "# Auto-generated: rewrites RUBIDIUM_BIN's LC_LOAD_DYLIB entries for\n"
      "# LIB_FILENAME (all universal arch slices) and LIB_COPY's LC_ID_DYLIB to\n"
      "# @executable_path. Scans otool -L on the fat binary — a single otool -D\n"
      "# on LIB_ORIG misses per-arch Wasmtime paths (x86_64 vs arm64).\n"
      "set (_target \"@executable_path/\${LIB_FILENAME}\")\n"
      "execute_process(\n"
      "   COMMAND install_name_tool -id \"\${_target}\" \"\${LIB_COPY}\")\n"
      "execute_process(\n"
      "   COMMAND otool -L \"\${RUBIDIUM_BIN}\"\n"
      "   OUTPUT_VARIABLE _load_lines\n"
      "   OUTPUT_STRIP_TRAILING_WHITESPACE)\n"
      "string (REPLACE \"\\n\" \";\" _load_lines \"\${_load_lines}\")\n"
      "set (_changed_paths \"\")\n"
      "foreach (_line IN LISTS _load_lines)\n"
      "   if (NOT _line MATCHES \"^\t\")\n"
      "      continue ()\n"
      "   endif ()\n"
      "   string (REGEX REPLACE \"^\t([^ ]+) .*\" \"\\\\1\" _dep \"\${_line}\")\n"
      "   if (NOT _dep MATCHES \"/\${LIB_FILENAME}\$\")\n"
      "      continue ()\n"
      "   endif ()\n"
      "   if (_dep STREQUAL _target)\n"
      "      continue ()\n"
      "   endif ()\n"
      "   list (FIND _changed_paths \"\${_dep}\" _ix)\n"
      "   if (_ix EQUAL -1)\n"
      "      list (APPEND _changed_paths \"\${_dep}\")\n"
      "      execute_process(\n"
      "         COMMAND install_name_tool -change \"\${_dep}\"\n"
      "            \"\${_target}\" \"\${RUBIDIUM_BIN}\")\n"
      "   endif ()\n"
      "endforeach ()\n"
      "# install_name_tool -add_rpath errors on duplicate; ignore failures.\n"
      "execute_process(\n"
      "   COMMAND install_name_tool -add_rpath \"@executable_path\"\n"
      "      \"\${LIB_COPY}\"\n"
      "   RESULT_VARIABLE _rpath_rc\n"
      "   OUTPUT_QUIET ERROR_QUIET)\n")

   foreach (_lib ${_rt_libs})
      get_filename_component (_libname "${_lib}" NAME)
      add_custom_command (TARGET Rubidium POST_BUILD
         COMMAND ${CMAKE_COMMAND}
            "-DLIB_ORIG=${_lib}"
            "-DLIB_COPY=$<TARGET_FILE_DIR:Rubidium>/${_libname}"
            "-DLIB_FILENAME=${_libname}"
            "-DRUBIDIUM_BIN=$<TARGET_FILE:Rubidium>"
            -P "${_fix_macos_rpaths_script}"
      )
   endforeach ()

   # Ship a Vulkan ICD JSON in Contents/Resources/ (not MacOS/ — a non-Mach-O file
   # beside the executable breaks Developer ID codesign). library_path is relative
   # to the JSON file. App_SDL.cpp sets VK_ICD_FILENAMES at startup.
   get_filename_component (_mvk_name "${_mvk_lib}" NAME)
   set (_mvk_icd "${CMAKE_CURRENT_BINARY_DIR}/MoltenVK_icd.json")
   file (WRITE "${_mvk_icd}"
      "{\n"
      "   \"file_format_version\": \"1.0.0\",\n"
      "   \"ICD\": {\n"
      "      \"library_path\": \"../MacOS/${_mvk_name}\",\n"
      "      \"api_version\": \"1.2.0\",\n"
      "      \"is_portability_driver\": true\n"
      "   }\n"
      "}\n")
   add_custom_command (TARGET Rubidium POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_mvk_icd}"
         "$<TARGET_BUNDLE_CONTENT_DIR:Rubidium>/Resources/MoltenVK_icd.json"
   )

   # MoltenVK from Homebrew may reference other Cellar dylibs. Copy them into
   # Contents/MacOS/ and rewrite load commands so the .app runs without brew.
   set (_bundle_macos_deps_script "${CMAKE_CURRENT_BINARY_DIR}/bundle_macos_dylib_deps.cmake")
   file (WRITE "${_bundle_macos_deps_script}"
      "file (GLOB _dylibs \"\${MACOS_DIR}/*.dylib\")\n"
      "set (_bins \"\${RUBIDIUM_BIN}\" \${_dylibs})\n"
      "foreach (_pass RANGE 2)\n"
      "   foreach (_bin \${_bins})\n"
      "      execute_process (COMMAND otool -L \"\${_bin}\"\n"
      "         OUTPUT_VARIABLE _lines RESULT_VARIABLE _rc)\n"
      "      if (NOT _rc EQUAL 0)\n"
      "         continue ()\n"
      "      endif ()\n"
      "      string (REPLACE \"\\n\" \";\" _lines \"\${_lines}\")\n"
      "      foreach (_line \${_lines})\n"
      "         if (_line MATCHES \"^\t([^ \t]+) \")\n"
      "            set (_dep \"\${CMAKE_MATCH_1}\")\n"
      "         else ()\n"
      "            continue ()\n"
      "         endif ()\n"
      "         if (_dep MATCHES \"^(/usr/lib|/System|@executable_path|@rpath|@loader_path)\")\n"
      "            continue ()\n"
      "         endif ()\n"
      "         if (NOT EXISTS \"\${_dep}\")\n"
      "            continue ()\n"
      "         endif ()\n"
      "         get_filename_component (_name \"\${_dep}\" NAME)\n"
      "         set (_dst \"\${MACOS_DIR}/\${_name}\")\n"
      "         if (NOT EXISTS \"\${_dst}\")\n"
      "            execute_process (COMMAND \"\${CMAKE_COMMAND}\" -E copy \"\${_dep}\" \"\${_dst}\")\n"
      "            execute_process (COMMAND install_name_tool -id\n"
      "               \"@executable_path/\${_name}\" \"\${_dst}\")\n"
      "            list (APPEND _dylibs \"\${_dst}\")\n"
      "            list (APPEND _bins \"\${_dst}\")\n"
      "         endif ()\n"
      "         execute_process (COMMAND install_name_tool -change \"\${_dep}\"\n"
      "            \"@executable_path/\${_name}\" \"\${_bin}\")\n"
      "      endforeach ()\n"
      "   endforeach ()\n"
      "endforeach ()\n")
   add_custom_command (TARGET Rubidium POST_BUILD
      COMMAND ${CMAKE_COMMAND}
         -DMACOS_DIR=$<TARGET_FILE_DIR:Rubidium>
         -DRUBIDIUM_BIN=$<TARGET_FILE:Rubidium>
         -P "${_bundle_macos_deps_script}"
   )

   set (_fix_macos_bundle_loads "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/fix-macos-bundle-loads.sh")
   add_custom_command (TARGET Rubidium POST_BUILD
      COMMAND bash "${_fix_macos_bundle_loads}" "$<TARGET_FILE:Rubidium>"
   )

   # App_SDL::LoadFonts resolves SDL_GetBasePath() + "fonts/" — on macOS that is
   # Contents/Resources/fonts/, not Contents/MacOS/fonts/.
   add_custom_command (TARGET Rubidium POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
         "${CMAKE_CURRENT_SOURCE_DIR}/../deps/fonts"
         "$<TARGET_BUNDLE_CONTENT_DIR:Rubidium>/Resources/fonts"
      COMMAND ${CMAKE_COMMAND} -E make_directory
         "$<TARGET_BUNDLE_CONTENT_DIR:Rubidium>/Resources/images"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
         "${CMAKE_CURRENT_SOURCE_DIR}/../pkg/logo.png"
         "$<TARGET_BUNDLE_CONTENT_DIR:Rubidium>/Resources/images/logo.png"
   )

   # Re-sign as the FINAL Rubidium step. The install_name_tool fixups above
   # rewrite load commands on the binary and the bundled dylibs, which
   # invalidates the ad-hoc signature the Apple-Silicon linker applied. dyld
   # SIGKILLs a binary (or any dylib it loads) whose signature no longer
   # matches its contents -- which is why such a build still runs under lldb
   # (the debugger disables that enforcement) yet dies with "zsh: killed" when
   # launched directly.
   #
   # RubidiumSetup's POST_BUILD later copies RubidiumSetup into the bundle and
   # re-signs the whole .app again; both build orderings therefore end with a
   # valid signature. Signing here additionally guarantees that an
   # Rubidium-only build -- the day-to-day `build-macos.sh` command, which does
   # not necessarily rebuild RubidiumSetup -- is launchable on its own.
   add_custom_command (TARGET Rubidium POST_BUILD
      COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:Rubidium>"
   )
endif ()

# ---------------------------------------------------------------------------
# Install targets
# ---------------------------------------------------------------------------

set (_install_targets Rubidium)
if (NOT ANDROID AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
   if (TARGET RubidiumSetup)
      list (APPEND _install_targets RubidiumSetup)
   endif ()
endif ()

if (APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
   install (TARGETS Rubidium BUNDLE DESTINATION .)
   # CPack DragNDrop ships everything under CMAKE_INSTALL_PREFIX. Runtime
   # dylibs must live in Contents/MacOS/ (POST_BUILD + install_name_tool), not
   # a sibling bin/ folder — that layout breaks drag-to-Applications installs.
   install (CODE "
      set (_macos \"\${CMAKE_INSTALL_PREFIX}/Rubidium.app/Contents/MacOS\")
      if (NOT EXISTS \"\${_macos}/Rubidium\")
         message (FATAL_ERROR \"Rubidium.app install incomplete: missing \${_macos}/Rubidium\")
      endif ()
      foreach (_name IN ITEMS libwasmtime.dylib libanari.0.dylib libanari_library_halogen.dylib libMoltenVK.dylib)
         if (NOT EXISTS \"\${_macos}/\${_name}\")
            message (FATAL_ERROR \"Rubidium.app missing runtime dylib \${_name} in \${_macos} (POST_BUILD bundle step did not run?)\")
         endif ()
      endforeach ()
      set (_resources \"\${CMAKE_INSTALL_PREFIX}/Rubidium.app/Contents/Resources\")
      if (NOT EXISTS \"\${_resources}/MoltenVK_icd.json\")
         message (FATAL_ERROR \"Rubidium.app missing MoltenVK_icd.json in Contents/Resources\")
      endif ()
      set (_fonts_src \"${CMAKE_CURRENT_SOURCE_DIR}/../deps/fonts\")
      set (_fonts_dst \"\${CMAKE_INSTALL_PREFIX}/Rubidium.app/Contents/Resources/fonts\")
      if (NOT EXISTS \"\${_fonts_src}/Inter/Inter-Regular.ttf\")
         message (FATAL_ERROR \"install: bundled fonts missing at \${_fonts_src} (run scripts/build-deps.sh --only fonts)\")
      endif ()
      file (MAKE_DIRECTORY \"\${_fonts_dst}\")
      file (COPY \"\${_fonts_src}/\" DESTINATION \"\${_fonts_dst}\" FILES_MATCHING PATTERN \"*.ttf\")
      file (MAKE_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/Rubidium.app/Contents/Resources/images\")
      file (COPY \"${CMAKE_CURRENT_SOURCE_DIR}/../pkg/logo.png\" DESTINATION \"\${CMAKE_INSTALL_PREFIX}/Rubidium.app/Contents/Resources/images\")
      if (NOT EXISTS \"\${_fonts_dst}/Inter/Inter-Regular.ttf\")
         message (FATAL_ERROR \"Rubidium.app missing UI fonts in Contents/Resources/fonts\")
      endif ()
      if (NOT DEFINED ENV{RUBIDIUM_SKIP_INSTALL_ADHOC_CODESIGN})
         execute_process (
            COMMAND codesign --force --deep --sign -
               \"\${CMAKE_INSTALL_PREFIX}/Rubidium.app\"
            RESULT_VARIABLE _sign_rc)
         if (NOT _sign_rc EQUAL 0)
            message (FATAL_ERROR \"codesign failed on installed Rubidium.app (rc=\${_sign_rc})\")
         endif ()
      endif ()
   ")
elseif (CMAKE_SYSTEM_NAME STREQUAL "iOS")
   install (TARGETS Rubidium BUNDLE DESTINATION .)
else ()
   install (TARGETS ${_install_targets} RUNTIME DESTINATION bin)
endif ()

if (WIN32)
   install (FILES
      "${SNEEZE_LIBS_BASE_GEN}/Wasmtime/install/lib/wasmtime.dll"
      "${SNEEZE_LIBS_BASE_GEN}/Halogen/install/bin/anari_library_halogen.dll"
      DESTINATION bin
   )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
   # Linux only — macOS runtime libs live inside Rubidium.app (POST_BUILD), not bin/.
   install (FILES
      "${SNEEZE_LIBS_BASE_GEN}/Wasmtime/install/lib/libwasmtime.so"
      "${SNEEZE_LIBS_BASE_GEN}/ANARI-SDK/install/lib/libanari.so"
      "${SNEEZE_LIBS_BASE_GEN}/Halogen/install/lib/libanari_library_halogen.so"
      DESTINATION bin
   )
   # halogen NEEDs the versioned soname libanari.so.0 (see POST_BUILD note).
   install (FILES
      "${SNEEZE_LIBS_BASE_GEN}/ANARI-SDK/install/lib/libanari.so"
      DESTINATION bin RENAME libanari.so.0
   )
   # Ship the clang/libc++ runtime in the package under its canonical soname
   # (resolved above). RENAME requires one install() per file.
   list (LENGTH RUBIDIUM_CXX_RUNTIME_SONAMES _n_cxxrt)
   if (_n_cxxrt GREATER 0)
      math (EXPR _last_cxxrt "${_n_cxxrt} - 1")
      foreach (_i RANGE ${_last_cxxrt})
         list (GET RUBIDIUM_CXX_RUNTIME_SONAMES ${_i} _soname)
         list (GET RUBIDIUM_CXX_RUNTIME_PATHS   ${_i} _sopath)
         install (FILES "${_sopath}" DESTINATION bin RENAME "${_soname}")
      endforeach ()
   endif ()
endif ()

# Bundle fonts into bin/fonts at install time (desktop: Windows + Linux).
# macOS ships them inside the .app; mobile bundles via platform assets.
# The POST_BUILD copy below populates the dev-run output dir; this install(CODE)
# builds the file list when cpack/install runs (not at Rubidium configure time).
if (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
   set (_RUBIDIUM_FONTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../deps/fonts")
   install (CODE "
      set (_src \"${_RUBIDIUM_FONTS_DIR}\")
      set (_dst \"\${CMAKE_INSTALL_PREFIX}/bin/fonts\")
      if (NOT EXISTS \"\${_src}/Inter/Inter-Regular.ttf\")
         message (FATAL_ERROR \"install: bundled fonts missing at \${_src} (expected Inter/Inter-Regular.ttf). Run build-deps.sh --only fonts (Linux/macOS) or build-windows.ps1 -Only fonts (Windows).\")
      endif ()
      file (MAKE_DIRECTORY \"\${_dst}\")
      file (COPY \"\${_src}/\" DESTINATION \"\${_dst}\" FILES_MATCHING PATTERN \"*.ttf\")
      file (MAKE_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/bin/images\")
      file (COPY \"${CMAKE_CURRENT_SOURCE_DIR}/../pkg/logo.png\" DESTINATION \"\${CMAKE_INSTALL_PREFIX}/bin/images\")
   ")
endif ()

# ---------------------------------------------------------------------------
# CPack - platform-native installer generation
# ---------------------------------------------------------------------------

# Per-config CPack output directory. CPack runs as a separate CMake-script
# invocation and does NOT evaluate generator expressions, so $<CONFIG> won't
# work in CPACK_PACKAGE_DIRECTORY. Instead, emit a CPack project config file
# that translates CPACK_BUILD_CONFIG (set by `cpack -C <cfg>`) into lowercase
# at CPack time and rewrites CPACK_PACKAGE_DIRECTORY accordingly.
# Normalize to forward slashes — backslashes in Windows paths become invalid
# escape sequences when CPack re-parses the generated script.
file (TO_CMAKE_PATH "${RUBIDIUM_BUILD_ROOT}" _cpack_build_root)
file (WRITE "${CMAKE_CURRENT_BINARY_DIR}/CPackProjectConfig.cmake"
"# Auto-generated. Do not edit.\n\
string (TOLOWER \"\${CPACK_BUILD_CONFIG}\" _cfg_lc)\n\
if (NOT _cfg_lc)\n\
   set (_cfg_lc \"release\")\n\
endif ()\n\
set (CPACK_PACKAGE_DIRECTORY \"${_cpack_build_root}/install/\${_cfg_lc}/pkg\")\n")
set (CPACK_PROJECT_CONFIG_FILE "${CMAKE_CURRENT_BINARY_DIR}/CPackProjectConfig.cmake")
set (CPACK_PACKAGE_DIRECTORY "${_cpack_build_root}/install/${RUBIDIUM_CONFIG_LC}/pkg")
set (CPACK_PACKAGE_NAME "${PRODUCT_NAME}")
set (CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set (CPACK_PACKAGE_VENDOR "${PRODUCT_COMPANY}")
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PRODUCT_DESCRIPTION}")
set (CPACK_PACKAGE_INSTALL_DIRECTORY "${PRODUCT_NAME}")

# Use the same platform slug (windows-x64, linux-x64, macos-arm64, ...) that
# the build scripts already use for every other path in the project. Without
# this override, CPack defaults to CPACK_SYSTEM_NAME (e.g. "win64") and the
# package_register target below -- which builds its filename from
# RUBIDIUM_PLATFORM -- would point at a file that doesn't exist.
set (CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-${RUBIDIUM_PLATFORM}")

if (WIN32)
   set (CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/../pkg/LICENSE.txt")
   set (CPACK_CLEAN_STAGING_DIRECTORY OFF)
   set (CPACK_GENERATOR "NSIS")
   set (CPACK_NSIS_PACKAGE_NAME "${PRODUCT_NAME}")
   set (CPACK_NSIS_DISPLAY_NAME "${PRODUCT_NAME}")
   set (CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64\\\\Metaversal")
   set (CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
   set (CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/../pkg/logo.ico")
   set (CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/../pkg/logo.ico")
   set (CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\${PRODUCT_NAME}.exe")

   set (CPACK_NSIS_CREATE_ICONS_EXTRA
      "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\${PRODUCT_NAME}.lnk' '$INSTDIR\\\\bin\\\\${PRODUCT_NAME}.exe'\n\
      CreateShortCut '$DESKTOP\\\\${PRODUCT_NAME}.lnk' '$INSTDIR\\\\bin\\\\${PRODUCT_NAME}.exe'"
   )
   set (CPACK_NSIS_DELETE_ICONS_EXTRA
      "Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\${PRODUCT_NAME}.lnk'\n\
      Delete '$DESKTOP\\\\${PRODUCT_NAME}.lnk'"
   )

   set (CPACK_NSIS_MUI_FINISHPAGE_RUN "${PRODUCT_NAME}.exe")
elseif (APPLE)
   set (CPACK_GENERATOR "DragNDrop")
   set (CPACK_DMG_VOLUME_NAME "${PRODUCT_NAME} ${PROJECT_VERSION}")
   # Do not embed pkg/LICENSE.txt as a DMG SLA (CPack CMP0133 defaults SLA ON when
   # CPACK_RESOURCE_FILE_LICENSE is set). SLA mounting breaks headless CI hdiutil
   # attach and floods logs with the full terms text. NSIS still uses LICENSE.txt.
   set (CPACK_DMG_SLA_USE_RESOURCE_FILE_LICENSE OFF)
   # Symlink /Applications into the DMG so the Finder window gives the user
   # the standard drag-to-install affordance instead of showing only the
   # raw Rubidium.app icon.
   set (CPACK_DMG_DISABLE_APPLICATIONS_SYMLINK OFF)
else ()
   set (CPACK_GENERATOR "TGZ")
   # Root the archive directly at bin/ (no "Rubidium-<ver>-<plat>/" wrapper dir).
   # RubidiumSetup's --apply extracts over the install root, so a wrapper dir would
   # unpack beside bin/ and never replace the binary. ApplyLinuxTarball tolerates
   # both layouts, but keeping the archive flat matches the DMG/NSIS install trees.
   set (CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
endif ()

include (CPack)

# ---------------------------------------------------------------------------
# GenerateManifest - build tool for updating pkg/manifest.json after CPack
# ---------------------------------------------------------------------------

add_executable (GenerateManifest EXCLUDE_FROM_ALL
   "${CMAKE_CURRENT_SOURCE_DIR}/../pkg/generate_manifest.cpp"
)

target_include_directories (GenerateManifest PRIVATE
   $<TARGET_PROPERTY:Sneeze,INTERFACE_INCLUDE_DIRECTORIES>
)

if (WIN32)
   target_link_libraries (GenerateManifest PRIVATE bcrypt)
else ()
   find_package (OpenSSL REQUIRED)
   target_link_libraries (GenerateManifest PRIVATE OpenSSL::Crypto)
endif ()

if (WIN32)
   set (_pkg_filename "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-${RUBIDIUM_PLATFORM}.exe")
elseif (APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
   set (_pkg_filename "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-${RUBIDIUM_PLATFORM}.dmg")
else ()
   set (_pkg_filename "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-${RUBIDIUM_PLATFORM}.tar.gz")
endif ()

add_custom_target (package_register
   COMMAND GenerateManifest register
      "${RUBIDIUM_BUILD_ROOT}/install/$<LOWER_CASE:$<CONFIG>>/pkg/${_pkg_filename}"
      "${PROJECT_VERSION}"
      "${RUBIDIUM_PLATFORM}"
      "${RUBIDIUM_CDN_URL}"
      "${CMAKE_CURRENT_SOURCE_DIR}/../pkg/manifest.json"
      "${CMAKE_CURRENT_SOURCE_DIR}/../RELEASE"
   DEPENDS GenerateManifest
   COMMENT "Registering release in pkg/manifest.json"
   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

add_custom_target (package_promote
   COMMAND GenerateManifest promote
      "${PROJECT_VERSION}"
      "${RUBIDIUM_PLATFORM}"
      "stable"
      "${CMAKE_CURRENT_SOURCE_DIR}/../pkg/manifest.json"
   DEPENDS GenerateManifest
   COMMENT "Promoting ${PROJECT_VERSION} to stable in pkg/manifest.json"
   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
