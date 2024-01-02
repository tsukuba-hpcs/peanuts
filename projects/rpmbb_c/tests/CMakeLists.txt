enable_testing()

# find_package(doctest REQUIRED)
find_package(Threads REQUIRED)
find_package(MPI REQUIRED C)

macro(build_test testname testfilepath)
  add_executable(${testname} ${CMAKE_CURRENT_LIST_DIR}/${testfilepath})
  target_link_libraries(
    ${testname}
    PRIVATE rpmbb::${PROJECT_NAME}
            MPI::MPI_C
            Threads::Threads
            doctest::doctest
  )
  target_include_directories(${testname} PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>)
  target_compile_features(${testname} PUBLIC cxx_std_20)
  set_target_properties(
    ${testname}
    PROPERTIES CXX_STANDARD 20
               CXX_STANDARD_REQUIRED ON
               CXX_EXTENSIONS OFF
  )
  target_compile_definitions(${testname} PRIVATE -DOMPI_SKIP_MPICXX -DMPICH_SKIP_MPICXX)
endmacro()

file(
  GLOB_RECURSE tests
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  "*_test.cpp"
)
foreach(test ${tests})
  message(STATUS "Test executable is found: ${test}")
  get_filename_component(testname ${test} NAME_WE)
  build_test(${testname} ${test})
  add_test(NAME ${testname} COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${testname})
endforeach(test)

file(
  GLOB_RECURSE mpitests
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  "*_mpitest.cpp"
)
foreach(test ${mpitests})
  message(STATUS "Test executable is found: ${test}")
  get_filename_component(testname ${test} NAME_WE)
  build_test(${testname} ${test})
  add_test(NAME ${testname}_np1 COMMAND ${MPIEXEC_EXECUTABLE} -n 1
                                        ${CMAKE_CURRENT_BINARY_DIR}/${testname}
  )
  add_test(NAME ${testname}_np2 COMMAND ${MPIEXEC_EXECUTABLE} -n 2
                                        ${CMAKE_CURRENT_BINARY_DIR}/${testname}
  )
  add_test(NAME ${testname}_np4 COMMAND ${MPIEXEC_EXECUTABLE} -n 4
                                        ${CMAKE_CURRENT_BINARY_DIR}/${testname}
  )
endforeach(test)
