#define DOCTEST_CONFIG_IMPLEMENT
// #define DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES
// #include "doctest/doctest.h"
#include "doctest/extensions/doctest_mpi.h"

#include "rpmbb.hpp"

#include <thread>
#include <mpi.h>

int main(int argc, char** argv) {
  // MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, nullptr);
  doctest::mpi_init_thread(argc, argv, MPI_THREAD_MULTIPLE);

  doctest::Context ctx;
  ctx.setOption("abort-after", 5);
  ctx.setOption("reporters", "MpiConsoleReporter");
  // ctx.setOption("reporters", "MpiFileReporter");
  ctx.setOption("force-colors", true);
  ctx.applyCommandLine(argc, argv);

  int test_result = ctx.run();

  // MPI_Finalize();
  doctest::mpi_finalize();

  return test_result;
}

DOCTEST_TEST_CASE("barrier") {
  MPI_Barrier(MPI_COMM_WORLD);
}
