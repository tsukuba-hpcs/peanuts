#pragma once

#include <ostream>

namespace rpmbb::util {
namespace detail {

template <typename T>
struct inspector {
  const T& obj_;
  explicit inspector(const T& obj) : obj_(obj) {}
};

template <typename T>
auto adl_inspect(std::ostream& os, const T& obj)
    -> decltype(inspect(os, obj), os) {
  return inspect(os, obj);
}

template <typename T>
auto adl_inspect(std::ostream& os, const T& obj)
    -> decltype(obj.inspect(os), os) {
  return obj.inspect(os);
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const inspector<T>& insp) {
  return adl_inspect(os, insp.obj_);
}

}  // namespace detail

template <typename T>
detail::inspector<T> make_inspector(const T& obj) {
  return detail::inspector<T>(obj);
}

}  // namespace rpmbb::util

/* Usage

// output of private members by intrusive inspection
namespace your_lib {

class your_class {
private:
  int begin_ = 0;
  int end_ = 10;

public:
  std::ostream& inspect(std::ostream& os) const {
    return os << begin_ << "-" << end_;
  }
};

}  // namespace your_lib

// output of public members by non-intrusive inspection
namespace user_defined {

struct user_struct {
  int x = 1;
  int y = 2;
};

// non-intrusive inspection needs to be defined in the same namespace as the
// type to use ADL.

std::ostream& inspect(std::ostream& os, const user_struct& obj)
{ return os << "{" << obj.x << ", " << obj.y << "}";
}

}  // namespace user_defined

int main() {
  your_lib::your_class obj1;
  user_defined::user_struct obj2;

  std::cout << rpmbb::util::make_inspector(obj1) << std::endl;
  std::cout << rpmbb::util::make_inspector(obj2) << std::endl;

  return 0;
}

*/
