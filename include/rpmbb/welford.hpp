#pragma once

#include <cmath>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace rpmbb {

class welford {
 public:
  void add(uint64_t x) {
    ++n_;
    double delta = static_cast<double>(x) - mean_;
    mean_ += delta / static_cast<double>(n_);
    double delta2 = static_cast<double>(x) - mean_;
    m2_ += delta * delta2;
  }

  auto n() const -> uint64_t { return n_; }

  auto mean() const -> double {
    if (n_ < 1) {
      throw std::runtime_error("Mean is undefined for empty data set");
    }
    return mean_;
  }

  auto var() const -> double {
    if (n_ < 2) {
      throw std::runtime_error(
          "Variance is undefined for data set with fewer than 2 elements");
    }
    return m2_ / static_cast<double>(n_ - 1);
  }

  auto std() const -> double { return std::sqrt(var()); }

  template <typename Iterator>
  static welford from_range(Iterator begin, Iterator end) {
    uint64_t total_n = 0;
    double total_mean = 0.0;
    double total_m2 = 0.0;

    for (auto it = begin; it != end; ++it) {
      uint64_t n = it->n();
      double mean = it->mean();

      total_n += n;
      total_mean += mean * n;
    }

    if (total_n == 0) {
      throw std::runtime_error("No data to aggregate");
    }

    double grand_mean = total_mean / total_n;

    for (auto it = begin; it != end; ++it) {
      uint64_t n = it->n();
      double mean = it->mean();
      double m2 = it->m2_;

      total_m2 += m2 + n * (mean - grand_mean) * (mean - grand_mean);
    }

    welford result;
    result.n_ = total_n;
    result.mean_ = grand_mean;
    result.m2_ = total_m2;

    return result;
  }

  template <typename Container>
  static welford from_range(const Container& container) {
    return from_range(std::begin(container), std::end(container));
  }

 private:
  uint64_t n_{0};
  double mean_{0.0};
  double m2_{0.0};
};

}  // namespace rpmbb
