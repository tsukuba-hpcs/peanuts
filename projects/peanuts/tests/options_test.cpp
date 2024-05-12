#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "peanuts/inspector.hpp"
#include "peanuts/options.hpp"

#include <cstdlib>

using namespace peanuts;

TEST_CASE("option_pmem_path") {
  CHECK(!option_pmem_path::initialized());
  option_pmem_path::init("/test/path");
  CHECK(option_pmem_path::initialized());

  option_pmem_path::set("/new/path");
  CHECK(option_pmem_path::value() == "/new/path");

  std::stringstream ss;
  ss << utils::make_inspector(option_pmem_path::get());
  CHECK(ss.str() == "PMEMBB_PMEM_PATH=/new/path");

  option_pmem_path::fini();
  CHECK(!option_pmem_path::initialized());
}

TEST_CASE("option_pmem_size") {
  CHECK(!option_pmem_size::initialized());
  option_pmem_size::init(123);
  CHECK(option_pmem_size::initialized());

  option_pmem_size::set(456);
  CHECK(option_pmem_size::value() == 456);

  std::stringstream ss;
  ss << utils::make_inspector(option_pmem_size::get());
  CHECK(ss.str() == "PMEMBB_PMEM_SIZE=456");

  option_pmem_size::fini();
  CHECK(!option_pmem_size::initialized());
}

TEST_CASE("option_initializer") {
  SUBCASE("default value") {
    // unset environment variables
    unsetenv("PMEMBB_PMEM_PATH");
    unsetenv("PMEMBB_PMEM_SIZE");

    CHECK(!option_pmem_path::initialized());
    CHECK(!option_pmem_size::initialized());
    option_initializer<option_pmem_path> path_init{};
    option_initializer<option_pmem_size> size_init{};
    CHECK(option_pmem_path::initialized());
    CHECK(option_pmem_size::initialized());

    CHECK(option_pmem_path::value() == "/dev/dax0.0");
    CHECK(option_pmem_size::value() == 0);
  }

  SUBCASE("set environment variables") {
    // set environment variables
    setenv("PMEMBB_PMEM_PATH", "/new/path", 1);
    setenv("PMEMBB_PMEM_SIZE", "456", 1);
    option_initializer<option_pmem_path> path_init{};
    option_initializer<option_pmem_size> size_init{};

    CHECK(option_pmem_path::value() == "/new/path");
    CHECK(option_pmem_size::value() == 456);
  }
}

TEST_CASE("runtime_options storage") {
  runtime_option_initializer opt_init;

  auto& options = runtime_options::get();
  bool has_path = false;
  bool has_size = false;

  for (const auto& option : options) {
    std::visit(
        [&](const auto& opt) {
          using T = std::decay_t<decltype(opt)>;
          if constexpr (std::is_same_v<T, option_pmem_path>) {
            has_path = true;
          } else if constexpr (std::is_same_v<T, option_pmem_size>) {
            has_size = true;
          }
        },
        option);
  }

  CHECK(has_path);
  CHECK(has_size);
}
