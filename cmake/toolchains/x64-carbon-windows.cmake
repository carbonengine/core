# Copyright © 2025 CCP ehf.
if (NOT _CCP_TOOLCHAIN_FILE_LOADED)
    set(_CCP_TOOLCHAIN_FILE_LOADED 1)

    set (VCPKG_USE_HOST_TOOLS ON CACHE STRING "")
    set (CMAKE_CXX_STANDARD 17 CACHE STRING "")
    set (CMAKE_CXX_STANDARD_REQUIRED ON CACHE STRING "")
    set (CMAKE_CXX_EXTENSIONS OFF CACHE STRING "")
    set (CMAKE_POSITION_INDEPENDENT_CODE ON CACHE STRING "")
    set (CMAKE_CXX_VISIBILITY_PRESET hidden CACHE STRING "")
    set (CMAKE_OBJCXX_VISIBILITY_PRESET hidden CACHE STRING "")
    set (CMAKE_INTERPROCEDURAL_OPTIMIZATION ON CACHE STRING "")

    set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL CACHE STRING INTERNAL FORCE)
    set(CMAKE_SYSTEM_VERSION 10.0.17763.0 CACHE STRING INTERNAL FORCE)
    set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION 10.0.17763.0 CACHE STRING INTERNAL FORCE)

    # Windows 10 is our minimum requirement, so make sure we're enforcing it.
    add_compile_definitions(WINVER=0x0A00)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
    add_compile_definitions(_WIN32_WINDOWS=0x0A00)
    add_compile_definitions(NTDDI_VERSION=0x0A000000)

    #[[
        - `CCP_PLATFORM` indicates the operating system a binary was built for
        - `CCP_ARCHITECTURE` indicates the hardware architecture a binary was built for
        - `CCP_TOOLSET` indicates the compiler (or toolset) a binary was built with
        - `CCP_VENDOR_LIB_PATH` is a convenience variable for find_package modules, to construct the default `lib` folder for a vendored SDK.
        - `CCP_VENDOR_BIN_PATH` is the same as CCP_VENDOR_LIB_PATH, but for the `bin` folder for a vendored SDK.

        See Platform Agnostic Developement section of the wiki:
        https://ccpgames.atlassian.net/wiki/spaces/PAD/overview?homepageId=171868162
    ]]
    set(CCP_PLATFORM Windows CACHE STRING "Target Platform")
    set(CCP_ARCHITECTURE x64 CACHE STRING "Target Architecture")
    set(CCP_TOOLSET v141 CACHE STRING "Target Toolset")

    # https://docs.microsoft.com/en-us/cpp/build/reference/mp-build-with-multiple-processes?view=msvc-150
    add_compile_options(/MP)

    add_compile_options(/W3)
    add_compile_options(/permissive-)
    # Ignore missing PDB file for libraries
    add_link_options(/IGNORE:4099)
    # https://docs.microsoft.com/en-us/cpp/error-messages/tool-errors/linker-tools-warning-lnk4098?view=msvc-150
    add_link_options(/NODEFAULTLIB:libcmt.lib)

    # https://docs.microsoft.com/en-us/cpp/text/support-for-multibyte-character-sets-mbcss?view=msvc-150
    # We don't want this, but we currently can't use /D UNICODE since this breaks all our legacy nonsense (which we should fix)
    # Replace with -D_UNICODE once we have fixed this
    # https://github.com/bluescarni/mppp/issues/177
    add_definitions(-D_SBCS)

    set(MATH_OPTIMIZE_FLAG "/fp:fast")

    # Disable /GL for /ZI support
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF)
endif ()
