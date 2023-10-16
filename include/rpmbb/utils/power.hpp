#pragma once

#include <cstdint>

namespace rpmbb::utils {

template <uintmax_t base, unsigned int exp>
constexpr auto power() -> uintmax_t {
  if constexpr (exp == 0) {
    return 1;
  }
  if constexpr (exp == 1) {
    return base;
  }

  uintmax_t tmp = power<base, exp / 2>();
  if constexpr (exp % 2 == 0) {
    return tmp * tmp;
  } else {
    return base * tmp * tmp;
  }
}

}  // namespace rpmbb::utils
