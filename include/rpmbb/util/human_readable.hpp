#pragma once

#include "rpmbb/util/power.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#include <iostream>

namespace rpmbb::util {

template <int DECIMAL_FLEX_LIMIT = 3, typename T>
std::string to_string_flexible_decimal(T value) {
  static_assert(std::is_arithmetic<T>::value,
                "Type must be integral or floating-point.");
  std::ostringstream os;
  if constexpr (std::is_integral_v<T>) {
    os << value;
  } else {
    int integral_digits =
        (value == 0) ? 1
                     : static_cast<int>(
                           std::log10(std::abs(static_cast<double>(value)))) +
                           1;

    int precision = std::max(0, DECIMAL_FLEX_LIMIT - integral_digits);
    double rounding_factor = std::pow(10, precision);
    value = std::round(value * rounding_factor) / rounding_factor;

    os << std::fixed << std::setprecision(precision) << value;
  }

  return os.str();
}

template <int BASE = 1024, typename T>
std::enable_if_t<std::is_arithmetic<T>::value, std::string> to_human(T num) {
  static const std::array<std::string, 17> to_unit{
      "y", "z", "a", "f", "p", "n", "u", "m", "", "K", "M", "G", "T", "P", "E"};

  double dbl_num = static_cast<double>(num);
  int exponent = 0;
  if (dbl_num != 0.0) {
    exponent = static_cast<int>(
        std::floor(std::log(std::abs(dbl_num)) / std::log(BASE)));
    dbl_num /= std::pow(BASE, exponent);
  }

  std::string str = to_string_flexible_decimal(dbl_num);

  // Remove trailing .00
  if (str.find(".00") != std::string::npos) {
    str.erase(str.size() - 3, 3);
  }

  if (exponent != 0) {
    str += to_unit[(exponent + 8)];
  }

  return str;
}

template <typename T, unsigned int BASE = 1024>
T from_human(std::string_view sv) {
  static_assert(std::is_arithmetic<T>::value,
                "Type must be integral or floating-point.");

  static const std::unordered_map<char, std::pair<T, T>> from_unit{
      {'y', {1, power<BASE, 8>()}}, {'z', {1, power<BASE, 7>()}},
      {'a', {1, power<BASE, 6>()}}, {'f', {1, power<BASE, 5>()}},
      {'p', {1, power<BASE, 4>()}}, {'n', {1, power<BASE, 3>()}},
      {'u', {1, power<BASE, 2>()}}, {'m', {1, power<BASE, 1>()}},
      {'K', {power<BASE, 1>(), 1}}, {'k', {power<BASE, 1>(), 1}},
      {'M', {power<BASE, 2>(), 1}}, {'G', {power<BASE, 3>(), 1}},
      {'T', {power<BASE, 4>(), 1}}, {'P', {power<BASE, 5>(), 1}},
      {'E', {power<BASE, 6>(), 1}},
  };

  T value;
  auto [ptr, ec] = std::from_chars(sv.begin(), sv.end(), value);

  if (ec == std::errc::invalid_argument) {
    throw std::invalid_argument("Invalid number format");
  }

  if (ptr == sv.end()) {
    return value;
  }

  auto unit_iter = from_unit.find(*ptr);
  if (unit_iter == from_unit.end()) {
    throw std::invalid_argument("Unknown unit suffix");
  }

  return value * unit_iter->second.first / unit_iter->second.second;
}

}  // namespace rpmbb::util
