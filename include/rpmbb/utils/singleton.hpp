#pragma once

#include <optional>
#include <utility>
#include <cassert>

namespace rpmbb::utils {

template <typename T>
class singleton {
 public:
  using instance_type = T;

  static auto& get() {
    assert(initialized());
    return *get_optional();
  }

  static bool initialized() { return get_optional().has_value(); }

  template <typename... Args>
  static void init(Args&&... args) {
    get_optional().emplace(std::forward<Args>(args)...);
  }

  static void fini() {
    assert(initialized());
    get_optional().reset();
  }

 private:
  static auto& get_optional() {
    static std::optional<T> instance;
    return instance;
  }
};

template <typename Singleton>
class singleton_initializer {
 public:
  template <typename... Args>
  singleton_initializer(Args&&... args) {
    if (!Singleton::initialized()) {
      Singleton::init(std::forward<Args>(args)...);
      should_finalize_ = true;
    }
  }

  ~singleton_initializer() {
    if (should_finalize_) {
      Singleton::fini();
    }
  }

  singleton_initializer(const singleton_initializer&) = delete;
  singleton_initializer& operator=(const singleton_initializer&) = delete;

  singleton_initializer(singleton_initializer&&) = delete;
  singleton_initializer& operator=(singleton_initializer&&) = delete;

  bool should_finalize() const { return should_finalize_; }

 private:
  bool should_finalize_ = false;
};

}  // namespace rpmbb::utils
