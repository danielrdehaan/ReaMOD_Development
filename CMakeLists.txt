cmake_minimum_required(VERSION 3.12)
project(ReaMOD)

# ---------- Architecture (Universal by default) ----------
# If you pass -DCMAKE_OSX_ARCHITECTURES=... on the CLI, this won't override it.
if(NOT DEFINED CMAKE_OSX_ARCHITECTURES)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures" FORCE)
endif()
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS target")

# ---------- C++ standard ----------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------- Paths (relative to project root) ----------
set(REAPER_SDK_DIR        "${CMAKE_SOURCE_DIR}/reaper-sdk")
set(REAPER_REAIMGUI_DIR   "${CMAKE_SOURCE_DIR}/ReaImGui")
set(FMOD_API_DIR          "${CMAKE_SOURCE_DIR}/fmod")
set(TINYFILEDIALOGS_DIR   "${CMAKE_SOURCE_DIR}/tinyfiledialogs")

# ---------- Includes ----------
include_directories(
  ${REAPER_SDK_DIR}
  ${CMAKE_SOURCE_DIR}/reaper-sdk/WDL
  ${REAPER_REAIMGUI_DIR}
  ${FMOD_API_DIR}/core/inc
  ${FMOD_API_DIR}/studio/inc
  ${TINYFILEDIALOGS_DIR}
)

# ---------- Sources ----------
set(SOURCE_FILES
  src/ReaMOD.cpp
  ${TINYFILEDIALOGS_DIR}/tinyfiledialogs.c
)

# ---------- Target ----------
add_library(reaper_ReaMODPlugin SHARED ${SOURCE_FILES})

# Link FMOD by full path (avoid link_directories → no absolute RPATH injection)
target_link_libraries(reaper_ReaMODPlugin
  "${FMOD_API_DIR}/core/lib/libfmod.dylib"
  "${FMOD_API_DIR}/studio/lib/libfmodstudio.dylib"
)

# ---------- RPATH (relative-only; portable) ----------
set(CMAKE_MACOSX_RPATH ON)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)

set_target_properties(reaper_ReaMODPlugin PROPERTIES
  # Use the install rpaths during the build too (prevents absolute build rpaths)
  BUILD_WITH_INSTALL_RPATH TRUE

  # Where dyld should search at runtime (relative to plugin location)
  INSTALL_RPATH "@loader_path;@loader_path/../ReaMOD/fmod/core/lib;@loader_path/../ReaMOD/fmod/studio/lib"

  # Don’t fold link dirs into rpath
  INSTALL_RPATH_USE_LINK_PATH FALSE

  # macOS naming
  PREFIX ""
  SUFFIX ".dylib"
  OUTPUT_NAME "reaper_ReaMOD_Plugin"
)

# Nice compile flags (optional)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(reaper_ReaMODPlugin PRIVATE -Wall -Wextra -Wpedantic)
endif()
