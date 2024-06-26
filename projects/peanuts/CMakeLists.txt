cmake_minimum_required(VERSION 3.14...3.22)

# ---- Project ----

project(
  peanuts
  VERSION 1.0.0
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
option(${PROJECT_NAME_UPPERCASE}_USE_DEFERRED_OPEN "Use deferred open" ON)
option(${PROJECT_NAME_UPPERCASE}_USE_AGG_READ "Use aggregate read" ON)
option(${PROJECT_NAME_UPPERCASE}_ENABLE_PROFILER "Enable profiler" OFF)

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
  OPTIONS "FMT_INSTALL YES" "FMT_HEADER_ONLY" # create an installable target
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
find_package(MPI REQUIRED C CXX)
find_package(PkgConfig REQUIRED)
pkg_check_modules(
  Libpmem2
  REQUIRED
  IMPORTED_TARGET
  libpmem2
)
# pkg_check_modules(
#   Liburing
#   REQUIRED
#   IMPORTED_TARGET
#   liburing
# )

# ---- headers ----
# config.h
configure_file(config.hpp.in ${PROJECT_BINARY_DIR}/include/peanuts/config.hpp)
install(FILES ${PROJECT_BINARY_DIR}/include/peanuts/config.hpp
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}-${PROJECT_VERSION}/peanuts"
)

file(
  GLOB_RECURSE
  ${PROJECT_NAME}_headers
  CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/include/*.hpp"
  "${PROJECT_SOURCE_DIR}/include/*.h"
)
add_library(${PROJECT_NAME} INTERFACE ${${PROJECT_NAME}_headers})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_include_directories(
  ${PROJECT_NAME}
  INTERFACE "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
            "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>"
            "$<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>"
)
target_link_libraries(${PROJECT_NAME} INTERFACE MPI::MPI_C PkgConfig::Libpmem2 fmt::fmt-header-only)

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

# generate pkg-config .pc file
configure_file(${PROJECT_NAME}.pc.in ${PROJECT_NAME}.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)

# ---- executables ----
add_subdirectory(src/benchmarks)

# ---- Tests ----
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
