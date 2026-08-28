cmake_minimum_required(VERSION 3.24)

# Build Cauldron (the vendored framework under libs/cauldron) DX12-only so the
# Vulkan SDK is never required just to build this sample.
set(GFX_API "DX12")

project(Surfels_${GFX_API})

if(MSVC_TOOLSET_VERSION VERSION_LESS 142)
    message(FATAL_ERROR "Cannot find MSVC toolset version 142 or greater. Please make sure Visual Studio 2019 or newer is installed")
endif()

# output exe (and copied shaders/runtime dlls) to the bin directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/bin)
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_HOME_DIRECTORY}/bin)
endforeach()

add_compile_options(/MP)

add_subdirectory(libs/cauldron)
add_subdirectory(src/DX12)
