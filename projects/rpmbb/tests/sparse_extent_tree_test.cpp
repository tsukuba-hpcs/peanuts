#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest/doctest.h>

#include "rpmbb/inspector.hpp"
#include "rpmbb/sparse_extent.hpp"
#include "rpmbb/sparse_extent_tree.hpp"

using namespace rpmbb;

TEST_CASE("sparse_extent") {
  sparse_extent ex{100, 10, 2, 3};
  MESSAGE(utils::to_string(ex));
  // CHECK(utils::to_string(ex) == "");
}

TEST_CASE("sparse_extent_tree::node") {
  sparse_extent_tree::node n{{100, 10, 2, 3}, 1000, 1};
  MESSAGE(utils::to_string(n));
}
