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
#include <optional>
#include <ostream>
#include <random>
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
  using namespace rpmbb;
  mpi::runtime runtime(&argc, &argv);
  cxxopts::Options options("rpmem_bench", "MPI_Get + lipmem2 benchmark");
  // clang-format off
  options.add_options()
    ("h,help", "Print usage")
    ("V,version", "Print version")
    ("prettify", "Prettify output")
    ("p,path", "path to pmem device", cxxopts::value<std::string>()->default_value("/dev/dax0.0"))
    ("t,transfer", "transfer size", cxxopts::value<size_t>()->default_value("4096"))
    ("b,block", "block size - contiguous bytes to write per task", cxxopts::value<size_t>()->default_value("2097152"))
    ("verify", "verify mode. need to set seed.", cxxopts::value<std::seed_seq::result_type>()->implicit_value("42"))
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
  const auto verify = parsed.count("verify") != 0U;
  const auto seed = parsed.count("verify") != 0U
                        ? std::optional<std::seed_seq::result_type>(
                              parsed["verify"].as<std::seed_seq::result_type>())
                        : std::nullopt;

  assert(block_size % transfer_size == 0);
  // assert(block_size >= rpm::pmem_alignment);

  rpmbb::topology topo{};
  rpmbb::rpm rpm{topo, parsed["path"].as<std::string>(),
                 utils::round_up_pow2(block_size, rpm::pmem_alignment) *
                     topo.intra_size()};
  // std::cout << utils::make_inspector(rpm) << std::endl;
  rpmbb::rpm_local_block rpm_block{rpm};

  ordered_json bench_result = {
      {"version", RPMBB_VERSION},
      {"timestamp", fmt::format("{:%FT%TZ}", std::chrono::system_clock::now())},
      {"transfer_size", transfer_size},
      {"block_size", block_size},
      {"np", topo.size()},
      {"ppn", topo.intra_size()},
      {"nnodes", topo.inter_size()},
      {"pmem_size", rpm.size()},
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

  auto my_seed = seed ? decltype(seed)(*seed + topo.rank()) : std::nullopt;
  auto rsg = utils::random_string_generator{my_seed};
  auto random_data_buffer = rsg.generate(transfer_size);
  // const auto random_data_buffer =
  //     std::string(transfer_size, std::to_string(topo.rank())[0]);
  auto xfer_buffer = std::vector<std::byte>(transfer_size);

  auto wf_read = utils::welford{};
  auto wf_write = utils::welford{};
  auto sw = utils::stopwatch<double>{};

  topo.comm().barrier();
  sw.reset();
  topo.comm().barrier();
  if (!verify) {
    for (size_t ofs = 0; ofs < block_size; ofs += transfer_size) {
      rpm_block.pwrite_nt(std::as_bytes(std::span{random_data_buffer}), ofs);
      wf_write.add(sw.lap_time().count());
    }
  } else {
    for (size_t ofs = 0; ofs < block_size; ofs += transfer_size) {
      rpm_block.pwrite_nt(std::as_bytes(std::span{random_data_buffer}), ofs);
      wf_write.add(sw.lap_time().count());
      random_data_buffer = rsg.generate(transfer_size);
    }
  }
  topo.comm().barrier();
  auto write_elapsed_time = sw.get();

  auto target_rank = (topo.rank() + shift_unit) % topo.size();
  auto rpm_blocks = rpmbb::rpm_blocks{rpm};
  auto remote_block = rpmbb::rpm_remote_block{rpm_blocks, target_rank};
  auto xfer_buffer_span = std::span{xfer_buffer};
  auto target_seed = seed ? decltype(seed)(*seed + target_rank) : std::nullopt;
  // fmt::print("myrank: {}, my_seed: {}, target_rank: {}, target_seed: {}\n",
  //            topo.rank(), my_seed.value_or(0), target_rank,
  //            target_seed.value_or(0));

  rsg = utils::random_string_generator{target_seed};
  topo.comm().barrier();

  // warm up
  for (size_t ofs = 0; ofs < block_size; ofs += transfer_size) {
    remote_block.pread_noflush(xfer_buffer_span, ofs);
  }
  remote_block.flush();

  topo.comm().barrier();
  sw.reset();
  topo.comm().barrier();

  // read
  if (!verify) {
    for (size_t ofs = 0; ofs < block_size; ofs += transfer_size) {
      remote_block.pread(xfer_buffer_span, ofs);
      wf_read.add(sw.lap_time().count());
    }
  } else {
    for (size_t ofs = 0; ofs < block_size; ofs += transfer_size) {
      remote_block.pread(xfer_buffer_span, ofs);
      auto target_random_data_buffer = rsg.generate(transfer_size);
      if (!compare_vector_string(xfer_buffer, target_random_data_buffer)) {
        fmt::print(stderr,
                   "verification error on rank "
                   "{} : read = {}, expected_read = {}\n ",
                   topo.rank(), to_string(xfer_buffer).substr(0, 8),
                   target_random_data_buffer.substr(0, 8));
      }
      wf_read.add(sw.lap_time().count());
    }
  }

  topo.comm().barrier();
  auto read_elapsed_time = sw.get();
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
