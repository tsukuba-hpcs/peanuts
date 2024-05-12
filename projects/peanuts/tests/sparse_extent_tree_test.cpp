#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest/doctest.h>

#include "peanuts/inspector.hpp"
#include "peanuts/sparse_extent.hpp"
#include "peanuts/sparse_extent_tree.hpp"

using namespace peanuts;

TEST_CASE("sparse_extent") {
  sparse_extent ex{100, 10, 2, 3};
}

TEST_CASE("spase_extent::slice") {
  sparse_extent ex{100, 15, 5, 5};
  CHECK(utils::to_string(ex) ==
        "[[100,115)[120,135)[140,155)[160,175)[180,195)]");

  SUBCASE("get_first_slice") {
    CHECK(utils::to_string(ex.get_first_slice(100, 195)) ==
          "[[100,115)[120,135)[140,155)[160,175)[180,195)]");
    CHECK(utils::to_string(ex.get_first_slice(110, 155)) == "[[110,115)]");
    CHECK(utils::to_string(ex.get_first_slice(118, 155)) == "[[120,135)[140,155)]");
    CHECK(utils::to_string(ex.get_first_slice(120, 155)) == "[[120,135)[140,155)]");
    CHECK(utils::to_string(ex.get_first_slice(130, 155)) == "[[130,135)]");
    CHECK(utils::to_string(ex.get_first_slice(138, 155)) == "[[140,155)]");
    CHECK(utils::to_string(ex.get_first_slice(138, 155)) == "[[140,155)]");
    CHECK(utils::to_string(ex.get_first_slice(140, 155)) == "[[140,155)]");
    CHECK(utils::to_string(ex.get_first_slice(145, 155)) == "[[145,155)]");
    CHECK(utils::to_string(ex.get_first_slice(145, 150)) == "[[145,150)]");
    CHECK(utils::to_string(ex.get_first_slice(145, 185)) == "[[145,155)]");
    CHECK(utils::to_string(ex.get_first_slice(160, 170)) == "[[160,170)]");
    CHECK(utils::to_string(ex.get_first_slice(160, 175)) == "[[160,175)]");
    CHECK(utils::to_string(ex.get_first_slice(160, 180)) == "[[160,175)]");
    CHECK(utils::to_string(ex.get_first_slice(160, 184)) == "[[160,175)]");
    CHECK(utils::to_string(ex.get_first_slice(160, 195)) == "[[160,175)[180,195)]");
    CHECK(utils::to_string(ex.get_first_slice(140, 190)) == "[[140,155)[160,175)]");
  }

  SUBCASE("get slices") {
    uint64_t pos = 158; // in the hole

    auto slice1 = ex.get_first_slice(ex.start(), pos);
    CHECK(utils::to_string(slice1) == "[[100,115)[120,135)[140,155)]");
    CHECK(slice1.stop() + slice1.hole >= pos);
    auto slice2 = ex.get_first_slice_start_at(slice1.stop());
    CHECK(utils::to_string(slice2) == "[[160,175)[180,195)]");

    pos = 150; // in the middle of a extent
    auto slice3 = ex.get_first_slice(ex.start(), pos);
    CHECK(utils::to_string(slice3) == "[[100,115)[120,135)]");
    CHECK(slice3.stop() + slice3.hole < pos);
    auto slice4 = ex.get_first_slice(slice3.stop(), pos);
    CHECK(utils::to_string(slice4) == "[[140,150)]");
    CHECK(slice4.stop() + slice4.hole >= pos);
    auto slice5 = ex.get_first_slice_start_at(slice4.stop());
    CHECK(utils::to_string(slice5) == "[[150,155)]");
    auto slice6 = ex.get_first_slice_start_at(slice5.stop());
    CHECK(utils::to_string(slice6) == "[[160,175)[180,195)]");
  }

  SUBCASE("get_first_slice_start_at") {
    auto slice1 = ex.get_first_slice_start_at(100);
    CHECK(utils::to_string(slice1) ==
          "[[100,115)[120,135)[140,155)[160,175)[180,195)]");
    auto slice2 = ex.get_first_slice_start_at(110);
    CHECK(utils::to_string(slice2) == "[[110,115)]");
    auto slice3 = ex.get_first_slice_start_at(115);
    CHECK(utils::to_string(slice3) == "[[120,135)[140,155)[160,175)[180,195)]");
    auto slice4 = ex.get_first_slice_start_at(118);
    CHECK(utils::to_string(slice4) == "[[120,135)[140,155)[160,175)[180,195)]");
    auto slice5 = ex.get_first_slice_start_at(120);
    CHECK(utils::to_string(slice5) == "[[120,135)[140,155)[160,175)[180,195)]");
    auto slice6 = ex.get_first_slice_start_at(150);
    CHECK(utils::to_string(slice6) == "[[150,155)]");
    auto slice7 = ex.get_first_slice_start_at(158);
    CHECK(utils::to_string(slice7) == "[[160,175)[180,195)]");
    auto slice8 = ex.get_first_slice_start_at(195);
    CHECK(utils::to_string(slice8) == "[]");
  }

  SUBCASE("get_first_slice_stop_at") {
    auto slice1 = ex.get_first_slice_stop_at(100);
    CHECK(utils::to_string(slice1) == "[]");
    auto slice2 = ex.get_first_slice_stop_at(110);
    CHECK(utils::to_string(slice2) == "[[100,110)]");
    auto slice3 = ex.get_first_slice_stop_at(113);
    CHECK(utils::to_string(slice3) == "[[100,113)]");
    auto slice4 = ex.get_first_slice_stop_at(118);
    CHECK(utils::to_string(slice4) == "[[100,115)]");
    auto slice5 = ex.get_first_slice_stop_at(120);
    CHECK(utils::to_string(slice5) == "[[100,115)]");
    auto slice6 = ex.get_first_slice_stop_at(150);
    CHECK(utils::to_string(slice6) == "[[100,115)[120,135)]");
    auto slice7 = ex.get_first_slice_stop_at(158);
    CHECK(utils::to_string(slice7) == "[[100,115)[120,135)[140,155)]");
    auto slice8 = ex.get_first_slice_stop_at(195);
    CHECK(utils::to_string(slice8) ==
          "[[100,115)[120,135)[140,155)[160,175)[180,195)]");
  }
}

TEST_CASE("sparse_extent_tree::node") {
  sparse_extent_tree::node n{{100, 10, 2, 3}, 1000, 1};
  // MESSAGE(utils::to_string(n));
}
