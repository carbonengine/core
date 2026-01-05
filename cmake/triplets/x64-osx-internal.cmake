# Copyright © 2025 CCP ehf.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_BUILD_TYPE "release")

set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES x86_64)
set(VCPKG_OSX_DEPLOYMENT_TARGET 10.14)

set(VCPKG_ENV_PASSTHROUGH_UNTRACKED VCPKG_ROOT)

set(CARBON_BUILD_TYPE "Internal")

if (PORT MATCHES "carbon-.*")
    set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../toolchains/x64-osx-triplet.cmake")
    set(VCPKG_HASH_ADDITIONAL_FILES "${CMAKE_CURRENT_LIST_DIR}/../toolchains/x64-carbon-osx.cmake")
endif ()

if (PORT MATCHES "libyaml")
    set(VCPKG_LIBRARY_LINKAGE static)
endif ()

if (PORT MATCHES "curl")
    set(VCPKG_LIBRARY_LINKAGE static)
endif ()

if (PORT MATCHES "openssl")
    set(VCPKG_LIBRARY_LINKAGE static)
endif ()

if (PORT MATCHES "protobuf")
    set(VCPKG_LIBRARY_LINKAGE static)
endif ()

if (PORT MATCHES "zlib")
    set(VCPKG_LIBRARY_LINKAGE static)
endif ()

if (PORT MATCHES "libuv")
    set(VCPKG_CMAKE_CONFIGURE_OPTIONS "-DBUILD_TESTING=OFF")
endif()

if (PORT MATCHES "carbon-pdmprotowrapper")
    set(VCPKG_LIBRARY_LINKAGE static)
endif ()
