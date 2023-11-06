#pragma once

#include <cassert>
#include <cstdint>
#include <optional>
#include <ostream>
#include <set>

#include "inspector.hpp"
#include "zpp_bits.h"

namespace rpmbb {

struct extent {
  uint64_t begin{};
  uint64_t end{};

  using serialize = zpp::bits::members<2>;

  extent() = default;
  explicit extent(uint64_t begin, uint64_t end) : begin(begin), end(end) {
    assert(begin <= end);
  }

  size_t size() const { return end - begin; }

  bool operator<(const extent& other) const { return begin < other.begin; }

  bool operator==(const extent& other) const {
    return begin == other.begin && end == other.end;
  }

  bool overlaps(const extent& other) const {
    return end > other.begin && begin < other.end;
  }

  std::optional<extent> get_non_overlapping(const extent& other) const {
    assert(overlaps(other));

    if (begin < other.begin) {
      return extent(begin, other.begin);
    } else if (end > other.end) {
      return extent(other.end, end);
    }
    return std::nullopt;
  }

  std::ostream& inspect(std::ostream& os) const {
    return os << begin << "-" << end;
  }
};

class extent_tree {
 public:
  struct node {
    extent ex;
    uint64_t ptr;
    int client_id;

    using serialize = zpp::bits::members<3>;

    node() = default;
    node(uint64_t begin, uint64_t end, uint64_t ptr, int client_id)
        : ex(begin, end), ptr(ptr), client_id(client_id) {}

    bool overlaps(const node& other) const { return ex.overlaps(other.ex); }

    bool operator==(const node& other) const {
      return ex == other.ex && ptr == other.ptr && client_id == other.client_id;
    }

    std::optional<node> get_non_overlapping(const node& other) const {
      assert(overlaps(other));

      auto non_overlapping = ex.get_non_overlapping(other.ex);
      if (non_overlapping.has_value()) {
        return node(non_overlapping->begin, non_overlapping->end,
                    ptr + (non_overlapping->begin - ex.begin), client_id);
      }
      return std::nullopt;
    }

    std::ostream& inspect(std::ostream& os) const {
      return os << "[" << utils::make_inspector(ex) << ":" << ptr << ":"
                << client_id << "]";
    }
  };

  struct comparator {
    bool operator()(const node& lhs, const node& rhs) const {
      return lhs.ex.end <= rhs.ex.begin;
    }
  };

  std::set<node, comparator> nodes_;

  using serialize = zpp::bits::members<1>;

  using iterator = std::set<node>::iterator;
  using const_iterator = std::set<node>::const_iterator;

  iterator begin() { return nodes_.begin(); }
  const_iterator begin() const { return nodes_.cbegin(); }
  const_iterator cbegin() const { return nodes_.cbegin(); }

  iterator end() { return nodes_.end(); }
  const_iterator end() const { return nodes_.cend(); }
  const_iterator cend() const { return nodes_.cend(); }

  void clear() noexcept { nodes_.clear(); }

  size_t size() const { return nodes_.size(); }

  // Find the first extent_tree::node that falls in a [begin, end) range.
  iterator find(uint64_t begin, uint64_t end) {
    node temp_node(begin, end, 0, 0);
    auto it = nodes_.lower_bound(temp_node);

    if (it != nodes_.end() && !it->overlaps(temp_node)) {
      return nodes_.end();
    }

    return it;
  }

  void merge(const extent_tree& other) {
    static comparator comp;

    auto in_it = other.nodes_.begin();
    if (in_it == other.nodes_.end()) {
      return;
    }
    auto out_it = nodes_.lower_bound(*in_it);
    for (; in_it != other.nodes_.end(); ++in_it) {
      out_it = do_insert(out_it, *in_it);
      while (out_it != nodes_.end() && !comp(*in_it, *out_it)) {
        ++out_it;
      }
    }
  }

  void add(uint64_t begin, uint64_t end, uint64_t ptr, int client_id) {
    auto new_node = node{begin, end, ptr, client_id};
    do_insert(nodes_.lower_bound(new_node), new_node);
  }

  void remove(uint64_t begin, uint64_t end) {
    node remove_node(begin, end, 0, 0);
    auto it = nodes_.lower_bound(remove_node);

    while (it != nodes_.end() && remove_node.overlaps(*it)) {
      auto non_overlapping = it->get_non_overlapping(remove_node);

      if (non_overlapping.has_value()) {
        if (non_overlapping->ex.end < it->ex.end) {
          // If rear part of existing node overlaps
          auto remaining =
              node(non_overlapping->ex.end, it->ex.end,
                   it->ptr + (non_overlapping->ex.end - it->ex.begin),
                   it->client_id);

          assert(remaining.overlaps(remove_node));

          // Erase the existing node first
          it = nodes_.erase(it);

          auto non_overlapping_rear =
              remaining.get_non_overlapping(remove_node);
          if (non_overlapping_rear.has_value()) {
            it = nodes_.insert(it, *non_overlapping_rear);
            nodes_.insert(it, *non_overlapping);
            break;  // no more overlap
          }
        } else {
          it = nodes_.erase(it);
        }
        nodes_.insert(it, *non_overlapping);
      } else {
        // Overlap overall
        it = nodes_.erase(it);
      }
    }
  }

  std::ostream& inspect(std::ostream& os) const {
    for (const auto& node : nodes_) {
      os << utils::make_inspector(node);
    }
    return os;
  }

 private:
  iterator do_insert(iterator it, const node& value) {
    while (it != nodes_.end() && value.overlaps(*it)) {
      auto non_overlapping = it->get_non_overlapping(value);
      if (non_overlapping.has_value()) {
        if (non_overlapping->ex.end < it->ex.end) {
          // Overlap rear part of existing node
          // check if there is a non-overlapping rear part
          auto remaining =
              node(non_overlapping->ex.end, it->ex.end,
                   it->ptr + (non_overlapping->ex.end - it->ex.begin),
                   it->client_id);

          assert(remaining.overlaps(value));

          // remove existing node first
          it = nodes_.erase(it);

          auto non_overlapping_rear = remaining.get_non_overlapping(value);
          if (non_overlapping_rear.has_value()) {
            it = nodes_.insert(it, *non_overlapping_rear);
          }
        } else {
          // Overlap only front part of existing node
          it = nodes_.erase(it);
        }

        nodes_.insert(it, *non_overlapping);
      } else {
        // Overlap overall
        it = nodes_.erase(it);
      }
    }

    it = nodes_.insert(it, value);

    // coalesce with previous node
    if (auto prev = it; prev != nodes_.begin()) {
      --prev;
      if (prev->client_id == it->client_id && prev->ex.end == it->ex.begin &&
          prev->ptr + prev->ex.size() == it->ptr) {
        node coalesced(prev->ex.begin, it->ex.end, prev->ptr, prev->client_id);
        nodes_.erase(prev);
        it = nodes_.erase(it);
        it = nodes_.insert(it, coalesced);
      }
    }

    // coalesce with next node
    if (auto next = it; ++next != nodes_.end()) {
      if (next->client_id == it->client_id && next->ex.begin == it->ex.end &&
          next->ptr == it->ptr + it->ex.size()) {
        node coalesced(it->ex.begin, next->ex.end, it->ptr, it->client_id);
        nodes_.erase(it);
        it = nodes_.erase(next);
        it = nodes_.insert(it, coalesced);
      }
    }
    return it;
  }
};

}  // namespace rpmbb
