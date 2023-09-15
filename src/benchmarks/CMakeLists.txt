macro(build_bench target file)
  add_executable(${target})
  target_sources(${target} PRIVATE ${file})
  target_compile_features(${target} PUBLIC cxx_std_23)
  set_target_properties(
    ${target}
    PROPERTIES CXX_EXTENSIONS OFF
               CXX_STANDARD_REQUIRED ON
               OUTPUT_NAME ${target}
  )
  target_compile_options(
    ${target}
    PRIVATE -march=native
            -Wall
            -Wextra
            -pedantic
  )
  target_link_libraries(
    ${target}
    PRIVATE Threads::Threads
            cxxopts::cxxopts
            fmt::fmt
            nlohmann_json::nlohmann_json
            ${PROJECT_NAME}::${PROJECT_NAME}
    # PkgConfig::Libpmem2
  )
endmacro()

file(
  GLOB_RECURSE bench_files
  RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
  "*.cpp"
)
foreach(file ${bench_files})
  get_filename_component(target ${file} NAME_WE)
  build_bench(${target} ${file})
  install(TARGETS ${target} DESTINATION "${CMAKE_INSTALL_BINDIR}")
endforeach(file)
