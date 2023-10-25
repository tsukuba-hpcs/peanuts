#pragma once

#include "rpmbb/mpi/win.hpp"
#include "rpmbb/pmem2.hpp"
#include "rpmbb/topology.hpp"
#include "rpmbb/utils/human_readable.hpp"
#include "rpmbb/utils/power.hpp"

#include <libpmem2.h>

#include <functional>
#include <mutex>
#include <ostream>

namespace rpmbb {
class rpm {
 public:
  static constexpr size_t pmem_alignment = 1ULL << 21;

  explicit rpm(std::reference_wrapper<const topology> topo_ref,
               std::string_view pmem_path,
               size_t pmem_size = 0)
      : topo_(std::move(topo_ref)),
        device_{create_pmem2_device(pmem_path, pmem_size)},
        source_{device_},
        config_{PMEM2_GRANULARITY_PAGE},
        map_{source_, config_},
        ops_{map_},
        win_{create_win()},
        win_mutex_{win_, MPI_MODE_NOCHECK},
        win_lock_{win_mutex_},
        local_size_{utils::round_down_pow2(map_.size(), pmem_alignment)},
        region_size_{utils::round_down_pow2<size_t>(
            local_size_ / topo_.get().intra_size(),
            pmem_alignment)},
        my_local_region_disp_{local_region_disp(topo_.get().intra_rank())} {}

  std::ostream& inspect(std::ostream& os) const {
    os << "rpm" << std::endl;
    os << "  local_size: " << utils::to_human(local_size()) << std::endl;
    os << "  region_size: " << utils::to_human(region_size()) << std::endl;
    return os;
  }

  rpm(const rpm&) = delete;
  auto operator=(const rpm&) -> rpm& = delete;
  rpm(rpm&&) = default;
  rpm& operator=(rpm&&) = default;

  auto file_ops() const -> const pmem2::file_operations& { return ops_; }
  auto mem_ops() const -> const pmem2::memory_operations& {
    return ops_.mem_ops();
  }
  auto win() const -> const mpi::win& { return win_; }

  auto get_store_granularity() const -> pmem2_granularity {
    return map_.store_granularity();
  }

  auto local_size() const -> size_t { return local_size_; }
  auto global_size() const -> size_t {
    return local_size() *
           topo_.get().inter_size();  // FIXME: use topo_->node_size()
  }

  auto region_size() const -> size_t { return region_size_; }
  auto local_region_disp(int intra_rank) const -> size_t {
    return intra_rank * region_size_;
  }

 private:
  pmem2::device create_pmem2_device(std::string_view pmem_path,
                                    size_t pmem_size) {
    auto device = pmem2::device{pmem_path};
    if (!device.is_devdax() && pmem_size > 0) {
      if (topo_.get().intra_rank() == 0) {
        device.truncate(pmem_size);
      }
    }
    return device;
  }

  mpi::win create_win() {
    if (topo_.get().intra_rank() == 0) {
      return mpi::win{topo_.get().comm(), map_.as_span()};
    } else {
      return mpi::win{topo_.get().comm(), std::span<std::byte>{}};
    }
  }

 private:
  std::reference_wrapper<const topology> topo_;
  pmem2::device device_{};
  pmem2::source source_{};
  pmem2::config config_{};
  pmem2::map map_{};
  pmem2::file_operations ops_{};
  mpi::win win_{};
  mpi::win_lock_all_adapter win_mutex_{};
  std::unique_lock<mpi::win_lock_all_adapter> win_lock_{};
  size_t local_size_ = 0;
  size_t region_size_ = 0;
  size_t my_local_region_disp_ = 0;
};
}  // namespace rpmbb
