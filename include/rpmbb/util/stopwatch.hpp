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

  basic_stopwatch() : start_time_(clock::now()) {}

  void reset() { start_time_ = clock::now(); }

  duration get() const {
    return std::chrono::duration_cast<duration>(clock::now() - start_time_);
  }

  duration get_and_reset() {
    auto current_time = clock::now();
    auto elapsed = current_time - start_time_;
    start_time_ = current_time;
    return std::chrono::duration_cast<duration>(elapsed);
  }

  rep count() const {
    return std::chrono::duration_cast<duration>(clock::now() - start_time_)
        .count();
  }

 private:
  typename clock::time_point start_time_;
};

template <typename Rep = std::chrono::high_resolution_clock::rep,
          typename Period = std::chrono::high_resolution_clock::period>
using stopwatch =
    basic_stopwatch<std::chrono::high_resolution_clock, Rep, Period>;

}  // namespace rpmbb::util
