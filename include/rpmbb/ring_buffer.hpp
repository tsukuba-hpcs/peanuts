#pragma once

#include "rpmbb/ring_tracker.hpp"
#include "rpmbb/rpm.hpp"

#include <optional>

namespace rpmbb {

template <typename BlockType>
class read_only_ring_buffer {
 public:
  using lsn_t = ring_tracker::lsn_t;
  using rpm_block_type = BlockType;

  explicit read_only_ring_buffer(const rpm_block_type& block)
      : read_only_ring_buffer(block, block.size()) {}

  read_only_ring_buffer(const rpm_block_type& block, size_t ring_size)
      : tracker_{ring_size}, block_{block} {
    assert(ring_size <= block_.size());
  }

  auto can_reserve(size_t size) const -> bool {
    return tracker_.can_allocate(size);
  }
  auto can_consume(size_t size) const -> bool {
    return tracker_.can_release(size);
  }

  size_t used_capacity() const { return tracker_.used_capacity(); }
  size_t free_capacity() const { return tracker_.free_capacity(); }
  size_t size() const { return tracker_.ring_size(); }
  lsn_t head() const { return tracker_.head(); }
  lsn_t tail() const { return tracker_.tail(); }
  uint64_t to_ofs(lsn_t lsn) const { return tracker_.to_ofs(lsn); }

  auto pread(std::span<std::byte> buf, lsn_t lsn) const -> void {
    auto ofs = tracker_.to_ofs(lsn);
    auto size = tracker_.first_segment_size_ofs(ofs, buf.size());
    if (size == buf.size()) {
      block_.pread(buf, ofs);
    } else {
      block_.pread(buf.subspan(0, size), ofs);
      block_.pread(buf.subspan(size), 0);
    }
  }

 protected:
  ring_tracker tracker_;
  rpm_block_type block_;
};

template <typename BlockType>
class ring_buffer : public read_only_ring_buffer<BlockType> {
 public:
  using Base = read_only_ring_buffer<BlockType>;
  using lsn_t = typename Base::lsn_t;

  using Base::Base;
};

template <>
class ring_buffer<rpm_remote_block>
    : public read_only_ring_buffer<rpm_remote_block> {
 public:
  using Base = read_only_ring_buffer<rpm_remote_block>;
  using lsn_t = typename Base::lsn_t;

  using Base::Base;

  auto pread_noflush(std::span<std::byte> buf, lsn_t lsn) const -> void {
    auto ofs = tracker_.to_ofs(lsn);
    auto size = tracker_.first_segment_size_ofs(ofs, buf.size());
    if (size == buf.size()) {
      block_.pread_noflush(buf, ofs);
    } else {
      block_.pread_noflush(buf.subspan(0, size), ofs);
      block_.pread_noflush(buf.subspan(size), 0);
    }
  }

  auto flush() const -> void { block_.flush(); }
};

template <>
class ring_buffer<rpm_local_block>
    : public read_only_ring_buffer<rpm_local_block> {
 public:
  using Base = read_only_ring_buffer<rpm_local_block>;
  using lsn_t = typename Base::lsn_t;

  using Base::Base;
  explicit ring_buffer(const rpm& rpm)
      : ring_buffer(rpm, rpm.topo().intra_rank()) {}
  explicit ring_buffer(const rpm& rpm, int intra_rank)
      : ring_buffer(rpm, intra_rank, rpm.block_size()) {}
  explicit ring_buffer(const rpm& rpm, int intra_rank, size_t ring_size)
      : Base{rpm_local_block{rpm, intra_rank}, ring_size} {}

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

  auto pwrite(std::span<const std::byte> buf, lsn_t lsn) const -> void {
    auto ofs = this->tracker_.to_ofs(lsn);
    auto size = this->tracker_.first_segment_size_ofs(ofs, buf.size());
    if (size == buf.size()) {
      this->block_.pwrite(buf, ofs);
    } else {
      this->block_.pwrite(buf.subspan(0, size), ofs);
      this->block_.pwrite(buf.subspan(size), 0);
    }
  }
};

using local_ring_buffer = ring_buffer<rpm_local_block>;
using remote_ring_buffer = ring_buffer<rpm_remote_block>;

}  // namespace rpmbb
