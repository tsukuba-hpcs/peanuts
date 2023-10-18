#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/extensions/doctest_mpi.h"

#include "rpmbb/rpm.hpp"
using namespace rpmbb;

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

TEST_CASE("rpm") {
  topology topo{};
  rpm rpm{&topo, "/tmp/pmem2_devtest", (1ULL << 21)};
}
