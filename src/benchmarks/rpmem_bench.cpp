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
#include "rpmbb/inspector/std.hpp"

#include <libpmem2.h>
#include <mpi.h>

using ordered_json = nlohmann::ordered_json;

struct bench_stats {
  rpmbb::util::welford<uint64_t> wf;
  uint64_t elapsed_cycles;
};

namespace nlohmann {
template <typename Rep, typename Period>
struct adl_serializer<std::chrono::duration<Rep, Period>> {
  static void to_json(ordered_json& j,
                      const std::chrono::duration<Rep, Period>& duration) {
    j = duration.count();
  }
};

template <>
struct adl_serializer<rpmbb::util::welford<uint64_t>> {
  static void to_json(ordered_json& j,
                      const rpmbb::util::welford<uint64_t>& welford) {
    j["n"] = welford.n();
    // j["mean"] = rpmbb::util::tsc::to_nsec(welford.mean());
    // j["var"] = rpmbb::util::tsc::to_nsec(welford.var());
    // j["std"] = rpmbb::util::tsc::to_nsec(welford.std());
    j["mean"] = welford.mean();
    j["var"] = welford.var();
    j["std"] = welford.std();
  }
};

template <>
struct adl_serializer<bench_stats> {
  static void to_json(ordered_json& j, const bench_stats& stats) {
    j["elapsed_cycles"] = stats.elapsed_cycles;
    j["ops_per_sec"] =
        stats.wf.n() / rpmbb::util::tsc::to_msec(stats.elapsed_cycles) * 1000;
    j["n"] = stats.wf.n();
    j["mean"] = stats.wf.mean();
    j["var"] = stats.wf.var();
    j["std"] = stats.wf.std();
  }
};

}  // namespace nlohmann

namespace rpmbb::util {
std::ostream& inspect(std::ostream& os, const welford<uint64_t>& wf) {
  os << "{n:" << wf.n() << ',';
  os << "mean:" << wf.mean() << ',';
  os << "var:" << wf.var() << ',';
  os << "std:" << wf.std() << '}';
  return os;
}
}  // namespace rpmbb::util

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

class time_window {
 private:
  uint64_t t_window_start_;
  uint64_t t_prev_;
  uint64_t t_now_;
  uint64_t window_cycles_;
  rpmbb::util::welford<uint64_t>& wf_;

 public:
  time_window(uint64_t window_cycles, rpmbb::util::welford<uint64_t>& wf)
      : t_window_start_(rpmbb::util::tsc::get()),
        t_prev_(t_window_start_),
        t_now_(t_window_start_),
        window_cycles_(window_cycles),
        wf_(wf) {}

  void update_time() { t_now_ = rpmbb::util::tsc::get(); }

  auto elapsed_cycles() const -> uint64_t { return t_now_ - t_window_start_; }

  bool should_reset_window() {
    if (elapsed_cycles() > window_cycles_) {
      t_window_start_ = t_now_;
      return true;
    }
    return false;
  }

  void update_welford() {
    wf_.add(t_now_ - t_prev_);
    t_prev_ = t_now_;
  }

  auto get_window_start() const -> uint64_t { return t_window_start_; }
};

std::vector<bench_stats> aggregate_status_in_same_window(
    std::vector<std::vector<bench_stats>>& bench_stats_per_thread) {
  std::vector<bench_stats> result;

  // bench_status_per_thread[thread_id][window_id]
  size_t window_id = 0;
  bool remain = true;
  while (remain) {
    uint64_t max_cycles = 0;
    remain = false;
    std::vector<rpmbb::util::welford<uint64_t>> welfords;
    for (auto& stats_in_a_thread : bench_stats_per_thread) {
      if (window_id < stats_in_a_thread.size()) {
        welfords.push_back(stats_in_a_thread[window_id].wf);
        max_cycles =
            std::max(max_cycles, stats_in_a_thread[window_id].elapsed_cycles);
        remain = true;
      }
    }
    if (remain) {
      result.push_back({rpmbb::util::welford<uint64_t>::from_range(
                            welfords.begin(), welfords.end()),
                        max_cycles});
    }
    ++window_id;
  }

  return result;
}

auto main(int argc, char* argv[]) -> int try {
  using namespace rpmbb;
  rpmbb::mpi::env env(&argc, &argv);
  cxxopts::Options options("rpmem_bench",
                           "MPI_Fetch_and_op + lipmem2 benchmark");
  // clang-format off
  options.add_options()
    ("h,help", "Print usage")
    ("V,version", "Print version")
    ("prettify", "Prettify output")
    ("w,window", "OP/window in msec", cxxopts::value<uint64_t>()->default_value("1000"))
    ("p,path", "path to pmem device", cxxopts::value<std::string>()->default_value("/dev/dax0.0"))
    ("t,transfer", "transfer size", cxxopts::value<size_t>()->default_value("256"))
    ("b,block", "block size - contiguous bytes to write per task", cxxopts::value<size_t>()->default_value("1073741824"))
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

  rpmbb::util::tsc::calibrate();

  const auto window = parsed["window"].as<uint64_t>();
  // const auto window_cycles = rpmbb::util::tsc::cycles_per_msec() * window;
  const auto transfer_size = parsed["transfer"].as<size_t>();
  const auto block_size = parsed["block"].as<size_t>();

  ordered_json bench_result = {
      {"version", RPMBB_VERSION},
      {"timestamp", fmt::format("{:%FT%TZ}", std::chrono::system_clock::now())},
      {"cycles_per_msec", rpmbb::util::tsc::cycles_per_msec()},
      {"window", window},
      {"transfer_size", transfer_size},
      {"block_size", block_size},
  };

  assert(block_size % transfer_size == 0);

  const auto& comm = rpmbb::mpi::comm::world();
  auto intra_comm = mpi::comm{comm, mpi::split_type::shared};
  auto inter_comm = mpi::comm{comm, intra_comm.rank()};

  struct process_map_entry {
    int intra_rank;
    int inter_rank;
    std::ostream& inspect(std::ostream& os) const {
      os << "{intra_rank:" << intra_rank << ",inter_rank:" << inter_rank << "}";
      return os;
    }
  };
  std::vector<process_map_entry> process_map(comm.size());
  {
    mpi::dtype process_map_entry_dtype = mpi::dtype{mpi::to_dtype<int>(), 2};
    process_map_entry_dtype.commit();
    const auto entry = process_map_entry{intra_comm.rank(), inter_comm.rank()};
    comm.all_gather(entry, process_map_entry_dtype, std::span{process_map},
                    process_map_entry_dtype);
  }
  int shift_unit = -1;
  if (comm.rank() == 0) {
    // find first process that is not in the same node
    for (const auto& [idx, entry] : util::cenumerate(process_map)) {
      if (entry.inter_rank != 0) {
        shift_unit = idx;
        break;
      }
    }
    if (shift_unit < 0) {
      fmt::print(stderr, "Warning: All processes are on the same node.\n");
      shift_unit = 1;
    }
  }
  comm.broadcast(shift_unit);
  // fmt::print("shft_unit: {}, my_rank: {}\n", shift_unit, comm.rank());

  // std::cout << rpmbb::util::make_inspector(process_map) << std::endl;

  auto device = rpmbb::pmem2::device{parsed["path"].as<std::string>()};
  if (!device.is_devdax()) {
    device.truncate(block_size * intra_comm.size());
  }
  auto source = rpmbb::pmem2::source{device};
  auto config = rpmbb::pmem2::config{PMEM2_GRANULARITY_PAGE};
  auto map = rpmbb::pmem2::map{source, config};

  auto disp = intra_comm.rank() * block_size;
  auto my_block_span = map.as_span().subspan(disp, block_size);

  // bench_result["pmem_addr"] = fmt::format("{}",
  // fmt::ptr(my_block_span.data()));
  bench_result["pmem_size"] = map.size();
  bench_result["pmem_store_granularity"] = map.store_granularity();

  auto ops = rpmbb::pmem2::file_operations(map);

  auto win = mpi::win{comm};
  win.attach(my_block_span);
  auto adapter = mpi::win_lock_all_adapter{win, MPI_MODE_NOCHECK};
  auto lock = std::unique_lock{adapter};

  // const auto random_data_buffer =
  //     util::generate_random_alphanumeric_string(transfer_size);
  const auto random_data_buffer =
      std::string(transfer_size, std::to_string(comm.rank())[0]);
  auto xfer_buffer = std::vector<std::byte>(transfer_size);

  // write
  auto disp_aint = mpi::aint{my_block_span.data()};

  std::vector<mpi::aint> disps(comm.size());
  comm.all_gather(disp_aint, std::span{disps});

  auto wf_write = rpmbb::util::welford<double>{};
  auto wf_read = rpmbb::util::welford<double>{};
  auto sw = rpmbb::util::stopwatch<double, std::ratio<1, 1>>{};
  for (size_t ofs = 0; ofs < block_size; ofs += transfer_size) {
    ops.pwrite_nt(std::as_bytes(std::span{random_data_buffer}), disp + ofs);
    wf_write.add(sw.get_and_reset().count());
  }

  win.sync();
  comm.barrier();
  win.sync();

  // read neighbor node's data
  auto target_rank = (comm.rank() + shift_unit) % comm.size();
  auto target_disp = disps[target_rank];
  // fmt::print("my_rank: {}, target_rank: {}, target_disp: {}\n", comm.rank(),
  //            target_rank, target_disp.native());

  sw.reset();
  for (size_t ofs = 0; ofs < block_size; ofs += transfer_size) {
    win.get(xfer_buffer, target_rank,
            target_disp + mpi::aint{static_cast<MPI_Aint>(ofs)});
    win.flush(target_rank);
    wf_read.add(sw.get_and_reset().count());
  }

  // verify xfer_buffer == neighbor's random_data_buffer
  auto neighbor_random_data_buffer = std::vector<std::byte>(transfer_size);
  comm.send_receive(random_data_buffer,
                    (comm.rank() + (comm.size() - shift_unit)) % comm.size(), 0,
                    neighbor_random_data_buffer, target_rank);
  if (!std::equal(xfer_buffer.begin(), xfer_buffer.end(),
                  neighbor_random_data_buffer.begin(),
                  neighbor_random_data_buffer.end())) {
    fmt::print(stderr,
               "Error: xfer_buffer != neighbor_random_data_buffer on rank {}\n",
               comm.rank());
    // if (comm.rank() == 0) {
    //   std::string str;
    //   str.reserve(xfer_buffer.size());
    //   std::transform(xfer_buffer.begin(), xfer_buffer.end(),
    //                  std::back_inserter(str),
    //                  [](std::byte b) { return static_cast<char>(b); });

    //   fmt::print(stderr, "xfer_buffer: {}\n", str);
    //   str.clear();
    //   std::transform(neighbor_random_data_buffer.begin(),
    //                  neighbor_random_data_buffer.end(),
    //                  std::back_inserter(str),
    //                  [](std::byte b) { return static_cast<char>(b); });

    //   fmt::print(stderr, "neighbor_random_data_buffer: {}\n", str);
    //   fmt::print(stderr, "my_random_data_buffer: {}\n", random_data_buffer);
    // }
    return 1;
  }

  if (comm.rank() == 0) {
    auto func_summary = [](size_t np, size_t n, double mean,
                           size_t transfer_size) {
      auto total_nops = n * np;
      auto elapsed_time_sec = mean * n;
      auto ops_per_sec = total_nops / elapsed_time_sec;
      auto bytes_per_sec = total_nops * transfer_size / elapsed_time_sec;
      return std::tie(total_nops, elapsed_time_sec, ops_per_sec, bytes_per_sec);
    };
    {
      auto [total_nops, elapsed_time_sec, ops_per_sec, bytes_per_sec] =
          func_summary(comm.size(), wf_write.n(), wf_write.mean(),
                       transfer_size);
      bench_result["write"] = {
          {"elapsed_time_sec", elapsed_time_sec},
          {"ops_per_sec", ops_per_sec},
          {"bytes_per_sec", bytes_per_sec},
          {"gbytes_per_sec", bytes_per_sec / (1 << 30)},
          {"nops_per_proc", wf_write.n()},
          {"mean", wf_write.mean()},
          {"var", wf_write.var()},
          {"std", wf_write.std()},
      };
    }

    {
      auto [total_nops, elapsed_time_sec, ops_per_sec, bytes_per_sec] =
          func_summary(comm.size(), wf_read.n(), wf_read.mean(), transfer_size);
      bench_result["read"] = {
          {"elapsed_time_sec", elapsed_time_sec},
          {"ops_per_sec", ops_per_sec},
          {"bytes_per_sec", bytes_per_sec},
          {"gbytes_per_sec", bytes_per_sec / (1 << 30)},
          {"nops_per_proc", wf_read.n()},
          {"mean", wf_read.mean()},
          {"var", wf_read.var()},
          {"std", wf_read.std()},
      };
    }

    if (parsed.count("prettify") != 0U) {
      std::cout << std::setw(4);
    }
    std::cout << bench_result << std::endl;
  }

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
