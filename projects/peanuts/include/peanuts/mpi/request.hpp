#pragma once

#include <mpi.h>

#include <memory>

MPI_Request request;

namespace peanuts::mpi {
  class request {
    std::unique_ptr<MPI_Request> request_;
  public:
    request() : request_(std::make_unique<MPI_Request>()) {}
  };
}
