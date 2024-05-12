#pragma once

#include "peanuts/extent_tree.hpp"
#include "peanuts/inspector.hpp"

#include <utility>

namespace peanuts {
struct sparse_extent {
  extent ex{};
  uint32_t hole{0};
  uint32_t count{0};

  using serialize = zpp::bits::members<4>;

  sparse_extent() = default;
  sparse_extent(uint64_t begin,
                uint64_t extent_size,
                uint32_t hole,
                uint32_t count)
      : ex(begin, begin + extent_size), hole(hole), count(count) {}
  explicit sparse_extent(const extent& ex) : ex{ex}, hole{0}, count{1} {}

  void normalize() {
    if (count <= 1) {
      hole = 0;
    }
    if (hole == 0 && count > 1) {
      ex.end = ex.begin + extent_size() * count;
      count = 1;
    }
  }

  uint64_t start() const { return ex.begin; }
  uint64_t stop() const { return ex.begin + size(); }

  size_t extent_size() const { return ex.size(); }
  size_t stride_size() const { return ex.size() + hole; }
  size_t hole_size() const { return hole; }

  size_t size() const { return stride_size() * count - hole; }
  extent true_extent() const { return extent{ex.begin, ex.begin + size()}; }
  extent outer_extent() const {
    return extent{ex.begin, ex.begin + size() + hole};
  }

  extent get_extent(uint32_t index) const {
    assert(index < count);
    auto disp = ex.begin + index * (stride_size());
    return extent{disp, disp + extent_size()};
  }

  bool overlaps(const sparse_extent& other) const {
    return true_extent().overlaps(other.true_extent());
  }

  size_t get_stride_index_at(uint64_t pos) const {
    assert(pos >= ex.begin && pos < outer_extent().end);
    return (pos - ex.begin) / stride_size();
  }

  size_t get_shifted_stride_index_at(uint64_t pos) const {
    assert(pos >= ex.begin && pos < outer_extent().end);
    return (pos + hole - ex.begin) / stride_size();
  }

  auto get_first_slice_start_at(uint64_t pos) -> sparse_extent {
    assert(pos >= ex.begin && pos < outer_extent().end);

    auto start_stride_idx =
        static_cast<decltype(count)>((pos + hole - ex.begin) / stride_size());
    if (start_stride_idx >= count) {
      // pos is after the last extent
      return sparse_extent{};
    }

    auto start_stride_pos = ex.begin + start_stride_idx * stride_size();

    if (pos > start_stride_pos) {
      // pos is in the middle of the extent
      return sparse_extent{pos, extent_size() - (pos - start_stride_pos), 0, 1};
    }

    // pos is in the hole
    return sparse_extent{start_stride_pos, extent_size(), hole,
                         count - start_stride_idx};
  }

  auto get_first_slice_stop_at(uint64_t pos) -> sparse_extent {
    assert(pos >= ex.begin && pos < outer_extent().end);

    if (pos <= ex.begin) {
      // pos is before the first extent
      return sparse_extent{};
    }

    auto stop_stride_idx =
        static_cast<decltype(count)>((pos - ex.begin) / stride_size());
    auto stop_stride_pos = ex.begin + stop_stride_idx * stride_size();

    if (pos < stop_stride_pos + extent_size()) {
      // pos is in the middle of the extent
      if (stop_stride_idx == 0) {
        return sparse_extent{ex.begin, pos - ex.begin, 0, 1};
      } else {
        stop_stride_idx--;
      }
    }

    return sparse_extent{ex.begin, extent_size(), hole, stop_stride_idx + 1};
  }

  auto get_first_slice(uint64_t start, uint64_t stop) {
    assert(start >= ex.begin && start < outer_extent().end);
    assert(stop > start && stop <= outer_extent().end);

    auto start_stride_idx =
        static_cast<decltype(count)>((start + hole - ex.begin) / stride_size());

    if (start_stride_idx >= count) {
      // pos is after the last extent
      return sparse_extent{};
    }
    auto start_stride_pos = ex.begin + start_stride_idx * stride_size();

    uint64_t new_start;
    if (start <= start_stride_pos) {
      // start is in the hole
      new_start = start_stride_pos;
    } else {
      // start is in the middle of the extent
      new_start = start;
    }

    auto stop_stride_idx =
        static_cast<decltype(count)>((stop - ex.begin) / stride_size());
    auto stop_stride_pos = ex.begin + stop_stride_idx * stride_size();

    if (stop < stop_stride_pos + extent_size()) {
      // stop is in the middle of the extent
      if (stop_stride_idx == start_stride_idx) {
        return sparse_extent{new_start, stop - new_start, 0, 1};
      } else {
        stop_stride_idx--;
      }
    }

    if (start <= start_stride_pos) {
      // both start and stop are in the hole
      return sparse_extent{new_start, extent_size(), hole,
                           stop_stride_idx - start_stride_idx + 1};
    } else {
      return sparse_extent{new_start,
                           extent_size() - (start - start_stride_pos), 0, 1};
    }
  }

  bool is_in_hole(uint64_t pos) const {
    assert(pos >= ex.begin && pos < outer_extent().end);
    return (pos - ex.begin) % stride_size() >= extent_size();
  }

  bool operator==(const sparse_extent& other) const {
    return ex == other.ex && hole == other.hole && count == other.count;
  }

  bool is_same_strided(const sparse_extent& other) const {
    return extent_size() == other.extent_size() && hole == other.hole;
  }

  bool followed_by(const sparse_extent& other) const {
    return is_same_strided(other) &&
           outer_extent().followed_by(other.outer_extent());
  }

  // const_iterator
  class const_iterator {
   public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = const extent;
    using difference_type = std::ptrdiff_t;
    using pointer = const value_type*;
    using reference = const value_type&;

    const_iterator(const sparse_extent* parent, uint32_t index)
        : parent_(parent), index_(index) {}

    reference operator*() const {
      current_ = parent_->get_extent(index_);
      return current_;
    }

    pointer operator->() const { return &(operator*()); }

    const_iterator& operator++() {
      ++index_;
      return *this;
    }

    const_iterator operator++(int) {
      const_iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    const_iterator& operator--() {
      --index_;
      return *this;
    }

    const_iterator operator--(int) {
      const_iterator tmp = *this;
      --(*this);
      return tmp;
    }

    const_iterator& operator+=(difference_type n) {
      index_ += n;
      return *this;
    }

    const_iterator& operator-=(difference_type n) {
      index_ -= n;
      return *this;
    }

    difference_type operator-(const const_iterator& other) const {
      return index_ - other.index_;
    }

    reference operator[](difference_type n) const {
      current_ = parent_->get_extent(index_ + n);
      return current_;
    }

    bool operator==(const const_iterator& other) const {
      return parent_ == other.parent_ && index_ == other.index_;
    }

    bool operator!=(const const_iterator& other) const {
      return !(*this == other);
    }

    bool operator<(const const_iterator& other) const {
      return index_ < other.index_;
    }

    bool operator>(const const_iterator& other) const {
      return index_ > other.index_;
    }

    bool operator<=(const const_iterator& other) const {
      return index_ <= other.index_;
    }

    bool operator>=(const const_iterator& other) const {
      return index_ >= other.index_;
    }

    friend const_iterator operator+(const_iterator it, difference_type n) {
      it += n;
      return it;
    }

    friend const_iterator operator+(difference_type n, const_iterator it) {
      it += n;
      return it;
    }

    friend const_iterator operator-(const_iterator it, difference_type n) {
      it -= n;
      return it;
    }

   private:
    const sparse_extent* parent_;
    uint32_t index_;
    mutable extent current_;
  };

  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator end() const { return const_iterator(this, count); }
  const_iterator cbegin() const { return begin(); }
  const_iterator cend() const { return end(); }

  std::ostream& inspect(std::ostream& os) const {
    os << "[";
    for (const auto& ex : *this) {
      os << "[" << ex.begin << "," << ex.end << ")";
    }
    return os << "]";
  }
};
}  // namespace peanuts
