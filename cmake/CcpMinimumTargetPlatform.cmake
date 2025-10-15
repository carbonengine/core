# Copyright © 2025 CCP ehf.
#[[
Configures key CMake variables to be correct for the operating system ("platforms") we target.

*Note*: On macOS, this module should be included before the first call to `project()` or `enable_language()`.
]]
if(APPLE)
    # Explicitly set the minimum macOS version we target; otherwise it defaults to whatever version
    # we are building on, but we want to stick to our policy of supporting the last three releases.
    set(CMAKE_OSX_DEPLOYMENT_TARGET 10.14 CACHE STRING "The minimum macOS version we target." FORCE)
    message(STATUS "Building for minimum macOS version: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
    message(STATUS "Building for ${CMAKE_OSX_ARCHITECTURES} architecture")
endif()

if(WIN32)
    # Windows 7 is our minimum requirement, so make sure we're enforcing it.
    add_compile_definitions(_WIN32_WINNT=0x0601)
    set(WIN_SDK_VERSION "10.0.17763.0")
    set(CMAKE_SYSTEM_VERSION ${WIN_SDK_VERSION} CACHE STRING INTERNAL FORCE)
    set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION ${WIN_SDK_VERSION} CACHE STRING INTERNAL FORCE)
    # Avoid using MultiThreadedDebugDLL, which we don't support
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING INTERNAL FORCE)
endif()
