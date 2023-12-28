#pragma once

#include "rpmbb/inspector.hpp"
#include "rpmbb/utils/env.hpp"
#include "rpmbb/utils/singleton.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <utility>
#include <variant>

namespace rpmbb {

template <typename Derived, typename T>
class option : public utils::singleton<Derived> {
  using base_t = utils::singleton<Derived>;

 public:
  using value_type = T;

  option(value_type val) : val_(val) {}

  static value_type value() {
    assert(base_t::initialized());
    return base_t::get().val_;
  }

  static void set(value_type val) {
    assert(base_t::initialized());
    base_t::get().val_ = val;
  }

  std::ostream& inspect(std::ostream& os) const {
    return os << Derived::name() << "=" << val_;
  }

  void print() const { inspect(std::cout) << std::endl; }

 protected:
  value_type val_;
};

struct option_pmem_path : public option<option_pmem_path, std::string> {
  using option::option;
  static const char* name() { return "PMEMBB_PMEM_PATH"; }
  static std::string default_value() { return "/dev/dax0.0"; }
};

struct option_pmem_size : public option<option_pmem_size, size_t> {
  using option::option;
  static const char* name() { return "PMEMBB_PMEM_SIZE"; }
  static size_t default_value() { return 0; }
};

struct runtime_options {
  using value_type = std::variant<option_pmem_path, option_pmem_size>;

  static auto get() -> std::vector<value_type>& {
    static std::vector<value_type> options;
    return options;
  }

  static void print() {
    for (const auto& option : get()) {
      std::visit([](const auto& opt) { opt.print(); }, option);
    }
  }
};

template <typename Option>
class option_initializer {
 public:
  option_initializer()
      : initializer_{utils::getenv_with_default(Option::name(),
                                                Option::default_value())} {
    if (initializer_.should_finalize()) {
      runtime_options::get().emplace_back(Option::get());
      should_pop_ = true;
    }
  }
  ~option_initializer() {
    if (should_pop_) {
      runtime_options::get().pop_back();
    }
  }

 private:
  utils::singleton_initializer<Option> initializer_;
  bool should_pop_ = false;
};

struct runtime_option_initializer {
  option_initializer<option_pmem_path> pmem_path;
  option_initializer<option_pmem_size> pmem_size;
};

}  // namespace rpmbb
