# Copyright 2026 Metaversal Corporation. All rights reserved.
#
# SDL3 — windowing, input, framebuffer display.
# Android apps need SDL3 as a shared library (.so) for JNI loading.
# All other platforms use static linking.

if (ANDROID)
   set (_sdl3_lib_args -DSDL_SHARED=ON -DSDL_STATIC=OFF)
else ()
   set (_sdl3_lib_args -DSDL_SHARED=OFF -DSDL_STATIC=ON)
endif ()

set (_repo "${RUBIDIUM_DEP_REPO}/SDL3")
if (EXISTS "${_repo}/.git")
   set (_git_args)
else ()
   set (_git_args
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG        release-3.4.2
      GIT_SHALLOW    ON
   )
endif ()

ExternalProject_Add (sdl3
   ${_git_args}
   LIST_SEPARATOR   "|"
   SOURCE_DIR       "${_repo}"
   BINARY_DIR       "${LIBS_DIR}/SDL3/build"
   INSTALL_DIR      "${LIBS_DIR}/SDL3/install"
   CMAKE_ARGS
      -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
      -DCMAKE_BUILD_TYPE=${RUBIDIUM_CONFIG}
      ${_sdl3_lib_args}
      -DSDL_TESTS=OFF
      ${CROSS_COMPILE_ARGS}
   BUILD_COMMAND    ${CMAKE_COMMAND} --build <BINARY_DIR> --config ${RUBIDIUM_CONFIG}
   INSTALL_COMMAND  ${CMAKE_COMMAND} --build <BINARY_DIR> --config ${RUBIDIUM_CONFIG} --target install
)
