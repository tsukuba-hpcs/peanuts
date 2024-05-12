#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "peanuts/ring_tracker.hpp"

#include <doctest/doctest.h>

using namespace peanuts;

TEST_CASE("Allocate and release") {
  ring_tracker rt(1000);  // 1000 bytes in total

  CHECK(rt.can_allocate(500));
  uint64_t segment_id = rt.allocate(500);
  CHECK(segment_id == 0);  // First allocation returns 0
  CHECK(rt.used_capacity() == 500);
  CHECK(rt.free_capacity() == 500);

  CHECK(rt.can_release(400));
  rt.release(400);
  CHECK(rt.used_capacity() == 100);
  CHECK(rt.free_capacity() == 900);

  CHECK(rt.can_allocate(500));
  segment_id = rt.allocate(500);
  CHECK(segment_id == 500);  // Next allocation returns updated head_id_
  CHECK(rt.used_capacity() == 600);
  CHECK(rt.free_capacity() == 400);
}

TEST_CASE("Cannot allocate and release") {
  ring_tracker rt(1000);  // 1000 bytes in total
  rt.allocate(1000);

  CHECK_FALSE(rt.can_allocate(1));
  CHECK_FALSE(rt.can_release(1001));
}

TEST_CASE("Wrapping check") {
  ring_tracker rt(1000);

  rt.allocate(800);
  rt.release(800);
  uint64_t segment_id = rt.allocate(250);
  CHECK(rt.is_wrapping(segment_id, 250));
}

TEST_CASE("First segment size") {
  ring_tracker rt(1000);

  rt.allocate(800);
  uint64_t segment_id = rt.allocate(300);
  CHECK(rt.first_segment_size(segment_id, 250) == 200);
}

TEST_CASE("Conversion to ring offset") {
  ring_tracker rt(1000);

  uint64_t segment_id = rt.allocate(500);
  CHECK(rt.to_ofs(segment_id) == 0);
}

TEST_CASE(
    "Allocate and release multiple times, verify offset conversion, segment "
    "size, and wrapping") {
  ring_tracker rt(1000);  // 1000 bytes in total

  uint64_t segment_id_1 = rt.allocate(400);
  rt.release(200);
  uint64_t segment_id_2 = rt.allocate(300);
  rt.release(200);
  uint64_t segment_id_3 = rt.allocate(400);  // Wrapping occurs here

  // At this point, tail is at 400 and head is at 1100 (after wrapping)
  // Current layout: [100 used | 400 free | 300 used | 300 used (wrapped)]

  CHECK(rt.to_ofs(segment_id_1) == 0);
  CHECK(rt.to_ofs(segment_id_2) == 400);
  CHECK(rt.to_ofs(segment_id_3) == 700);

  CHECK_FALSE(rt.is_wrapping(segment_id_1, 100));
  CHECK_FALSE(rt.is_wrapping(segment_id_2, 100));
  CHECK(rt.is_wrapping(segment_id_3, 400));

  CHECK(rt.first_segment_size(segment_id_3, 400) == 300);
}
