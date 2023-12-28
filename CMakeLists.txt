cmake_minimum_required(VERSION 3.14...3.22)

project(
  rpmbb_build_all
  LANGUAGES CXX C
)

enable_testing()

add_subdirectory(projects/rpmbb)
add_subdirectory(projects/rpmbb_c)
