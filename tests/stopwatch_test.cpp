#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "rpmbb/utils/stopwatch.hpp"
#include <doctest/doctest.h>

#include <thread>

TEST_CASE_TEMPLATE("Generic Stopwatch Test",
                   T,
                   rpmbb::utils::stopwatch<double, std::nano>,
                   rpmbb::utils::stopwatch<uint64_t, std::milli>,
                   rpmbb::utils::stopwatch<>) {
  using namespace std::chrono_literals;
  T sw;

  auto epsilon = 30ms;
  auto sleep_duration = 100ms;

  auto initial_duration = sw.get();
  CHECK(initial_duration <= epsilon);

  std::this_thread::sleep_for(sleep_duration);

  auto after_sleep_duration = sw.get();
  CHECK(after_sleep_duration >= sleep_duration - epsilon);
  CHECK(after_sleep_duration <= sleep_duration + epsilon);

  auto first_lap_time = sw.lap_time();
  CHECK(first_lap_time >= after_sleep_duration - epsilon);
  CHECK(first_lap_time <= after_sleep_duration + epsilon);

  auto second_lap_time = sw.lap_time();
  CHECK(second_lap_time <= epsilon);

  auto get_and_reset = sw.get_and_reset();
  CHECK(second_lap_time <= get_and_reset);
  CHECK(initial_duration < get_and_reset);
  CHECK(after_sleep_duration <= get_and_reset);

  auto after_reset_duration = sw.get();
  CHECK(after_reset_duration <= epsilon);
}
