#pragma once

#include <mpi.h>
#include <memory>

#include "rpmbb/mpi/error.hpp"

namespace rpmbb::mpi::raii {
namespace detail {

template <typename T, T null_value>
struct native_handle {
  T native{null_value};
  native_handle() = default;
  native_handle(std::nullptr_t) {}
  native_handle(T native) : native(native) {}
  explicit operator bool() const { return native != null_value; }
  friend bool operator==(native_handle l, native_handle r) {
    return l.native == r.native;
  }
  friend bool operator!=(native_handle l, native_handle r) { return !(l == r); }
};

using bool_handle = native_handle<bool, false>;

struct env_deleter {
  using pointer = bool_handle;
  void operator()(pointer) { MPI_Finalize(); }
};

struct comm_deleter {
  using pointer = native_handle<MPI_Comm, MPI_COMM_NULL>;
  void operator()(pointer comm) { MPI_Comm_free(&comm.native); }
};

struct win_deleter {
  using pointer = native_handle<MPI_Win, MPI_WIN_NULL>;
  void operator()(pointer win) { MPI_Win_free(&win.native); }
};

struct info_deleter {
  using pointer = native_handle<MPI_Info, MPI_INFO_NULL>;
  void operator()(pointer info) { MPI_Info_free(&info.native); }
};

}  // namespace detail

using unique_env = std::unique_ptr<void, detail::env_deleter>;
using unique_comm = std::unique_ptr<void, detail::comm_deleter>;
using unique_win = std::unique_ptr<void, detail::win_deleter>;
using unique_info = std::unique_ptr<void, detail::info_deleter>;

}  // namespace rpmbb::mpi::raii