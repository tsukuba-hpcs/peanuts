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

  auto intra_offset(int intra_rank, off_t block_offset) const -> off_t {
    return block_disp(intra_rank) + block_offset;
  }

  auto win_target_rank(int global_rank) const -> int {
    return topo().global2intra_rank0_global_rank(global_rank);
  }

  auto win_block_disp(int global_rank) const -> off_t {
    return block_disp(topo().global2intra_rank(global_rank));
  }

  auto get(std::span<std::byte> buf, int win_target_rank, off_t ofs) const {
    win_.get(buf, win_target_rank, mpi::aint(ofs));
  }
  auto flush(int win_target_rank) const { win_.flush(win_target_rank); }

  auto pread(std::span<std::byte> buf, int global_rank, off_t ofs) const {
    auto rank = win_target_rank(global_rank);
    get(buf, rank, win_block_disp(global_rank) + ofs);
    flush(rank);
  }
  auto pread_noflush(std::span<std::byte> buf,
                     int global_rank,
                     off_t ofs) const {
    get(buf, win_target_rank(global_rank), win_block_disp(global_rank) + ofs);
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

class rpm_remote_block {
 public:
  explicit rpm_remote_block(const rpm& rpm, int global_rank)
      : rpm_ref_{std::cref(rpm)},
        win_target_rank_{rpm.win_target_rank(global_rank)},
        win_block_disp_{rpm.win_block_disp(global_rank)} {}

  auto rpm() const -> const class rpm& { return rpm_ref_.get(); }

  auto size() const -> size_t { return rpm().block_size(); }

  auto flush() const { rpm().win().flush(win_target_rank_); }

  auto get(std::span<std::byte> buf, off_t ofs) const {
    rpm().win().get(buf, win_target_rank_, mpi::aint(win_block_disp_ + ofs));
  }

  auto pread_noflush(std::span<std::byte> buf, off_t ofs) const {
    get(buf, ofs);
  }

  auto pread(std::span<std::byte> buf, off_t ofs) const {
    get(buf, ofs);
    flush();
  }

 private:
  std::reference_wrapper<const rpmbb::rpm> rpm_ref_;
  int win_target_rank_ = 0;
  off_t win_block_disp_ = 0;
};

}  // namespace rpmbb
