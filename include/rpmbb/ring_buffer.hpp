#pragma once

#include "rpmbb/ring_tracker.hpp"
#include "rpmbb/rpm.hpp"

#include <optional>

namespace rpmbb {

class ring_buffer {
 public:
  using lsn_t = ring_tracker::lsn_t;

  explicit ring_buffer(const rpm& rpm)
      : ring_buffer(rpm, rpm.topo().intra_rank()) {}
  ring_buffer(const rpm& rpm, int intra_rank)
      : ring_buffer(rpm, intra_rank, rpm.block_size()) {}
  ring_buffer(const rpm& rpm, int intra_rank, size_t ring_size)
      : tracker_{ring_size}, rpm_block_{rpm, intra_rank} {
    assert(ring_size <= rpm.block_size());
  }

  auto can_reserve(size_t size) const -> bool {
    return tracker_.can_allocate(size);
  }
  auto can_consume(size_t size) const -> bool {
    return tracker_.can_release(size);
  }

  auto reserve_nb(size_t size) -> std::optional<lsn_t> {
    if (can_reserve(size)) {
      return tracker_.allocate(size);
    } else {
      return std::nullopt;
    }
  }

  auto reserve_unsafe(size_t size) -> lsn_t { return tracker_.allocate(size); }

  auto consume_nb(size_t size) -> std::optional<lsn_t> {
    if (can_consume(size)) {
      tracker_.release(size);
      return tracker_.tail();
    } else {
      return std::nullopt;
    }
  }

  auto consume_unsafe(size_t size) -> lsn_t {
    tracker_.release(size);
    return tracker_.tail();
  }

  size_t used_capacity() const { return tracker_.used_capacity(); }
  size_t free_capacity() const { return tracker_.free_capacity(); }
  size_t size() const { return tracker_.ring_size(); }
  lsn_t head() const { return tracker_.head(); }
  lsn_t tail() const { return tracker_.tail(); }
  uint64_t to_ofs(lsn_t lsn) const { return tracker_.to_ofs(lsn); }
  static constexpr uint64_t to_ofs(lsn_t lsn, size_t ring_size) {
    return ring_tracker::to_ofs(lsn, ring_size);
  }

  auto pread(std::span<std::byte> buf, lsn_t lsn) const -> void {
    auto ofs = tracker_.to_ofs(lsn);
    auto size = tracker_.first_segment_size_ofs(ofs, buf.size());
    if (size == buf.size()) {
      rpm_block_.pread(buf, ofs);
    } else {
      rpm_block_.pread(buf.subspan(0, size), ofs);
      rpm_block_.pread(buf.subspan(size), 0);
    }
  }

  auto pwrite(std::span<const std::byte> buf, lsn_t lsn) const -> void {
    auto ofs = tracker_.to_ofs(lsn);
    auto size = tracker_.first_segment_size_ofs(ofs, buf.size());
    if (size == buf.size()) {
      rpm_block_.pwrite(buf, ofs);
    } else {
      rpm_block_.pwrite(buf.subspan(0, size), ofs);
      rpm_block_.pwrite(buf.subspan(size), 0);
    }
  }

 private:
  ring_tracker tracker_;
  rpm_local_block rpm_block_;
};

}  // namespace rpmbb
