#pragma once

#include <optional>
#include <ostream>
#include <sstream>
#include <type_traits>

namespace peanuts::utils {

template <typename T>
struct inspector {
  static std::ostream& inspect(std::ostream& os, const T& obj) {
    return os << obj;
  }
};

namespace detail {

template <typename T, typename = void, typename = void>
constexpr bool has_inspect_member = false;

template <typename T>
constexpr bool has_inspect_member<
    T,
    std::void_t<decltype(std::declval<T&>().inspect(
        std::declval<std::ostream&>()))>,
    std::enable_if_t<std::is_same_v<std::ostream&,
                                    decltype(std::declval<T&>().inspect(
                                        std::declval<std::ostream&>()))>>> =
    true;

template <typename T, typename = void, typename = void>
constexpr bool has_adl_inspect = false;

template <typename T>
constexpr bool has_adl_inspect<
    T,
    std::void_t<decltype(inspect(std::declval<std::ostream&>(),
                                 std::declval<const T&>()))>,
    std::enable_if_t<
        std::is_same_v<std::ostream&,
                       decltype(inspect(std::declval<std::ostream&>(),
                                        std::declval<const T&>()))>>> = true;

template <typename T>
struct inspector_wrapper {
  const T& obj_;
  explicit inspector_wrapper(const T& obj) : obj_(obj) {}
};

template <typename T>
std::ostream& operator<<(std::ostream& os, const inspector_wrapper<T>& insp) {
  if constexpr (has_inspect_member<T>) {
    return insp.obj_.inspect(os);
  } else if constexpr (has_adl_inspect<T>) {
    return inspect(os, insp.obj_);
  } else {
    return inspector<T>::inspect(os, insp.obj_);
  }
}

}  // namespace detail

template <typename T>
detail::inspector_wrapper<T> make_inspector(const T& obj) {
  return detail::inspector_wrapper<T>(obj);
}

template <typename T>
std::string to_string(const T& obj) {
  std::stringstream ss;
  ss << make_inspector(obj);
  return ss.str();
}

}  // namespace peanuts::utils
