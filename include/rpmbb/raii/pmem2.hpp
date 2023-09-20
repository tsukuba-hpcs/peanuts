#pragma once

#include <libpmem2.h>

namespace rpmbb::raii {
namespace detail {

struct pmem2_source_deleter {
  using pointer = ::pmem2_source*;
  void operator()(pointer src) { ::pmem2_source_delete(&src); }
};

struct pmem2_config_deleter {
  using pointer = ::pmem2_config*;
  void operator()(pointer cfg) { ::pmem2_config_delete(&cfg); }
};

struct pmem2_map_deleter {
  using pointer = ::pmem2_map*;
  void operator()(pointer map) { ::pmem2_map_delete(&map); }
};

}  // namespace detail

using pmem2_source =
    std::unique_ptr<::pmem2_source, detail::pmem2_source_deleter>;

using pmem2_config =
    std::unique_ptr<::pmem2_config, detail::pmem2_config_deleter>;

using pmem2_map = std::unique_ptr<::pmem2_map, detail::pmem2_map_deleter>;

}  // namespace rpmbb::raii
