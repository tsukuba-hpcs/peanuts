#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "peanuts/extent_tree.hpp"
#include <doctest/doctest.h>
#include "peanuts/inspector.hpp"

using namespace peanuts;

TEST_CASE("Test empty extent_tree") {
  extent_tree tree;
  CHECK(utils::to_string(tree) == "");
}

TEST_CASE("Test single extent") {
  extent_tree tree;
  tree.add(0, 10, 100, 1);
  CHECK(utils::to_string(tree) == "[0-10:100:1]");
}

TEST_CASE("Test multiple extents no overlap") {
  extent_tree tree;
  tree.add(0, 10, 100, 1);
  tree.add(10, 20, 110, 2);
  CHECK(utils::to_string(tree) == "[0-10:100:1][10-20:110:2]");
}

TEST_CASE("Test overlapping extents with same client") {
  extent_tree tree;
  tree.add(0, 10, 100, 1);
  tree.add(5, 15, 105, 1);
  CHECK(utils::to_string(tree) == "[0-15:100:1]");
}

TEST_CASE("Test overlapping extents with different clients") {
  extent_tree tree;
  tree.add(0, 10, 100, 1);
  tree.add(5, 15, 105, 2);
  CHECK(utils::to_string(tree) == "[0-5:100:1][5-15:105:2]");
}

TEST_CASE("Test ptr update when begin changes") {
  extent_tree tree;
  tree.add(10, 20, 100, 1);
  tree.add(5, 15, 200, 1);
  CHECK(utils::to_string(tree) == "[5-15:200:1][15-20:105:1]");
}

TEST_CASE("Test removing extent") {
  extent_tree tree;
  tree.add(0, 10, 100, 1);
  tree.add(10, 20, 110, 2);
  tree.remove(5, 15);
  CHECK(utils::to_string(tree) == "[0-5:100:1][15-20:115:2]");
}

TEST_CASE("Test removing multiple extents") {
  extent_tree tree;
  tree.add(0, 10, 100, 1);
  tree.add(10, 20, 110, 2);
  tree.add(20, 30, 120, 3);
  tree.remove(5, 25);
  CHECK(utils::to_string(tree) == "[0-5:100:1][25-30:125:3]");
}

TEST_CASE("Test find") {
  extent_tree tree;
  tree.add(10, 20, 100, 1);
  tree.add(30, 40, 200, 1);
  tree.add(50, 60, 300, 2);

  auto it = tree.find(15, 35);
  REQUIRE(it != tree.end());
  CHECK(utils::to_string(*it) == "[10-20:100:1]");
}

TEST_CASE("Test iterator ordering and overwrite") {
  extent_tree tree;
  tree.add(50, 60, 300, 2);
  tree.add(30, 40, 200, 1);
  tree.add(10, 20, 100, 1);
  tree.add(20, 35, 105, 1);  // Overwrite

  std::string expected = "[10-20:100:1][20-35:105:1][35-40:205:1][50-60:300:2]";
  std::string result;

  for (auto it = tree.begin(); it != tree.end(); ++it) {
    result += utils::to_string(*it);
  }

  CHECK(result == expected);
}

TEST_CASE("Test long scenario imported from UnifyFS test code") {
  extent_tree tree;

  SUBCASE("add") {
    // initial state
    tree.add(5, 11, 0, 0);
    CHECK(utils::to_string(tree) == "[5-11:0:0]");

    // non-overlapping insert
    tree.add(100, 151, 100, 0);
    CHECK(utils::to_string(tree) == "[5-11:0:0][100-151:100:0]");

    // Add range overlapping part of the left size
    tree.add(2, 8, 200, 0);
    CHECK(utils::to_string(tree) == "[2-8:200:0][8-11:3:0][100-151:100:0]");

    // Add range overlapping part of the right size
    tree.add(9, 13, 300, 0);
    CHECK(utils::to_string(tree) ==
          "[2-8:200:0][8-9:3:0][9-13:300:0][100-151:100:0]");

    // Add range totally within existing range
    tree.add(3, 5, 400, 0);
    CHECK(utils::to_string(tree) ==
          "[2-3:200:0][3-5:400:0][5-8:203:0][8-9:3:0][9-13:300:0][100-151:100:"
          "0]");

    // test counts
    CHECK(tree.size() == 6);
    CHECK((--tree.end())->ex.end == 151);

    // add a range that blows away multiple ranges, and overlaps
    tree.add(4, 121, 500, 0);
    CHECK(utils::to_string(tree) ==
          "[2-3:200:0][3-4:400:0][4-121:500:0][121-151:121:0]");

    // test counts
    CHECK(tree.size() == 4);
    CHECK((--tree.end())->ex.end == 151);

    // clear
    tree.clear();
    CHECK(utils::to_string(tree) == "");
    CHECK(tree.size() == 0);
  }

  SUBCASE("add and find") {
    // Now let's write a long extent, and then sawtooth over it with 1 byte
    // extents.
    tree.add(0, 51, 50, 0);
    tree.add(0, 1, 0, 0);
    tree.add(2, 3, 2, 0);
    tree.add(4, 5, 4, 0);
    tree.add(6, 7, 6, 0);
    CHECK(utils::to_string(tree) ==
          "[0-1:0:0][1-2:51:0][2-3:2:0][3-4:53:0]"
          "[4-5:4:0][5-6:55:0][6-7:6:0][7-51:57:0]");
    CHECK(tree.size() == 8);
    CHECK((--tree.end())->ex.end == 51);

    // test find()
    // Find between a range that multiple segments.
    // It should return the first one.
    auto it = tree.find(2, 8);
    CHECK(utils::to_string(*it) == "[2-3:2:0]");

    // Test finding a segment that partially overlaps our range
    tree.add(100, 201, 100, 0);
    it = tree.find(90, 121);
    CHECK(utils::to_string(*it) == "[100-201:100:0]");

    // Look for a range that doesn't exist.  Should return end()
    it = tree.find(2000, 3001);
    CHECK(it == tree.end());
  }

  SUBCASE("add same extent but different ptr") {
    // Write a range, then completely overwrite it with the
    // same range.  Use a different buf value to verify it changed.
    tree.add(20, 31, 0, 0);
    CHECK(utils::to_string(tree) == "[20-31:0:0]");
    tree.add(20, 31, 8, 0);
    CHECK(utils::to_string(tree) == "[20-31:8:0]");
  }

  SUBCASE("Test coalescing") {
    // initial state
    tree.add(5, 11, 105, 0);
    CHECK(utils::to_string(tree) == "[5-11:105:0]");

    // non-overlapping insert
    tree.add(100, 151, 200, 0);
    CHECK(utils::to_string(tree) == "[5-11:105:0][100-151:200:0]");

    // add range overlapping part of the left size check that it coalesces
    tree.add(2, 8, 102, 0);
    CHECK(utils::to_string(tree) == "[2-11:102:0][100-151:200:0]");

    // add range overlapping part of the right size check that it coalesces
    tree.add(9, 13, 109, 0);
    CHECK(utils::to_string(tree) == "[2-13:102:0][100-151:200:0]");

    // add range totally within existing range check that it coalesces
    tree.add(3, 5, 103, 0);
    CHECK(utils::to_string(tree) == "[2-13:102:0][100-151:200:0]");

    CHECK(tree.size() == 2);
    CHECK((--tree.end())->ex.end == 151);

    // add a range that connects two existing ranges
    tree.add(4, 121, 104, 0);
    CHECK(utils::to_string(tree) == "[2-151:102:0]");

    CHECK(tree.size() == 1);
    CHECK((--tree.end())->ex.end == 151);
  }

  SUBCASE("test remove") {
    tree.add(0, 1, 0, 0);
    tree.add(1, 11, 101, 0);
    tree.add(20, 31, 20, 0);
    tree.add(31, 41, 131, 0);

    // remove a single entry
    tree.remove(0, 1);
    CHECK(utils::to_string(tree) == "[1-11:101:0][20-31:20:0][31-41:131:0]");

    // remove a range spanning the two boardering ranges [20-31) and [31-41)
    tree.remove(25, 32);
    CHECK(utils::to_string(tree) == "[1-11:101:0][20-25:20:0][32-41:132:0]");
  }
}

TEST_CASE("extent serialization") {
  auto [data, in, out] = zpp::bits::data_in_out();

  extent e{100, 200};

  out(e).or_throw();
  extent e2{};
  in(e2).or_throw();

  CHECK(e == e2);
}

TEST_CASE("extent_tree::node serialization") {
  auto [data, in, out] = zpp::bits::data_in_out();

  extent_tree::node n{100, 200, 1024, 2};
  out(n).or_throw();

  extent_tree::node n2{};
  in(n2).or_throw();

  CHECK(n == n2);
}

TEST_CASE("extent_tree serialization") {
  auto [data, in, out] = zpp::bits::data_in_out();

  extent_tree tree;
  tree.add(0, 1, 0, 0);
  tree.add(1, 11, 101, 0);
  tree.add(20, 31, 20, 0);
  tree.add(31, 41, 131, 0);

  out(tree).or_throw();
  out(tree).or_throw();

  extent_tree tree2;
  in(tree2).or_throw();
  CHECK(std::equal(tree.begin(), tree.end(), tree2.begin(), tree2.end()));

  in(tree2).or_throw();
  CHECK(std::equal(tree.begin(), tree.end(), tree2.begin(), tree2.end()));
}

TEST_CASE("Testing extent_tree::merge") {
  extent_tree tree1;
  tree1.add(0, 100, 1000, 1);
  tree1.add(200, 300, 2000, 2);
  CHECK(utils::to_string(tree1) == "[0-100:1000:1][200-300:2000:2]");

  extent_tree tree2;
  tree2.add(100, 200, 1100, 1);
  tree2.add(300, 400, 2100, 2);
  CHECK(utils::to_string(tree2) == "[100-200:1100:1][300-400:2100:2]");

  // Case 1: Non-overlapping merge
  tree1.merge(tree2);
  CHECK(utils::to_string(tree1) == "[0-200:1000:1][200-400:2000:2]");
  CHECK(tree1.size() == 2);

  // Case 2: Overlapping merge with different client_ids
  extent_tree tree3;
  tree3.add(50, 150, 1050, 3);
  tree1.merge(tree3);
  CHECK(utils::to_string(tree1) == "[0-50:1000:1][50-150:1050:3][150-200:1150:1][200-400:2000:2]");
  CHECK(tree1.size() == 4);

  // Case 3: Overlapping merge with same client_ids should coalesce
  extent_tree tree4;
  tree4.add(400, 500, 2200, 2);
  tree1.merge(tree4);
  CHECK(utils::to_string(tree1) == "[0-50:1000:1][50-150:1050:3][150-200:1150:1][200-500:2000:2]");
  CHECK(tree1.size() == 4);

  // Case 4: Partially overlapping merge
  extent_tree tree5;
  tree5.add(250, 350, 2050, 4);
  tree1.merge(tree5);
  CHECK(utils::to_string(tree1) == "[0-50:1000:1][50-150:1050:3][150-200:1150:1][200-250:2000:2][250-350:2050:4][350-500:2150:2]");
  CHECK(tree1.size() == 6);

}
