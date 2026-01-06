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
    set (CMAKE_XCODE_GENERATE_SCHEME ON CACHE STRING "")
    set (CMAKE_INTERPROCEDURAL_OPTIMIZATION ON CACHE STRING "")
    set (CMAKE_OSX_DEPLOYMENT_TARGET 10.14 CACHE STRING "")

    #[[
        - `CCP_PLATFORM` indicates the operating system a binary was built for
        - `CCP_ARCHITECTURE` indicates the hardware architecture a binary was built for
        - `CCP_TOOLSET` indicates the compiler (or toolset) a binary was built with
        - `CCP_VENDOR_LIB_PATH` is a convenience variable for find_package modules, to construct the default `lib` folder for a vendored SDK.
        - `CCP_VENDOR_BIN_PATH` is the same as CCP_VENDOR_LIB_PATH, but for the `bin` folder for a vendored SDK.

        See Platform Agnostic Developement section of the wiki:
        https://ccpgames.atlassian.net/wiki/spaces/PAD/overview?homepageId=171868162
    ]]
    set(CCP_PLATFORM "macOS" CACHE STRING "Target Platform")
    set(CCP_ARCHITECTURE "arm64" CACHE STRING "Target Architecture")
    set(CCP_TOOLSET "AppleClang" CACHE STRING "Target Toolset")

    # adjust warning settings for all our projects, but do not treat them as errors just yet.
    add_compile_options(-Wall)
    # we want to use the two ones below once we're good with -Wall
    #    add_compile_options(-Wpedantic)
    #    add_compile_options(-Wextra)

    # We're using a lot of MSVC specific pragmas in our codebase, so we silence those warnings until we got around to
    # cleaning them up
    add_compile_options(-Wno-unknown-pragmas)
    # There's a surprising amount of unused functions, we need to investigate this deeper at one point
    add_compile_options(-Wno-unused-function)
    # Ditto, much like the functions there are also a lot of unused variables it appears
    add_compile_options(-Wno-unused-variable)
    # We've not been very good at keeping order
    add_compile_options(-Wno-reorder)
    # -Wmissing-braces should only be used by C / ObjectiveC, but for some reason it shows up for our C++ code, too.
    add_compile_options(-Wno-missing-braces)

    # Manually add debug symbols to builds
    add_compile_options(-g)

    set(MATH_OPTIMIZE_FLAG -ffast-math -ffp-model=fast)
endif ()
