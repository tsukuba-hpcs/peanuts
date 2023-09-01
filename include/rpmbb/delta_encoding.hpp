#pragma once

#include <cstdint>

namespace rpmbb {

class delta_encoder {
 private:
  uint64_t prev_ = 0;

 public:
  auto encode(uint64_t value) -> uint64_t {
    uint64_t delta = value - prev_;
    prev_ = value;
    return delta;
  }
};

class delta_decoder {
 private:
  uint64_t prev_ = 0;

 public:
  auto decode(uint64_t delta) -> uint64_t {
    uint64_t value = delta + prev_;
    prev_ = value;
    return value;
  }
};

}  // namespace rpmbb
