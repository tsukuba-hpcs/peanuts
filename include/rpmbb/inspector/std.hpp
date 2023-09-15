#pragma once

#include <optional>
#include <vector>

#include "../inspector.hpp"

namespace rpmbb::util {

template <typename T>
struct inspector<std::optional<T>> {
  static std::ostream& inspect(std::ostream& os, const std::optional<T>& obj) {
    if (obj) {
      return os << make_inspector(*obj);
    } else {
      return os << "nullopt";
    }
  }
};

template <typename T>
struct inspector<std::vector<T>> {
  static std::ostream& inspect(std::ostream& os, const std::vector<T>& obj) {
    os << '[';
    for (const auto& item : obj) {
      os << make_inspector(item) << ", ";
    }
    return os << ']';
  }
};

}  // namespace rpmbb::util
