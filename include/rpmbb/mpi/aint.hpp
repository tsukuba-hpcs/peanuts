#pragma once

#include <mpi.h>

#include "rpmbb/mpi/error.hpp"

namespace rpmbb::mpi {
class aint {
 public:
  aint() = default;
  aint(MPI_Aint addr) : addr_(addr) {}
  template <typename T = void>
  explicit aint(const T* ptr) : addr_(to_aint<T>(ptr)) {}

  template <typename T = void>
  static MPI_Aint to_aint(const T* ptr) {
    MPI_Aint addr;
    MPI_CHECK_ERROR_CODE(MPI_Get_address(ptr, &addr));
    return addr;
  }

  MPI_Aint native() const { return addr_; }
  operator MPI_Aint() const { return addr_; }

  auto operator+(aint disp) -> aint const { return MPI_Aint_add(addr_, disp); }
  auto operator+=(aint disp) -> aint& {
    addr_ = MPI_Aint_add(addr_, disp);
    return *this;
  }
  auto operator-(aint sub) -> aint const { return MPI_Aint_diff(addr_, sub); }
  auto operator-=(aint sub) -> aint& {
    addr_ = MPI_Aint_diff(addr_, sub);
    return *this;
  }

 private:
  MPI_Aint addr_{0};
};

}  // namespace rpmbb::mpi
