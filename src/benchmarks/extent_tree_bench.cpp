#include <fmt/chrono.h>
#include <fmt/core.h>
#include <rpmbb/version.h>

#include <chrono>
#include <cxxopts.hpp>
#include <fstream>
#include <iostream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <thread>
#include <vector>

#include "rpmbb.hpp"
#include "rpmbb/inspector/std.hpp"

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
  static void to_json(ordered_json& j, const rpmbb::util::welford<uint64_t>& welford) {
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
      result.push_back(
          {rpmbb::util::welford<uint64_t>::from_range(welfords.begin(), welfords.end()),
           max_cycles});
    }
    ++window_id;
  }

  return result;
}

auto main(int argc, char const* argv[]) -> int {
  cxxopts::Options options("extent_tree_bench", "extent_tree benchmark");
  // clang-format off
  options.add_options()
    ("h,help", "Print usage")
    ("V,version", "Print version")
    ("prettify", "Prettify output")
    ("n,nthreads", "number of threads", cxxopts::value<size_t>()->default_value("1"))
    ("i,iter", "Number of iterations per thread", cxxopts::value<size_t>()->default_value("100"))
    ("a,add", "do tree.add()")
    ("f,find", "do tree.find()")
    ("r,remove", "do tree.remove()")
    ("e,extent_size", "extent size", cxxopts::value<size_t>()->default_value("4096"))
    ("s,stride", "stride", cxxopts::value<size_t>()->default_value("4096"))
    ("w,window", "OP/window in msec", cxxopts::value<uint64_t>()->default_value("1000"))
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

  const auto nthreads = parsed["nthreads"].as<size_t>();
  const auto iter = parsed["iter"].as<size_t>();
  const auto add = parsed.count("add") != 0U;
  const auto find = parsed.count("find") != 0U;
  const auto remove = parsed.count("remove") != 0U;
  const auto extent_size = parsed["extent_size"].as<size_t>();
  const auto stride = parsed["stride"].as<size_t>();
  const auto window = parsed["window"].as<uint64_t>();

  rpmbb::util::tsc::calibrate();

  const auto window_cycles = rpmbb::util::tsc::cycles_per_msec() * window;

  ordered_json bench_result = {
      {"version", RPMBB_VERSION},
      {"timestamp", fmt::format("{:%FT%TZ}", std::chrono::system_clock::now())},
      {"cycles_per_msec", rpmbb::util::tsc::cycles_per_msec()},
      {"nthreads", nthreads},
      {"iter", iter},
      {"extent_size", extent_size},
      {"stride", stride},
      {"window", window},
      {"add", {}},
      {"find", {}},
      {"remove", {}},
  };

  std::vector<std::thread> threads;
  std::vector<std::vector<bench_stats>> bench_stats_per_thread(nthreads);
  std::vector<size_t> tree_sizes(nthreads);
  rpmbb::util::sense_barrier barrier(nthreads);

  for (size_t tid = 0; tid < nthreads; ++tid) {
    threads.emplace_back([&, tid]() {
      using rpmbb::util::tsc;
      rpmbb::util::cpu_affinity_manager cpu_affinity(tid);

      rpmbb::extent_tree tree;

      if (add) {
        rpmbb::extent ex(0, extent_size);
        rpmbb::util::welford<uint64_t> wf;
        barrier.wait();

        time_window tw(window_cycles, wf);
        auto t0 = tw.get_window_start();

        for (size_t j = 0; j < iter; ++j) {
          tree.add(ex.begin, ex.end, ex.begin, tid);
          ex.begin += stride;
          ex.end += stride;
          tw.update_time();
          if (tw.should_reset_window()) {
            bench_stats_per_thread[tid].push_back({wf, tw.elapsed_cycles()});
            wf.clear();
          }
          tw.update_welford();
        }
        if (wf.n() != 0) {
          bench_stats_per_thread[tid].push_back({wf, tw.elapsed_cycles()});
        }

        tree_sizes[tid] = tree.size();
        barrier.wait();
        auto t1 = tsc::get();

        if (tid == 0) {
          auto stats = aggregate_status_in_same_window(bench_stats_per_thread);
          bench_stats_per_thread.clear();
          bench_stats_per_thread.resize(nthreads);

          std::vector<rpmbb::util::welford<uint64_t>> welfords;
          for (auto& st : stats) {
            welfords.push_back(st.wf);
          }
          // std::cout << rpmbb::util::make_inspector(welfords) << std::endl;
          auto combined_welford =
              rpmbb::util::welford<uint64_t>::from_range(welfords.begin(), welfords.end());

          bench_result["add"] = {
              // {"elapsed", elapsed_ms},
              // {"ops_per_sec", combined_welford.n() / elapsed_ms * 1000},
              {"total", bench_stats{combined_welford, t1 - t0}},
              {"per_window", stats},
              {"memory_usage_kb", memory_usage::get_current_rss_kb()},
              {"tree_size", tree_sizes},
          };
        }
      }

      if (find) {
        rpmbb::extent ex(0, extent_size);
        rpmbb::util::welford<uint64_t> wf;
        barrier.wait();

        time_window tw(window_cycles, wf);
        auto t0 = tw.get_window_start();

        for (size_t j = 0; j < iter; ++j) {
          tree.find(ex.begin, ex.end);
          ex.begin += stride;
          ex.end += stride;
          tw.update_time();
          if (tw.should_reset_window()) {
            bench_stats_per_thread[tid].push_back({wf, tw.elapsed_cycles()});
            wf.clear();
          }
          tw.update_welford();
        }
        if (wf.n() != 0) {
          bench_stats_per_thread[tid].push_back({wf, tw.elapsed_cycles()});
        }

        barrier.wait();
        auto t1 = tsc::get();

        if (tid == 0) {
          auto stats = aggregate_status_in_same_window(bench_stats_per_thread);
          bench_stats_per_thread.clear();
          bench_stats_per_thread.resize(nthreads);

          std::vector<rpmbb::util::welford<uint64_t>> welfords;
          for (auto& st : stats) {
            welfords.push_back(st.wf);
          }
          auto combined_welford =
              rpmbb::util::welford<uint64_t>::from_range(welfords.begin(), welfords.end());

          bench_result["find"] = {
              {"total", bench_stats{combined_welford, t1 - t0}},
              {"per_window", stats},
          };
        }
      }

      if (remove) {
        rpmbb::extent ex(0, extent_size);
        rpmbb::util::welford<uint64_t> wf;
        barrier.wait();

        time_window tw(window_cycles, wf);
        auto t0 = tw.get_window_start();

        for (size_t j = 0; j < iter; ++j) {
          tree.remove(ex.begin, ex.end);
          ex.begin += stride;
          ex.end += stride;
          tw.update_time();
          if (tw.should_reset_window()) {
            bench_stats_per_thread[tid].push_back({wf, tw.elapsed_cycles()});
            wf.clear();
          }
          tw.update_welford();
        }
        if (wf.n() != 0) {
          bench_stats_per_thread[tid].push_back({wf, tw.elapsed_cycles()});
        }

        barrier.wait();
        auto t1 = tsc::get();

        if (tid == 0) {
          auto stats = aggregate_status_in_same_window(bench_stats_per_thread);

          std::vector<rpmbb::util::welford<uint64_t>> welfords;
          for (auto& st : stats) {
            welfords.push_back(st.wf);
          }
          auto combined_welford =
              rpmbb::util::welford<uint64_t>::from_range(welfords.begin(), welfords.end());

          bench_result["remove"] = {
              {"total", bench_stats{combined_welford, t1 - t0}},
              {"per_window", stats},
          };
        }
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  if (parsed.count("prettify") != 0U) {
    std::cout << std::setw(4);
  }
  std::cout << bench_result << std::endl;

  return 0;
}
