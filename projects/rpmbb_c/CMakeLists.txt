cmake_minimum_required(VERSION 3.14...3.22)

project(
  rpmbb_c
  VERSION 0.6.0
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
option(${PROJECT_NAME_UPPERCASE}_BUILD_TESTS "Build testing executables" ON)
option(${PROJECT_NAME_UPPERCASE}_BUILD_SHARED_LIBS "Build librpmbb-c as a shared library" ON)

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

# ---- Library build type ----
set(${PROJECT_NAME_UPPERCASE}_BUILD_TYPE STATIC)
if(${PROJECT_NAME_UPPERCASE}_BUILD_SHARED_LIBS)
  set(${PROJECT_NAME_UPPERCASE}_BUILD_TYPE SHARED)
endif()

# ---- Add dependencies via CPM ----
include(cmake/CPM.cmake)

if(NOT TARGET rpmbb::rpmbb)
  CPMAddPackage(NAME rpmbb SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../rpmbb)
endif()

# ---- Create library ----
file(
  GLOB_RECURSE
  headers
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)
file(
  GLOB_RECURSE
  sources
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(${PROJECT_NAME} ${${PROJECT_NAME_UPPERCASE}_BUILD_TYPE} ${headers} ${sources})
add_library(rpmbb::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES CXX_STANDARD 20
             CXX_STANDARD_REQUIRED ON
             CXX_EXTENSIONS OFF
             VERSION ${PROJECT_VERSION}
             PUBLIC_HEADER "${headers}"
)
target_compile_options(
  ${PROJECT_NAME}
  PRIVATE -march=native
          -Wall
          -Wextra
          -pedantic
)
target_compile_definitions(${PROJECT_NAME} PRIVATE -DOMPI_SKIP_MPICXX -DMPICH_SKIP_MPICXX)

target_link_libraries(${PROJECT_NAME} PRIVATE rpmbb::rpmbb)

target_include_directories(
  ${PROJECT_NAME} PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
                         "$<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>"
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

# generate pkg-config .pc file
configure_file(${PROJECT_NAME}.pc.in ${PROJECT_NAME}.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)

# tests
if(${PROJECT_NAME_UPPERCASE}_BUILD_TESTS AND BUILD_TESTS)
  if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_subdirectory(tests)
  endif()
endif()
