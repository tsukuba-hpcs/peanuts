#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

namespace rpmbb {

class ring_tracker {
 public:
  explicit ring_tracker(size_t region_size)
      : region_size_(region_size), head_id_(0), tail_id_(0) {}

  uint64_t allocate(size_t size) {
    return std::exchange(head_id_, head_id_ + size);
  }

  bool can_allocate(size_t size) const { return free_capacity() >= size; }

  void release(size_t size) { tail_id_ += size; }

  bool can_release(size_t size) const { return used_capacity() >= size; }

  size_t first_segment_size(uint64_t segment_id, size_t size) const {
    uint64_t ring_offset = to_ring_offset(segment_id);
    uint64_t ring_offset_end = ring_offset + size;
    if (ring_offset_end <= region_size_) {
      return size;
    } else {
      return region_size_ - ring_offset;
    }
  }

  bool is_wrapping(uint64_t segment_id, size_t size) const {
    uint64_t ring_offset = to_ring_offset(segment_id);
    return (ring_offset + size) > region_size_;
  }

  uint64_t to_ring_offset(uint64_t segment_id) const {
    return segment_id % region_size_;
  }

  uint64_t used_capacity() const { return head_id_ - tail_id_; }

  uint64_t free_capacity() const { return region_size_ - used_capacity(); }

  size_t region_size() const { return region_size_; }

 private:
  size_t region_size_;
  uint64_t head_id_;
  uint64_t tail_id_;
};

}  // namespace rpmbb
