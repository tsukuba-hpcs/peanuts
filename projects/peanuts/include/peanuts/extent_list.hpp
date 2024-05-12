#pragma once

#include <forward_list>
#include <initializer_list>

#include "peanuts/extent_tree.hpp"

namespace peanuts {

class extent_list {
 public:
  using list_t = std::forward_list<extent>;

  extent_list() = default;
  extent_list(std::initializer_list<extent> extents) : list_{extents} {}

  auto begin() { return list_.begin(); }
  auto end() { return list_.end(); }

  auto begin() const { return list_.begin(); }
  auto end() const { return list_.end(); }

  bool empty() const { return list_.empty(); }

  void clear() { list_.clear(); }

  void add(extent new_ex) {
    auto it = list_.before_begin();
    auto next_it = std::next(it);

    while (next_it != list_.end()) {
      auto& ex = *next_it;
      if (new_ex.end < ex.begin) {
        break;
      } else if (ex.end < new_ex.begin) {
        it = next_it;
        ++next_it;
      } else {
        new_ex = new_ex.get_union(ex);
        next_it = list_.erase_after(it);
      }
    }

    list_.insert_after(it, new_ex);
  }

  auto inverse(extent ex) -> extent_list {
    extent_list result;
    auto& result_list = result.list_;

    auto it = result_list.before_begin();
    for (auto& list_ex : list_) {
      if (ex.begin < list_ex.begin) {
        it = result_list.insert_after(
            it, {ex.begin, std::min(list_ex.begin, ex.end)});
      }
      if (ex.begin < list_ex.end) {
        ex.begin = list_ex.end;
        if (ex.begin >= ex.end)
          break;
      }
    }
    if (ex.begin < ex.end) {
      result_list.insert_after(it, ex);
    }
    return result;
  }

  auto outer_extent() const -> extent {
    if (list_.empty()) {
      return {0, 0};
    } else {
      return {list_.front().begin, max()};
    }
  }

  std::ostream& inspect(std::ostream& os) const {
    for (const auto& ex : list_) {
      os << "[" << utils::make_inspector(ex) << ")";
    }
    return os;
  }

 private:
  uint64_t max() const {
    if (list_.empty()) {
      return 0;
    } else {
      auto it = list_.begin();
      while (std::next(it) != list_.end()) {
        ++it;
      }
      return it->end;
    }
  }

 private:
  list_t list_{};

  friend inline auto intersection(const extent_list& lhs,
                                  const extent_list& rhs) -> extent_list;
};

inline auto intersection(const extent_list& lhs, const extent_list& rhs)
    -> extent_list {
  extent_list result;
  auto& result_list = result.list_;

  auto res_it = result_list.before_begin();
  auto lhs_it = lhs.list_.begin();
  auto rhs_it = rhs.list_.begin();

  while (lhs_it != lhs.list_.end() && rhs_it != rhs.list_.end()) {
    auto& lhs_ex = *lhs_it;
    auto& rhs_ex = *rhs_it;

    if (lhs_ex.end <= rhs_ex.begin) {
      ++lhs_it;
    } else if (rhs_ex.end <= lhs_ex.begin) {
      ++rhs_it;
    } else {
      res_it =
          result_list.insert_after(res_it, lhs_ex.get_intersection(rhs_ex));
      if (lhs_ex.end < rhs_ex.end) {
        ++lhs_it;
      } else if (rhs_ex.end < lhs_ex.end) {
        ++rhs_it;
      } else {
        ++lhs_it;
        ++rhs_it;
      }
    }
  }

  return result;
}

}  // namespace peanuts
