#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "rpmbb/util/singleton.hpp"

TEST_CASE("Test singleton") {
  rpmbb::util::singleton<int>::init(42);
  CHECK(rpmbb::util::singleton<int>::initialized() == true);
  CHECK(rpmbb::util::singleton<int>::get() == 42);
  rpmbb::util::singleton<int>::fini();
  CHECK(rpmbb::util::singleton<int>::initialized() == false);
}

TEST_CASE("Test singleton_initializer") {
  {
    rpmbb::util::singleton_initializer<rpmbb::util::singleton<int>> initializer(
        42);
    CHECK(initializer.should_finalize() == true);
    CHECK(rpmbb::util::singleton<int>::get() == 42);
  }

  CHECK(rpmbb::util::singleton<int>::initialized() == false);
}
