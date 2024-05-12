#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "peanuts/extent_list.hpp"
#include "peanuts/inspector.hpp"

using namespace peanuts;

TEST_SUITE("extent_list") {
  TEST_CASE("Create empty extent_list") {
    extent_list el;
    CHECK(el.empty());
    CHECK(utils::to_string(el.outer_extent()) == "0-0");
  }

  TEST_CASE("Add and access extents in extent_list") {
    extent_list el;
    el.add({0, 10});
    el.add({20, 30});
    CHECK(utils::to_string(el) == "[0-10)[20-30)");
    CHECK(utils::to_string(el.outer_extent()) == "0-30");
  }

  TEST_CASE("Add overlapping extents and check merge") {
    extent_list el;
    el.add({0, 10});
    el.add({5, 15});
    el.add({15, 20});
    CHECK(std::distance(el.begin(), el.end()) == 1);
    CHECK(utils::to_string(el) == "[0-20)");
    CHECK(utils::to_string(el.outer_extent()) == "0-20");
  }

  TEST_CASE("Clear extent_list") {
    extent_list el;
    el.add({0, 10});
    el.clear();
    CHECK(el.empty());
    CHECK(utils::to_string(el.outer_extent()) == "0-0");
  }

  TEST_CASE("Inverse of an empty extent_list") {
    extent_list el;
    auto inverse_el = el.inverse({0, 10});
    CHECK(utils::to_string(inverse_el) == "[0-10)");
    CHECK(utils::to_string(inverse_el.outer_extent()) == "0-10");
  }

  TEST_CASE("Inverse of extent_list with non-overlapping extent") {
    extent_list el;
    el.add({20, 30});
    auto inverse_el = el.inverse({0, 10});
    CHECK(utils::to_string(inverse_el) == "[0-10)");
    CHECK(utils::to_string(inverse_el.outer_extent()) == "0-10");
  }

  TEST_CASE("Inverse of extent_list with partially overlapping extent") {
    extent_list el;
    el.add({5, 15});
    auto inverse_el = el.inverse({0, 10});
    CHECK(utils::to_string(inverse_el) == "[0-5)");
    CHECK(utils::to_string(inverse_el.outer_extent()) == "0-5");
  }

  TEST_CASE("Inverse of extent_list with fully overlapping extent") {
    extent_list el;
    el.add({0, 10});
    auto inverse_el = el.inverse({0, 10});
    CHECK(inverse_el.empty());
    CHECK(utils::to_string(inverse_el.outer_extent()) == "0-0");
  }

  TEST_CASE("Inverse of extent_list with multiple extents") {
    extent_list el;
    el.add({0, 5});
    el.add({10, 15});
    auto inverse_el = el.inverse({0, 20});
    CHECK(utils::to_string(inverse_el) == "[5-10)[15-20)");
    CHECK(utils::to_string(inverse_el.outer_extent()) == "5-20");
  }

  TEST_CASE(
      "Inverse of extent_list with multiple extents and overlapping "
      "extent") {
    extent_list el{{2, 5}, {6, 9}, {11, 20}, {50, 100}};
    CHECK(utils::to_string(el.inverse({0, 120})) ==
          "[0-2)[5-6)[9-11)[20-50)[100-120)");
    extent_list inverse_el;
    CHECK(utils::to_string(inverse_el = el.inverse({0, 100})) ==
          "[0-2)[5-6)[9-11)[20-50)");
    CHECK(utils::to_string(inverse_el.outer_extent()) == "0-50");
    CHECK(utils::to_string(inverse_el = el.inverse({0, 25})) ==
          "[0-2)[5-6)[9-11)[20-25)");
    CHECK(utils::to_string(inverse_el.outer_extent()) == "0-25");
    CHECK(utils::to_string(inverse_el = el.inverse({8, 15})) == "[9-11)");
    CHECK(utils::to_string(inverse_el.outer_extent()) == "9-11");
    CHECK(utils::to_string(inverse_el = el.inverse({30, 40})) == "[30-40)");
    CHECK(utils::to_string(inverse_el.outer_extent()) == "30-40");
    CHECK(utils::to_string(inverse_el = el.inverse({50, 100})) == "");
    CHECK(utils::to_string(inverse_el.outer_extent()) == "0-0");
    CHECK(utils::to_string(inverse_el = el.inverse({60, 90})) == "");
    CHECK(utils::to_string(inverse_el.outer_extent()) == "0-0");
    CHECK(utils::to_string(inverse_el = el.inverse({2, 5})) == "");
    CHECK(utils::to_string(inverse_el.outer_extent()) == "0-0");
    CHECK(utils::to_string(inverse_el = el.inverse({2, 6})) == "[5-6)");
    CHECK(utils::to_string(inverse_el.outer_extent()) == "5-6");

    extent_list el_empty{};
    CHECK(utils::to_string(inverse_el = el_empty.inverse({0, 100})) ==
          "[0-100)");
    CHECK(utils::to_string(inverse_el.outer_extent()) == "0-100");
  }

  TEST_CASE("intersection(lhs, rhs)") {
    CHECK(intersection({}, {}).empty());
    CHECK(intersection({{0, 10}}, {}).empty());
    CHECK(intersection({{0, 10}}, {{20, 30}}).empty());
    CHECK(utils::to_string(intersection({{0, 15}}, {{10, 20}})) == "[10-15)");

    extent_list el1{{0, 10}, {15, 25}, {30, 40}};
    extent_list el2{{5, 20}, {35, 45}};
    CHECK(utils::to_string(intersection(el1, el2)) == "[5-10)[15-20)[35-40)");
  }
}
