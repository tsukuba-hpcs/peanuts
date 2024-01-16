#pragma once

#include "rpmbb/extent_tree.hpp"
#include "rpmbb/inspector.hpp"

#include <utility>

namespace rpmbb {
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

  auto slice_start_at(uint64_t pos) const
      -> std::pair<sparse_extent, sparse_extent> {
    assert(pos >= ex.begin && pos < ex.end);
    auto index = get_stride_index_at(pos);
    auto stride_start = ex.begin + index * stride_size();
    auto in_hole = stride_start + extent_size() <= pos;
    auto first = sparse_extent{};
    auto second = sparse_extent{};
    if (!in_hole) {
      first = sparse_extent{pos, (pos - stride_start) - extent_size(), 0, 1};
    }
    auto remaining_count = count - static_cast<uint32_t>(index + 1);
    if (remaining_count > 0) {
      second = sparse_extent{(index + 1) * stride_size(), extent_size(),
                             remaining_count >= 2 ? hole : 0, remaining_count};
    }
    return {first, second};
  }

  // auto slice_stop_at(uint64_t pos) const
  //     -> std::pair<sparse_extent, sparse_extent> {
    // assert(pos >= ex.begin && pos < ex.end);
    // auto index = get_stride_index_at(pos);
    // auto stride_start = index * stride_size();
  // }

  size_t get_stride_index_at(uint64_t pos) const {
    assert(pos >= ex.begin && pos < ex.end);
    return (pos - ex.begin) / stride_size();
  }

  bool is_in_hole(uint64_t pos) const {
    assert(pos >= ex.begin && pos < ex.end);
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
}  // namespace rpmbb
