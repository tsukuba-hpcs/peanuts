#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "rpmbb/utils/singleton.hpp"

TEST_CASE("Test singleton") {
  rpmbb::utils::singleton<int>::init(42);
  CHECK(rpmbb::utils::singleton<int>::initialized() == true);
  CHECK(rpmbb::utils::singleton<int>::get() == 42);
  rpmbb::utils::singleton<int>::fini();
  CHECK(rpmbb::utils::singleton<int>::initialized() == false);
}

TEST_CASE("Test singleton_initializer") {
  {
    rpmbb::utils::singleton_initializer<rpmbb::utils::singleton<int>> initializer(
        42);
    CHECK(initializer.should_finalize() == true);
    CHECK(rpmbb::utils::singleton<int>::get() == 42);
  }

  CHECK(rpmbb::utils::singleton<int>::initialized() == false);
}
