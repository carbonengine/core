# Copyright © 2025 CCP ehf.

enable_testing()

find_package(GTest CONFIG REQUIRED)
add_executable(CcpCoreTest
    CcpAtomic.cpp
    CCPCallstack.cpp
    CcpCoreTest.cpp
    CCPHash.cpp
    CcpFileUtils.cpp
    CcpMemory.cpp
    CCPMemoryTracker.cpp
    CcpSecureCrt.cpp
    CcpStatistics.cpp
    CcpTelemetry.cpp
    CcpThread.cpp
    CcpTime.cpp
    StringConversions.cpp
    TempFile.cpp
    CCPLog.cpp
    TracyTestClient.cpp
)


# Find Tracy's source directory
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    find_path(TRACY_COMMON_DIR common/TracySocket.cpp
            # Use wildcard for the Tracy version, instead of explicitly setting it to a vX.Y.Z-hash.clean.
            # This however is brittle, as the buildtrees folder may have multiple versions available.
            PATHS "${CMAKE_SOURCE_DIR}/vendor/github.com/microsoft/vcpkg/buildtrees/tracy/src/*/public"
    )

    target_sources(CcpCoreTest PRIVATE
            ${TRACY_COMMON_DIR}/common/TracySocket.cpp
            ${TRACY_COMMON_DIR}/common/tracy_lz4.cpp
    )
endif ()


target_link_libraries(CcpCoreTest PRIVATE CcpCore GTest::gtest GTest::gtest_main)
if(APPLE)
    target_link_libraries(CcpCoreTest PRIVATE "-framework CoreFoundation")
endif()

# CcpCore uses CMAKE_CURRENT_BINARY_DIR as a private include directory which CcpCoreTest requires
target_include_directories(CcpCoreTest PRIVATE $<TARGET_PROPERTY:CcpCore,INCLUDE_DIRECTORIES>)

if(CMAKE_IMPORT_LIBRARY_SUFFIX)
    add_custom_command(TARGET CcpCoreTest POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:CcpCoreTest> $<TARGET_FILE_DIR:CcpCoreTest>
            COMMAND_EXPAND_LISTS
            VERBATIM
    )
endif()

include(GoogleTest)

set_target_properties(CcpCoreTest PROPERTIES FOLDER "Tests")
# Test discovery needs to be delayed to PRE_TEST (https://gitlab.kitware.com/cmake/cmake/-/issues/21845)
gtest_discover_tests(CcpCoreTest DISCOVERY_MODE PRE_TEST)
