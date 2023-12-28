cmake_minimum_required(VERSION 3.14...3.22)

project(
  rpmbb_build_all
  LANGUAGES CXX C
)

include(cmake/CPM.cmake)
CPMAddPackage(NAME rpmbb SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/projects/rpmbb)
CPMAddPackage(NAME rpmbb_c SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/projects/rpmbb_c)
