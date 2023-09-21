#pragma once

#include <mpi.h>

#include "rpmbb/mpi/error.hpp"
#include "rpmbb/mpi/type.hpp"

namespace rpmbb::mpi {

// template <typename T = void>
// T* allocate(const aint size, const information& info = information()) {
//   T* result{};
//   MPI_CHECK_ERROR_CODE(MPI_Alloc_mem(size, info.native(), result));
//   return result;
// }

template <typename T = void>
void free(T* location) {
  MPI_CHECK_ERROR_CODE(MPI_Free_mem(location));
}

template <typename T = void>
aint get_address(const T* location) {
  aint result;
  MPI_CHECK_ERROR_CODE(MPI_Get_address(location, &result));
  return result;
}

inline aint add(const aint& lhs, const aint& rhs) {
  return MPI_Aint_add(lhs, rhs);
}

inline aint difference(const aint& lhs, const aint& rhs) {
  return MPI_Aint_diff(lhs, rhs);
}

}  // namespace rpmbb::mpi
