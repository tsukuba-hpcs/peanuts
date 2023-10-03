#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "rpmbb/util/welford.hpp"
#include <doctest/doctest.h>

#include <algorithm>
#include <random>
#include <vector>

using namespace rpmbb::util;

TEST_CASE("Welford mean variance and std test") {
  welford<uint64_t> stats;

  // Test empty
  CHECK_THROWS(stats.mean());
  CHECK_THROWS(stats.var());
  CHECK_THROWS(stats.std());

  // Test one element
  stats.add(1);
  CHECK(stats.mean() == 1.0);
  CHECK_THROWS(stats.var());
  CHECK_THROWS(stats.std());

  // Test multiple elements
  stats.add(2);
  CHECK(stats.mean() == 1.5);
  CHECK(stats.var() == 0.5);
  CHECK(stats.std() == std::sqrt(0.5));

  stats.add(3);
  CHECK(stats.mean() == 2.0);
  CHECK(stats.var() == 1.0);
  CHECK(stats.std() == std::sqrt(1.0));
}

TEST_CASE("Test from_range method in welford class") {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> distr(1, 100);

  const int n = 10;  // number of welford instances
  const int i = 100;   // number of samples in each welford instance
  std::vector<welford<uint64_t>> welfords(n);
  welford<uint64_t> overall_welford;

  for (int j = 0; j < n; ++j) {
    for (int k = 0; k < i; ++k) {
      uint64_t random_value = distr(gen);
      welfords[j].add(random_value);
      overall_welford.add(random_value);
    }
  }

  auto combined_welford =
      welford<uint64_t>::from_range(welfords.begin(), welfords.end());

  CHECK(combined_welford.n() == overall_welford.n());
  CHECK(doctest::Approx(combined_welford.mean()) == overall_welford.mean());
  CHECK(doctest::Approx(combined_welford.var()) == overall_welford.var());
  CHECK(doctest::Approx(combined_welford.std()) == overall_welford.std());
}

TEST_CASE("Welford<double>") {
  rpmbb::util::welford<double> wf;
  wf.add(1.0);
  wf.add(2.0);
  wf.add(3.0);

  REQUIRE(wf.n() == 3);
  REQUIRE(wf.mean() == doctest::Approx(2.0));
  REQUIRE(wf.var() == doctest::Approx(1.0));
  REQUIRE(wf.std() == doctest::Approx(1.0));
}
