#pragma once

#include <mpi.h>

#include "rpmbb/mpi/error.hpp"
#include "rpmbb/mpi/raii.hpp"

namespace rpmbb::mpi {

class comm {
  MPI_Comm comm_{MPI_COMM_NULL};
  raii::unique_comm managed_comm_{MPI_COMM_NULL};

 public:
  static auto world() -> const comm& {
    static comm instance{MPI_COMM_WORLD};
    return instance;
  }

  static auto self() -> const comm& {
    static comm instance{MPI_COMM_SELF};
    return instance;
  }

 public:
  comm() = default;
  explicit comm(const MPI_Comm native, const bool managed = false)
      : comm_(native), managed_comm_(managed ? native : MPI_COMM_NULL) {}

  auto native() const -> MPI_Comm { return comm_; }

  int rank() const {
    int result;
    MPI_CHECK_ERROR_CODE(MPI_Comm_rank(comm_, &result));
    return result;
  }

  int size() const {
    int result;
    MPI_CHECK_ERROR_CODE(MPI_Comm_size(comm_, &result));
    return result;
  }
};

}  // namespace rpmbb::mpi
