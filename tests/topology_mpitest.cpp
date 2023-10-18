#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/extensions/doctest_mpi.h"

#include "rpmbb/inspector.hpp"
#include "rpmbb/topology.hpp"
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

TEST_CASE("topology") {
  rpmbb::topology topo{};

  int world_rank;
  int world_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  CHECK(topo.rank() == world_rank);
  CHECK(topo.size() == world_size);
  CHECK(topo.rank_pair2global_rank(topo.inter_rank(), topo.intra_rank()) ==
        topo.rank());
  CHECK(topo.global2intra_rank(topo.rank()) == topo.intra_rank());
  CHECK(topo.global2inter_rank(topo.rank()) == topo.inter_rank());

  if (topo.rank() == 0) {
    auto intra_size = topo.intra_size();
    if (topo.inter_size() > 1) {
      CHECK(topo.rank_pair2global_rank(1, 0) == intra_size);
      CHECK(topo.global2rank_pair(intra_size) == rank_pair{1, 0});
    }
  }
}
