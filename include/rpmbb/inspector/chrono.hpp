#pragma once

#include <chrono>
#include <ratio>
#include <string>

#include "../inspector.hpp"

namespace rpmbb::util {

template <typename Period>
std::string get_duration_unit() {
  using namespace std::chrono;
  if constexpr (std::ratio_equal_v<Period, years::period>) {
    return "y";
  } else if constexpr (std::ratio_equal_v<Period, months::period>) {
    return "mo";
  } else if constexpr (std::ratio_equal_v<Period, weeks::period>) {
    return "w";
  } else if constexpr (std::ratio_equal_v<Period, days::period>) {
    return "d";
  } else if constexpr (std::ratio_equal_v<Period, hours::period>) {
    return "h";
  } else if constexpr (std::ratio_equal_v<Period, minutes::period>) {
    return "min";
  } else if constexpr (std::ratio_equal_v<Period, seconds::period>) {
    return "s";
  } else if constexpr (std::ratio_equal_v<Period, milliseconds::period>) {
    return "ms";
  } else if constexpr (std::ratio_equal_v<Period, microseconds::period>) {
    return "us";
  } else if constexpr (std::ratio_equal_v<Period, nanoseconds::period>) {
    return "ns";
  } else {
    return "unk";
  }
}

template <typename Rep, typename Period>
struct inspector<std::chrono::duration<Rep, Period>> {
  static auto inspect(std::ostream& os,
                      const std::chrono::duration<Rep, Period>& obj)
      -> std::ostream& {
    return os << obj.count() << get_duration_unit<Period>();
  }
};

}  // namespace rpmbb::util
