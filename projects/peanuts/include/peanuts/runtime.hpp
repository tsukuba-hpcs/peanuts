#pragma once

#include "peanuts/mpi.hpp"
#include "peanuts/options.hpp"
#include "peanuts/topology.hpp"

namespace peanuts {

class runtime {
 public:
  runtime(mpi::comm comm = mpi::comm{MPI_COMM_WORLD})
      : topo_{std::move(comm)}, runtime_opt_init_{} {}

  auto comm() const -> const mpi::comm& { return topo_.comm(); }

 private:
  topology topo_;
  runtime_option_initializer runtime_opt_init_;
};

}  // namespace peanuts
