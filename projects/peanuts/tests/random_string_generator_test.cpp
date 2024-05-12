#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "peanuts/utils/gen_random_string.hpp"

#include <doctest/doctest.h>

using namespace peanuts;

TEST_CASE("random_string_generator") {
  using namespace peanuts::utils;

  SUBCASE("Same seed produces same string") {
    random_string_generator gen1(42);
    random_string_generator gen2(42);
    CHECK(gen1.generate(10) == gen2.generate(10));
    CHECK(gen1.generate(5) == gen2.generate(5));
    CHECK(gen1.generate(20) == gen2.generate(20));
  }

  SUBCASE("Different seed produces different string") {
    random_string_generator gen1(42);
    random_string_generator gen2(43);
    CHECK(gen1.generate(10) != gen2.generate(10));
  }

  SUBCASE("Default constructor uses random seed") {
    random_string_generator gen1;
    random_string_generator gen2;
    CHECK(gen1.generate(10) != gen2.generate(10));
  }
}
