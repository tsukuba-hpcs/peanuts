#pragma once

#include "peanuts/inspector.hpp"
#include "peanuts/sparse_extent.hpp"

namespace peanuts {
class sparse_extent_tree {
 public:
  struct node {
    sparse_extent se;
    uint64_t dev_addr;
    int dev_id;

    using serialize = zpp::bits::members<3>;

    node() = default;
    node(const sparse_extent& se, uint64_t device_addr, int device_id)
        : se(se), dev_addr(device_addr), dev_id(device_id) {}

    bool overlaps(const node& other) const { return se.overlaps(other.se); }

    bool operator==(const node& other) const {
      return se == other.se && dev_addr == other.dev_addr &&
             dev_id == other.dev_id;
    }

    auto get_nth_dev_addr(uint32_t index) const -> uint64_t {
      assert(index < se.count);
      return dev_addr + index * se.stride_size();
    }

    std::ostream& inspect(std::ostream& os) const {
      os << "[";
      uint32_t i = 0;
      for (const auto& ex : se) {
        os << "[[" << ex.begin << "," << ex.end << ")," << dev_id << ","
           << get_nth_dev_addr(i) << "]";
        ++i;
      }
      return os << "]";
    }
  };

  struct comparator {
    bool operator()(const node& lhs, const node& rhs) const {
      // return lhs.se.outer_extent().end <= rhs.se.outer_extent().begin;
      return lhs.se.ex.begin < rhs.se.ex.begin;
    }
  };

  std::set<node, comparator> nodes_;

  using serialize = zpp::bits::members<1>;

  using iterator = std::set<node>::iterator;
  using const_iterator = std::set<node>::const_iterator;

  auto operator==(const sparse_extent_tree& other) const -> bool {
    return nodes_ == other.nodes_;
  }

  auto find(const extent& ex) -> iterator { return find(ex.begin, ex.end); }
  auto find(uint64_t begin, uint64_t end) -> iterator;
  void add(uint64_t begin, uint64_t end, uint64_t dev_addr, int dev_id) {
    add(extent{begin, end}, dev_addr, dev_id);
  }
  void add(const extent& ex, uint64_t dev_addr, int dev_id) {
    auto new_node = node{sparse_extent{ex}, dev_addr, dev_id};
    do_insert(nodes_.lower_bound(new_node), new_node);
  }

  void remove(const extent& ex) { remove(ex.begin, ex.end); }
  void remove(uint64_t begin, uint64_t end);
  void merge(const sparse_extent_tree& other);
  size_t size() const { return nodes_.size(); }
  void clear() { nodes_.clear(); }

  iterator begin() { return nodes_.begin(); }
  const_iterator begin() const { return nodes_.cbegin(); }
  const_iterator cbegin() const { return nodes_.cbegin(); }
  iterator end() { return nodes_.end(); }
  const_iterator end() const { return nodes_.cend(); }
  const_iterator cend() const { return nodes_.cend(); }
  auto back() const -> const node& { return *std::prev(end()); }

 private:
  auto do_insert(iterator it, const node& n) -> iterator;
};

inline auto sparse_extent_tree::find(uint64_t begin, uint64_t end) -> iterator {
  return this->end();
}

inline auto sparse_extent_tree::do_insert(iterator it, const node& n)
    -> iterator {
  // while (it != nodes_.end() && it->overlaps(n)) {
  //   ++it;
  // }
  return this->end();
}

inline auto sparse_extent_tree::remove(uint64_t begin, uint64_t end) -> void {}

inline auto sparse_extent_tree::merge(const sparse_extent_tree& other) -> void {
  // static comparator comp;

  // auto in_it = other.nodes_.begin();
  // if (in_it == other.nodes_.end()) {
  //   return;
  // }
  // auto out_it = nodes_.lower_bound(*in_it);
  // for (; in_it != other.nodes_.end(); ++in_it) {
  //   out_it = nodes_.insert(out_it, *in_it);
  //   while (out_it != nodes_.end() && !comp(*in_it, *out_it)) {
  //     ++out_it;
  //   }
  // }
}

}  // namespace peanuts
