#pragma once

#include <unistd.h>
#include <cstddef>

namespace peanuts::utils {

inline std::size_t get_page_size() {
  static std::size_t pagesize = sysconf(_SC_PAGE_SIZE);
  return pagesize;
}

}  // namespace peanuts::utils
