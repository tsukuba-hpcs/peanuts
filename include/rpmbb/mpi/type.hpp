#pragma once

#include <mpi.h>

namespace rpmbb::mpi {
using count = MPI_Count;
using offset = MPI_Offset;

enum class split_type : int {
  undefined = MPI_UNDEFINED,
  shared = MPI_COMM_TYPE_SHARED
};

}  // namespace rpmbb::mpi
