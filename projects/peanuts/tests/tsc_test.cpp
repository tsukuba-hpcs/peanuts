#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "peanuts/utils/tsc.hpp"
#include <doctest/doctest.h>
using namespace peanuts;

#include <fmt/chrono.h>
#include <fmt/core.h>

TEST_CASE("tsc always monotonically increasing") {
  auto tsc2 = utils::tsc::get();

  constexpr size_t ntimes = 1000;
  for (size_t i = 0; i < ntimes; ++i) {
    utils::detail::random_cpu_selector cpu_selector{};
    {
      utils::cpu_affinity_manager::set_affinity(cpu_selector());
      auto tsc = tsc2;
      tsc2 = utils::tsc::get();
      CHECK(tsc <= tsc2);
    }
  }
}

TEST_CASE("tsc::to_msec()") {
  using utils::tsc;
  tsc::calibrate();

  std::vector<uint64_t> msec = {1000, 10000, 23042, 123103, 121231};
  for (auto ms : msec) {
    CHECK(tsc::to_msec(ms * tsc::cycles_per_msec()) == doctest::Approx(ms));
  }
}
