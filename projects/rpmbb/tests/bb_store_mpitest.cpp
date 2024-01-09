#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/extensions/doctest_mpi.h"

#include "rpmbb/bb.hpp"
#include "rpmbb/raii/fd.hpp"
#include "rpmbb/ring_buffer.hpp"
#include "rpmbb/rpm.hpp"
#include "rpmbb/topology.hpp"
#include "rpmbb/utils/fs.hpp"
using namespace rpmbb;

#include <fmt/format.h>
#include <zpp/file.h>

#include <mpi.h>

#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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

TEST_CASE("bb_store save/load") {
  const char* pmem_path = "/tmp/pmem2_devtest_save_load";
  const char* file_path = "/tmp/bb_testfile";
  {
    topology topo{};
    rpm rpm{std::cref(topo), pmem_path, (2ULL << 20) * topo.intra_size()};
    auto store = bb_store{rpm};
    auto file = zpp::filesystem::file(zpp::filesystem::file_handle{
        ::open(file_path, O_RDWR | O_CREAT, 0644)});
    auto handler = store.open(file.get());
    MESSAGE("ino: ", utils::get_ino(file.get()));
    handler->pwrite(std::as_bytes(std::span{"hello world.", 12}), 0);
    handler->pwrite(std::as_bytes(std::span{"hoge", 4}), 256);
    handler.reset();
    store.save();
  }

  {
    topology topo{};
    rpm rpm{std::cref(topo), pmem_path, (2ULL << 20) * topo.intra_size()};
    auto store = bb_store{rpm};
    store.load();
    auto file = zpp::filesystem::file(zpp::filesystem::file_handle{
        ::open(file_path, O_RDWR | O_CREAT, 0644)});
    auto handler = store.open(file.get());
    std::string buf(12, '\0'); 
    handler->pread(std::as_writable_bytes(std::span{buf}), 0);
    CHECK(std::string_view{"hello world."} == buf);
    handler->pread(std::as_writable_bytes(std::span{buf}.subspan(0, 4)), 256);
    CHECK(std::string_view{"hoge"} == buf.substr(0, 4));
    handler.reset();
    store.save();
  }
}

TEST_CASE("bb_store") {
  topology topo{};
  rpm rpm{std::cref(topo), "/tmp/pmem2_devtest",
          (2ULL << 20) * topo.intra_size()};
  auto store = bb_store{rpm};

  SUBCASE("basic usage senario") {
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

  SUBCASE("save and load") {
    auto dat = fmt::format("myrank: {}", topo.rank());
    {
      auto h1 = store.open(ino_t{1});
      auto h2 = store.open(ino_t{2});

      h2->pwrite(std::as_bytes(std::span{dat.data(), dat.size()}),
                 dat.size() * topo.rank());
      h2->sync();
      store.save();
    }

    auto store2 = bb_store{rpm};
    store2.load();
    CHECK(store2.open(ino_t{1}) != nullptr);
    auto h2 = store2.open(ino_t{2});
    CHECK(h2 != nullptr);
    auto dat2 = std::vector<char>(dat.size());
    h2->pread(std::as_writable_bytes(std::span{dat2}),
              dat.size() * topo.rank());
    CHECK(std::equal(dat.begin(), dat.end(), dat2.begin(), dat2.end()));
  }
}

TEST_CASE("Testing serialization and deserialization of bb") {
  topology topo{};
  // Create the original bb object
  rpmbb::bb original_bb;
  original_bb.ino = 12345;
  for (int rank = 0; rank < topo.size(); ++rank) {
    original_bb.global_tree.add(rank * 100, 100 + rank * 100, 0, rank);
  }
  original_bb.local_tree.add(topo.size() * 100, topo.size() * 100 + 100, 100,
                             topo.rank());

  auto [serialized_data, in, out] = zpp::bits::data_in_out();
  // Serialize
  out(original_bb).or_throw();

  // Deserialize
  rpmbb::bb deserialized_bb;
  in(deserialized_bb).or_throw();

  // Compare original and deserialized objects
  CHECK(deserialized_bb.ino == original_bb.ino);
  CHECK(deserialized_bb.global_tree == original_bb.global_tree);
  CHECK(deserialized_bb.local_tree == original_bb.local_tree);
}

TEST_CASE(
    "Testing serialization and deserialization of sizeof(size_t) and bb") {
  topology topo{};
  // Create an original bb object
  rpmbb::bb original_bb;
  original_bb.ino = 12345;  // Set an example ino value
  for (int rank = 0; rank < topo.size(); ++rank) {
    original_bb.global_tree.add(rank * 100, 100 + rank * 100, 0, rank);
  }
  original_bb.local_tree.add(topo.size() * 100, topo.size() * 100 + 100, 100,
                             topo.rank());

  // Serialize size and bb object
  auto [ser_bb, out] = zpp::bits::data_out();
  ser_bb.resize(sizeof(size_t));
  out.position() += sizeof(size_t);
  out(original_bb).or_throw();
  *reinterpret_cast<size_t*>(ser_bb.data()) = ser_bb.size() - sizeof(size_t);

  // Deserialize size and bb object
  size_t serialized_size = *reinterpret_cast<size_t*>(ser_bb.data());
  auto ser_bb_content = std::span(ser_bb).subspan(sizeof(size_t));
  CHECK(serialized_size == ser_bb_content.size());
  auto in = zpp::bits::in{ser_bb_content};
  rpmbb::bb deserialized_bb;
  in(deserialized_bb).or_throw();

  // Compare original and deserialized bb objects
  CHECK(deserialized_bb.ino == original_bb.ino);
  CHECK(deserialized_bb.global_tree == original_bb.global_tree);
  CHECK(deserialized_bb.local_tree == original_bb.local_tree);
}
