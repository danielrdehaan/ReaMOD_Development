cmake_minimum_required(VERSION 3.12)
project(ReaMOD)

# Set C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to Reaper SDK and ReaImGui
set(REAPER_SDK_DIR "${CMAKE_SOURCE_DIR}/reaper-sdk")
set(REAPER_REAIMGUI_DIR "${CMAKE_SOURCE_DIR}/ReaImGui")

# Path to tinyfiledialogs
set(TINYFILEDIALOGS_DIR "${CMAKE_SOURCE_DIR}/tinyfiledialogs")

# Optional path to the FMOD Studio API headers (not required for configuration)
set(FMOD_SDK_DIR "" CACHE PATH "Path to the FMOD Studio API (headers only)")

# Add include directories for Reaper SDK, ReaImGui, and tinyfiledialogs
include_directories(
    ${REAPER_SDK_DIR}
    ${CMAKE_SOURCE_DIR}/reaper-sdk/WDL
    ${REAPER_REAIMGUI_DIR}  # Include ReaImGui directory
    ${TINYFILEDIALOGS_DIR}  # Include tinyfiledialogs directory
)

if(FMOD_SDK_DIR)
    include_directories(
        ${FMOD_SDK_DIR}/core/inc
        ${FMOD_SDK_DIR}/studio/inc
    )
endif()

# Define the source files
set(SOURCE_FILES
    src/ReaMOD.cpp  # Path to your source files
    src/FMODDynLoad.cpp
    ${TINYFILEDIALOGS_DIR}/tinyfiledialogs.c  # Add tinyfiledialogs source file
)

# Add the shared library target for macOS or Linux (.dylib / .so)
add_library(reaper_ReaMODPlugin SHARED ${SOURCE_FILES})

# Mac-specific settings to remove the "lib" prefix and set .dylib suffix
if(APPLE)
    set_target_properties(reaper_ReaMODPlugin PROPERTIES
        PREFIX ""  # This removes the 'lib' prefix
        SUFFIX ".dylib"
        OUTPUT_NAME "reaper_ReaMOD_Plugin"  # Custom output name
    )
endif()