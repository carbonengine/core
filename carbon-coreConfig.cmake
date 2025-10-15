# Copyright © 2025 CCP ehf.
include(CMakeFindDependencyMacro)

# This file is generated automatically by cmake as part of the install step
include(${CMAKE_CURRENT_LIST_DIR}/carbon-core.cmake)

# Please specify all of this projects transitive dependencies here with calls
# In order for a consuming cmake project system to locate any transitive dependencies of this project, they must be
# specified here in a call to find_dependency(...)
#
# Example:
#
# My project CMakeLists.txt file looks like this:
# ------------------------
# MyProject/CMakeLists.txt
# ------------------------
#
# find_package(a CONFIG NO_CMAKE_PATH REQUIRED)
# find_package(b CONFIG NO_CMAKE_PATH REQUIRED)
# find_package(c CONFIG NO_CMAKE_PATH REQUIRED)
# target_link_libraries(MyProjectTarget PRIVATE package_a PUBLIC package_b INTERFACE package_c)
# . . .
#

# Then the myprojectConfig file (this file) looks like this:
#--------------------------------
# MyProject/myprojectConfig.cmake
# -------------------------------
#
# include(CMakeFindDependencyMacro)
# include(${CMAKE_CURRENT_LIST_DIR}/myproject.cmake)
#
# find_dependency(b CONFIG NO_CMAKE_PATH REQUIRED)
# find_dependency(c CONFIG NO_CMAKE_PATH REQUIRED)
#

find_dependency(Tracy CONFIG REQUIRED)

