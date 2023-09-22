#pragma once

#include <mpi.h>

#include "rpmbb/mpi/aint.hpp"
#include "rpmbb/mpi/comm.hpp"
#include "rpmbb/mpi/error.hpp"
#include "rpmbb/mpi/info.hpp"
#include "rpmbb/mpi/raii.hpp"

#include <span>
#include <string>
#include <utility>

namespace rpmbb::mpi {

class win {
  raii::unique_win win_{MPI_WIN_NULL};

 public:
  win() = default;
  win(const comm& comm, const info& info = MPI_INFO_NULL) {
    MPI_Win win;
    MPI_CHECK_ERROR_CODE(
        MPI_Win_create_dynamic(info.native(), comm.native(), &win));
    win_.reset(win);
  }

  operator MPI_Win() const { return native(); }
  auto native() const -> MPI_Win { return win_.get().native; }

  auto lock_all(int mode = 0) const -> void {
    MPI_CHECK_ERROR_CODE(MPI_Win_lock_all(mode, native()));
  }

  auto unlock_all() const -> void {
    MPI_CHECK_ERROR_CODE(MPI_Win_unlock_all(native()));
  }

  auto flush(int target) const -> void {
    MPI_CHECK_ERROR_CODE(MPI_Win_flush(target, native()));
  }

  auto flush_all() const -> void {
    MPI_CHECK_ERROR_CODE(MPI_Win_flush_all(native()));
  }

  auto flush_local(int target) const -> void {
    MPI_CHECK_ERROR_CODE(MPI_Win_flush_local(target, native()));
  }

  auto flush_local_all() const -> void {
    MPI_CHECK_ERROR_CODE(MPI_Win_flush_local_all(native()));
  }

  auto sync() const -> void { MPI_CHECK_ERROR_CODE(MPI_Win_sync(native())); }

  auto lock(int target, int lock_type, int mode = 0) const -> void {
    MPI_CHECK_ERROR_CODE(MPI_Win_lock(lock_type, target, mode, native()));
  }

  auto lock_exclusive(int target, int mode = 0) const -> void {
    lock(target, MPI_LOCK_EXCLUSIVE, mode);
  }

  auto lock_shared(int target, int mode = 0) const -> void {
    lock(target, MPI_LOCK_SHARED, mode);
  }

  auto unlock(int target) const -> void {
    MPI_CHECK_ERROR_CODE(MPI_Win_unlock(target, native()));
  }

  template <typename T>
  auto get_attr(int key) const -> std::optional<T> {
    T* val;
    int exists;
    MPI_CHECK_ERROR_CODE(MPI_Win_get_attr(native(), key, &val, &exists));
    return static_cast<bool>(exists) ? *val : std::optional<T>{std::nullopt};
  }

  auto get_memory_model() const -> std::optional<int> {
    return get_attr<int>(MPI_WIN_MODEL);
  }

  auto is_unified_memory_model() const -> bool {
    return get_memory_model() == MPI_WIN_UNIFIED;
  }

  auto is_separate_memory_model() const -> bool {
    return get_memory_model() == MPI_WIN_SEPARATE;
  }

  auto get_name() const -> std::string {
    int len{0};
    std::string result(MPI_MAX_OBJECT_NAME, '\0');
    MPI_CHECK_ERROR_CODE(MPI_Win_get_name(native(), &result[0], &len));
    result.resize(static_cast<size_t>(len));
    return result;
  }

  auto set_name(const std::string& name) const -> void {
    MPI_CHECK_ERROR_CODE(MPI_Win_set_name(native(), name.c_str()));
  }

  auto get_info() const -> info {
    MPI_Info info;
    MPI_CHECK_ERROR_CODE(MPI_Win_get_info(native(), &info));
    return info;
  }

  auto set_info(const info& info) const -> void {
    MPI_CHECK_ERROR_CODE(MPI_Win_set_info(native(), info));
  }

  auto attach(void* base, aint size) const -> void {
    MPI_CHECK_ERROR_CODE(MPI_Win_attach(native(), base, size));
  }

  auto attach(std::span<std::byte> buf) const -> void {
    attach(buf.data(), buf.size());
  }
};

class win_lock_all_adapter {
 public:
  win_lock_all_adapter() = default;
  win_lock_all_adapter(const win_lock_all_adapter&) = default;
  auto operator=(const win_lock_all_adapter&)
      -> win_lock_all_adapter& = default;
  win_lock_all_adapter(win_lock_all_adapter&&) = default;
  auto operator=(win_lock_all_adapter&&) -> win_lock_all_adapter& = default;
  ~win_lock_all_adapter() = default;

  explicit win_lock_all_adapter(const win& win, int mode = 0)
      : win_(&win), mode_(mode) {}

  void lock() { win_->lock_all(mode_); }
  void unlock() { win_->unlock_all(); }

 private:
  const win* win_{nullptr};
  int mode_ = 0;
};

}  // namespace rpmbb::mpi
