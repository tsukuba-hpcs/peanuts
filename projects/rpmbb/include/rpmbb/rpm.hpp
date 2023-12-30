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
               const std::string& pmem_path,
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
        block_size_{
            utils::round_down_pow2<size_t>(map_.size() / topo().intra_size(),
                                           pmem_alignment)},
        aligned_size_{block_size_ * topo().intra_size()} {}

  std::ostream& inspect(std::ostream& os) const {
    os << "rpm" << std::endl;
    os << "  size: " << utils::to_human(size()) << std::endl;
    os << "  block_size: " << utils::to_human(block_size()) << std::endl;
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
  auto topo() const -> const topology& { return topo_.get(); }

  auto data() const -> void* { return map_.address(); }
  auto size() const -> size_t { return aligned_size_; }

  auto block_data(int intra_rank) const -> void* {
    return static_cast<std::byte*>(data()) + block_disp(intra_rank);
  }
  auto block_size() const -> size_t { return block_size_; }
  auto block_disp(int intra_rank) const -> off_t {
    return intra_rank * block_size_;
  }
  auto block_disp_from_global(int global_rank) const -> off_t {
    return block_disp(topo().global2intra_rank(global_rank));
  }

  auto win_target_rank(int global_rank) const -> int {
    return topo().global2intra_rank0_global_rank(global_rank);
  }

  auto get(std::span<std::byte> buf, int win_target_rank, off_t ofs) const {
    win_.get(buf, win_target_rank, mpi::aint(ofs));
  }
  auto flush(int win_target_rank) const { win_.flush(win_target_rank); }
  auto flush_all() const { win_.flush_all(); }

 private:
  pmem2::device create_pmem2_device(const std::string& pmem_path, size_t pmem_size) {
    if (topo().intra_rank() == 0) {
      auto device = pmem2::device{pmem_path};
      if (!device.is_devdax() && pmem_size > 0) {
        if (topo_.get().intra_rank() == 0) {
          device.truncate(pmem_size);
        }
      }
      topo().intra_comm().barrier();
      return device;
    } else {
      topo().intra_comm().barrier();
      return pmem2::device{pmem_path};
    }
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
  size_t block_size_ = 0;
  size_t aligned_size_ = 0;
};

class rpm_local_block {
 public:
  explicit rpm_local_block(const rpm& rpm)
      : rpm_local_block(rpm, rpm.topo().intra_rank()) {}
  explicit rpm_local_block(const rpm& rpm, int intra_rank)
      : rpm_ref_{std::cref(rpm)}, disp_{rpm.block_disp(intra_rank)} {}

  auto rpm() const -> const class rpm& { return rpm_ref_.get(); }

  auto data() const -> void* {
    return static_cast<std::byte*>(rpm().data()) + disp_;
  }
  auto size() const -> size_t { return rpm().block_size(); }

  auto pwrite(std::span<const std::byte> buf,
              off_t offset,
              unsigned flags = 0) const -> void {
    rpm().file_ops().pwrite(buf, disp_ + offset, flags);
  }
  auto pwrite_nt(std::span<const std::byte> buf, off_t offset) const -> void {
    pwrite(buf, offset, PMEM2_F_MEM_NONTEMPORAL);
  }
  auto pread(std::span<std::byte> buf, off_t offset) const -> void {
    rpm().file_ops().pread(buf, disp_ + offset);
  }

  auto drain() const -> void { rpm().mem_ops().drain(); }
  auto flush(off_t ofs, size_t size) const -> void {
    rpm().file_ops().flush(disp_ + ofs, size);
  }
  auto persist(off_t ofs, size_t size) const -> void {
    rpm().file_ops().persist(disp_ + ofs, size);
  }

 private:
  std::reference_wrapper<const rpmbb::rpm> rpm_ref_;
  off_t disp_ = 0;
};

class rpm_blocks {
 public:
  explicit rpm_blocks(const rpm& rpm_instance)
      : rpm_ref_{std::cref(rpm_instance)},
        rank_accessed_{NO_RANK_ACCESSED},
        rank_info_{initialize_rank_info()} {}

  auto block_size() const -> size_t { return rpm_ref_.get().block_size(); }

  void pread(std::span<std::byte> buf, int rank, off_t offset) const {
    pread_noflush(buf, rank, offset);
    flush();
  }

  void pread_noflush(std::span<std::byte> buf, int rank, off_t offset) const {
    const auto& info = rank_info_[rank];
    if (!info.is_local) {
      rpm_ref_.get().get(buf, info.win_target_rank,
                         mpi::aint(info.block_disp + offset));
      if (rank_accessed_ == NO_RANK_ACCESSED) {
        rank_accessed_ = info.win_target_rank;
      } else if (rank_accessed_ != info.win_target_rank) {
        rank_accessed_ = MULTIPLE_RANKS_ACCESSED;
      }
    } else {
      rpm_ref_.get().file_ops().pread(buf, info.block_disp + offset);
    }
  }

  void flush() const {
    if (rank_accessed_ >= 0) {
      rpm_ref_.get().flush(rank_accessed_);
    } else if (rank_accessed_ == MULTIPLE_RANKS_ACCESSED) {
      rpm_ref_.get().flush_all();
    }
    rank_accessed_ = NO_RANK_ACCESSED;
  }

  std::ostream& inspect(std::ostream& os) const {
    os << "rpm_blocks" << std::endl;
    os << "  rank_accessed: " << rank_accessed_ << std::endl;
    os << "  rank_info: " << std::endl;
    for (int rank = 0; rank < rpm_ref_.get().topo().size(); ++rank) {
      const auto& info = rank_info_[rank];
      os << "    rank: " << rank << std::endl;
      os << "      is_local: " << info.is_local << std::endl;
      os << "      win_target_rank: " << info.win_target_rank << std::endl;
      os << "      block_disp: " << info.block_disp << std::endl;
    }
    return os;
  }

 private:
  static constexpr int NO_RANK_ACCESSED = -1;
  static constexpr int MULTIPLE_RANKS_ACCESSED = -2;

  struct rank_info {
    bool is_local;
    int win_target_rank;
    off_t block_disp;
  };

  auto initialize_rank_info() const -> std::vector<rank_info> {
    auto rank_info = std::vector<rpm_blocks::rank_info>();
    auto world_size = rpm_ref_.get().topo().size();
    rank_info.reserve(world_size);
    for (int rank = 0; rank < world_size; ++rank) {
      rank_info[rank] = {
          rpm_ref_.get().topo().is_local(rank),
          rpm_ref_.get().win_target_rank(rank),
          rpm_ref_.get().block_disp_from_global(rank),
      };
    }
    return rank_info;
  }

  std::reference_wrapper<const rpm> rpm_ref_;
  mutable int rank_accessed_;
  std::vector<rank_info> rank_info_;
};

class rpm_remote_block {
 public:
  rpm_remote_block(const rpm_blocks& rpm_blocks, int global_rank)
      : rpm_blocks_ref_{std::cref(rpm_blocks)}, global_rank_{global_rank} {}

  auto size() const -> size_t { return rpm_blocks().block_size(); }
  auto pread(std::span<std::byte> buf, off_t ofs) const -> void {
    rpm_blocks().pread(buf, global_rank_, ofs);
  }
  auto pread_noflush(std::span<std::byte> buf, off_t ofs) const -> void {
    rpm_blocks().pread_noflush(buf, global_rank_, ofs);
  }
  auto flush() const -> void { rpm_blocks().flush(); }

 private:
  auto rpm_blocks() const -> const class rpm_blocks& {
    return rpm_blocks_ref_.get();
  }

  std::reference_wrapper<const rpmbb::rpm_blocks> rpm_blocks_ref_;
  int global_rank_;
};

}  // namespace rpmbb
