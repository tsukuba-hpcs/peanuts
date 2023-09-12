#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "rpmbb/extent_tree.hpp"
#include <doctest/doctest.h>

using namespace rpmbb;

TEST_CASE("extent") {
  extent e1(0, 10);
  extent e2(10, 20);
  extent e3(5, 15);

  CHECK(e1 < e2);
  CHECK(!(e1 == e2));
  CHECK((e1 < e3));
  CHECK(!(e3 < e1));
  CHECK(!e1.overlaps(e2));
  CHECK(e1.overlaps(e3));
}

TEST_CASE("extent::get_non_overlapping") {
  extent e1(10, 20);
  extent e2(15, 25);

  if (e1.overlaps(e2)) {
    auto non_overlapping = e1.get_non_overlapping(e2);
    CHECK(non_overlapping.has_value());
    CHECK(non_overlapping->begin == 10);
    CHECK(non_overlapping->end == 15);
  }

  if (e2.overlaps(e1)) {
    auto non_overlapping = e2.get_non_overlapping(e1);
    CHECK(non_overlapping.has_value());
    CHECK(non_overlapping->begin == 20);
    CHECK(non_overlapping->end == 25);
  }

  extent e3(10, 15);
  if (e1.overlaps(e3)) {
    auto non_overlapping = e1.get_non_overlapping(e3);
    CHECK(non_overlapping.has_value());
    CHECK(non_overlapping->begin == 15);
    CHECK(non_overlapping->end == 20);
  }

  if (e3.overlaps(e1)) {
    auto non_overlapping = e3.get_non_overlapping(e1);
    CHECK(!non_overlapping.has_value());
  }

  extent e4(5, 25);
  extent e5(10, 20);
  CHECK(e4.overlaps(e5));
  {
    auto non_overlapping = e4.get_non_overlapping(e5);
    // return front portion of non overlapping extent in e4
    CHECK(non_overlapping.has_value());
    CHECK(non_overlapping->begin == 5);
    CHECK(non_overlapping->end == 10);
  }

  CHECK(e5.overlaps(e4));
  {
    auto non_overlapping = e5.get_non_overlapping(e4);
    // no non overlapping extent in e5 because e5 is fully contained in e4
    CHECK(!non_overlapping.has_value());
  }
}

TEST_CASE("extent_tree::add and find") {
  extent_tree tree;
  tree.add(0, 10, 100, 1);

  auto it = tree.find(0, 10);
  CHECK(it != tree.end());
  CHECK(it->ex.begin == 0);
  CHECK(it->ex.end == 10);
  CHECK(it->ptr == 100);
  CHECK(it->client_id == 1);

  it = tree.find(10, 20);
  CHECK(it == tree.end());

  it = tree.find(5, 7);
  CHECK(it != tree.end());
  CHECK(it->ex.begin == 0);
  CHECK(it->ex.end == 10);
}

TEST_CASE("extent_tree::add") {
  extent_tree tree;

  tree.add(10, 20, 100, 1);  // Insert initial range

  SUBCASE("Add non-overlapping extents") {
    tree.add(30, 40, 130, 2);
    CHECK(tree.size() == 2);
    auto it = tree.find(30, 40);
    CHECK(it != tree.end());
    CHECK(it->ex.begin == 30);
    CHECK(it->ex.end == 40);
  }

  SUBCASE("Add overlapping extents, but not completely encompassed") {
    tree.add(15, 25, 105, 2);
    CHECK(tree.size() == 2);
    auto it = tree.find(10, 15);
    CHECK(it != tree.end());
    CHECK(it->ex.begin == 10);
    CHECK(it->ex.end == 15);

    it = tree.find(15, 25);
    CHECK(it != tree.end());
    CHECK(it->ex.begin == 15);
    CHECK(it->ex.end == 25);

    it = tree.find(25, 30);
    CHECK(it == tree.end());
  }

  SUBCASE("Add overlapping extent completely encompassed") {
    tree.add(12, 18, 102, 2);
    CHECK(tree.size() == 3);

    auto it = tree.begin();
    REQUIRE(it != tree.end());
    CHECK(it->ex.begin == 10);
    CHECK(it->ex.end == 12);
    ++it;
    REQUIRE(it != tree.end());
    CHECK(it->ex.begin == 12);
    CHECK(it->ex.end == 18);
    ++it;
    REQUIRE(it != tree.end());
    CHECK(it->ex.begin == 18);
    CHECK(it->ex.end == 20);
    ++it;
    CHECK(it == tree.end());
  }

  SUBCASE("Add extent that completely encompasses an existing extent") {
    tree.add(5, 25, 95, 2);

    auto it = tree.find(5, 25);
    CHECK(it != tree.end());
    CHECK(it->ex.begin == 5);
    CHECK(it->ex.end == 25);

    it = tree.find(10, 20);
    CHECK(it != tree.end());
    CHECK(it->ex.begin == 5);
    CHECK(it->ex.end == 25);
  }
}

TEST_CASE("extent_tree::add corner cases") {
  extent_tree tree;

  // Setup initial state
  tree.add(10, 20, 100, 1);  // [10, 20)
  tree.add(30, 40, 300, 1);  // [30, 40)

  SUBCASE("Case 1: New extent completely overlaps existing extent.") {
    tree.add(5, 25, 200, 2);  // Should remove [10, 20)
    auto it = tree.find(5, 25);
    CHECK(it != tree.end());
    CHECK(it->ex.begin == 5);
    CHECK(it->ex.end == 25);
  }

  SUBCASE(
      "Case 2: New extent partially overlaps, but the remaining part doesn't "
      "overlap.") {
    tree.add(15, 35, 150,
             2);  // Should split [10, 20) to [10, 15) and remove [30, 40)

    auto it1 = tree.find(10, 15);
    CHECK(it1 != tree.end());
    CHECK(it1->ex.begin == 10);
    CHECK(it1->ex.end == 15);

    auto it2 = tree.find(15, 35);
    CHECK(it2 != tree.end());
    CHECK(it2->ex.begin == 15);
    CHECK(it2->ex.end == 35);
  }
}

using en = rpmbb::extent_tree::node;

TEST_CASE("extent_tree::remove") {
  rpmbb::extent_tree tree;
  tree.add(10, 20, 100, 1);
  tree.add(30, 40, 200, 2);
  tree.add(50, 60, 300, 3);

  SUBCASE("Remove exact match") {
    tree.remove(10, 20);
    REQUIRE(tree.size() == 2);
    auto it = tree.begin();
    CHECK(*it++ == en(30, 40, 200, 2));
    CHECK(*it++ == en(50, 60, 300, 3));
    CHECK(it == tree.end());
  }

  SUBCASE("Remove partial overlap") {
    tree.remove(15, 20);
    REQUIRE(tree.size() == 3);
    auto it = tree.begin();
    CHECK(*it++ == en(10, 15, 100, 1));
    CHECK(*it++ == en(30, 40, 200, 2));
    CHECK(*it++ == en(50, 60, 300, 3));
    CHECK(it == tree.end());
  }

  SUBCASE("Remove encompassing multiple nodes") {
    tree.remove(10, 60);
    REQUIRE(tree.size() == 0);
  }

  SUBCASE("Remove no overlap") {
    tree.remove(90, 100);
    REQUIRE(tree.size() == 3);
    auto it = tree.begin();
    CHECK(*it++ == en(10, 20, 100, 1));
    CHECK(*it++ == en(30, 40, 200, 2));
    CHECK(*it++ == en(50, 60, 300, 3));
    CHECK(it == tree.end());
  }
}
