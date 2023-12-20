#pragma once

#include <forward_list>
#include <initializer_list>

#include "rpmbb/extent_tree.hpp"

namespace rpmbb {

class extent_list {
 public:
  using list_t = std::forward_list<extent>;

  extent_list() = default;
  extent_list(std::initializer_list<extent> extents) : list_{extents} {}

  auto& get() { return list_; }
  auto& get() const { return list_; }

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
    auto& result_list = result.get();

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

  std::ostream& inspect(std::ostream& os) const {
    for (const auto& ex : list_) {
      os << "[" << utils::make_inspector(ex) << ")";
    }
    return os;
  }

 private:
  list_t list_{};
};

}  // namespace rpmbb
