#pragma once

#include <array>
#include <deque>
#include <forward_list>
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <ostream>
#include <queue>
#include <set>
#include <sstream>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <utility>  // for std::pair
#include <vector>

#include "../inspector.hpp"

namespace rpmbb::util {
namespace detail {

template <typename Iter>
auto array_like_inspect(std::ostream& os, Iter begin, Iter end)
    -> std::ostream& {
  os << '[';
  if (begin != end) {
    os << make_inspector(*begin++);
    for (auto it = begin; it != end; ++it) {
      os << ", " << make_inspector(*it);
    }
  }
  os << ']';
  return os;
}

template <typename MapIter>
auto map_like_inspect(std::ostream& os, MapIter begin, MapIter end)
    -> std::ostream& {
  os << '{';
  if (begin != end) {
    os << make_inspector(begin->first) << ": " << make_inspector(begin->second);
    ++begin;
    for (auto it = begin; it != end; ++it) {
      os << ", " << make_inspector(it->first) << ": "
         << make_inspector(it->second);
    }
  }
  os << '}';
  return os;
}

}  // namespace detail

template <typename T>
struct inspector<std::optional<T>> {
  static auto inspect(std::ostream& os, const std::optional<T>& obj)
      -> std::ostream& {
    if (obj) {
      return os << make_inspector(*obj);
    }
    return os << "nullopt";
  }
};

template <typename T>
struct inspector<std::vector<T>> {
  static auto inspect(std::ostream& os, const std::vector<T>& obj)
      -> std::ostream& {
    return detail::array_like_inspect(os, obj.begin(), obj.end());
  }
};

template <typename T, std::size_t N>
struct inspector<std::array<T, N>> {
  static auto inspect(std::ostream& os, const std::array<T, N>& obj)
      -> std::ostream& {
    return detail::array_like_inspect(os, obj.begin(), obj.end());
  }
};

template <typename T>
struct inspector<std::deque<T>> {
  static auto inspect(std::ostream& os, const std::deque<T>& obj)
      -> std::ostream& {
    return detail::array_like_inspect(os, obj.begin(), obj.end());
  }
};

template <typename T>
struct inspector<std::forward_list<T>> {
  static auto inspect(std::ostream& os, const std::forward_list<T>& obj)
      -> std::ostream& {
    return detail::array_like_inspect(os, obj.begin(), obj.end());
  }
};

template <typename T>
struct inspector<std::list<T>> {
  static auto inspect(std::ostream& os, const std::list<T>& obj)
      -> std::ostream& {
    return detail::array_like_inspect(os, obj.begin(), obj.end());
  }
};

template <typename K, typename V>
struct inspector<std::map<K, V>> {
  static auto inspect(std::ostream& os, const std::map<K, V>& obj)
      -> std::ostream& {
    return detail::map_like_inspect(os, obj.begin(), obj.end());
  }
};

template <typename K, typename V>
struct inspector<std::unordered_map<K, V>> {
  static auto inspect(std::ostream& os, const std::unordered_map<K, V>& obj)
      -> std::ostream& {
    return detail::map_like_inspect(os, obj.begin(), obj.end());
  }
};

template <typename K, typename V>
struct inspector<std::multimap<K, V>> {
  static auto inspect(std::ostream& os, const std::multimap<K, V>& obj)
      -> std::ostream& {
    return detail::map_like_inspect(os, obj.begin(), obj.end());
  }
};

template <typename K, typename V>
struct inspector<std::unordered_multimap<K, V>> {
  static auto inspect(std::ostream& os,
                      const std::unordered_multimap<K, V>& obj)
      -> std::ostream& {
    return detail::map_like_inspect(os, obj.begin(), obj.end());
  }
};

template <typename T>
struct inspector<std::set<T>> {
  static auto inspect(std::ostream& os, const std::set<T>& obj)
      -> std::ostream& {
    return detail::array_like_inspect(os, obj.begin(), obj.end());
  }
};

template <typename T>
struct inspector<std::unordered_set<T>> {
  static auto inspect(std::ostream& os, const std::unordered_set<T>& obj)
      -> std::ostream& {
    return detail::array_like_inspect(os, obj.begin(), obj.end());
  }
};

template <typename T>
struct inspector<std::multiset<T>> {
  static auto inspect(std::ostream& os, const std::multiset<T>& obj)
      -> std::ostream& {
    return detail::array_like_inspect(os, obj.begin(), obj.end());
  }
};

template <typename T>
struct inspector<std::unordered_multiset<T>> {
  static auto inspect(std::ostream& os, const std::unordered_multiset<T>& obj)
      -> std::ostream& {
    return detail::array_like_inspect(os, obj.begin(), obj.end());
  }
};

// Specialization for std::stack
template <typename T>
struct inspector<std::stack<T>> {
  static auto inspect(std::ostream& os, std::stack<T> obj) -> std::ostream& {
    os << '[';
    if (!obj.empty()) {
      os << make_inspector(obj.top());
      obj.pop();
      while (!obj.empty()) {
        os << ", " << make_inspector(obj.top());
        obj.pop();
      }
    }
    return os << ']';
  }
};

// Specialization for std::queue
template <typename T>
struct inspector<std::queue<T>> {
  static auto inspect(std::ostream& os, std::queue<T> obj) -> std::ostream& {
    os << '[';
    if (!obj.empty()) {
      os << make_inspector(obj.front());
      obj.pop();
      while (!obj.empty()) {
        os << ", " << make_inspector(obj.front());
        obj.pop();
      }
    }
    return os << ']';
  }
};

// Specialization for std::priority_queue
template <typename T>
struct inspector<std::priority_queue<T>> {
  static auto inspect(std::ostream& os, std::priority_queue<T> obj)
      -> std::ostream& {
    os << '[';
    if (!obj.empty()) {
      os << make_inspector(obj.top());
      obj.pop();
      while (!obj.empty()) {
        os << ", " << make_inspector(obj.top());
        obj.pop();
      }
    }
    return os << ']';
  }
};

}  // namespace rpmbb::util
