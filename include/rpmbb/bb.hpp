#pragma once

#include "rpmbb/extent_tree.hpp"
#include "rpmbb/raii/fd.hpp"
#include "rpmbb/ring_buffer.hpp"
#include "rpmbb/rpm.hpp"
#include "rpmbb/utils/fs.hpp"

#include <zpp/file.h>

#include <sys/types.h>
#include <functional>
#include <memory>
#include <unordered_map>

namespace rpmbb {

struct bb {
  ino_t ino;
  extent_tree global_tree{};
  extent_tree local_tree{};
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
  bb_handler(std::reference_wrapper<rpmbb::rpm> rpm_ref,
             std::reference_wrapper<ring_buffer> local_ring,
             std::shared_ptr<bb> bb,
             zpp::filesystem::weak_file_handle fd =
                 zpp::filesystem::weak_file::invalid_file_handle)
      : rpm_ref_{rpm_ref},
        local_ring_{local_ring},
        bb_{std::move(bb)},
        file_{fd},
        rank_{rpm().topo().rank()} {}

  auto bb_ref() -> rpmbb::bb& { return *bb_; }

  auto has_valid_file() const -> bool { return static_cast<bool>(file_); }

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
    // TODO: priority = local_tree -> global_tree -> file
    // now only global_tree -> file
    auto it = bb_->global_tree.find(ofs, ofs + buf.size());
    size_t bytes_read = 0;
    while (bytes_read < buf.size()) {
      if (it == bb_->global_tree.end()) {
        return bytes_read;
      }

      auto read_ofs = ofs + bytes_read;
      auto extent_ofs = it->ex.begin;
      if (read_ofs < extent_ofs) {
        // need to read from file
        file_.seek(read_ofs, zpp::filesystem::seek_mode::set);
        auto gap_size = extent_ofs - read_ofs;
        auto rsize = file_.read(buf.subspan(bytes_read, gap_size));
        bytes_read += rsize;
        if (rsize < gap_size) {
          // reach the end of file, in this case, this is not an actual EOF.
          // fill the rest with zeros
          auto fill_size = std::min(buf.size() - bytes_read, gap_size - rsize);
          std::memset(buf.data() + bytes_read, 0, fill_size);
          bytes_read += fill_size;
        }
        read_ofs += rsize;
      }
      assert(read_ofs >= extent_ofs);

      auto ofs_in_extent = read_ofs - extent_ofs;
      auto rlsn = it->ptr + ofs_in_extent;
      auto rsize = std::min(buf.size() - bytes_read, it->ex.end - read_ofs);
      // FIXME: need to consider wrapping around
      // FIXME: use rpm_remote_block
      rpm().pread(buf.subspan(bytes_read, rsize), it->client_id, ring().to_ofs(rlsn));
      bytes_read += rsize;
      ++it;
    }
    return bytes_read;
  }

 private:
  auto ring() const -> ring_buffer& { return local_ring_.get(); }
  auto rpm() const -> rpm& { return rpm_ref_.get(); }

 private:
  std::reference_wrapper<rpmbb::rpm> rpm_ref_;
  std::reference_wrapper<ring_buffer> local_ring_;
  std::shared_ptr<bb> bb_;
  zpp::filesystem::weak_file file_;
  int rank_;
};

class bb_store {
 public:
  bb_store(std::reference_wrapper<rpm> rpm_ref)
      : rpm_ref_(rpm_ref), local_ring_{rpm_ref_.get()} {}

  std::unique_ptr<bb_handler> open(ino_t ino, int fd) {
    auto bb_obj = std::make_shared<bb>(bb{ino});
    auto [it, inserted] = bb_store_.insert(bb_obj);
    if (fd >= 0) {
      return std::make_unique<bb_handler>(rpm_ref_, std::ref(local_ring_), *it,
                                          fd);
    } else {
      return std::make_unique<bb_handler>(rpm_ref_, std::ref(local_ring_), *it);
    }
  }

  std::unique_ptr<bb_handler> open(int fd) {
    return open(utils::get_ino(fd), fd);
  }

  std::unique_ptr<bb_handler> open(ino_t ino) { return open(ino, -1); }

  void unlink(ino_t ino) { bb_store_.erase(std::make_shared<bb>(bb{ino})); }
  void unlink(int fd) { unlink(utils::get_ino(fd)); }

  auto local_ring() -> ring_buffer& { return local_ring_; }

 private:
  std::unordered_set<std::shared_ptr<bb>, detail::bb_hash, detail::bb_equal>
      bb_store_{};
  std::reference_wrapper<rpm> rpm_ref_;
  ring_buffer local_ring_;
};

}  // namespace rpmbb
