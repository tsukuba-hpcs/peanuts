#pragma once
#include <atomic>
#include <vector>

#include "tls.hpp"

namespace rpmbb::utils {

class sense_barrier {
  sense_barrier() = delete;

 public:
  explicit sense_barrier(size_t nthreads)
      : nthreads_(static_cast<int>(nthreads)),
        waits_(static_cast<int>(nthreads)),
        sense_(true),
        tl_sense_(false) {}

  void wait() {
    bool my_sense = tl_sense_.get();
    tl_sense_.set(!my_sense);

    if (waits_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      waits_.store(nthreads_, std::memory_order_relaxed);
      sense_.store(my_sense, std::memory_order_release);
    } else {
      while (my_sense != sense_.load(std::memory_order_acquire))
        ;
    }
  }

 private:
  const int nthreads_;
  std::atomic<int> waits_;
  std::atomic<bool> sense_;
  thread_local_value<bool> tl_sense_;
};

}  // namespace rpmbb::utils
