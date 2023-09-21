#pragma once

#include <system_error>

#include <mpi.h>

namespace rpmbb::mpi {

class mpi_error_category : public std::error_category {
 public:
  auto name() const noexcept -> const char* override { return "mpi"; }

  auto message(int ev) const -> std::string override {
    char msg[MPI_MAX_ERROR_STRING];
    int resultlen;
    if (MPI_Error_string(ev, msg, &resultlen) != MPI_SUCCESS) {
      return "Unknown MPI error";
    }
    return msg;
  }
};

inline auto mpi_category() -> const std::error_category& {
  static mpi_error_category instance;
  return instance;
}

inline auto make_error_code(int ev) -> std::error_code {
  return {ev, mpi_category()};
}

class mpi_error : public std::system_error {
 public:
  explicit mpi_error(int ev, const std::string& what_arg = "")
      : std::system_error(make_error_code(ev), what_arg) {}

  int error_code() const noexcept { return code().value(); }
};

#define MPI_CHECK_ERROR_CODE(call)            \
  do {                                        \
    if (int ev = (call); ev != MPI_SUCCESS) { \
      throw rpmbb::mpi::mpi_error(ev, #call); \
    }                                         \
  } while (0)

}  // namespace rpmbb::mpi
