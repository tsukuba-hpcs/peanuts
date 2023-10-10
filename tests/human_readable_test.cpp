#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "rpmbb/util/human_readable.hpp"
#include "rpmbb/util/power.hpp"

#include <doctest/doctest.h>

using namespace rpmbb::util;

TEST_CASE("Testing power function") {
  CHECK(power<2, 0>() == 1);
  CHECK(power<2, 1>() == 2);
  CHECK(power<2, 2>() == 4);
  CHECK(power<2, 3>() == 8);
  CHECK(power<2, 4>() == 16);
  CHECK(power<3, 3>() == 27);
}

TEST_CASE("to_string_flexible_decimal") {
  CHECK(to_string_flexible_decimal(123) == "123");
  CHECK(to_string_flexible_decimal<4>(123) == "123");
  CHECK(to_string_flexible_decimal(12345) == "12345");
  CHECK(to_string_flexible_decimal<2>(12345) == "12345");
  CHECK(to_string_flexible_decimal(123.456f) == "123");
  CHECK(to_string_flexible_decimal<4>(0.123456789) == "0.123");
  CHECK(to_string_flexible_decimal(1234.5678f) == "1235");
  CHECK(to_string_flexible_decimal<6>(1234.5678) == "1234.57");
  CHECK(to_string_flexible_decimal(-123) == "-123");
  CHECK(to_string_flexible_decimal<4>(-123) == "-123");
  CHECK(to_string_flexible_decimal(-12345) == "-12345");
  CHECK(to_string_flexible_decimal<2>(-12345) == "-12345");
  CHECK(to_string_flexible_decimal(-123.456f) == "-123");
  CHECK(to_string_flexible_decimal<4>(-0.123456789) == "-0.123");
  CHECK(to_string_flexible_decimal(-1234.5678f) == "-1235");
  CHECK(to_string_flexible_decimal<6>(-1234.5678) == "-1234.57");
  CHECK(to_string_flexible_decimal(2.) == "2.00");
  CHECK(to_string_flexible_decimal(static_cast<uint64_t>(
            10000000000000000000ULL)) == "10000000000000000000");
}

TEST_CASE("Testing to_human function") {
  CHECK(to_human(1024) == "1K");
  CHECK(to_human<1000>(1024) == "1.02K");
  CHECK(to_human<1000>(0.00123) == "1.23m");
  CHECK(to_human(static_cast<uint64_t>(1024 * 1024 * 1024)) == "1G");
  CHECK(to_human(123.456) == "123");
  CHECK(to_human(123.456f) == "123");
  CHECK(to_human(0) == "0");
  CHECK(to_human(500) == "500");
  CHECK(to_human(0.5) == "512m");
  CHECK(to_human<1000>(1000000) == "1M");
}
TEST_CASE("Testing from_human function") {
  CHECK(from_human<int>("1K") == 1024);
  CHECK(from_human<int>("1k") == 1024);
  CHECK(from_human<int, 1000>("1K") == 1000);
  CHECK(from_human<int, 1000>("1k") == 1000);
  CHECK(from_human<float, 1000>("1.23m") == doctest::Approx(0.00123));
  CHECK(from_human<double>("1G") == doctest::Approx(1 << 30));
  CHECK(from_human<short>("1K") == 1024);
  CHECK(from_human<unsigned short>("32K") == (1 << 15));
}
