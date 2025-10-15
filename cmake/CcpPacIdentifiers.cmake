# Copyright © 2025 CCP ehf.
#[[
This module implements the necessary parts for our <platform> / <arch> / <compiler> pattern.

In details, the following variables are provided:
 - `CCP_PLATFORM` / `CCP_VENDOR_PLATFORM` to indicate the operating system a binary was built for
 - `CCP_ARCHITECTURE` / `CCP_VENDOR_ARCHITECTURE` to indicate the hardware architecture a binary was built for
 - `CCP_TOOLSET` / `CCP_VENDOR_TOOLSET` to indicate the compiler (or toolset) a binary was built with
 - `CCP_VENDOR_LIB_PATH` as a convenience variable for find_package modules, to construct the default `lib` folder for a vendored SDK.

See https://wiki.ccpgames.com/pages/viewpage.action?pageId=127502625 for details on our identifiers.
]]

# ==== Platform detection
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(CCP_VENDOR_PLATFORM "macOS")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(CCP_VENDOR_PLATFORM "Windows")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()
set(CCP_PLATFORM ${CCP_VENDOR_PLATFORM})

# ==== Processor architecture detection
if(${CMAKE_SIZEOF_VOID_P} STREQUAL "8")
    set(CCP_VENDOR_ARCH_PREFIX "x64")
else()
    message(FATAL_ERROR "Unsupported architecture with sizeof(void*) == ${CMAKE_SIZEOF_VOID_P}")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # We use combined x86_64/arm64 binaries on macOS
    set(CCP_ARCHITECTURE "universal")
else()
    set(CCP_ARCHITECTURE ${CCP_VENDOR_ARCH_PREFIX})
endif()

# ==== Toolset detection
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(CCP_VENDOR_TOOLSET ${CMAKE_CXX_COMPILER_ID})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if (NOT MSVC)
        message(FATAL_ERROR "Currently only MSVC is supported on Windows")
    endif()
    if (MSVC_IDE)
        set(CCP_VENDOR_TOOLSET "${CMAKE_VS_PLATFORM_TOOLSET}")
    else()
        set(CCP_VENDOR_TOOLSET "v${MSVC_TOOLSET_VERSION}")
    endif()
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

set(CCP_TOOLSET ${CCP_VENDOR_TOOLSET})

# Convenience variable that can be used to locate the default `lib` folder inside a vendored package
set(CCP_VENDOR_LIB_PATH "lib/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/")
