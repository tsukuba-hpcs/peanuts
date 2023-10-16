#define DOCTEST_CONFIG_IMPLEMENT
// #define DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES
// #include "doctest/doctest.h"
#include "doctest/extensions/doctest_mpi.h"

#include "rpmbb.hpp"
#include "rpmbb/inspector/mpi.hpp"
using namespace rpmbb;

#include <mpi.h>
#include <mutex>
#include <span>
#include <thread>

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

TEST_CASE("barrier") {
  MPI_Barrier(MPI_COMM_WORLD);
}

TEST_CASE("mpi::mpi_error_category") {
  rpmbb::mpi::mpi_error_category cat;
  auto ec = cat.default_error_condition(MPI_ERR_FILE_IN_USE);
  CHECK(ec.category().name() == "mpi");
  CHECK(ec.value() == MPI_ERR_FILE_IN_USE);
  MESSAGE(ec.message());
}

TEST_CASE("mpi::env") {
  rpmbb::mpi::env env;
  rpmbb::mpi::env env2 = std::move(env);
  rpmbb::mpi::env env3{std::move(env2)};
  CHECK(rpmbb::mpi::env::is_initialized() == true);
  CHECK(rpmbb::mpi::env::is_finalize() == false);
  CHECK(rpmbb::mpi::env::query_thread_support() == MPI_THREAD_MULTIPLE);
  CHECK(rpmbb::mpi::env::is_thread_main() == true);
  MESSAGE(rpmbb::mpi::env::processor_name());
  CHECK(rpmbb::mpi::env::processor_name().size() > 0);
}

TEST_CASE("comm") {
  const auto& comm = rpmbb::mpi::comm::world();
  CHECK(comm.size() == doctest::mpi_comm_world_size());

  SUBCASE("all_gather") {
    const std::vector<int> send_data{1, 2, 3};
    std::vector<int> recv_data(send_data.size() * comm.size());

    comm.all_gather(std::span{send_data}, std::span{recv_data});

    for (auto [i, v] : utils::enumerate(recv_data)) {
      CHECK(v == i % 3 + 1);
    }
  }

  SUBCASE("all_gather byte") {
    const std::vector<int> send_data{1, 2, 3};
    std::vector<int> recv_data(send_data.size() * comm.size());

    comm.all_gather(std::as_bytes(std::span{send_data}),
                    std::as_writable_bytes(std::span{recv_data}));

    for (auto [i, v] : utils::enumerate(recv_data)) {
      CHECK(v == i % 3 + 1);
    }
  }
}

TEST_CASE("win") {
  const auto& comm = mpi::comm::world();
  mpi::win win{comm};
  win.lock_all(MPI_MODE_NOCHECK);
  win.unlock_all();
  mpi::win_lock_all_adapter adapter{win, MPI_MODE_NOCHECK};
  std::unique_lock lock{adapter};

  CHECK((win.is_separate_memory_model() || win.is_unified_memory_model()) ==
        true);

  win.set_name("test");
  CHECK(win.get_name() == "test");

  auto info = win.get_info();
  MESSAGE(rpmbb::utils::to_string(info));

  char buf[4096];
  win.attach(std::as_writable_bytes(std::span(buf)));
}

TEST_CASE("info") {
  mpi::info info{};
  CHECK(info != mpi::info{MPI_INFO_NULL});
  mpi::info info_null{MPI_INFO_NULL};
  CHECK(info_null == MPI_INFO_NULL);
  CHECK(info_null == mpi::info::null());
}

TEST_CASE("info: set and get") {
  rpmbb::mpi::info my_info;

  // Test setting and getting key-value pairs
  my_info.set("key1", "value1");
  auto value1 = my_info.get("key1");
  CHECK(value1.has_value());
  CHECK(value1.value() == "value1");

  // Test setting and getting key-value pairs with different key
  my_info.set("key2", "value2");
  auto value2 = my_info.get("key2");
  CHECK(value2.has_value());
  CHECK(value2.value() == "value2");

  // Test removing key-value pairs
  my_info.remove("key1");
  auto removed_value = my_info.get("key1");
  CHECK(!removed_value.has_value());

  // Test get_nkeys() function
  int nkeys = my_info.get_nkeys();
  CHECK(nkeys == 1);  // "key2" should be the only remaining key

  // Test get_nthkey() function
  auto nth_key = my_info.get_nthkey(0);
  CHECK(nth_key == "key2");
}

TEST_CASE("info: Conversion to and from map") {
  rpmbb::mpi::info my_info;

  // Set key-value pairs
  my_info.set("key1", "value1");
  my_info.set("key2", "value2");

  // Convert to map
  auto map = my_info.to_map();
  CHECK(map.size() == 2);
  CHECK(map["key1"] == "value1");
  CHECK(map["key2"] == "value2");

  // Create info object from map
  rpmbb::mpi::info new_info(map);
  CHECK(new_info.get("key1").value() == "value1");
  CHECK(new_info.get("key2").value() == "value2");
}

TEST_CASE("info class: Initialize from initializer list") {
  rpmbb::mpi::info my_info = {{"key1", "value1"}, {"key2", "value2"}};

  CHECK(my_info.get("key1").value() == "value1");
  CHECK(my_info.get("key2").value() == "value2");
}

TEST_CASE("info class: Initializer list with duplicate keys") {
  rpmbb::mpi::info my_info = {
      {"key", "value1"}, {"key", "value2"}, {"key", "value3"}};

  // The last value should overwrite the previous ones
  CHECK(my_info.get("key").value() == "value3");
}

TEST_CASE("info iterator") {
  using rpmbb::mpi::info;
  using map_type = info::map_type;

  map_type expected_map = {{"key1", "value1"}, {"key2", "value2"}};
  info info_obj(expected_map);

  map_type observed_map;
  for (const auto& [key, value] : info_obj) {
    observed_map[key] = value;
  }

  REQUIRE(expected_map == observed_map);
}

TEST_CASE("aint") {
  // Constructor from MPI_Aint
  SUBCASE("Construct from MPI_Aint") {
    rpmbb::mpi::aint a(42);
    CHECK(a.native() == 42);
  }

  // Constructor from void pointer
  SUBCASE("Construct from void pointer") {
    int data;
    rpmbb::mpi::aint a(&data);
    CHECK(a.native() == rpmbb::mpi::aint::to_aint(&data));
  }

  // Test type conversion to MPI_Aint
  SUBCASE("Type conversion to MPI_Aint") {
    rpmbb::mpi::aint a(42);
    MPI_Aint mpi_aint = a;
    CHECK(mpi_aint == 42);
  }

  // Test addition operator
  SUBCASE("Operator +") {
    rpmbb::mpi::aint a(40);
    rpmbb::mpi::aint b(2);
    rpmbb::mpi::aint result = a + b;
    CHECK(result.native() == 42);
  }

  // Test addition-assignment operator
  SUBCASE("Operator +=") {
    rpmbb::mpi::aint a(40);
    rpmbb::mpi::aint b(2);
    a += b;
    CHECK(a.native() == 42);
  }

  // Test subtraction operator
  SUBCASE("Operator -") {
    rpmbb::mpi::aint a(44);
    rpmbb::mpi::aint b(2);
    rpmbb::mpi::aint result = a - b;
    CHECK(result.native() == 42);
  }

  // Test subtraction-assignment operator
  SUBCASE("Operator -=") {
    rpmbb::mpi::aint a(44);
    rpmbb::mpi::aint b(2);
    a -= b;
    CHECK(a.native() == 42);
  }
}

TEST_CASE("mpi") {
  SUBCASE("is_basic") {
    const auto& int_dtype = mpi::dtype{MPI_INT, false};
    CHECK(int_dtype.is_basic() == true);
    auto derived_dtype = mpi::dtype{int_dtype, 10};
    CHECK(derived_dtype.is_basic() == false);
  }
}
