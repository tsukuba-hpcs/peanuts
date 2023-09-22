#pragma once

#include <mpi.h>

#include "rpmbb/mpi/aint.hpp"
#include "rpmbb/mpi/error.hpp"
#include "rpmbb/mpi/info.hpp"
#include "rpmbb/mpi/type.hpp"

namespace rpmbb::mpi {

template <typename T = void>
T* allocate(aint size, const info& info = MPI_INFO_NULL) {
  T* result{};
  MPI_CHECK_ERROR_CODE(MPI_Alloc_mem(size, info.native(), result));
  return result;
}

template <typename T = void>
void free(T* location) {
  MPI_CHECK_ERROR_CODE(MPI_Free_mem(location));
}

}  // namespace rpmbb::mpi
