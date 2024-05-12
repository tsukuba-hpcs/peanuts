#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "peanuts/utils/singleton.hpp"

TEST_CASE("Test singleton") {
  peanuts::utils::singleton<int>::init(42);
  CHECK(peanuts::utils::singleton<int>::initialized() == true);
  CHECK(peanuts::utils::singleton<int>::get() == 42);
  peanuts::utils::singleton<int>::fini();
  CHECK(peanuts::utils::singleton<int>::initialized() == false);
}

TEST_CASE("Test singleton_initializer") {
  {
    peanuts::utils::singleton_initializer<peanuts::utils::singleton<int>> initializer(
        42);
    CHECK(initializer.should_finalize() == true);
    CHECK(peanuts::utils::singleton<int>::get() == 42);
  }

  CHECK(peanuts::utils::singleton<int>::initialized() == false);
}
