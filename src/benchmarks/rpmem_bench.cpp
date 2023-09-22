#include <fmt/chrono.h>
#include <fmt/core.h>
#include <rpmbb/version.h>

#include <chrono>
#include <cxxopts.hpp>
#include <fstream>
#include <iostream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <span>
#include <thread>
#include <vector>

#include "rpmbb.hpp"
#include "rpmbb/inspector/std.hpp"

#include <libpmem2.h>
#include <mpi.h>

using ordered_json = nlohmann::ordered_json;

struct bench_stats {
  rpmbb::welford wf;
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
struct adl_serializer<rpmbb::welford> {
  static void to_json(ordered_json& j, const rpmbb::welford& welford) {
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

namespace rpmbb {
std::ostream& inspect(std::ostream& os, const welford& wf) {
  os << "{n:" << wf.n() << ',';
  os << "mean:" << wf.mean() << ',';
  os << "var:" << wf.var() << ',';
  os << "std:" << wf.std() << '}';
  return os;
}
}  // namespace rpmbb

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
  rpmbb::welford& wf_;

 public:
  time_window(uint64_t window_cycles, rpmbb::welford& wf)
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
    std::vector<rpmbb::welford> welfords;
    for (auto& stats_in_a_thread : bench_stats_per_thread) {
      if (window_id < stats_in_a_thread.size()) {
        welfords.push_back(stats_in_a_thread[window_id].wf);
        max_cycles =
            std::max(max_cycles, stats_in_a_thread[window_id].elapsed_cycles);
        remain = true;
      }
    }
    if (remain) {
      result.push_back(
          {rpmbb::welford::from_range(welfords.begin(), welfords.end()),
           max_cycles});
    }
    ++window_id;
  }

  return result;
}

auto main(int argc, char const* argv[]) -> int try {
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
    ("s,segment", "number of segments", cxxopts::value<size_t>()->default_value("1"))
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
  const auto nsegments = parsed["segment"].as<size_t>();

  ordered_json bench_result = {
      {"version", RPMBB_VERSION},
      {"timestamp", fmt::format("{:%FT%TZ}", std::chrono::system_clock::now())},
      {"cycles_per_msec", rpmbb::util::tsc::cycles_per_msec()},
      {"window", window},
      {"transfer_size", transfer_size},
      {"block_size", block_size},
      {"nsegments", nsegments},
  };

  auto device = rpmbb::pmem2::device{parsed["path"].as<std::string>()};
  if (!device.is_devdax()) {
    device.truncate(block_size * nsegments);
  }
  auto source = rpmbb::pmem2::source{device};
  auto config = rpmbb::pmem2::config{};
  config.set_required_store_granularity(PMEM2_GRANULARITY_PAGE);
  auto map = rpmbb::pmem2::map{source, config};

  auto* pmem_addr = static_cast<char*>(map.address());
  bench_result["pmem_addr"] = fmt::format("{}", fmt::ptr(pmem_addr));
  bench_result["pmem_size"] = map.size();
  bench_result["pmem_store_granularity"] = map.store_granularity();

  auto ops = rpmbb::pmem2::file_operations(map);

  ops.pwrite_nt(std::as_bytes(std::span("test")), 0);
  ops.pwrite_nt(std::as_bytes(std::span("hoge")), 4);
  char buf[10];
  ops.pread(std::as_writable_bytes(std::span{buf}), 2);
  buf[4] = '\0';
  if (std::string_view("stho") == buf) {
    fmt::print("ok\n");
  } else {
    fmt::print("ng\n");
  }

  if (parsed.count("prettify") != 0U) {
    std::cout << std::setw(4);
  }
  std::cout << bench_result << std::endl;

  return 0;
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
