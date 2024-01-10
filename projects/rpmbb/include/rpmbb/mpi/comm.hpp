#pragma once

#include <mpi.h>

#include "rpmbb/mpi/detail/container_adapter.hpp"
#include "rpmbb/mpi/dtype.hpp"
#include "rpmbb/mpi/error.hpp"
#include "rpmbb/mpi/group.hpp"
#include "rpmbb/mpi/info.hpp"
#include "rpmbb/mpi/raii.hpp"
#include "rpmbb/mpi/status.hpp"
#include "rpmbb/mpi/type.hpp"
#include "rpmbb/mpi/type_traits.hpp"

#include <numeric>
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

  auto group() const -> group {
    MPI_Group group;
    MPI_CHECK_ERROR_CODE(MPI_Comm_group(native(), &group));
    return mpi::group{group, true};
  }

  auto barrier() const -> void { MPI_CHECK_ERROR_CODE(MPI_Barrier(native())); }

  template <typename T, typename U>
  void all_gather(std::span<const T> send_data,
                  const dtype& send_dtype,
                  std::span<U> recv_data,
                  const dtype& recv_dtype) const {
    MPI_CHECK_ERROR_CODE(MPI_Allgather(
        send_data.data(), static_cast<int>(send_data.size()), send_dtype,
        recv_data.data(), static_cast<int>(recv_data.size()) / size(),
        recv_dtype, native()));
  }

  template <typename T, typename U>
  void all_gather(std::span<const T> send_data, std::span<U> recv_data) const {
    all_gather(send_data, to_dtype<std::remove_cv_t<T>>(), recv_data,
               to_dtype<std::remove_cv_t<U>>());
  }

  template <typename T, typename U>
  void all_gather(const T& send_data,
                  const dtype& send_dtype,
                  std::span<U> recv_data,
                  const dtype& recv_dtype) const {
    using send_adapter = detail::container_adapter<const T>;
    all_gather(send_adapter::to_cspan(send_data), send_dtype, recv_data,
               recv_dtype);
  }

  template <typename T, typename U>
  void all_gather(const T& send_data, std::span<U> recv_data) const {
    using send_adapter = detail::container_adapter<const T>;
    all_gather(send_adapter::to_cspan(send_data), send_adapter::to_dtype(),
               recv_data, mpi::to_dtype<std::remove_cv_t<U>>());
  }

  template <typename T, typename U>
  void all_gather_v(std::span<const T> send_data,
                    const dtype& send_dtype,
                    std::span<U> recv_data,
                    const dtype& recv_dtype,
                    std::span<const int> recv_counts,
                    std::span<const int> recv_displs) const {
    MPI_CHECK_ERROR_CODE(
        MPI_Allgatherv(send_data.data(), static_cast<int>(send_data.size()),
                       send_dtype, recv_data.data(), recv_counts.data(),
                       recv_displs.data(), recv_dtype, native()));
  }

  template <typename T, typename U>
  void all_gather_v(const T& send_data,
                    std::span<U> recv_data,
                    std::span<const int> recv_counts,
                    std::span<const int> recv_displs) const {
    using send_adapter = detail::container_adapter<const T>;
    all_gather_v(send_adapter::to_cspan(send_data), send_adapter::to_dtype(),
                 recv_data, mpi::to_dtype<std::remove_cv_t<U>>(), recv_counts,
                 recv_displs);
  }

  template <typename T, typename U>
  void all_gather_v(const T& send_data,
                    std::span<U> recv_data,
                    std::span<const int> recv_counts) const {
    std::vector<int> recv_displs(recv_counts.size());
    std::exclusive_scan(recv_counts.begin(), recv_counts.end(),
                        recv_displs.begin(), 0);
    all_gather_v(send_data, recv_data, recv_counts, recv_displs);
  }

  template <typename T>
  void broadcast(std::span<T> send_recv_data,
                 const dtype& dtype,
                 int root = 0) const {
    MPI_CHECK_ERROR_CODE(MPI_Bcast(send_recv_data.data(),
                                   static_cast<int>(send_recv_data.size()),
                                   dtype, root, native()));
  }

  template <typename T>
  void broadcast(std::span<T> send_recv_data, int root = 0) const {
    broadcast(send_recv_data, to_dtype<std::remove_cv_t<T>>(), root);
  }

  template <typename T>
  void broadcast(T& send_recv_data, const dtype& dtype, int root = 0) const {
    using adapter = detail::container_adapter<T>;
    broadcast(adapter::to_span(send_recv_data), dtype, root);
  }

  template <typename T>
  void broadcast(T& send_recv_data, int root = 0) const {
    using adapter = detail::container_adapter<T>;
    broadcast(adapter::to_span(send_recv_data), adapter::to_dtype(), root);
  }

  // sendrecv
  template <typename T, typename U>
  auto send_receive(std::span<const T> send_data,
                    const dtype& send_dtype,
                    const int send_rank,
                    const int send_tag,
                    std::span<U> recv_data,
                    const dtype& recv_dtype,
                    const int recv_rank = MPI_ANY_SOURCE,
                    const int recv_tag = MPI_ANY_TAG) const -> status {
    mpi::status status;
    MPI_CHECK_ERROR_CODE(
        MPI_Sendrecv(send_data.data(), static_cast<int>(send_data.size()),
                     send_dtype, send_rank, send_tag, recv_data.data(),
                     static_cast<int>(recv_data.size()), recv_dtype, recv_rank,
                     recv_tag, native(), &status.native()));
    return status;
  }

  template <typename T, typename U>
  auto send_receive(const T& send_data,
                    const int send_rank,
                    const int send_tag,
                    U& recv_data,
                    const int recv_rank = MPI_ANY_SOURCE,
                    const int recv_tag = MPI_ANY_TAG) const -> status {
    using send_adapter = detail::container_adapter<const T>;
    using recv_adapter = detail::container_adapter<U>;
    return send_receive(send_adapter::to_cspan(send_data),
                        send_adapter::to_dtype(), send_rank, send_tag,
                        recv_adapter::to_span(recv_data),
                        recv_adapter::to_dtype(), recv_rank, recv_tag);
  }
};

}  // namespace rpmbb::mpi
