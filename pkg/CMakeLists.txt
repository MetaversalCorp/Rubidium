# Copyright 2026 Metaversal Corporation. All rights reserved.
#
# Standalone CMake project to build GenerateManifest without configuring Rubidium.
# Used by GitHub Actions (.github/workflows/register-manifest.yml).
#
#   cmake -S pkg -B build-manifest
#   cmake --build build-manifest

cmake_minimum_required (VERSION 3.20)
project (ManifestTool LANGUAGES CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

include (FetchContent)
FetchContent_Declare (
   nlohmann_json
   GIT_REPOSITORY https://github.com/nlohmann/json.git
   GIT_TAG v3.11.3
)
FetchContent_MakeAvailable (nlohmann_json)

add_executable (GenerateManifest generate_manifest.cpp)
target_link_libraries (GenerateManifest PRIVATE nlohmann_json::nlohmann_json)

if (WIN32)
   target_link_libraries (GenerateManifest PRIVATE bcrypt)
else ()
   find_package (OpenSSL REQUIRED)
   target_link_libraries (GenerateManifest PRIVATE OpenSSL::Crypto)
endif ()
