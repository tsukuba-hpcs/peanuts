#pragma once

#include "rpmbb/config.hpp"
#include "rpmbb/deferred_file.hpp"
#include "rpmbb/extent_list.hpp"
#include "rpmbb/extent_tree.hpp"
#include "rpmbb/raii/fd.hpp"
#include "rpmbb/ring_buffer.hpp"
#include "rpmbb/rpm.hpp"
#include "rpmbb/utils/fs.hpp"

#include <zpp/file.h>
#include <zpp_bits.h>

#include <sys/types.h>
#include <functional>
#include <memory>
#include <span>
#include <unordered_map>

namespace rpmbb {

struct bb {
  ino_t ino;
  extent_tree global_tree{};
  extent_tree local_tree{};

  using serialize = zpp::bits::members<3>;
};

namespace detail {
struct bb_hash {
  std::size_t operator()(const std::shared_ptr<bb>& bb) const {
    return std::hash<ino_t>()(bb->ino);
  }
};

struct bb_equal {
  bool operator()(const std::shared_ptr<bb>& lhs,
                  const std::shared_ptr<bb>& rhs) const {
    return lhs->ino == rhs->ino;
  }
};
}  // namespace detail

class bb_handler {
 public:
  bb_handler(rpmbb::rpm& rpm_ref,
             local_ring_buffer& local_ring,
             const std::vector<remote_ring_buffer>& remote_rings,
             std::shared_ptr<bb> bb,
             mpi::comm comm,
             rpmbb::deferred_file&& file,
             size_t initial_file_size)
      : rpm_ref_{std::ref(rpm_ref)},
        local_ring_{std::ref(local_ring)},
        remote_rings_{std::cref(remote_rings)},
        bb_{std::move(bb)},
        comm_{std::move(comm)},
        file_{std::move(file)},
        global_rank_{rpm().topo().rank()},
        deferred_file_size_{initial_file_size} {}

  auto bb_ref() -> rpmbb::bb& { return *bb_; }

  auto size() const -> size_t {
    size_t size = 0;
    if (bb_->global_tree.size() != 0) {
      size = bb_->global_tree.back().ex.end;
    }
    if (bb_->local_tree.size() != 0) {
      size = std::max(size, bb_->local_tree.back().ex.end);
    }

    size = std::max(size, deferred_file_size_);
    return size;
  }

  // collective
  auto truncate(size_t size) -> void {
    int truncated = 0;
    if (comm_.rank() == 0) {
      try {
        file_.truncate(size);
      } catch (...) {
        truncated = -1;
      }
    }

    comm_.broadcast(truncated);
    if (truncated != 0) {
      throw std::system_error{errno, std::system_category(),
                              "Failed to truncate file"};
    }

    if (bb_->local_tree.size() != 0 && bb_->local_tree.back().ex.end > size) {
      bb_->local_tree.remove(size, UINT64_MAX);
    }

    if (bb_->global_tree.size() != 0 && bb_->global_tree.back().ex.end > size) {
      bb_->global_tree.remove(size, UINT64_MAX);
    }

    deferred_file_size_ = size;
  }

  // collective
  auto sync() {
    // serialize local extent tree
    auto [ser_local_tree, out] = zpp::bits::data_out();
    out(bb_->local_tree).or_throw();

    // all_gather serialized local tree size
    const auto& topo = rpm().topo();
    auto sizes = std::vector<int>(topo.size());
    comm_.all_gather(static_cast<int>(ser_local_tree.size()), std::span{sizes});

    // all_gather_v serialized local tree
    auto [ser_local_trees, in] = zpp::bits::data_in();
    ser_local_trees.resize(std::accumulate(sizes.begin(), sizes.end(), 0ULL));
    comm_.all_gather_v(std::as_bytes(std::span{ser_local_tree}),
                       std::as_writable_bytes(std::span{ser_local_trees}),
                       std::span{sizes});

    // deserialize remote local trees and merge into global tree
    auto tmp_tree = extent_tree{};
    for (size_t i = 0; i < sizes.size(); ++i) {
      if (in.remaining_data().empty()) {
        break;
      }
      in(tmp_tree).or_throw();
      bb_->global_tree.merge(tmp_tree);
    }

    // clear merged local tree
    bb_->local_tree.clear();

    deferred_file_size_ = get_and_broadcast_file_size();
  }

  auto pwrite(std::span<const std::byte> buf, off_t ofs) const -> ssize_t {
    auto lsn = ring().reserve_nb(buf.size());
    if (!lsn) {
      throw std::system_error{ENOSPC, std::system_category(),
                              "ring buffer full"};
    }
    ring().pwrite(buf, *lsn);
    bb_->local_tree.add(ofs, ofs + buf.size(), *lsn, global_rank_);
    return buf.size();
  }

  auto flush() const -> void { rring(0).flush(); }

  auto pread(std::span<std::byte> buf, off_t ofs) -> ssize_t {
    auto ret = pread_noflush(buf, ofs);
    flush();
    return ret;
  }

  auto pread_noflush(std::span<std::byte> buf, off_t ofs) -> ssize_t {
    auto el = extent_list{};
    auto user_buf_extent = extent{static_cast<uint64_t>(ofs),
                                  static_cast<uint64_t>(ofs) + buf.size()};
    uint64_t eof = 0;

    if (bb_->local_tree.size() != 0) {
      eof = bb_->local_tree.back().ex.end;

      // read from local ring
      for (auto it = bb_->local_tree.find(user_buf_extent);
           it != bb_->local_tree.end(); ++it) {
        if (!it->ex.overlaps(user_buf_extent)) {
          break;
        }
        auto valid_extent = it->ex.get_intersection(user_buf_extent);
        el.add(valid_extent);
        ring().pread(buf.subspan(valid_extent.begin - ofs, valid_extent.size()),
                     it->ptr + (valid_extent.begin - it->ex.begin));
      }
    }

    auto hole_el = el.inverse(user_buf_extent);
    if (hole_el.empty()) {
      return buf.size();
    }

    // read remaining from remote rings
    if (bb_->global_tree.size() != 0) {
      eof = std::max(eof, bb_->global_tree.back().ex.end);

      auto hole_it = hole_el.begin();
      auto global_it = bb_->global_tree.find(hole_el.outer_extent());
      while (hole_it != hole_el.end() && global_it != bb_->global_tree.end()) {
        auto& hole_ex = *hole_it;
        auto& global_ex = global_it->ex;

        if (hole_ex.end <= global_ex.begin) {
          ++hole_it;
        } else if (global_ex.end <= hole_ex.begin) {
          ++global_it;
        } else {
          // has overlap
          auto valid_ex = hole_ex.get_intersection(global_ex);
          el.add(valid_ex);

          // read from remote rings
          rring(global_it->client_id)
              .pread_noflush(
                  buf.subspan(valid_ex.begin - ofs, valid_ex.size()),
                  global_it->ptr + (valid_ex.begin - global_ex.begin));

          if (hole_ex.end < global_ex.end) {
            ++hole_it;
          } else if (global_ex.end < hole_ex.end) {
            ++global_it;
          } else {
            ++hole_it;
            ++global_it;
          }
        }
      }

      hole_el = el.inverse(user_buf_extent);
      if (hole_el.empty()) {
        return buf.size();
      }
    }

    // read remaining from file
    for (auto it = hole_el.begin(); it != hole_el.end(); ++it) {
      const auto& hole_ex = *it;
      auto rsize = file_.pread(buf.subspan(hole_ex.begin - ofs, hole_ex.size()),
                               hole_ex.begin);
      auto remain = hole_ex.size() - rsize;
      if (remain > 0) {
        // reach the end of file,
        if (hole_ex.begin + rsize >= eof) {
          break;
        }

        // in this case, this is not an actual EOF.
        // fill the rest with zeros
        // until reach the EOF from the extent_tree view.

        auto fill_size = std::min(remain, eof - (hole_ex.begin + rsize));
        std::memset(buf.data() + (hole_ex.begin - ofs + rsize), 0, fill_size);

        // remaining holes should be filled with zeros
        for (; it != hole_el.end(); ++it) {
          const auto& hole_ex = *it;
          if (hole_ex.begin >= eof) {
            // reach true EOF
            break;
          }
          fill_size = std::min(hole_ex.size(), eof - hole_ex.begin);
          std::memset(buf.data() + (hole_ex.begin - ofs), 0, fill_size);
        }
      }
    }

    if (ofs + buf.size() >= eof) {
      return eof - ofs;
    } else {
      return buf.size();
    }
  }

 private:
  auto ring() const -> local_ring_buffer& { return local_ring_.get(); }
  auto rring(int rank) const -> const remote_ring_buffer& {
    return remote_rings_.get()[rank];
  }
  auto rpm() const -> rpm& { return rpm_ref_.get(); }

  auto get_and_broadcast_file_size() -> ssize_t {
    ssize_t file_size = -1;
    if (comm_.rank() == 0) {
      try {
        file_size = file_.size();
      } catch (...) {
      }
    }
    comm_.broadcast(file_size);
    if (file_size < 0) {
      throw std::system_error{errno, std::system_category(),
                              "Failed to get file size"};
    }
    return file_size;
  }

 private:
  std::reference_wrapper<rpmbb::rpm> rpm_ref_;
  std::reference_wrapper<local_ring_buffer> local_ring_;
  std::reference_wrapper<const std::vector<remote_ring_buffer>> remote_rings_;
  std::shared_ptr<bb> bb_;
  mpi::comm comm_;
  deferred_file file_;
  int global_rank_;
  size_t deferred_file_size_ = 0;
};

class bb_store {
  struct block_metadata {
    ring_tracker tracker;
    local_ring_buffer::lsn_t snapshot_lsn;
    size_t snapshot_size;
  };
  static_assert(std::is_trivially_copyable_v<block_metadata>);

  struct ino_and_size {
    ino_t ino;
    ssize_t size;
  };

 public:
  explicit bb_store(rpm& rpm)
      : rpm_ref_(std::ref(rpm)),
        local_block_{rpm_ref_.get(), rpm.topo().intra_rank()},
        local_ring_{create_local_ring()},
        rpm_blocks_{rpm_ref_.get()},
        remote_rings_{create_remote_rings()},
        ino_and_size_dtype_{create_ino_and_size_dtype()} {}

  auto save() -> void {
    // save bb indices
    auto snapshot_lsn = local_ring_.head();
    auto [ser_bb, out] = zpp::bits::data_out();
    for (const auto& bb_ptr : bb_store_) {
      ser_bb.resize(sizeof(size_t));
      out.position() += sizeof(size_t);
      out(*bb_ptr).or_throw();
      *reinterpret_cast<size_t*>(ser_bb.data()) =
          ser_bb.size() - sizeof(size_t);
      auto lsn = local_ring_.reserve_nb(ser_bb.size());
      if (!lsn) {
        throw std::runtime_error("bb_store::save(): local ring is full");
      }
      local_ring_.pwrite(ser_bb, *lsn);
      ser_bb.clear();
      out.reset();
    }
    save_block_metadata_to_local_block(
        block_metadata{local_ring_.tracker(), snapshot_lsn,
                       local_ring_.head() - snapshot_lsn});
  }

  auto load() -> void {
    local_ring_.set_tracker(local_block_metadata().tracker);
    auto snapshot_lsn = local_block_metadata().snapshot_lsn;

    // load bb indices
    auto cur_lsn = snapshot_lsn;
    while (cur_lsn < local_ring_.head()) {
      size_t serialized_size;
      // Read the size
      local_ring_.pread(std::as_writable_bytes(std::span{&serialized_size, 1}),
                        cur_lsn);
      cur_lsn += sizeof(size_t);

      auto temp_buffer = std::vector<std::byte>(serialized_size);
      local_ring_.pread(temp_buffer, cur_lsn);
      cur_lsn += serialized_size;

      auto in = zpp::bits::in{temp_buffer};
      auto bb_ptr = std::make_shared<bb>();
      in(*bb_ptr).or_throw();
      bb_store_.insert(bb_ptr);
    }
  }

  auto open(mpi::comm comm,
            const std::string& pathname,
            int flags,
            mode_t mode) {
#ifndef RPMBB_USE_DEFERRED_OPEN
#warning "RPMBB_USE_DEFERRED_OPEN is not defined. This may cause performance degradation."
    auto meta = ino_and_size{0, -1};
    auto file = rpmbb::deferred_file{pathname, flags, mode};
    try {
      file.open();
      struct stat stat_buf;
      if (fstat(file.fd(), &stat_buf) != 0) {
        throw std::system_error(errno, std::system_category(),
                                "bb_store::open(): Failed fstat");
      }
      meta.ino = stat_buf.st_ino;
      meta.size = static_cast<ssize_t>(stat_buf.st_size);
    } catch (...) {
    }
#else
    if (comm.rank() != 0) {
      flags &= ~(O_CREAT | O_EXCL | O_TRUNC);
    }

    auto file = rpmbb::deferred_file{pathname, flags, mode};
    auto meta = ino_and_size{0, -1};
    try {
      if (comm.rank() == 0) {
        file.open();

        struct stat stat_buf;
        if (fstat(file.fd(), &stat_buf) != 0) {
          throw std::system_error(errno, std::system_category(),
                                  "bb_store::open(): Failed fstat");
        }

        meta.ino = stat_buf.st_ino;
        meta.size = static_cast<ssize_t>(stat_buf.st_size);
      }
    } catch (...) {
    }
#endif

    comm.broadcast(meta, ino_and_size_dtype_);
    if (meta.size < 0) {
      throw std::system_error{errno, std::system_category(),
                              "bb_store::open(): Failed to get file size"};
    }

    auto bb_obj = std::make_shared<bb>(bb{meta.ino});
    auto [it, inserted] = bb_store_.insert(bb_obj);
    return std::make_unique<bb_handler>(rpm_ref_.get(), local_ring_,
                                        remote_rings_, *it, std::move(comm),
                                        std::move(file), meta.size);
  }

  void unlink(const std::string& pathname) { unlink(utils::get_ino(pathname)); }
  void unlink(ino_t ino) { bb_store_.erase(std::make_shared<bb>(bb{ino})); }
  void unlink(int fd) { unlink(utils::get_ino(fd)); }

  auto local_ring() -> local_ring_buffer& { return local_ring_; }

  std::ostream& inspect(std::ostream& os) const {
    os << "rpm_blocks: " << utils::make_inspector(rpm_blocks_) << "\n";
    return os;
  }

 private:
  auto ring_size() const -> size_t {
    return rpm_ref_.get().block_size() - sizeof(block_metadata);
  }

  auto create_local_ring() -> local_ring_buffer {
    auto& rpm = rpm_ref_.get();
    if (rpm.block_size() < sizeof(block_metadata)) {
      throw std::runtime_error("pmem size is too small");
    }
    return local_ring_buffer{local_block_, ring_size()};
  }

  auto create_remote_rings() -> std::vector<remote_ring_buffer> {
    auto& rpm = rpm_ref_.get();
    auto remote_rings = std::vector<remote_ring_buffer>{};
    auto world_size = rpm.topo().size();
    remote_rings.reserve(world_size);
    for (int rank = 0; rank < world_size; ++rank) {
      remote_rings.emplace_back(rpm_remote_block{rpm_blocks_, rank},
                                ring_size());
    }
    return remote_rings;
  }

  auto create_ino_and_size_dtype() -> mpi::dtype {
    auto dtypes = std::vector<MPI_Datatype>{mpi::to_dtype<ino_t>().native(),
                                            mpi::to_dtype<ssize_t>().native()};
    auto block_lengths = std::vector<int>{1, 1};
    auto displacements = std::vector<MPI_Aint>{offsetof(ino_and_size, ino),
                                               offsetof(ino_and_size, size)};
    auto dtype = mpi::dtype{dtypes, block_lengths, displacements};
    dtype.commit();
    return dtype;
  }

  auto save_block_metadata_to_local_block(const block_metadata& meta) -> void {
    local_block_.pwrite_nt(
        std::span<const std::byte>{reinterpret_cast<const std::byte*>(&meta),
                                   sizeof(block_metadata)},
        static_cast<off_t>(ring_size()));
  }

  auto local_block_metadata() const -> const block_metadata& {
    return *reinterpret_cast<const block_metadata*>(
        static_cast<std::byte*>(local_block_.data()) + ring_size());
  }

  std::unordered_set<std::shared_ptr<bb>, detail::bb_hash, detail::bb_equal>
      bb_store_{};
  std::reference_wrapper<rpm> rpm_ref_;
  rpm_local_block local_block_;
  local_ring_buffer local_ring_;
  rpm_blocks rpm_blocks_;
  std::vector<remote_ring_buffer> remote_rings_;
  mpi::dtype ino_and_size_dtype_;
};

}  // namespace rpmbb
