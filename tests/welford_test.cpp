#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "rpmbb/welford.hpp"
#include <doctest/doctest.h>
using namespace rpmbb;

TEST_CASE("Welford mean variance and std test") {
    welford stats;

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
