#include <fmt/chrono.h>
#include <fmt/core.h>
#include <peanuts/version.h>

#include <chrono>
#include <cxxopts.hpp>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <ostream>
#include <random>
#include <span>
#include <thread>
#include <vector>
#include "peanuts.hpp"
#include "peanuts/inspector/std_chrono.hpp"
#include "peanuts/inspector/std_containers.hpp"

#include <libpmem2.h>
#include <liburing.h>
#include <sys/uio.h>

using ordered_json = nlohmann::ordered_json;

struct bench_stats {
  peanuts::utils::stopwatch<>::duration elapsed_time;
  size_t total_num_ops;
  size_t total_transfer_size;

  std::ostream& inspect(std::ostream& os) const {
    os << peanuts::utils::make_inspector(elapsed_time) << ", ";
    os << peanuts::utils::to_human<1000>(total_num_ops / elapsed_time.count())
       << "ops/sec, ";
    os << peanuts::utils::to_human(total_transfer_size / elapsed_time.count())
       << "iB/s";
    return os;
  }
};

namespace nlohmann {
template <>
struct adl_serializer<bench_stats> {
  static void to_json(ordered_json& j, const bench_stats& stats) {
    j["elapsed_time"] = stats.elapsed_time.count();
    j["ops"] = stats.total_num_ops / stats.elapsed_time.count();
    j["ops_human"] = peanuts::utils::to_human<1000>(stats.total_num_ops /
                                                  stats.elapsed_time.count()) +
                     "ops/sec";
    j["bw"] = stats.total_transfer_size / stats.elapsed_time.count();
    j["bw_human"] = peanuts::utils::to_human<1000>(stats.total_transfer_size /
                                                 stats.elapsed_time.count()) +
                    "B/s";
  }
};

template <typename Rep, typename Period>
struct adl_serializer<std::chrono::duration<Rep, Period>> {
  static void to_json(ordered_json& j,
                      const std::chrono::duration<Rep, Period>& duration) {
    j = duration.count();
  }
};

template <>
struct adl_serializer<peanuts::utils::welford> {
  static void to_json(ordered_json& j, const peanuts::utils::welford& welford) {
    j["n"] = welford.n();
    // j["mean"] = peanuts::utils::tsc::to_nsec(welford.mean());
    // j["var"] = peanuts::utils::tsc::to_nsec(welford.var());
    // j["std"] = peanuts::utils::tsc::to_nsec(welford.std());
    j["mean"] = welford.mean();
    j["var"] = welford.var();
    j["std"] = welford.std();
  }
};

}  // namespace nlohmann

namespace peanuts::utils {
std::ostream& inspect(std::ostream& os, const welford& wf) {
  os << "{n:" << wf.n() << ',';
  os << "mean:" << wf.mean() << ',';
  os << "var:" << wf.var() << ',';
  os << "std:" << wf.std() << '}';
  return os;
}
}  // namespace peanuts::utils

auto to_string(std::span<const std::byte> span) -> std::string {
  std::string str;
  str.reserve(span.size());
  std::transform(span.begin(), span.end(), std::back_inserter(str),
                 [](std::byte b) { return static_cast<char>(b); });
  return str;
}

bool compare_vector_string(const std::vector<std::byte>& v,
                           const std::string& s) {
  if (v.size() != s.size()) {
    return false;
  }

  for (std::size_t i = 0; i < v.size(); ++i) {
    if (v[i] != static_cast<std::byte>(s[i])) {
      return false;
    }
  }
  return true;
}

auto main(int argc, char* argv[]) -> int try {
  using namespace peanuts;
  mpi::runtime runtime(&argc, &argv);
  cxxopts::Options options("io_uring_bench", "io_uring + lipmem2 benchmark");
  // clang-format off
  options.add_options()
    ("h,help", "Print usage")
    ("V,version", "Print version")
    ("prettify", "Prettify output")
    ("pmem", "path to pmem device", cxxopts::value<std::string>()->default_value("/dev/dax0.0"))
    ("file", "path to posix file", cxxopts::value<std::string>()->default_value("/tmp/io_uring_bench_file"))
    ("t,transfer", "transfer size", cxxopts::value<size_t>()->default_value("4096"))
    ("b,block", "block size - contiguous bytes to write per task", cxxopts::value<size_t>()->default_value("2097152"))
    ("seed", "set seed.", cxxopts::value<std::seed_seq::result_type>()->default_value("42"))
  ;
  // clang-format on

  auto parsed = options.parse(argc, argv);
  if (parsed.count("help") != 0U) {
    fmt::print("{}\n", options.help());
    return 0;
  }

  if (parsed.count("version") != 0U) {
    fmt::print("{}\n", PEANUTS_VERSION);
    return 0;
  }

  // utils::tsc::calibrate();

  const auto transfer_size = parsed["transfer"].as<size_t>();
  const auto block_size = parsed["block"].as<size_t>();
  const auto seed = parsed["seed"].as<std::seed_seq::result_type>();

  assert(block_size % transfer_size == 0);

  auto pmem_device = pmem2::device{parsed["pmem"].as<std::string>()};
  if (!pmem_device.is_devdax()) {
    pmem_device.truncate(block_size);
  }
  auto pmem_source = pmem2::source{pmem_device};
  auto pmem_config = pmem2::config{PMEM2_GRANULARITY_PAGE};
  auto pmem_map = pmem2::map{pmem_source, pmem_config};
  auto pmem_ops = pmem2::file_operations{pmem_map};

  // open posix file
  raii::file_descriptor fd{::open(parsed["file"].as<std::string>().c_str(),
                                  O_RDWR | O_CREAT | O_TRUNC, 0644)};

  ordered_json bench_result = {
      {"version", PEANUTS_VERSION},
      {"timestamp", fmt::format("{:%FT%TZ}", std::chrono::system_clock::now())},
      {"transfer_size", transfer_size},
      {"block_size", block_size},
  };

  auto rsg = utils::random_string_generator{seed};
  auto xfer_buffer = std::vector<std::byte>(transfer_size);

  // fill pmem with random data
  const auto fill_xfer_size = std::min(block_size, (1UL << 20));
  for (size_t ofs = 0; ofs < block_size; ofs += fill_xfer_size) {
    const auto random_data = rsg.generate(fill_xfer_size);
    pmem_ops.pwrite_nt(std::as_bytes(std::span{random_data}), ofs);
  }

  auto test_buffer = rsg.generate(block_size);

  // init io_uring
  struct io_uring ring;
  ::io_uring_queue_init(32, &ring, 0);
  // struct iovec iov = {pmem_map.address(), pmem_map.size()};
  // fmt::print("pmem_map.address(): {}\n", fmt::ptr(pmem_map.address()));
  // struct iovec iov = {test_buffer.data(), test_buffer.size()};
  // if(int ec = ::io_uring_register_buffers(&ring, &iov, 1); ec < 0) {
  //   fmt::print(stderr, "io_uring_register_buffers: {}\n", ec);
  //   throw std::system_error{-ec, std::system_category()};
  // }

  // copy pmem to posix file
  ::io_uring_cqe* cqe;
  for (size_t ofs = 0; ofs < block_size; ofs += transfer_size) {
    ::io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    ::io_uring_prep_write(sqe, fd.get(),
                          static_cast<std::byte*>(pmem_map.address()) + ofs,
                          transfer_size, ofs);
    // ::io_uring_prep_write_fixed(sqe, fd.get(), pmem_map.address() + ofs,
    //                             transfer_size, ofs, 0);
    // ::io_uring_prep_write_fixed(sqe, fd.get(), test_buffer.data() + ofs,
    //                             transfer_size, ofs, 0);
    // ::io_uring_sqe_set_data(sqe, );
    while (io_uring_submit(&ring) == 0)
      ;

    if (int ec = ::io_uring_wait_cqe(&ring, &cqe); ec < 0) {
      fmt::print(stderr, "io_uring_wait_cqe: {}\n", ec);
      throw std::system_error{-ec, std::system_category()};
    }
    if (cqe->res < 0) {
      int ec = -cqe->res;
      fmt::print(stderr, "cqe->res: {}\n", ec);
      ::io_uring_cqe_seen(&ring, cqe);
      throw std::system_error{ec, std::system_category()};
    }
    ::io_uring_cqe_seen(&ring, cqe);
  }

  ::io_uring_queue_exit(&ring);

  // auto wf_read = utils::welford{};
  // auto wf_write = utils::welford{};
  // auto sw = utils::stopwatch<double>{};

  // sw.reset();
  // if (!verify) {
  //   for (size_t ofs = 0; ofs < block_size; ofs += transfer_size) {
  //     rpm.file_ops().pwrite_nt(std::as_bytes(std::span{random_data_buffer}),
  //                              local_region_disp + ofs);
  //     wf_write.add(sw.lap_time().count());
  //   }
  // } else {
  //   for (size_t ofs = 0; ofs < block_size; ofs += transfer_size) {
  //     rpm.file_ops().pwrite_nt(std::as_bytes(std::span{random_data_buffer}),
  //                              local_region_disp + ofs);
  //     wf_write.add(sw.lap_time().count());
  //     random_data_buffer = rsg.generate(transfer_size);
  //   }
  // }
  // topo.comm().barrier();
  // auto write_elapsed_time = sw.get();

  // auto target_rank = topo.rank_pair2global_rank(
  //     (topo.inter_rank() + 1) % topo.inter_size(), 0);
  // // fmt::print("myrank: {}, target_rank: {}\n", topo.rank(), target_rank);
  // auto target_seed =
  //     seed ? decltype(seed)(*seed + target_rank + topo.intra_rank())
  //          : std::nullopt;
  // // fmt::print("myrank: {}, my_seed: {}, target_rank: {}, target_seed:
  // {}\n",
  // //            topo.rank(), my_seed.value_or(0), target_rank,
  // //            target_seed.value_or(0));

  // rsg = utils::random_string_generator{target_seed};
  // topo.comm().barrier();

  // // warm up
  // for (size_t ofs = 0; ofs < block_size; ofs += transfer_size) {
  //   rpm.win().get(xfer_buffer, target_rank,
  //                 static_cast<MPI_Aint>(local_region_disp + ofs));
  // }
  // rpm.win().flush(target_rank);

  // topo.comm().barrier();
  // sw.reset();
  // topo.comm().barrier();

  // // read
  // if (!verify) {
  //   for (size_t ofs = 0; ofs < block_size; ofs += transfer_size) {
  //     rpm.win().get(xfer_buffer, target_rank,
  //                   static_cast<MPI_Aint>(local_region_disp + ofs));
  //     rpm.win().flush(target_rank);
  //     wf_read.add(sw.lap_time().count());
  //   }
  // } else {
  //   for (size_t ofs = 0; ofs < block_size; ofs += transfer_size) {
  //     rpm.win().get(xfer_buffer, target_rank,
  //                   static_cast<MPI_Aint>(local_region_disp + ofs));
  //     rpm.win().flush(target_rank);
  //     auto neighbor_random_data_buffer = rsg.generate(transfer_size);
  //     if (!compare_vector_string(xfer_buffer, neighbor_random_data_buffer)) {
  //       fmt::print(stderr,
  //                  "verification error on rank "
  //                  "{} : read = {}, expected_read = {}\n ",
  //                  topo.rank(), to_string(xfer_buffer).substr(0, 8),
  //                  neighbor_random_data_buffer.substr(0, 8));
  //     }
  //     wf_read.add(sw.lap_time().count());
  //   }
  // }

  // topo.comm().barrier();
  // auto read_elapsed_time = sw.get();
  // topo.comm().barrier();

  mpi::run_on_rank0([&] {
    // bench_result["write"] = bench_stats{
    //     write_elapsed_time,
    //     topo.size() * block_size / transfer_size,
    //     topo.size() * block_size,
    // };
    // bench_result["read"] = bench_stats{
    //     read_elapsed_time,
    //     topo.size() * block_size / transfer_size,
    //     topo.size() * block_size,
    // };

    if (parsed.count("prettify") != 0U) {
      std::cout << std::setw(4);
    }
    std::cout << bench_result << std::endl;
  });
  return 0;
} catch (const peanuts::mpi::mpi_error& e) {
  fmt::print(stderr, "mpi_error: {}\n", e.what());
  return 1;
} catch (const peanuts::pmem2::pmem2_error& e) {
  fmt::print(stderr, "pmem2_error: {}\n", e.what());
  return 1;
} catch (const std::system_error& e) {
  fmt::print(stderr, "system_error: {}\n", e.what());
  return 1;
} catch (const std::exception& e) {
  fmt::print(stderr, "exception: {}\n", e.what());
  return 1;
} catch (...) {
  fmt::print(stderr, "unknown exception\n");
  return 1;
}
