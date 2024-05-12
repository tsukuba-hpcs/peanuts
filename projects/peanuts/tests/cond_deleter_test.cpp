#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "peanuts/utils/cond_deleter.hpp"
#include <doctest/doctest.h>

using namespace peanuts::utils;

class dummy_deleter {
 public:
  using pointer = int*;

  std::shared_ptr<int> call_count = std::make_shared<int>(0);

  void operator()(pointer ptr) const {
    ++(*call_count);
    delete ptr;
  }
};

TEST_CASE("Test cond_deleter") {
  using pointer = dummy_deleter::pointer;

  auto shared_call_count = std::make_shared<int>(0);
  auto d = dummy_deleter{shared_call_count};

  SUBCASE("Deleter should delete") {
    auto cd = cond_deleter<dummy_deleter>(true, d);
    auto p = new int(42);
    cd(p);

    CHECK(*shared_call_count == 1);
  }

  SUBCASE("Deleter should not delete") {
    auto cd = cond_deleter<dummy_deleter>(false, d);
    auto p = new int(42);
    cd(p);

    CHECK(*shared_call_count == 0);
    delete p;
  }

  SUBCASE("Deleter comparison with different states") {
    auto cd1 = cond_deleter<dummy_deleter>(true, d);
    auto cd2 = cond_deleter<dummy_deleter>(false, d);

    auto ptr1 =
        std::unique_ptr<int, cond_deleter<dummy_deleter>>(new int(1), cd1);
    auto ptr2 =
        std::unique_ptr<int, cond_deleter<dummy_deleter>>(new int(1), cd2);

    CHECK(ptr1.get() != ptr2.get());
    CHECK(ptr1.get() == ptr1.get());  // Same ptr should be equal
    CHECK(ptr1 != ptr2);              // Different deleter states
    ptr2.get_deleter().set_should_delete(true);
  }
}
