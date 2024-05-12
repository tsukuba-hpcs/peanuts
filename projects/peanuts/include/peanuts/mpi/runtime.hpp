#pragma once

#include <mpi.h>

#include "peanuts/mpi/error.hpp"
#include "peanuts/mpi/raii.hpp"

namespace peanuts::mpi {
class runtime final {
  raii::unique_env env_{};

 public:
  runtime() : runtime(nullptr, nullptr) {}
  explicit runtime(int* argc, char*** argv) {
    if (!is_initialized()) {
      MPI_CHECK_ERROR_CODE(MPI_Init(argc, argv));
      env_ = raii::unique_env{true};
    }
  }
  explicit runtime(int* argc, char*** argv, int required_thread_support) {
    if (!is_initialized()) {
      int provided;
      MPI_CHECK_ERROR_CODE(
          MPI_Init_thread(argc, argv, required_thread_support, &provided));
      env_ = raii::unique_env{true};
    }
  }

  static auto is_initialized() -> bool {
    int flag;
    MPI_CHECK_ERROR_CODE(MPI_Initialized(&flag));
    return flag != 0;
  }

  static auto is_finalize() -> bool {
    int flag;
    MPI_CHECK_ERROR_CODE(MPI_Finalized(&flag));
    return flag != 0;
  }

  static int query_thread_support() {
    int provided_thread_support;
    MPI_CHECK_ERROR_CODE(MPI_Query_thread(&provided_thread_support));
    return provided_thread_support;
  }

  static bool is_thread_main() {
    int result;
    MPI_CHECK_ERROR_CODE(MPI_Is_thread_main(&result));
    return static_cast<bool>(result);
  }

  static std::string processor_name() {
    std::string result(MPI_MAX_PROCESSOR_NAME, '\n');
    int size{0};
    MPI_CHECK_ERROR_CODE(MPI_Get_processor_name(&result[0], &size));
    result.resize(static_cast<std::size_t>(size));
    return result;
  }
};

}  // namespace peanuts::mpi
