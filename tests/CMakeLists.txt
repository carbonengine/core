# Copyright © 2025 CCP ehf.

enable_testing()

find_package(GTest CONFIG REQUIRED)
find_package(lz4 CONFIG REQUIRED)
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
target_link_libraries(CcpCoreTest PRIVATE CcpCore GTest::gtest GTest::gtest_main lz4::lz4)
if(WIN32)
    target_link_libraries(CcpCoreTest PRIVATE ws2_32)
endif()
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
