#pragma once

#include <functional>
#include <memory>

namespace rpmbb::util {

template <typename Deleter>
class cond_deleter {
 public:
  using pointer = typename Deleter::pointer;

  cond_deleter(bool should_delete = true, Deleter deleter = Deleter())
      : should_delete_(should_delete), deleter_(deleter) {}

  void set_should_delete(bool should_delete) { should_delete_ = should_delete; }

  void operator()(pointer ptr) const {
    if (should_delete_ && ptr) {
      deleter_(ptr);
    }
  }

 private:
  bool should_delete_{true};
  Deleter deleter_;
};

}  // namespace rpmbb::util
