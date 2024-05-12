#pragma once

#include "peanuts/mpi/dtype.hpp"
#include "peanuts/mpi/error.hpp"
#include "peanuts/mpi/type.hpp"

#include <mpi.h>

#include <memory>

namespace peanuts::mpi {

class status {
  MPI_Status status_;

 public:
  status() = default;
  status(const status&) = default;
  auto operator=(const status&) -> status& = default;
  status(status&&) = default;
  auto operator=(status&&) -> status& = default;
  status(const MPI_Status& status) : status_(status) {}

  auto native() const -> const MPI_Status& { return status_; }
  auto native() -> MPI_Status& { return status_; }
  operator const MPI_Status&() const { return status_; }
  operator MPI_Status&() { return status_; }

  auto count(const dtype& type) const -> int {
    int result;
    MPI_CHECK_ERROR_CODE(MPI_Get_count(&native(), type, &result));
    MPI_CHECK_UNDEFINED(result, "MPI_Get_count");
    return result;
  }

  auto element_count(const dtype& type) const -> mpi::count {
    mpi::count result;
    MPI_CHECK_ERROR_CODE(MPI_Get_elements_x(&native(), type, &result));
    MPI_CHECK_UNDEFINED(result, "MPI_Get_elements");
    return result;
  }

  auto canelled() const -> bool {
    int result{0};
    MPI_CHECK_ERROR_CODE(MPI_Test_cancelled(&native(), &result));
    return static_cast<bool>(result);
  }

  auto source() const -> int { return native().MPI_SOURCE; }
  auto tag() const -> int { return native().MPI_TAG; }
  auto error_code() const -> int { return native().MPI_ERROR; }
};

}  // namespace peanuts::mpi
