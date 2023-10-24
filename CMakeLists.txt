cmake_minimum_required(VERSION 3.14...3.22)

# ---- Project ----

# Note: update this to your new project's name and version
project(
  rpmbb
  VERSION 0.1.0
  LANGUAGES CXX C
)
string(TOLOWER ${PROJECT_NAME} PROJECT_NAME_LOWERCASE)
string(TOUPPER ${PROJECT_NAME} PROJECT_NAME_UPPERCASE)

# ---- Include guards ----
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

# ---- Options ----
option(BUILD_TESTS "Build testing executables" ON)

# ---- Set default build type ----
# Encourage user to specify a build type (e.g. Release, Debug, etc.), otherwise set it to Release.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'RelWithDebInfo' as none was specified.")
  set(CMAKE_BUILD_TYPE
      "RelWithDebInfo"
      CACHE STRING "Choose the type of build." FORCE
  )
  # Set the possible values of build type for cmake-gui
  set_property(
    CACHE CMAKE_BUILD_TYPE
    PROPERTY STRINGS
             "Debug"
             "Release"
             "MinSizeRel"
             "RelWithDebInfo"
  )
endif()

# ---- Add dependencies via CPM ----
# see https://github.com/TheLartians/CPM.cmake for more info

include(cmake/CPM.cmake)

# PackageProject.cmake will be used to make our target installable
CPMAddPackage("gh:TheLartians/PackageProject.cmake@1.11.0")

CPMAddPackage(
  NAME fmt
  GIT_TAG 10.0.0
  GITHUB_REPOSITORY fmtlib/fmt
  OPTIONS "FMT_INSTALL YES" # create an installable target
  EXCLUDE_FROM_ALL YES
)

CPMAddPackage(
  GITHUB_REPOSITORY jarro2783/cxxopts
  VERSION 3.1.1
  OPTIONS "CXXOPTS_BUILD_EXAMPLES NO" "CXXOPTS_BUILD_TESTS NO" "CXXOPTS_ENABLE_INSTALL YES"
  EXCLUDE_FROM_ALL YES
)

CPMAddPackage(
  NAME nlohmann_json
  VERSION 3.11.2
  GITHUB_REPOSITORY nlohmann/json
  OPTIONS "JSON_BuildTests OFF"
  EXCLUDE_FROM_ALL YES
)

CPMAddPackage("gh:onqtam/doctest@2.4.11")

# ---- Other dependencies ----
find_package(Threads REQUIRED)
find_package(fmt REQUIRED)
find_package(cxxopts REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(MPI REQUIRED CXX)
find_package(PkgConfig REQUIRED)
pkg_check_modules(
  Libpmem2
  REQUIRED
  IMPORTED_TARGET
  libpmem2
)
pkg_check_modules(
  Liburing
  REQUIRED
  IMPORTED_TARGET
  liburing
)

# ---- headers ----
file(
  GLOB_RECURSE
  headers
  CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/include/*.hpp"
)
add_library(${PROJECT_NAME} INTERFACE ${headers})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

# target_link_libraries(${PROJECT_NAME} INTERFACE fmt::fmt)
target_include_directories(
  ${PROJECT_NAME} INTERFACE "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
                            "$<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>"
)
target_link_libraries(
  ${PROJECT_NAME} INTERFACE MPI::MPI_CXX PkgConfig::Libpmem2 PkgConfig::Liburing
)

include(cmake/version.cmake)
target_add_version_header(
  ${PROJECT_NAME}
  VERSION_HEADER ${PROJECT_NAME}/version.h
  INSTALL include/${PROJECT_NAME}-${PROJECT_VERSION}
)

# ---- Install ----
# ~~~
packageProject(
  NAME ${PROJECT_NAME}
  VERSION ${PROJECT_VERSION}
  # NAMESPACE ${PROJECT_NAME}
  BINARY_DIR ${PROJECT_BINARY_DIR}
  INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include
  INCLUDE_DESTINATION include/${PROJECT_NAME}-${PROJECT_VERSION}
  # VERSION_HEADER "${PROJECT_NAME}/version.h"
  COMPATIBILITY SameMajorVersion
  DISABLE_VERSION_SUFFIX YES
  CPACK YES
)

# ---- executables ----
add_subdirectory(src)

# ---- Tests ----
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
