#pragma once

#include <ostream>

namespace rpmbb::util {

template <typename T>
class inspector {
 public:
  explicit inspector(const T& obj) : obj_(obj) {}

  friend inline std::ostream& operator<<(std::ostream& os,
                                         const inspector<T>& insp) {
    return insp.obj_.inspect(os);
  }

 private:
  const T& obj_;
};

template <typename T>
auto make_inspector(const T& obj) -> inspector<T> {
  return inspector<T>(obj);
}

/* Usage

class your_class {
 public:
  int begin_{0};
  int end_{0};

  your_class(int begin, int end) : begin_(begin), end_(end) {}

  std::ostream& inspect(std::ostream& os) const {
    return os << begin_ << "-" << end_;
  }
};

TEST_CASE("your_class inspect and operator<<") {
  your_class obj(1, 10);

  std::ostringstream oss;
  oss << "[" << make_inspector(obj) << "]" << std::endl;
  CHECK(oss.str() == "[1-10]\n");
}

*/

}  // namespace rpmbb::util
