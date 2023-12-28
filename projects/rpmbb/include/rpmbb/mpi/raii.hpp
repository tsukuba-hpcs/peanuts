#pragma once

#include <mpi.h>
#include <memory>

#include "rpmbb/mpi/error.hpp"
#include "rpmbb/utils/cond_deleter.hpp"

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
  void operator()(pointer) const { MPI_Finalize(); }
};

struct comm_deleter {
  using pointer = native_handle<MPI_Comm, MPI_COMM_NULL>;
  void operator()(pointer comm) const { MPI_Comm_free(&comm.native); }
};

struct win_deleter {
  using pointer = native_handle<MPI_Win, MPI_WIN_NULL>;
  void operator()(pointer win) const { MPI_Win_free(&win.native); }
};

struct info_deleter {
  using pointer = native_handle<MPI_Info, MPI_INFO_NULL>;
  void operator()(pointer info) const { MPI_Info_free(&info.native); }
};

struct dtype_deleter {
  using pointer = native_handle<MPI_Datatype, MPI_DATATYPE_NULL>;
  void operator()(pointer dtype) const { MPI_Type_free(&dtype.native); }
};

struct request_deleter {
  using pointer = native_handle<MPI_Request, MPI_REQUEST_NULL>;
  void operator()(pointer request) const { MPI_Request_free(&request.native); }
};

}  // namespace detail

using unique_env = std::unique_ptr<void, detail::env_deleter>;
using unique_comm =
    std::unique_ptr<void, utils::cond_deleter<detail::comm_deleter>>;
using unique_win = std::unique_ptr<void, detail::win_deleter>;
using unique_info = std::unique_ptr<void, detail::info_deleter>;
using unique_dtype =
    std::unique_ptr<void, utils::cond_deleter<detail::dtype_deleter>>;
using unique_request = std::unique_ptr<void, utils::cond_deleter<detail::request_deleter>>;

}  // namespace rpmbb::mpi::raii
