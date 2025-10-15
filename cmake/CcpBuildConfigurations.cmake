# Copyright © 2025 CCP ehf.
#[[
Defines our supported build configurations, also known as build flavors.
]]
# this applies to multi-configuration build system, e.g. XCode, Visual Studio, Ninja Multi-Config
set(CMAKE_CONFIGURATION_TYPES Debug TrinityDev Internal Release)

get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(is_multi_config)
    set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}"
        CACHE
        STRING
        "Reset the configurations to what we need"
        FORCE
        )
else()
    if (NOT CMAKE_BUILD_TYPE)
        message(STATUS "No CMAKE_BUILD_TYPE was specified, defaulting to 'Debug'")
        set(CMAKE_BUILD_TYPE "Debug")
    endif()
    # Update the documentation string of CMAKE_BUILD_TYPE for GUIs
    set(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}"
        CACHE
        STRING
        "Choose the type of build, options are: ${CMAKE_CONFIGURATION_TYPES}."
        FORCE
        )

    # check if a valid build type was supplied
    if (CMAKE_BUILD_TYPE IN_LIST CMAKE_CONFIGURATION_TYPES)
        message(STATUS "Building configuration ${CMAKE_BUILD_TYPE}")
    else()
        message(FATAL_ERROR "Invalid configuration ${CMAKE_BUILD_TYPE}. Available options are: ${CMAKE_CONFIGURATION_TYPES}")
    endif()
endif()

# Create a new configuration based on an existing one
function(create_new_build_config config prototype)
    message(STATUS "Creating new build config '${config}' based on '${prototype}'")

    string(TOUPPER ${config} CONFIG)
    string(TOUPPER ${prototype} PROTOTYPE)

    set(CMAKE_CXX_FLAGS_${CONFIG}
        ${CMAKE_CXX_FLAGS_${PROTOTYPE}} CACHE STRING
        "Flags used by the C++ compiler during ${config} builds."
        FORCE)
    set(CMAKE_C_FLAGS_${CONFIG}
        ${CMAKE_C_FLAGS_${PROTOTYPE}} CACHE STRING
        "Flags used by the C compiler during ${config} builds."
        FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_${CONFIG}
        ${CMAKE_EXE_LINKER_FLAGS_${PROTOTYPE}} CACHE STRING
        "Flags used for linking binaries during ${config} builds."
        FORCE)
    set(CMAKE_SHARED_LINKER_FLAGS_${CONFIG}
        ${CMAKE_SHARED_LINKER_FLAGS_${PROTOTYPE}} CACHE STRING
        "Flags used by the shared libraries linker during ${config} builds."
        FORCE)
    mark_as_advanced(
        CMAKE_CXX_FLAGS_${CONFIG}
        CMAKE_C_FLAGS_${CONFIG}
        CMAKE_EXE_LINKER_FLAGS_${CONFIG}
        CMAKE_SHARED_LINKER_FLAGS_${CONFIG})
endfunction()

create_new_build_config(Internal Release)
create_new_build_config(TrinityDev Internal)

function(add_trinity_dev_debug_flags target)
    if (MSVC)
        # Set debug options
        get_target_property(options ${target} COMPILE_OPTIONS)
        string(REGEX REPLACE "<CONFIG:TrinityDev>,/Zi,>" "<CONFIG:TrinityDev>,/ZI,>" options "${options}")
        string(REGEX REPLACE "<CONFIG:TrinityDev>,/O2,>" "<CONFIG:TrinityDev>,/Od,>" options "${options}")
        set_target_properties(${target} PROPERTIES COMPILE_OPTIONS "${options}")
        # Disable /GL and /LTCG for /ZI support
        set_target_properties(${target} PROPERTIES INTERPROCEDURAL_OPTIMIZATION_TRINITYDEV OFF)
        target_link_options(${target} PRIVATE "$<IF:$<CONFIG:TrinityDev>,/LTCG:OFF,>")
    elseif(APPLE)
        target_compile_options(${target} PRIVATE "$<IF:$<CONFIG:TrinityDev>,-Og,>")
    endif()
endfunction()

if (MSVC)
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

    # https://docs.microsoft.com/en-us/cpp/build/reference/z7-zi-zi-debug-information-format?view=msvc-150
    add_compile_options($<IF:$<OR:$<CONFIG:Release>,$<CONFIG:Internal>>,/Zi,>)
    # Generate Debug Info
    add_link_options($<IF:$<CONFIG:Release>,/DEBUG:FULL,/DEBUG:FASTLINK>)

    # Take manual control over these flags: https://gitlab.kitware.com/cmake/cmake/-/issues/19084
    set(CMAKE_CXX_FLAGS_DEBUG "")
    set(CMAKE_CXX_FLAGS_TRINITYDEV "")
    set(CMAKE_C_FLAGS_TRINITYDEV "")
    set(CMAKE_SHARED_LINKER_FLAGS_TRINITYDEV "")
    set(CMAKE_EXE_LINKER_FLAGS_TRINITYDEV "")

    add_compile_options($<IF:$<CONFIG:Debug>,/ZI,>)
    add_compile_options($<IF:$<CONFIG:Debug>,/Od,>)
    # Disable /GL for /ZI support
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF)

    # Default flags which we can override
    add_compile_options($<IF:$<CONFIG:TrinityDev>,/Zi,>)
    add_compile_options($<IF:$<CONFIG:TrinityDev>,/O2,>)

    set(MATH_OPTIMIZE_FLAG "/fp:fast")
elseif(APPLE)

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
endif()
