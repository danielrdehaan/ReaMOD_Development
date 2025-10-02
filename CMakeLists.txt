cmake_minimum_required(VERSION 3.12)
project(ReaMOD)

# ---------- Platforms / Architectures ----------
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

# Common flags/defs
if(MSVC)
  target_compile_definitions(reaper_ReaMOD_Plugin PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
  target_compile_options(reaper_ReaMOD_Plugin PRIVATE /W3)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
  target_compile_options(reaper_ReaMOD_Plugin PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------- Platform-specific linking ----------
if(APPLE)
  # macOS: link the .dylibs (keeps your original behavior)
  target_link_libraries(reaper_ReaMOD_Plugin
    "${FMOD_API_DIR}/core/lib/libfmod.dylib"
    "${FMOD_API_DIR}/studio/lib/libfmodstudio.dylib"
  )

  # Relative-only rpaths
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
  # Windows: link against FMOD import libraries (not DLLs)
  # Try plain lib/ first, then lib/x64/ (adjust if your SDK uses different layout)
  set(FMOD_CORE_LIB_CANDIDATES
    "${FMOD_API_DIR}/core/lib/fmod_vc.lib"
    "${FMOD_API_DIR}/core/lib/x64/fmod_vc.lib"
  )
  set(FMOD_STUDIO_LIB_CANDIDATES
    "${FMOD_API_DIR}/studio/lib/fmodstudio_vc.lib"
    "${FMOD_API_DIR}/studio/lib/x64/fmodstudio_vc.lib"
  )

  set(FMOD_CORE_LIB "")
  foreach(p ${FMOD_CORE_LIB_CANDIDATES})
    if(EXISTS "${p}") set(FMOD_CORE_LIB "${p}") break() endif()
  endforeach()

  set(FMOD_STUDIO_LIB "")
  foreach(p ${FMOD_STUDIO_LIB_CANDIDATES})
    if(EXISTS "${p}") set(FMOD_STUDIO_LIB "${p}") break() endif()
  endforeach()

  if(NOT FMOD_CORE_LIB OR NOT FMOD_STUDIO_LIB)
    message(FATAL_ERROR
      "FMOD import libraries not found.\n"
      "Expected one of:\n  ${FMOD_CORE_LIB_CANDIDATES}\n  ${FMOD_STUDIO_LIB_CANDIDATES}\n"
      "Download FMOD for Windows and place fmod_vc.lib and fmodstudio_vc.lib accordingly.")
  endif()

  target_link_libraries(reaper_ReaMOD_Plugin
    "${FMOD_CORE_LIB}"
    "${FMOD_STUDIO_LIB}"
  )

  # Export symbols if you don't already __declspec(dllexport) them
  set_target_properties(reaper_ReaMOD_Plugin PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS ON
    PREFIX ""
    SUFFIX ".dll"
    OUTPUT_NAME "reaper_ReaMOD_Plugin"
  )

  # Ensure /MD (dynamic runtime) in Release configs
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
