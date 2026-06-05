# Copyright © 2025 CCP ehf.

cmake_minimum_required(VERSION 3.19)

project(CcpCore)

set(CCP_TOOLSET ${CMAKE_GENERATOR_TOOLSET})

include(GenerateExportHeader)

include(cmake/CcpTargetConfigurations.cmake)
include(cmake/CcpDocsGenerator.cmake)
include(cmake/CcpPackageConfigHelpers.cmake)
include(cmake/CcpBuildConfigurations.cmake)

option(WITH_TELEMETRY "Enable profiler integration" ON)
option(WITH_MEMORY_TRACKING "Enable tracking of memory allocations." ON)

set(SRC_FILES
        CcpCore.cpp
        CCPAssert.cpp
        CCPCallstack.cpp
        CCPDefines.cpp
        CcpFileUtils.cpp
        CCPHash.cpp
        CCPMemory.cpp
        CCPMemoryTracker.cpp
        CCPMemoryTrackerMutex.h
        CcpProcess.cpp
        CcpSecureCrt.cpp
        CcpSemaphore.cpp
        CcpStatistics.cpp
        CcpTelemetry.cpp
        CcpThread.cpp
        CcpThreadClass.cpp
        CcpTime.cpp
        StringConversions.cpp
        CCPLog.cpp
)

add_library(CcpCore SHARED ${SRC_FILES})

get_target_property(_SOURCES CcpCore SOURCES)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}"
        PREFIX "Sources"
        FILES ${_SOURCES}
)


set_build_flavor(CcpCore)

generate_export_header(CcpCore
    BASE_NAME CARBON_CORE
    EXPORT_FILE_NAME carbon_core_export.h
    EXPORT_MACRO_NAME CARBON_CORE_API
)

set(PUBLIC_HEADER_FILES
        include/CachedAllocator.h
        include/CCPAssert.h
        include/CcpAtomic.h
        include/CCPCallstack.h
        include/CcpCore.h
        include/CcpCrash.h
        include/CcpDefines.h
        include/CcpFileUtils.h
        include/CCPHash.h
        include/CcpMacros.h
        include/CCPMemory.h
        include/CCPMemoryTracker.h
        include/CcpMutex.h
        include/CcpPairingHeap.h
        include/CcpProcess.h
        include/CcpScopeGuard.h
        include/CcpSecureCrt.h
        include/CcpSemaphore.h
        include/CcpStatistics.h
        include/CcpTelemetry.h
        include/CcpThread.h
        include/CcpTime.h
        include/CcpTypes.h
        include/ICcpStream.h
        include/ICrashReporter.h
        include/StringConversions.h
        include/TrackableContainer.h
        include/CCPLog.h
        ${CMAKE_CURRENT_BINARY_DIR}/carbon_core_export.h
)
target_sources(CcpCore PRIVATE ${PUBLIC_HEADER_FILES})

target_precompile_headers(CcpCore PRIVATE StdAfx.h)

if(APPLE)
    set_prefix_and_suffix(CcpCore)
    enable_language(OBJCXX)
    target_sources(CcpCore PRIVATE
        StringConversions.mm
    )
    externalize_apple_debuginfo(CcpCore)
endif()
target_link_libraries(CcpCore PUBLIC $<$<PLATFORM_ID:Darwin>:-framework\ AppKit>)

if(WITH_TELEMETRY)
    message(STATUS "Enabling profiler integration")
    find_package(Tracy CONFIG REQUIRED)
    target_compile_definitions(CcpCore PUBLIC CCP_TELEMETRY_ENABLED=1)
    target_link_libraries(CcpCore PUBLIC Tracy::TracyClient)
    # Tracy's vcpkg package is always built as Release (with NDEBUG). CcpTelemetry.cpp
    # must also be compiled with NDEBUG to match Tracy's struct layout: in 0.13.1 the
    # Profiler class has a debug-only member (std::atomic_bool m_inUse, #ifndef NDEBUG)
    # that shifts m_programNameLock and other trailing fields by 8 bytes, causing
    # SetProgramName() — inlined from TracyProfiler.hpp — to lock the wrong memory
    # address and fail with EINVAL. CCP_ASSERT_ENABLED is set explicitly via CMake so
    # CCP_ASSERT macros are unaffected by this NDEBUG definition.
    set_source_files_properties(CcpTelemetry.cpp PROPERTIES COMPILE_DEFINITIONS NDEBUG)
else()
    target_compile_definitions(CcpCore PUBLIC CCP_TELEMETRY_ENABLED=0)
endif()

if(WITH_MEMORY_TRACKING)
    message(STATUS "Enabling memory tracking")
    target_compile_definitions(CcpCore PUBLIC CCP_MEMORY_DEBUG=1)
else()
    target_compile_definitions(CcpCore PUBLIC CCP_MEMORY_DEBUG=0)
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    option(BUILD_TESTING "Build and run tests. Enabled by default." ON)
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()

target_compile_definitions(CcpCore PRIVATE PLATFORM_TOOLSET=${CCP_TOOLSET})
target_compile_definitions(CcpCore PUBLIC CCP_ASSERT_ENABLED=$<NOT:$<CONFIG:Release>>)

target_include_directories(CcpCore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_BINARY_DIR}
)

# Provide an install target iff this is the top level project only
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    # Determine if/when to generate/build documentation. The rules are as follows:
    # - If running locally - Build documentation is OFF by default
    # - If running on a build agent (TeamCity) - Build documentation is ON by default
    # - If -DBUILD_DOCUMENTATION=ON/OFF flag is explicitly set, use it.
    set(BUILD_DOCUMENTATION_DEFAULT_FLAG OFF)
    if(DEFINED ENV{TEAMCITY_VERSION})
        set(BUILD_DOCUMENTATION_DEFAULT_FLAG ON)
    endif()
    message(STATUS "Document generation settings: default=${BUILD_DOCUMENTATION_DEFAULT_FLAG}, option BUILD_DOCUMENTATION=${BUILD_DOCUMENTATION}")
    option(BUILD_DOCUMENTATION "Build Documentation" ${BUILD_DOCUMENTATION_DEFAULT_FLAG})

    if(BUILD_DOCUMENTATION)
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        include(cmake/CcpDocsGenerator.cmake)

        if(WIN32)
            set(PYTHONPATH_ENV_SPHINX "$<TARGET_FILE_DIR:CcpCore>\;${CMAKE_SOURCE_DIR}/python")
        elseif(APPLE)
            set(PYTHONPATH_ENV_SPHINX "$<TARGET_FILE_DIR:CcpCore>:${CMAKE_SOURCE_DIR}/python")
        endif()

        set(SPHINX_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/doc/source)
        set(SPHINX_BUILD ${CMAKE_CURRENT_BINARY_DIR}/doc/build)

        create_carbon_docs_sphinx_target(
            PYTHON_EXE ${Python3_EXECUTABLE}
            VENV_NAME docs_generation_venv
            PYTHONPATH_ENV ${PYTHONPATH_ENV_SPHINX}
            SPHINX_SOURCE ${SPHINX_SOURCE}
            SPHINX_BUILD ${SPHINX_BUILD}
            DOXYGEN_SRC_FILES ${SRC_FILES}
            SPHINX_TARGET_NAME Sphinx
            DOXYGEN_TARGET_NAME Doxygen
            INSTALL_DESTINATION documentation/carbon-core
        )
    endif()

    option(INSTALL_TO_MONOLITH "Install this package to a perforce branch vendor folder - provided for backwards compatability" OFF)
    if(INSTALL_TO_MONOLITH)

        # Provide a platform dependent interface include directory. ${CCP_VENDOR_PLATFORM} is escaped in order for the
        # generated package configuration to postpone its evaluation until the package is imported.
        set_target_properties(CcpCore PROPERTIES PUBLIC_HEADER "${PUBLIC_HEADER_FILES}" INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/include/\${CCP_VENDOR_PLATFORM}")

        # Install rule to ensure that our runtime and linker files are in the expected, platform-specific folders
        install(
                TARGETS CcpCore
                EXPORT CcpCoreTargets
                CONFIGURATIONS ${CMAKE_CONFIGURATION_TYPES}
                PUBLIC_HEADER DESTINATION include/${CCP_PLATFORM}
                ARCHIVE DESTINATION lib/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
                LIBRARY DESTINATION lib/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
                RUNTIME DESTINATION bin/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
        )

        # Start with an empty list of Core Dependencies, then add to it as needed:
        set(CONFIGURED_CARBON_CORE_DEPENDENCIES "")
        if (WITH_TELEMETRY)
            list(APPEND CONFIGURED_CARBON_CORE_DEPENDENCIES "Tracy")
        endif ()

        configure_ccp_vendor_config_file(
                TARGET CcpCore
                DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/CcpCoreConfig.cmake"
                DEPENDENCIES "${CONFIGURED_CARBON_CORE_DEPENDENCIES}"
                EXPORT_DLL
        )
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/CcpCoreConfig.cmake DESTINATION .)
    else() # VCPKG installation mode
        # Install rule to ensure that our runtime and linker files are in the vcpkg structure
        install(
                TARGETS CcpCore
                EXPORT CcpCoreTargets
                CONFIGURATIONS ${CMAKE_CONFIGURATION_TYPES}
                ARCHIVE DESTINATION lib/
                LIBRARY DESTINATION lib/
                RUNTIME DESTINATION bin/
        )
        # Install rule for available public headers
        install(FILES ${PUBLIC_HEADER_FILES} DESTINATION include)
        install(
                EXPORT CcpCoreTargets
                DESTINATION ${CMAKE_INSTALL_PREFIX}/share/carbon-core
                FILE carbon-core.cmake
        )
        install(
                FILES carbon-coreConfig.cmake
                DESTINATION ${CMAKE_INSTALL_PREFIX}/share/carbon-core
        )
    endif()
endif()