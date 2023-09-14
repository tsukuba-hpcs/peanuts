#pragma once

#include "cpu_affinity_manager.hpp"

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <optional>
#include <random>
#include <thread>
#include <vector>

namespace rpmbb::util {
namespace detail {

class random_cpu_selector {
 private:
  unsigned int max_core_;
  std::default_random_engine generator_;
  std::uniform_int_distribution<unsigned int> distribution_;

 public:
  random_cpu_selector()
      : max_core_(std::thread::hardware_concurrency()),
        generator_(std::random_device{}()),
        distribution_(0, max_core_ - 1) {}

  auto get_random_cpu_id() -> unsigned int { return distribution_(generator_); }

  auto operator()() -> unsigned int { return get_random_cpu_id(); }
};

}  // namespace detail

class tsc {
 public:
  static uint64_t get() {
    unsigned long long cycles_low, cycles_high;
    asm volatile(
        "RDTSCP\n\t"
        "mov %%rdx, %0\n\t"
        "mov %%rax, %1\n\t"
        "LFENCE\n\t"
        : "=r"(cycles_high), "=r"(cycles_low)::"%rax", "%rcx", "%rdx");
    return ((uint64_t)cycles_high << 32) | cycles_low;
  }

  static auto cycles_per_msec() -> uint64_t {
    assert(calibrated());
    return get_optional().value();
  }

  static auto to_msec(uint64_t cycles,
                      uint64_t cycles_per_msec = tsc::cycles_per_msec())
      -> uint64_t {
    return (cycles * 1000 + (cycles_per_msec / 2)) / (cycles_per_msec * 1000);
  }

  static void calibrate(std::optional<unsigned int> cpuid = std::nullopt) {
    get_optional().emplace(measure_cycles_per_msec_with_affinity(cpuid));
  }

  static bool calibrated() { return get_optional().has_value(); }

 private:
  static auto get_optional() -> std::optional<uint64_t>& {
    static std::optional<uint64_t> cycles_per_msec;
    return cycles_per_msec;
  }

  static auto measure_cycles_per_msec() -> uint64_t {
    uint64_t c_s;
    uint64_t c_e;
    std::chrono::nanoseconds elapsed_ns;

    auto start = std::chrono::steady_clock::now();
    decltype(start) end;

    c_s = get();
    do {
      c_e = get();
      end = std::chrono::steady_clock::now();

      elapsed_ns = end - start;
      if (elapsed_ns.count() >= 1280000) {
        break;
      }
    } while (true);

    return (c_e - c_s) * 1000000 / elapsed_ns.count();
  }

  static auto measure_cycles_per_msec_without_outlier() -> uint64_t {
    constexpr size_t nsamples = 100;
    std::vector<uint64_t> cycles_per_msec(nsamples);
    double mean;
    double stddev;
    stddev = mean = 0.0;
    for (size_t i = 0; i < nsamples; ++i) {
      cycles_per_msec[i] = measure_cycles_per_msec();
      double delta = static_cast<double>(cycles_per_msec[i]) - mean;
      if (delta != 0.0) {
        mean += delta / (i + 1.0);
        stddev += delta * (static_cast<double>(cycles_per_msec[i]) - mean);
      }
    }
    stddev = sqrt(stddev / (nsamples - 1.0));

    uint64_t sum = 0;
    size_t samples = 0;
    for (auto const cpm : cycles_per_msec) {
      if (fmax(cpm, mean) - fmin(cpm, mean) < stddev) {
        sum += cpm;
        samples++;
      }
    }
    return sum / samples;
  }

  static auto measure_cycles_per_msec_with_affinity(
      std::optional<size_t> cpuid = std::nullopt) -> uint64_t {
    if (!cpuid.has_value()) {
      detail::random_cpu_selector selector;
      cpuid = selector.get_random_cpu_id();
    }
    cpu_affinity_manager cpu_affinity(*cpuid);
    return measure_cycles_per_msec_without_outlier();
  }
};

}  // namespace rpmbb::util
