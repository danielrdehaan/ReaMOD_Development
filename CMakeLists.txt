cmake_minimum_required(VERSION 3.12)
project(ReaMOD)

# Set C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to Reaper SDK and ReaImGui
set(REAPER_SDK_DIR "${CMAKE_SOURCE_DIR}/reaper-sdk")
set(REAPER_REAIMGUI_DIR "${CMAKE_SOURCE_DIR}/ReaImGui")

# Path to FMOD API - select the subdirectory for the current platform
if(APPLE)
    set(FMOD_API_DIR "${CMAKE_SOURCE_DIR}/fmod/Mac")
elseif(WIN32)
    set(FMOD_API_DIR "${CMAKE_SOURCE_DIR}/fmod/Windows")
endif()

# Path to tinyfiledialogs
set(TINYFILEDIALOGS_DIR "${CMAKE_SOURCE_DIR}/tinyfiledialogs")

# Add include directories for Reaper SDK, ReaImGui, FMOD, and tinyfiledialogs
include_directories(
    ${REAPER_SDK_DIR}
    ${FMOD_API_DIR}/core/inc
    ${FMOD_API_DIR}/studio/inc
    ${CMAKE_SOURCE_DIR}/reaper-sdk/WDL
    ${REAPER_REAIMGUI_DIR}  # Include ReaImGui directory
    ${TINYFILEDIALOGS_DIR}  # Include tinyfiledialogs directory
)

# Link FMOD libraries
link_directories(
    ${FMOD_API_DIR}/core/lib
    ${FMOD_API_DIR}/studio/lib
)

# Define the source files
set(SOURCE_FILES
    src/ReaMOD.cpp  # Path to your source files
    ${TINYFILEDIALOGS_DIR}/tinyfiledialogs.c  # Add tinyfiledialogs source file
)

# Add the shared library target for macOS or Linux (.dylib / .so)
add_library(reaper_ReaMODPlugin SHARED ${SOURCE_FILES})

# Link FMOD libraries to the correct target (reaper_ReaMODPlugin)
if(WIN32)
    target_link_libraries(reaper_ReaMODPlugin
        fmod_vc
        fmodstudio_vc
    )
else()
    target_link_libraries(reaper_ReaMODPlugin
        fmod
        fmodstudio
    )
endif()

# Mac-specific settings to remove the "lib" prefix, set .dylib suffix,
# and search for FMOD libraries relative to the plugin location
if(APPLE)
    set_target_properties(reaper_ReaMODPlugin PROPERTIES
        PREFIX ""  # This removes the 'lib' prefix
        SUFFIX ".dylib"
        OUTPUT_NAME "reaper_ReaMOD_Plugin"  # Custom output name
        MACOSX_RPATH ON
        BUILD_RPATH "@loader_path"
        INSTALL_RPATH "@loader_path"
    )
endif()