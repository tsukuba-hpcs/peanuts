#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/extensions/doctest_mpi.h"

#include "rpmbb_c.h"

#include <mpi.h>

#include <fcntl.h>
#include <string>
#include <vector>

int main(int argc, char** argv) {
  doctest::mpi_init_thread(argc, argv, MPI_THREAD_MULTIPLE);

  doctest::Context ctx;
  ctx.setOption("abort-after", 5);
  ctx.setOption("reporters", "MpiConsoleReporter");
  // ctx.setOption("reporters", "MpiFileReporter");
  ctx.setOption("force-colors", true);
  ctx.applyCommandLine(argc, argv);

  int test_result = ctx.run();

  doctest::mpi_finalize();

  return test_result;
}

TEST_CASE("rpmbb store and handler operations") {
  int world_rank, world_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);
  const char* pmem_path = "/tmp/pmem2_devtest_c";
  size_t pmem_size = (2ULL << 20) * world_size;

  auto store = rpmbb_store_create(MPI_COMM_WORLD, pmem_path, pmem_size);
  REQUIRE(store != nullptr);

  SUBCASE("rpmbb store save and load") {
    int save_result = rpmbb_store_save(store);
    CHECK(save_result == 0);

    int load_result = rpmbb_store_load(store);
    CHECK(load_result == 0);
  }

  SUBCASE("rpmbb handler operations") {
    const auto files = std::vector<std::string>{"./bb_store_test_file1",
                                                "./bb_store_test_file2",
                                                "./bb_store_test_file3"};
    auto handler = rpmbb_store_open(store, MPI_COMM_WORLD, files[0].c_str(),
                                    O_RDWR | O_CREAT, 0644);
    REQUIRE(handler != nullptr);

    const char* test_data = "Hello, rpmbb!";
    ssize_t written = rpmbb_bb_pwrite(handler, test_data, strlen(test_data), 0);
    CHECK(written >= 0);

    char read_buf[50];
    ssize_t read = rpmbb_bb_pread(handler, read_buf, strlen(test_data), 0);
    CHECK(read >= 0);
    CHECK(strncmp(test_data, read_buf, read) == 0);

    int sync_result = rpmbb_bb_sync(handler);
    CHECK(sync_result == 0);

    int close_result = rpmbb_bb_close(handler);
    CHECK(close_result == 0);
  }

  int free_result = rpmbb_store_free(store);
  CHECK(free_result == 0);
}
