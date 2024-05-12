#pragma once

#include <mpi.h>

#include "peanuts/mpi/aint.hpp"
#include "peanuts/mpi/error.hpp"
#include "peanuts/mpi/info.hpp"
#include "peanuts/mpi/type.hpp"

namespace peanuts::mpi {

template <typename T = void>
T* allocate(aint size, const info& info = MPI_INFO_NULL) {
  T* result{};
  MPI_CHECK_ERROR_CODE(MPI_Alloc_mem(size, info.native(), &result));
  return result;
}

template <typename T = void>
void free(T* location) {
  MPI_CHECK_ERROR_CODE(MPI_Free_mem(location));
}

}  // namespace peanuts::mpi
