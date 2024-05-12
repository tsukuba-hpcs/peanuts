#pragma once

#include <mpi.h>

#include "peanuts/mpi/error.hpp"

namespace peanuts::mpi {
class aint {
 public:
  static auto bottom() -> aint { return aint{MPI_BOTTOM}; }

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
  MPI_Aint& native() { return addr_; }
  operator MPI_Aint&() { return addr_; }

  auto operator+(aint disp) -> aint const {
    return MPI_Aint_add(addr_, disp.addr_);
  }
  auto operator+=(aint disp) -> aint& {
    addr_ = MPI_Aint_add(addr_, disp.addr_);
    return *this;
  }
  auto operator-(aint sub) -> aint const {
    return MPI_Aint_diff(addr_, sub.addr_);
  }
  auto operator-=(aint sub) -> aint& {
    addr_ = MPI_Aint_diff(addr_, sub.addr_);
    return *this;
  }

 private:
  MPI_Aint addr_{0};
};

}  // namespace peanuts::mpi
