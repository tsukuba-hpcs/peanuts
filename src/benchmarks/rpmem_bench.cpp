#include <fmt/chrono.h>
#include <fmt/core.h>
#include <rpmbb/version.h>

#include <chrono>
#include <cxxopts.hpp>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <ostream>
#include <span>
#include <thread>
#include <vector>
#include "rpmbb.hpp"
#include "rpmbb/inspector/std_chrono.hpp"
#include "rpmbb/inspector/std_containers.hpp"

#include <libpmem2.h>
#include <mpi.h>

using ordered_json = nlohmann::ordered_json;

struct bench_stats {
  rpmbb::utils::stopwatch<>::duration elapsed_time;
  size_t total_num_ops;
  size_t total_transfer_size;

  std::ostream& inspect(std::ostream& os) const {
    os << rpmbb::utils::make_inspector(elapsed_time) << ", ";
    os << rpmbb::utils::to_human<1000>(total_num_ops / elapsed_time.count())
       << "ops/sec, ";
    os << rpmbb::utils::to_human(total_transfer_size / elapsed_time.count())
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
    j["ops_human"] = rpmbb::utils::to_human<1000>(stats.total_num_ops /
                                                  stats.elapsed_time.count()) +
                     "ops/sec";
    j["bw"] = stats.total_transfer_size / stats.elapsed_time.count();
    j["bw_human"] = rpmbb::utils::to_human<1000>(stats.total_transfer_size /
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
struct adl_serializer<rpmbb::utils::welford> {
  static void to_json(ordered_json& j, const rpmbb::utils::welford& welford) {
    j["n"] = welford.n();
    // j["mean"] = rpmbb::utils::tsc::to_nsec(welford.mean());
    // j["var"] = rpmbb::utils::tsc::to_nsec(welford.var());
    // j["std"] = rpmbb::utils::tsc::to_nsec(welford.std());
    j["mean"] = welford.mean();
    j["var"] = welford.var();
    j["std"] = welford.std();
  }
};

}  // namespace nlohmann

namespace rpmbb::utils {
std::ostream& inspect(std::ostream& os, const welford& wf) {
  os << "{n:" << wf.n() << ',';
  os << "mean:" << wf.mean() << ',';
  os << "var:" << wf.var() << ',';
  os << "std:" << wf.std() << '}';
  return os;
}
}  // namespace rpmbb::utils

class memory_usage {
 public:
  static int get_current_rss_kb() {
    std::ifstream status_file("/proc/self/status");
    std::string line;

    while (std::getline(status_file, line)) {
      if (line.find("VmRSS:") != std::string::npos) {
        std::istringstream iss(line);
        std::string key;
        int value;
        iss >> key >> value;
        return value;
      }
    }

    return -1;
  }
};

auto to_string(std::span<const std::byte> span) -> std::string {
  std::string str;
  str.reserve(span.size());
  std::transform(span.begin(), span.end(), std::back_inserter(str),
                 [](std::byte b) { return static_cast<char>(b); });
  return str;
}

auto main(int argc, char* argv[]) -> int try {
  using namespace rpmbb;
  mpi::env env(&argc, &argv);
  cxxopts::Options options("rpmem_bench", "MPI_Get + lipmem2 benchmark");
  // clang-format off
  options.add_options()
    ("h,help", "Print usage")
    ("V,version", "Print version")
    ("prettify", "Prettify output")
    ("p,path", "path to pmem device", cxxopts::value<std::string>()->default_value("/dev/dax0.0"))
    ("t,transfer", "transfer size", cxxopts::value<size_t>()->default_value("4096"))
    ("b,block", "block size - contiguous bytes to write per task", cxxopts::value<size_t>()->default_value("2097152"))
  ;
  // clang-format on

  auto parsed = options.parse(argc, argv);
  if (parsed.count("help") != 0U) {
    fmt::print("{}\n", options.help());
    return 0;
  }

  if (parsed.count("version") != 0U) {
    fmt::print("{}\n", RPMBB_VERSION);
    return 0;
  }

  // utils::tsc::calibrate();

  const auto transfer_size = parsed["transfer"].as<size_t>();
  const auto block_size = parsed["block"].as<size_t>();

  assert(block_size % transfer_size == 0);
  assert(block_size >= rpm::pmem_alignment);

  rpmbb::topology topo{};
  rpmbb::rpm rpm{&topo, parsed["path"].as<std::string>(),
                 utils::round_up_pow2(block_size * topo.intra_size(),
                                      rpm::pmem_alignment)};

  ordered_json bench_result = {
      {"version", RPMBB_VERSION},
      {"timestamp", fmt::format("{:%FT%TZ}", std::chrono::system_clock::now())},
      {"transfer_size", transfer_size},
      {"block_size", block_size},
      {"np", topo.size()},
      {"ppn", topo.intra_size()},
      {"nnodes", topo.inter_size()},
      {"pmem_size", rpm.local_size()},
      {"pmem_store_granularity", rpm.get_store_granularity()},
  };

  int shift_unit = -1;
  mpi::run_on_rank0([&] {
    // find first process that is not in the same node
    shift_unit = topo.rank_pair2global_rank(
        (topo.inter_rank() + 1) % topo.inter_size(), 0);
    if (shift_unit == 0) {
      fmt::print(stderr, "Warning: All processes are on the same node.\n");
      shift_unit = 1;
    }
  });

  topo.comm().broadcast(shift_unit);
  // fmt::print("shft_unit: {}, my_rank: {}\n", shift_unit, comm.rank());

  auto local_region_disp = rpm.local_region_disp(topo.intra_rank());
  // fmt::print("myrank: {}, local_region_disp: {}\n", topo.rank(),
  //            utils::to_human(local_region_disp));

  // const auto random_data_buffer =
  //     utils::generate_random_alphanumeric_string(transfer_size);
  const auto random_data_buffer =
      std::string(transfer_size, std::to_string(topo.rank())[0]);
  auto xfer_buffer = std::vector<std::byte>(transfer_size);

  auto wf_read = utils::welford{};
  auto wf_write = utils::welford{};
  auto sw = utils::stopwatch<double>{};

  topo.comm().barrier();
  sw.reset();
  topo.comm().barrier();
  for (size_t ofs = 0; ofs < block_size; ofs += transfer_size) {
    rpm.file_ops().pwrite_nt(std::as_bytes(std::span{random_data_buffer}),
                             local_region_disp + ofs);
    wf_write.add(sw.lap_time().count());
  }
  topo.comm().barrier();
  auto write_elapsed_time = sw.get();

  auto target_rank = topo.rank_pair2global_rank(
      (topo.inter_rank() + 1) % topo.inter_size(), 0);
  // fmt::print("myrank: {}, target_rank: {}\n", topo.rank(), target_rank);
  topo.comm().barrier();

  // warm up
  for (size_t ofs = 0; ofs < block_size; ofs += transfer_size) {
    rpm.win().get(xfer_buffer, target_rank,
                  static_cast<MPI_Aint>(local_region_disp + ofs));
  }
  rpm.win().flush(target_rank);

  topo.comm().barrier();
  sw.reset();
  topo.comm().barrier();

  // read
  for (size_t ofs = 0; ofs < block_size; ofs += transfer_size) {
    rpm.win().get(xfer_buffer, target_rank,
                  static_cast<MPI_Aint>(local_region_disp + ofs));
    rpm.win().flush(target_rank);
    wf_read.add(sw.lap_time().count());
  }

  topo.comm().barrier();
  auto read_elapsed_time = sw.get();

  // verify xfer_buffer == neighbor's random_data_buffer
  auto neighbor_random_data_buffer = std::vector<std::byte>(transfer_size);
  topo.comm().send_receive(
      random_data_buffer,
      (topo.rank() + (topo.size() - shift_unit)) % topo.size(), 0,
      neighbor_random_data_buffer);
  if (!std::equal(xfer_buffer.begin(), xfer_buffer.end(),
                  neighbor_random_data_buffer.begin(),
                  neighbor_random_data_buffer.end())) {
    fmt::print(stderr,
               "Error: xfer_buffer != neighbor_random_data_buffer on rank {} : "
               "write = {}, read = {}, expected_read = {}\n ",
               topo.rank(), random_data_buffer.substr(0, 8),
               to_string(xfer_buffer).substr(0, 8),
               to_string(neighbor_random_data_buffer).substr(0, 8));
  }

  topo.comm().barrier();

  mpi::run_on_rank0([&] {
    bench_result["write"] = bench_stats{
        write_elapsed_time,
        topo.size() * block_size / transfer_size,
        topo.size() * block_size,
    };
    bench_result["read"] = bench_stats{
        read_elapsed_time,
        topo.size() * block_size / transfer_size,
        topo.size() * block_size,
    };

    if (parsed.count("prettify") != 0U) {
      std::cout << std::setw(4);
    }
    std::cout << bench_result << std::endl;
  });
  return 0;
} catch (const rpmbb::mpi::mpi_error& e) {
  fmt::print(stderr, "mpi_error: {}\n", e.what());
  return 1;
} catch (const rpmbb::pmem2::pmem2_error& e) {
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
