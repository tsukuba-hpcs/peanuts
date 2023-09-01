#pragma once
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace rpmbb {

class welford {
 public:
  welford() : n_(0), mean_(0.0), m2_(0.0) {}

  void add(uint64_t x) {
    ++n_;
    double delta = static_cast<double>(x) - mean_;
    mean_ += delta / static_cast<double>(n_);
    double delta2 = static_cast<double>(x) - mean_;
    m2_ += delta * delta2;
  }

  double mean() const {
    if (n_ < 1) {
      throw std::runtime_error("Mean is undefined for empty data set");
    }
    return mean_;
  }

  double var() const {
    if (n_ < 2) {
      throw std::runtime_error(
          "Variance is undefined for data set with fewer than 2 elements");
    }
    return m2_ / static_cast<double>(n_ - 1);
  }

  double std() const { return std::sqrt(var()); }

 private:
  uint64_t n_;
  double mean_;
  double m2_;
};

}  // namespace rpmbb
