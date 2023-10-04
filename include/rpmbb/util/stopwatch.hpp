#pragma once
#include <chrono>

namespace rpmbb::util {

template <typename Clock = std::chrono::high_resolution_clock,
          typename Rep = typename Clock::rep,
          typename Period = typename Clock::period>
class basic_stopwatch {
 public:
  using clock = Clock;
  using rep = Rep;
  using period = Period;
  using duration = std::chrono::duration<rep, period>;
  using time_point = std::chrono::time_point<clock, duration>;

  basic_stopwatch() : start_time_(clock::now()), last_lap_time_(start_time_) {}

  void reset() { reset(clock::now()); }

  duration get() const {
    return std::chrono::duration_cast<duration>(clock::now() - start_time_);
  }

  duration get_and_reset() {
    auto current_time = clock::now();
    auto elapsed_time = current_time - start_time_;
    reset(current_time);
    return std::chrono::duration_cast<duration>(elapsed_time);
  }

  duration lap_time() {
    auto current_time = clock::now();
    auto lap = current_time - last_lap_time_;
    last_lap_time_ = current_time;
    return std::chrono::duration_cast<duration>(lap);
  }

 private:
  void reset(const clock::time_point& tp) {
    start_time_ = tp;
    last_lap_time_ = start_time_;
  }

 private:
  typename clock::time_point start_time_;
  typename clock::time_point last_lap_time_;
};

template <typename Rep = double, typename Period = std::ratio<1, 1>>
using stopwatch =
    basic_stopwatch<std::chrono::high_resolution_clock, Rep, Period>;

}  // namespace rpmbb::util
