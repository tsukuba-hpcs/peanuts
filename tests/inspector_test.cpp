#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "rpmbb/inspector.hpp"
#include <doctest/doctest.h>
#include <optional>
#include <sstream>

namespace user_defined_ns {

class intrusive_class {
  int begin_ = 0;
  int end_ = 10;

 public:
  std::ostream& inspect(std::ostream& os) const {
    return os << begin_ << "-" << end_;
  }
};

struct non_intrusive_class {
  int a = 5, b = 10;
};

struct non_intrusive_with_intrusive {
  intrusive_class ic;
};

class intrusive_with_non_intrusive {
  non_intrusive_class nic;

 public:
  std::ostream& inspect(std::ostream& os) const {
    return os << "(" << rpmbb::util::make_inspector(nic) << ")";
  }
};

}  // namespace user_defined_ns

namespace user_defined_ns {

std::ostream& inspect(std::ostream& os, const non_intrusive_class& obj) {
  return os << obj.a << "," << obj.b;
}

std::ostream& inspect(std::ostream& os,
                      const non_intrusive_with_intrusive& obj) {
  return os << "(" << rpmbb::util::make_inspector(obj.ic) << ")";
}

}  // namespace user_defined_ns

TEST_CASE("Inspect intrusive_class") {
  user_defined_ns::intrusive_class obj;
  std::stringstream ss;
  ss << rpmbb::util::make_inspector(obj);
  CHECK(ss.str() == "0-10");
}

TEST_CASE("Inspect non_intrusive_class") {
  user_defined_ns::non_intrusive_class obj;
  std::stringstream ss;
  ss << rpmbb::util::make_inspector(obj);
  CHECK(ss.str() == "5,10");
}

TEST_CASE("Inspect non_intrusive_with_intrusive") {
  user_defined_ns::non_intrusive_with_intrusive obj;
  std::stringstream ss;
  ss << rpmbb::util::make_inspector(obj);
  CHECK(ss.str() == "(0-10)");
}

TEST_CASE("Inspect intrusive_with_non_intrusive") {
  user_defined_ns::intrusive_with_non_intrusive obj;
  std::stringstream ss;
  ss << rpmbb::util::make_inspector(obj);
  CHECK(ss.str() == "(5,10)");
}

TEST_CASE("to_string with intrusive_class") {
  user_defined_ns::intrusive_class obj;
  CHECK(rpmbb::util::to_string(obj) == "0-10");
}

TEST_CASE("to_string with non_intrusive_class") {
  user_defined_ns::non_intrusive_class obj;
  CHECK(rpmbb::util::to_string(obj) == "5,10");
}

TEST_CASE("to_string with non_intrusive_with_intrusive") {
  user_defined_ns::non_intrusive_with_intrusive obj;
  CHECK(rpmbb::util::to_string(obj) == "(0-10)");
}

TEST_CASE("to_string with intrusive_with_non_intrusive") {
  user_defined_ns::intrusive_with_non_intrusive obj;
  CHECK(rpmbb::util::to_string(obj) == "(5,10)");
}

TEST_CASE("inspect std::optional<T>") {
  std::optional<int> obj;
  {
    std::stringstream ss;
    ss << rpmbb::util::make_inspector(obj);
    CHECK(ss.str() == "nullopt");
  }
  obj = int{10};
  {
    std::stringstream ss;
    ss << rpmbb::util::make_inspector(obj);
    CHECK(ss.str() == "10");
  }
}

// Specialization for std::optional<T>
namespace rpmbb::util {
template <typename T>
struct inspector<std::optional<T>> {
  static std::ostream& inspect(std::ostream& os, const std::optional<T>& obj) {
    if (obj) {
      return os << *obj;
    } else {
      return os << "nullopt";
    }
  }
};
}  // namespace rpmbb::util

TEST_CASE("inspect std::optional<T>") {
  std::optional<int> a = 10;
  std::stringstream ss2;
  ss2 << rpmbb::util::make_inspector(a);
  CHECK(ss2.str() == "10");

  std::optional<int> b = std::nullopt;
  std::stringstream ss3;
  ss3 << rpmbb::util::make_inspector(b);
  CHECK(ss3.str() == "nullopt");
}
