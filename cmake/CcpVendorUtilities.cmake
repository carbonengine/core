# Copyright © 2025 CCP ehf.
# This module implements the necessary parts for our <platform> / <arch> / <compiler> pattern.

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
    #set(CCP_ARCHITECTURE ${CCP_VENDOR_ARCH_PREFIX})
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

set(CCP_VENDOR_LIB_PATH "lib/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/")
set(CCP_VENDOR_BIN_PATH "bin/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/")
