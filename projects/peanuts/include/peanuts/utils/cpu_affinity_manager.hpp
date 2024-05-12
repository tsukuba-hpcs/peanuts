#pragma once

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <pthread.h>
#include <sched.h>
#include <system_error>

namespace peanuts::utils {

class cpu_affinity_manager {
 public:
  cpu_affinity_manager(const cpu_affinity_manager&) = delete;
  auto operator=(const cpu_affinity_manager&) -> cpu_affinity_manager& = delete;
  cpu_affinity_manager(cpu_affinity_manager&&) = delete;
  auto operator=(cpu_affinity_manager&&) -> cpu_affinity_manager& = delete;

  explicit cpu_affinity_manager(unsigned int cpu_id)
      : original_mask_(get_current_mask()), cpu_id_(cpu_id) {
    set_affinity(cpu_id_);
  }

  ~cpu_affinity_manager() {
    try {
      set_affinity(original_mask_);
    } catch (...) {
    }
  }

  static auto get_current_mask() -> cpu_set_t {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &mask);
    return mask;
  }

  static void set_affinity(int cpu_id) {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu_id, &mask);
    return set_affinity(mask);
  }

  static void set_affinity(const cpu_set_t& mask) {
    if (int rc =
            pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &mask);
        rc != 0) {
      throw std::system_error(rc, std::system_category(),
                              "Failed to set CPU affinity");
    }
  }

  static auto get_current_cpu_id() -> int { return sched_getcpu(); }

  cpu_set_t original_mask_;
  unsigned int cpu_id_;
};

}  // namespace peanuts::utils
