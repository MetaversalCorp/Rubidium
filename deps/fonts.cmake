# Copyright 2026 Metaversal Corporation. All rights reserved.
#
# Downloads bundled font families into deps/fonts/. No build step — just
# fetch and extract. Stamp-cached: skips download if target files exist.

set (_FONTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/fonts")

# --- Inter (SIL OFL 1.1) ---------------------------------------------------

set (_INTER_VERSION  "4.1")
set (_INTER_URL      "https://github.com/rsms/inter/releases/download/v${_INTER_VERSION}/Inter-${_INTER_VERSION}.zip")
set (_INTER_DIR      "${_FONTS_DIR}/Inter")
set (_INTER_STAMP    "${_INTER_DIR}/Inter-Regular.ttf")

if (NOT EXISTS "${_INTER_STAMP}")
   message (STATUS "Downloading Inter ${_INTER_VERSION}...")
   set (_INTER_ZIP "${CMAKE_CURRENT_BINARY_DIR}/Inter-${_INTER_VERSION}.zip")
   file (DOWNLOAD "${_INTER_URL}" "${_INTER_ZIP}" STATUS _dl_status)
   list (GET _dl_status 0 _dl_code)
   if (NOT _dl_code EQUAL 0)
      message (FATAL_ERROR "Failed to download Inter: ${_dl_status}")
   endif ()

   file (ARCHIVE_EXTRACT INPUT "${_INTER_ZIP}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/inter-extract")
   file (MAKE_DIRECTORY "${_INTER_DIR}")

   set (_INTER_WEIGHTS
      Inter-Regular Inter-Italic
      Inter-Bold Inter-BoldItalic
      Inter-Medium Inter-MediumItalic
      Inter-SemiBold Inter-SemiBoldItalic
      Inter-Light Inter-LightItalic)

   foreach (_w ${_INTER_WEIGHTS})
      file (GLOB_RECURSE _found "${CMAKE_CURRENT_BINARY_DIR}/inter-extract/${_w}.ttf")
      if (_found)
         list (GET _found 0 _src)
         file (COPY "${_src}" DESTINATION "${_INTER_DIR}")
      endif ()
   endforeach ()

   file (REMOVE "${_INTER_ZIP}")
   file (REMOVE_RECURSE "${CMAKE_CURRENT_BINARY_DIR}/inter-extract")
   message (STATUS "Inter ${_INTER_VERSION} installed to ${_INTER_DIR}")
else ()
   message (STATUS "Inter — already present, skipping download")
endif ()

# --- JetBrains Mono (SIL OFL 1.1) ------------------------------------------

set (_JBM_VERSION "2.304")
set (_JBM_URL     "https://github.com/JetBrains/JetBrainsMono/releases/download/v${_JBM_VERSION}/JetBrainsMono-${_JBM_VERSION}.zip")
set (_JBM_DIR     "${_FONTS_DIR}/JetBrainsMono")
set (_JBM_STAMP   "${_JBM_DIR}/JetBrainsMono-Regular.ttf")

if (NOT EXISTS "${_JBM_STAMP}")
   message (STATUS "Downloading JetBrains Mono ${_JBM_VERSION}...")
   set (_JBM_ZIP "${CMAKE_CURRENT_BINARY_DIR}/JetBrainsMono-${_JBM_VERSION}.zip")
   file (DOWNLOAD "${_JBM_URL}" "${_JBM_ZIP}" STATUS _dl_status)
   list (GET _dl_status 0 _dl_code)
   if (NOT _dl_code EQUAL 0)
      message (FATAL_ERROR "Failed to download JetBrains Mono: ${_dl_status}")
   endif ()

   file (ARCHIVE_EXTRACT INPUT "${_JBM_ZIP}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/jbm-extract")
   file (MAKE_DIRECTORY "${_JBM_DIR}")

   set (_JBM_WEIGHTS JetBrainsMono-Regular JetBrainsMono-Bold JetBrainsMono-Italic)

   foreach (_w ${_JBM_WEIGHTS})
      file (GLOB_RECURSE _found "${CMAKE_CURRENT_BINARY_DIR}/jbm-extract/${_w}.ttf")
      if (_found)
         list (GET _found 0 _src)
         file (COPY "${_src}" DESTINATION "${_JBM_DIR}")
      endif ()
   endforeach ()

   file (REMOVE "${_JBM_ZIP}")
   file (REMOVE_RECURSE "${CMAKE_CURRENT_BINARY_DIR}/jbm-extract")
   message (STATUS "JetBrains Mono ${_JBM_VERSION} installed to ${_JBM_DIR}")
else ()
   message (STATUS "JetBrains Mono — already present, skipping download")
endif ()

# --- Material Symbols Outlined (Apache 2.0) ---------------------------------

set (_MSO_URL   "https://github.com/google/material-design-icons/raw/master/variablefont/MaterialSymbolsOutlined%5BFILL%2CGRAD%2Copsz%2Cwght%5D.ttf")
set (_MSO_DIR   "${_FONTS_DIR}/MaterialSymbolsOutlined")
set (_MSO_STAMP "${_MSO_DIR}/MaterialSymbolsOutlined.ttf")

if (NOT EXISTS "${_MSO_STAMP}")
   message (STATUS "Downloading Material Symbols Outlined...")
   file (MAKE_DIRECTORY "${_MSO_DIR}")
   file (DOWNLOAD "${_MSO_URL}" "${_MSO_STAMP}" STATUS _dl_status)
   list (GET _dl_status 0 _dl_code)
   if (NOT _dl_code EQUAL 0)
      file (REMOVE "${_MSO_STAMP}")
      message (FATAL_ERROR "Failed to download Material Symbols: ${_dl_status}")
   endif ()
   message (STATUS "Material Symbols Outlined installed to ${_MSO_DIR}")
else ()
   message (STATUS "Material Symbols Outlined — already present, skipping download")
endif ()

# No-op build target — all work happens at configure time via file(DOWNLOAD).
add_custom_target (fonts)
