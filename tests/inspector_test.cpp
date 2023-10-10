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

#include "rpmbb/inspector/chrono.hpp"

TEST_CASE("Inspect std::chrono::duration") {
  using namespace rpmbb::util;
  SUBCASE("Inspect integral rep") {
    std::ostringstream os;
    std::chrono::years years(1);
    inspector<decltype(years)>::inspect(os, years);
    CHECK(os.str() == "1y");

    os.str("");
    std::chrono::duration<int, std::ratio<2629746>> months(1);
    inspector<decltype(months)>::inspect(os, months);
    CHECK(os.str() == "1mo");

    os.str("");
    std::chrono::duration<int, std::ratio<604800>> weeks(1);
    inspector<decltype(weeks)>::inspect(os, weeks);
    CHECK(os.str() == "1w");

    os.str("");
    std::chrono::days days(1);
    inspector<decltype(days)>::inspect(os, days);
    CHECK(os.str() == "1d");

    os.str("");
    std::chrono::hours hours(1);
    inspector<decltype(hours)>::inspect(os, hours);
    CHECK(os.str() == "1h");

    os.str("");
    std::chrono::minutes minutes(1);
    inspector<decltype(minutes)>::inspect(os, minutes);
    CHECK(os.str() == "1min");

    os.str("");
    std::chrono::seconds seconds(1);
    inspector<decltype(seconds)>::inspect(os, seconds);
    CHECK(os.str() == "1s");

    os.str("");
    std::chrono::milliseconds ms(1);
    inspector<decltype(ms)>::inspect(os, ms);
    CHECK(os.str() == "1ms");

    os.str("");
    std::chrono::microseconds us(1);
    inspector<decltype(us)>::inspect(os, us);
    CHECK(os.str() == "1us");

    os.str("");
    std::chrono::nanoseconds ns(1);
    inspector<decltype(ns)>::inspect(os, ns);
    CHECK(os.str() == "1ns");
  }

  SUBCASE("Inspect floating-point rep") {
    std::ostringstream os;
    os << std::fixed << std::setprecision(2);

    std::chrono::duration<double, std::chrono::seconds::period> seconds(1.23);
    inspector<decltype(seconds)>::inspect(os, seconds);
    CHECK(os.str() == "1.23s");

    os.str("");

    std::chrono::duration<double, std::chrono::milliseconds::period> ms(1.23);
    inspector<decltype(ms)>::inspect(os, ms);
    CHECK(os.str() == "1.23ms");

    os.str("");

    std::chrono::duration<double, std::chrono::microseconds::period> us(1.23);
    inspector<decltype(us)>::inspect(os, us);
    CHECK(os.str() == "1.23us");
  }
}
