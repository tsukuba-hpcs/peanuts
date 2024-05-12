#pragma once

#include <cstdint>
#include <cassert>

namespace peanuts::utils {

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

inline uint64_t next_pow2(uint64_t x) {
  x--;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  x |= x >> 32;
  return x + 1;
}

template <typename T>
inline bool is_pow2(T x) {
  return !(x & (x - 1));
}

template <typename T>
inline T round_down_pow2(T x, T alignment) {
  assert(is_pow2(alignment));
  return x & ~(alignment - 1);
}

template <typename T>
inline T* round_down_pow2(T* x, uintptr_t alignment) {
  assert(is_pow2(alignment));
  return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(x) &
                              ~(alignment - 1));
}

template <typename T>
inline T round_up_pow2(T x, T alignment) {
  assert(is_pow2(alignment));
  return (x + alignment - 1) & ~(alignment - 1);
}

template <typename T>
inline T* round_up_pow2(T* x, uintptr_t alignment) {
  assert(is_pow2(alignment));
  return reinterpret_cast<T*>((reinterpret_cast<uintptr_t>(x) + alignment - 1) &
                              ~(alignment - 1));
}

}  // namespace peanuts::utils
