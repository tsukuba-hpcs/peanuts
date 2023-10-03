#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "rpmbb/util/stopwatch.hpp"
#include <doctest/doctest.h>

#include <thread>

TEST_CASE_TEMPLATE("Generic Stopwatch Test",
                   T,
                   rpmbb::util::stopwatch<double, std::nano>,
                   rpmbb::util::stopwatch<uint64_t, std::milli>,
                   rpmbb::util::stopwatch<>) {
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

  auto same_duration = sw.get_and_reset();
  CHECK(same_duration >= after_sleep_duration - epsilon);
  CHECK(same_duration <= after_sleep_duration + epsilon);

  auto after_reset_duration = sw.get();
  CHECK(after_reset_duration <= epsilon);
}
