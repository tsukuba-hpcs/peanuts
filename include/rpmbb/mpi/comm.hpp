#pragma once

#include <mpi.h>

#include "rpmbb/mpi/dtype.hpp"
#include "rpmbb/mpi/error.hpp"
#include "rpmbb/mpi/info.hpp"
#include "rpmbb/mpi/raii.hpp"
#include "rpmbb/mpi/type.hpp"

#include <span>

namespace rpmbb::mpi {

class comm {
  raii::unique_comm comm_{MPI_COMM_NULL};

 public:
  static auto world() -> const comm& {
    static comm instance{MPI_COMM_WORLD, false};
    return instance;
  }

  static auto self() -> const comm& {
    static comm instance{MPI_COMM_SELF, false};
    return instance;
  }

 public:
  comm() = default;
  explicit comm(const MPI_Comm native, const bool managed = false)
      : comm_(native, managed) {}
  explicit comm(const comm& base,
                const split_type type,
                const int key = 0,
                const info& info = MPI_INFO_NULL) {
    MPI_Comm comm;
    MPI_CHECK_ERROR_CODE(
        MPI_Comm_split_type(base, static_cast<int>(type), key, info, &comm));
    comm_.reset(comm);
    comm_.get_deleter().set_should_delete(true);
  }
  explicit comm(const comm& base, const int color, const int key = 0) {
    MPI_Comm comm;
    MPI_CHECK_ERROR_CODE(MPI_Comm_split(base, color, key, &comm));
    comm_.reset(comm);
    comm_.get_deleter().set_should_delete(true);
  }

  operator MPI_Comm() const { return native(); }
  auto native() const -> MPI_Comm { return comm_.get().native; }

  int rank() const {
    int result;
    MPI_CHECK_ERROR_CODE(MPI_Comm_rank(native(), &result));
    return result;
  }

  int size() const {
    int result;
    MPI_CHECK_ERROR_CODE(MPI_Comm_size(native(), &result));
    return result;
  }

  template <typename T, typename U>
  void all_gather(std::span<const T> send_data,
                  std::span<U> recv_data,
                  const dtype& send_dtype,
                  const dtype& recv_dtype) const {
    MPI_CHECK_ERROR_CODE(MPI_Allgather(
        send_data.data(), static_cast<int>(send_data.size()), send_dtype,
        recv_data.data(), static_cast<int>(recv_data.size()) / size(),
        recv_dtype, native()));
  }

  template <typename T, typename U>
  void all_gather(std::span<const T> send_data, std::span<U> recv_data) const {
    all_gather(send_data, recv_data, to_dtype<std::remove_cv_t<T>>(),
               to_dtype<std::remove_cv_t<U>>());
  }

  template <typename T, typename U>
  void all_gather(const T& send_data, std::span<U> recv_data) const {
    all_gather(std::span<const T>(&send_data, 1), recv_data);
  }
};

}  // namespace rpmbb::mpi
