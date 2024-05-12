#pragma once

#include <unistd.h>
#include <memory>

namespace peanuts::raii {

namespace detail {

class fd_handle {
  int fd_ = -1;

 public:
  fd_handle() = default;
  fd_handle(std::nullptr_t) {}
  fd_handle(int fd) : fd_(fd) {}
  operator int() { return fd_; }
  explicit operator bool() const { return fd_ >= 0; }
  friend bool operator==(fd_handle l, fd_handle r) { return l.fd_ == r.fd_; }
  friend bool operator!=(fd_handle l, fd_handle r) { return !(l == r); }
};

struct fd_deleter {
  using pointer = fd_handle;
  void operator()(pointer fd) { ::close(fd); }
};

}  // namespace detail

using file_descriptor = std::unique_ptr<void, detail::fd_deleter>;

}  // namespace peanuts::raii
