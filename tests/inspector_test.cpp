#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "rpmbb/inspector.hpp"
#include <doctest/doctest.h>
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

std::ostream& inspect(std::ostream& os, const non_intrusive_with_intrusive& obj) {
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
