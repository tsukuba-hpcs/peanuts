#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/extensions/doctest_mpi.h"

#include "rpmbb/inspector.hpp"
#include "rpmbb/ring_buffer.hpp"
using namespace rpmbb;

#include <mpi.h>

#include <unordered_set>

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

TEST_CASE("local_ring_buffer") {
  topology topo{};
  rpm rpm_instance{topo, "/tmp/pmem2_devtest", (16ULL << 20)};
  local_ring_buffer buffer(rpm_instance, topo.intra_rank());

  SUBCASE("initial state of ring_buffer") {
    CHECK(buffer.head() == 0);
    CHECK(buffer.tail() == 0);
    CHECK(buffer.used_capacity() == 0);
    CHECK(buffer.free_capacity() == buffer.size());
  }

  SUBCASE("reserve and release without wraparound") {
    size_t size_to_reserve = buffer.size() / 2;
    auto reserved_lsn = buffer.reserve_unsafe(size_to_reserve);
    CHECK(reserved_lsn == 0);
    CHECK(buffer.used_capacity() == size_to_reserve);

    buffer.consume_unsafe(size_to_reserve);
    CHECK(buffer.tail() == reserved_lsn + size_to_reserve);
    CHECK(buffer.used_capacity() == 0);
  }

  SUBCASE("pread and pwrite without wraparound") {
    std::vector<std::byte> write_data(buffer.size() / 2, std::byte{0xAA});
    auto write_lsn = buffer.reserve_unsafe(write_data.size());
    buffer.pwrite(write_data, write_lsn);

    std::vector<std::byte> read_data(write_data.size());
    buffer.pread(read_data, write_lsn);
    CHECK(write_data == read_data);
  }

  SUBCASE("pread and pwrite with wraparound") {
    std::vector<std::byte> write_data(buffer.size() / 2, std::byte{0xAA});
    // reserve and consume 80% of buffer caps
    CHECK(buffer.reserve_nb(buffer.size() * 0.8).has_value());
    CHECK(buffer.consume_nb(buffer.size() * 0.8).has_value());

    auto write_lsn = buffer.reserve_unsafe(write_data.size());
    buffer.pwrite(write_data, write_lsn);

    std::vector<std::byte> read_data(write_data.size());
    buffer.pread(read_data, write_lsn);
    CHECK(write_data == read_data);
  }

  SUBCASE("wraparound behavior") {
    size_t initial_reserve = buffer.size() * 0.8;
    buffer.reserve_unsafe(initial_reserve);
    buffer.consume_unsafe(initial_reserve);

    size_t wraparound_reserve = buffer.size() * 0.8;
    auto reserve_result = buffer.reserve_nb(wraparound_reserve);
    CHECK(reserve_result.has_value());

    auto extra_reserve = buffer.reserve_nb(buffer.size() * 0.8);
    CHECK_FALSE(extra_reserve.has_value());

    auto release_result = buffer.consume_nb(wraparound_reserve);
    CHECK(release_result.has_value());

    extra_reserve = buffer.reserve_nb(buffer.size() * 0.8);
    CHECK(extra_reserve.has_value());
  }

  SUBCASE("multiple head and tail movements") {
    size_t chunk_size = buffer.size() / 10;
    std::optional<local_ring_buffer::lsn_t> last_reserved_lsn;
    for (int i = 0; i < 5; ++i) {
      last_reserved_lsn = buffer.reserve_nb(chunk_size);
      CHECK(last_reserved_lsn.has_value());
    }

    CHECK(buffer.head() == last_reserved_lsn.value() + chunk_size);

    for (int i = 0; i < 5; ++i) {
      auto release_result = buffer.consume_nb(chunk_size);
      CHECK(release_result.has_value());
    }

    CHECK(buffer.tail() == buffer.head());
  }
}
