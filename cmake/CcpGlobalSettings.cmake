# Copyright © 2025 CCP ehf.
if(APPLE)
    # Explicitly set the minimum macOS version we target; otherwise it defaults to whatever version
    # we are building on, but we want to stick to our policy of supporting the last three releases.
    set(CMAKE_OSX_DEPLOYMENT_TARGET 10.14 CACHE STRING "The minimum macOS version we target." FORCE)
    message(STATUS "Building for minimum macOS version: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
    message(STATUS "Building for ${CMAKE_OSX_ARCHITECTURES} architecture")
endif()

if(WIN32)
    # Windows 10 is our minimum requirement, so make sure we're enforcing it.
    add_compile_definitions(WINVER=0x0A00)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
    add_compile_definitions(_WIN32_WINDOWS=0x0A00)
    add_compile_definitions(NTDDI_VERSION=0x0A000000)
    set(WIN_SDK_VERSION "10.0.17763.0")
    set(CMAKE_SYSTEM_VERSION ${WIN_SDK_VERSION} CACHE STRING INTERNAL FORCE)
    set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION ${WIN_SDK_VERSION} CACHE STRING INTERNAL FORCE)
    # Avoid using MultiThreadedDebugDLL, which we don't support
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING INTERNAL FORCE)
endif()

# Require out-of-source builds
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" LOC_PATH)
if(EXISTS "${LOC_PATH}")
    message(FATAL_ERROR "You cannot build in a source directory (or any directory with a CMakeLists.txt file). Please make a build subdirectory. Feel free to remove CMakeCache.txt and CMakeFiles.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_OBJCXX_VISIBILITY_PRESET hidden)
set(CMAKE_XCODE_GENERATE_SCHEME ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(cmake/CcpVendorUtilities.cmake)
include(cmake/CcpBuildConfigurations.cmake)

message(STATUS "CCP Platform is ${CCP_PLATFORM}")
message(STATUS "CCP Architecture is ${CCP_ARCHITECTURE}")
message(STATUS "CCP Toolset is ${CCP_TOOLSET}")
