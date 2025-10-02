cmake_minimum_required(VERSION 3.12)
project(ReaMOD)

# ---------- Architecture / Platforms ----------
# macOS universal unless overridden on CLI
if(APPLE)
  if(NOT DEFINED CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures" FORCE)
  endif()
  set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS target")
endif()

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
add_library(reaper_ReaMOD_Plugin SHARED ${SOURCE_FILES})

# Helpful defines across platforms
target_compile_definitions(reaper_ReaMOD_Plugin
  PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS;NOMINMAX>
)

# ---------- Linking (platform-specific) ----------
if(APPLE)
  # Link FMOD by full path to .dylib (keeps mac behavior)
  target_link_libraries(reaper_ReaMOD_Plugin
    "${FMOD_API_DIR}/core/lib/libfmod.dylib"
    "${FMOD_API_DIR}/studio/lib/libfmodstudio.dylib"
  )

  # RPATH (relative-only; portable)
  set(CMAKE_MACOSX_RPATH ON)
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)

  set_target_properties(reaper_ReaMOD_Plugin PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "@loader_path;@loader_path/../ReaMOD/fmod/core/lib;@loader_path/../ReaMOD/fmod/studio/lib"
    INSTALL_RPATH_USE_LINK_PATH FALSE
    PREFIX ""
    SUFFIX ".dylib"
    OUTPUT_NAME "reaper_ReaMOD_Plugin"
  )

elseif(WIN32)
  # On Windows, link against FMOD import libraries (not DLLs).
  # Typical FMOD folder layout has *_vc.lib for MSVC.
  # Adjust names if your FMOD SDK uses different import-lib names.
  set(FMOD_CORE_LIB   "${FMOD_API_DIR}/core/lib/fmod_vc.lib")
  set(FMOD_STUDIO_LIB "${FMOD_API_DIR}/studio/lib/fmodstudio_vc.lib")

  target_link_libraries(reaper_ReaMOD_Plugin
    "${FMOD_CORE_LIB}"
    "${FMOD_STUDIO_LIB}"
  )

  # Export all symbols as a fallback if your code doesn't explicitly dllexport.
  # (Remove this if you already export the REAPER entry points explicitly.)
  set_target_properties(reaper_ReaMOD_Plugin PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS ON
    PREFIX ""
    SUFFIX ".dll"
    OUTPUT_NAME "reaper_ReaMOD_Plugin"
  )

  # Use the dynamic runtime by default (/MD); adjust if you prefer static (/MT).
  if(MSVC)
    foreach(flag_var
      CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_RELWITHDEBINFO
      CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELWITHDEBINFO)
      if(${flag_var} MATCHES "/MT")
        string(REPLACE "/MT" "/MD" ${flag_var} "${${flag_var}}")
        set(${flag_var} "${${flag_var}}" CACHE STRING "" FORCE)
      endif()
    endforeach()
  endif()
endif()

# ---------- Warnings (optional) ----------
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
  target_compile_options(reaper_ReaMOD_Plugin PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
  target_compile_options(reaper_ReaMOD_Plugin PRIVATE /W3)
endif()
