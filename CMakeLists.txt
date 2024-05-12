cmake_minimum_required(VERSION 3.14...3.22)

project(
  peanuts_build_all
  LANGUAGES CXX C
)

enable_testing()

add_subdirectory(projects/peanuts)
add_subdirectory(projects/peanuts_c)
