#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "rpmbb/utils/sense_barrier.hpp"
#include <doctest/doctest.h>
#include <thread>
#include <vector>

TEST_CASE("sense_barrier") {
  using namespace rpmbb::utils;

  constexpr size_t nthreads = 10;
  std::vector<std::thread> threads;
  sense_barrier barrier(nthreads);

  for (size_t i = 0; i < nthreads; ++i) {
    threads.emplace_back([&, i] {
      for (size_t j = 0; j < 100; ++j) {
        barrier.wait();
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }
}
