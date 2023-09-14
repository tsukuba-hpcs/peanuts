#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "rpmbb/tsc.hpp"
#include <doctest/doctest.h>
using namespace rpmbb;

#include <fmt/chrono.h>
#include <fmt/core.h>

TEST_CASE("tsc always monotonically increasing") {
  auto tsc2 = util::tsc::get();

  constexpr size_t ntimes = 1000;
  for (size_t i = 0; i < ntimes; ++i) {
    util::detail::random_cpu_selector cpu_selector{};
    {
      util::cpu_affinity_manager::set_affinity(cpu_selector());
      auto tsc = tsc2;
      tsc2 = util::tsc::get();
      CHECK(tsc <= tsc2);
    }
  }
}

TEST_CASE("tsc::to_msec()") {
  using util::tsc;
  tsc::calibrate();

  constexpr size_t ntimes = 100;
  auto tsc2 = tsc::get();
  for (size_t i = 0; i < ntimes; ++i) {
    auto tsc1 = tsc2;
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    tsc2 = tsc::get();
    auto msec = tsc::to_msec(tsc2 - tsc1, tsc::cycles_per_msec());
    CHECK(msec == doctest::Approx(3));
  }

  std::vector<uint64_t> msec = {1000, 10000, 23042, 123103, 121231};
  for (auto ms : msec) {
    CHECK(tsc::to_msec(ms * tsc::cycles_per_msec()) == doctest::Approx(ms));
  }
}
