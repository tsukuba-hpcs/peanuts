#pragma once

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
             zpp::filesystem::weak_file_handle fd =
                 zpp::filesystem::weak_file::invalid_file_handle)
      : rpm_ref_{std::ref(rpm_ref)},
        local_ring_{std::ref(local_ring)},
        remote_rings_{std::cref(remote_rings)},
        bb_{std::move(bb)},
        file_{fd},
        rank_{rpm().topo().rank()} {}

  auto bb_ref() -> rpmbb::bb& { return *bb_; }

  auto has_valid_file() const -> bool { return static_cast<bool>(file_); }

  auto sync() {
    // serialize local extent tree
    auto [ser_local_tree, out] = zpp::bits::data_out();
    out(bb_->local_tree).or_throw();

    // all_gather serialized local tree size
    const auto& topo = rpm().topo();
    auto sizes = std::vector<int>(topo.size());
    topo.comm().all_gather(static_cast<int>(ser_local_tree.size()),
                           std::span{sizes});

    // all_gather_v serialized local tree
    auto [ser_local_trees, in] = zpp::bits::data_in();
    ser_local_trees.resize(std::accumulate(sizes.begin(), sizes.end(), 0ULL));
    topo.comm().all_gather_v(std::as_bytes(std::span{ser_local_tree}),
                             std::as_writable_bytes(std::span{ser_local_trees}),
                             std::span{sizes});

    // deserialize remote local trees and merge into global tree
    auto tmp_tree = extent_tree{};
    for (size_t i = 0; i < sizes.size(); ++i) {
      in(tmp_tree).or_throw();
      bb_->global_tree.merge(tmp_tree);
    }
  }

  auto pwrite(std::span<const std::byte> buf, off_t ofs) const -> ssize_t {
    auto lsn = ring().reserve_nb(buf.size());
    if (!lsn) {
      throw std::system_error{ENOSPC, std::system_category(),
                              "ring buffer full"};
    }
    ring().pwrite(buf, *lsn);
    bb_->local_tree.add(ofs, ofs + buf.size(), *lsn, rank_);
    return buf.size();
  }

  auto pread(std::span<std::byte> buf, off_t ofs) const -> ssize_t {
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
        rring(0).flush();
        return buf.size();
      }
    }

    // read remaining from file
    for (auto it = hole_el.begin(); it != hole_el.end(); ++it) {
      const auto& hole_ex = *it;
      file_.seek(hole_ex.begin, zpp::filesystem::seek_mode::set);
      auto rsize = file_.read(buf.subspan(hole_ex.begin - ofs, hole_ex.size()));
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

    // make sure read from all remote rings are completed
    rring(0).flush();

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

 private:
  std::reference_wrapper<rpmbb::rpm> rpm_ref_;
  std::reference_wrapper<local_ring_buffer> local_ring_;
  std::reference_wrapper<const std::vector<remote_ring_buffer>> remote_rings_;
  std::shared_ptr<bb> bb_;
  zpp::filesystem::weak_file file_;
  int rank_;
};

class bb_store {
  struct block_metadata {
    ring_tracker tracker;
    local_ring_buffer::lsn_t snapshot_lsn;
  };
  static_assert(std::is_trivially_copyable_v<block_metadata>);

 public:
  explicit bb_store(rpm& rpm)
      : rpm_ref_(std::ref(rpm)),
        local_block_{rpm_ref_.get(), rpm.topo().intra_rank()},
        local_ring_{create_local_ring()},
        rpm_blocks_{rpm_ref_.get()},
        remote_rings_{create_remote_rings()} {}

  std::unique_ptr<bb_handler> open(ino_t ino, int fd) {
    auto bb_obj = std::make_shared<bb>(bb{ino});
    auto [it, inserted] = bb_store_.insert(bb_obj);
    if (fd >= 0) {
      return std::make_unique<bb_handler>(rpm_ref_.get(), local_ring_,
                                          remote_rings_, *it, fd);
    } else {
      return std::make_unique<bb_handler>(rpm_ref_.get(), local_ring_,
                                          remote_rings_, *it);
    }
  }

  std::unique_ptr<bb_handler> open(int fd) {
    return open(utils::get_ino(fd), fd);
  }

  std::unique_ptr<bb_handler> open(ino_t ino) { return open(ino, -1); }

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
};

}  // namespace rpmbb
