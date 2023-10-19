#pragma once

#include <algorithm>
#include <array>
#include <cstring>
#include <functional>
#include <optional>
#include <random>
#include <string>

namespace rpmbb::utils {

template <typename T = std::mt19937>
inline auto random_generator(
    std::optional<std::seed_seq::result_type> seed = std::nullopt) -> T {
  if (seed) {
    return T{*seed};
  }

  auto constexpr seed_bytes = sizeof(typename T::result_type) * T::state_size;
  auto constexpr seed_len = seed_bytes / sizeof(std::seed_seq::result_type);
  auto seed_array = std::array<std::seed_seq::result_type, seed_len>();
  auto dev = std::random_device();
  std::generate_n(begin(seed_array), seed_len, std::ref(dev));
  auto seed_seq = std::seed_seq(begin(seed_array), end(seed_array));
  return T{seed_seq};
}

class random_string_generator {
 public:
  random_string_generator(
      std::optional<std::seed_seq::result_type> seed = std::nullopt)
      : rng_(random_generator<>(seed)) {}

  auto generate(std::size_t len) -> std::string {
    static constexpr auto alnum_chars =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    auto dist = std::uniform_int_distribution{{}, std::strlen(alnum_chars) - 1};
    auto result = std::string(len, '\0');
    std::generate_n(begin(result), len,
                    [&, this]() { return alnum_chars[dist(rng_)]; });
    return result;
  }

 private:
  std::mt19937 rng_;
};

inline auto generate_random_alphanumeric_string(std::size_t len)
    -> std::string {
  thread_local auto generator = random_string_generator{};
  return generator.generate(len);
}

}  // namespace rpmbb::utils
