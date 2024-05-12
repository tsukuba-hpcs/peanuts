#pragma once

#include "comm.hpp"

#include <functional>
#include <ostream>

namespace peanuts::mpi {

template <typename Func>
void run_on_rank0(Func&& func, const mpi::comm& comm = mpi::comm::world()) {
  if (comm.rank() == 0) {
    std::invoke(std::forward<Func>(func));
  }
}

}  // namespace peanuts::mpi
