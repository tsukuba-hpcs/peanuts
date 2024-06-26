#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "peanuts/utils/enumerate.hpp"

#include <doctest/doctest.h>
#include <utility>
#include <vector>

TEST_CASE("Test enumerate with std::vector") {
  std::vector<int> v = {4, 5, 6};

  size_t index = 0;
  for (auto [i, val] : peanuts::utils::enumerate(v)) {
    CHECK(i == index);
    CHECK(val == v[index]);
    ++index;
  }
}

TEST_CASE("Test cenumerate with std::vector") {
  std::vector<int> v = {7, 8, 9};

  size_t index = 0;
  for (auto [i, val] : peanuts::utils::cenumerate(v)) {
    CHECK(i == index);
    CHECK(val == v[index]);
    ++index;
  }
}
