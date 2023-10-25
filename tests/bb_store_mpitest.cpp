#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/extensions/doctest_mpi.h"

#include "rpmbb/bb.hpp"
#include "rpmbb/raii/fd.hpp"
#include "rpmbb/rpm.hpp"
#include "rpmbb/topology.hpp"
#include "rpmbb/utils/fs.hpp"
using namespace rpmbb;

#include <mpi.h>

#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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

TEST_CASE("bb_store") {
  topology topo{};
  rpm rpm{std::cref(topo), "/tmp/pmem2_devtest",
          (1ULL << 21) * topo.intra_size()};
  auto store = bb_store{rpm};

  const auto files = std::vector<std::string>{"/tmp/bb_store_test_file1",
                                              "/tmp/bb_store_test_file2",
                                              "/tmp/bb_store_test_file3"};

  auto fd1 =
      raii::file_descriptor{::open(files[0].c_str(), O_RDWR | O_CREAT, 0644)};
  auto fd2 =
      raii::file_descriptor{::open(files[1].c_str(), O_RDWR | O_CREAT, 0644)};
  auto handler1 = store.open(fd1.get());
  auto handler2 = store.open(fd2.get());

  CHECK(handler1 != nullptr);
  CHECK(handler2 != nullptr);

  auto ino1 = utils::get_ino(fd1.get());
  store.unlink(fd1.get());

  auto fd3 =
      raii::file_descriptor{::open(files[2].c_str(), O_RDWR | O_CREAT, 0644)};

  // Open new object with same id as unlinked
  auto handler3 = store.open(ino1, fd3.get());

  CHECK(handler3 != nullptr);
}
