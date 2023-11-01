#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

namespace rpmbb {

class ring_tracker {
 public:
  using lsn_t = uint64_t;

  explicit ring_tracker(size_t ring_size)
      : ring_size_(ring_size), head_lsn_(0), tail_lsn_(0) {}

  uint64_t allocate(size_t size) {
    return std::exchange(head_lsn_, head_lsn_ + size);
  }

  bool can_allocate(size_t size) const { return free_capacity() >= size; }

  void release(size_t size) { tail_lsn_ += size; }

  bool can_release(size_t size) const { return used_capacity() >= size; }

  size_t first_segment_size(lsn_t lsn, size_t size) const {
    return first_segment_size_ofs(to_ofs(lsn), size);
  }

  size_t first_segment_size_ofs(uint64_t ofs, size_t size) const {
    uint64_t ring_offset_end = ofs + size;
    if (ring_offset_end <= ring_size_) {
      return size;
    } else {
      return ring_size_ - ofs;
    }
  }

  bool is_wrapping(lsn_t lsn, size_t size) const {
    return is_wrapping_ofs(to_ofs(lsn), size);
  }

  bool is_wrapping_ofs(uint64_t ofs, size_t size) const {
    return (ofs + size) > ring_size_;
  }

  uint64_t to_ofs(uint64_t lsn) const { return lsn % ring_size_; }

  size_t used_capacity() const { return head_lsn_ - tail_lsn_; }
  size_t free_capacity() const { return ring_size_ - used_capacity(); }
  size_t ring_size() const { return ring_size_; }
  lsn_t head() const { return head_lsn_; }
  lsn_t tail() const { return tail_lsn_; }

 private:
  size_t ring_size_;
  lsn_t head_lsn_;
  lsn_t tail_lsn_;
};

}  // namespace rpmbb
