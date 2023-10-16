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

constexpr size_t hugepage_size = 2 * 1024 * 1024;  // 2MiB

size_t align_down_to_hugepage(size_t size) {
  return (size / hugepage_size) * hugepage_size;
}

struct process_map_entry {
  int inter_rank;
  int intra_rank;

  bool operator<(const process_map_entry& other) const {
    return std::tie(inter_rank, intra_rank) <
           std::tie(other.inter_rank, other.intra_rank);
  }

  std::ostream& inspect(std::ostream& os) const {
    os << "{intra_rank:" << intra_rank << ",inter_rank:" << inter_rank << "}";
    return os;
  }
};

auto create_inverted_process_map(
    const std::vector<process_map_entry>& process_map)
    -> std::map<process_map_entry, int> {
  std::map<process_map_entry, int> inverted_map;
  for (const auto& [global_rank, entry] :
       rpmbb::utils::cenumerate(process_map)) {
    inverted_map[process_map[global_rank]] = global_rank;
  }
  return inverted_map;
}

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

  // utils::tsc::calibrate();

  const auto transfer_size = parsed["transfer"].as<size_t>();
  const auto block_size = parsed["block"].as<size_t>();

  assert(block_size % transfer_size == 0);

  const auto& comm = mpi::comm::world();
  auto intra_comm = mpi::comm{comm, mpi::split_type::shared};
  auto inter_comm = mpi::comm{comm, intra_comm.rank()};

  ordered_json bench_result = {
      {"version", RPMBB_VERSION},
      {"timestamp", fmt::format("{:%FT%TZ}", std::chrono::system_clock::now())},
      // {"cycles_per_msec", utils::tsc::cycles_per_msec()},
      {"transfer_size", transfer_size},
      {"block_size", block_size},
      {"np", comm.size()},
      {"ppn", intra_comm.size()},
      {"nnodes", inter_comm.size()},
  };

  std::vector<process_map_entry> process_map(comm.size());
  {
    mpi::dtype process_map_entry_dtype = mpi::dtype{mpi::to_dtype<int>(), 2};
    process_map_entry_dtype.commit();
    const auto entry = process_map_entry{inter_comm.rank(), intra_comm.rank()};
    comm.all_gather(entry, process_map_entry_dtype, std::span{process_map},
                    process_map_entry_dtype);
  }
  int shift_unit = -1;
  if (comm.rank() == 0) {
    // find first process that is not in the same node
    for (const auto& [idx, entry] : utils::cenumerate(process_map)) {
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

  auto inverted_process_map = create_inverted_process_map(process_map);

  // std::cout << utils::make_inspector(process_map) << std::endl;

  auto device = pmem2::device{parsed["path"].as<std::string>()};
  if (!device.is_devdax()) {
    device.truncate(block_size * intra_comm.size());
  }
  auto source = pmem2::source{device};
  auto config = pmem2::config{PMEM2_GRANULARITY_PAGE};
  auto map = pmem2::map{source, config};

  // auto disp = intra_comm.rank() * block_size;
  auto register_size = align_down_to_hugepage(map.size() / intra_comm.size());
  auto disp = intra_comm.rank() * register_size;
  // auto my_memory_block =
  //     std::unique_ptr<void, decltype(&mpi::free<void>)>{
  //         mpi::allocate(block_size), &mpi::free<void>};
  // auto my_block_span = std::span{
  //     reinterpret_cast<std::byte*>(my_memory_block.get()), block_size};
  // auto my_block_span = map.as_span().subspan(disp, block_size);
  auto my_block_span = map.as_span().subspan(disp, register_size);

  // bench_result["pmem_addr"] = fmt::format("{}",
  // fmt::ptr(my_block_span.data()));
  bench_result["pmem_size"] = map.size();
  bench_result["pmem_store_granularity"] = map.store_granularity();

  auto ops = pmem2::file_operations(map);

  auto sw_milli = utils::stopwatch<double, std::milli>{};
  mpi::win win;
  if (intra_comm.rank() == 0) {
    win = mpi::win{comm, map.as_span()};
  } else {
    win = mpi::win{comm, std::span<std::byte>{}};
  }
  mpi::run_on_rank0([&] {
    std::cout << "create win: "
              << utils::make_inspector(sw_milli.get_and_reset()) << std::endl;
  });
  // win.attach(my_block_span);
  auto adapter = mpi::win_lock_all_adapter{win, MPI_MODE_NOCHECK};
  auto lock = std::unique_lock{adapter};
  mpi::run_on_rank0([&] {
    std::cout << "win_lock_all: "
              << utils::make_inspector(sw_milli.get_and_reset()) << std::endl;
  });

  const auto random_data_buffer =
      utils::generate_random_alphanumeric_string(transfer_size);
  // const auto random_data_buffer =
  //     std::string(transfer_size, std::to_string(comm.rank())[0]);
  auto xfer_buffer = std::vector<std::byte>(transfer_size);

  // write
  // auto disp_aint = mpi::aint{my_block_span.data()};

  // std::vector<mpi::aint> disps(comm.size());
  // comm.all_gather(disp_aint, std::span{disps});

  auto wf_write = utils::welford{};
  auto wf_read = utils::welford{};

  comm.barrier();
  auto sw = utils::stopwatch<double>{};
  for (size_t ofs = 0; ofs < block_size; ofs += transfer_size) {
    ops.pwrite_nt(std::as_bytes(std::span{random_data_buffer}), disp + ofs);
    wf_write.add(sw.lap_time().count());
  }
  comm.barrier();
  auto write_elapsed_time_sec = sw.get().count();

  win.sync();
  comm.barrier();
  auto target_rank =
      inverted_process_map[{(inter_comm.rank() + 1) % inter_comm.size(), 0}];
  // auto target_rank = (comm.ran k() + shift_unit) % comm.size();
  auto target_disp = (intra_comm.rank() * register_size);
  // auto target_disp = disps[target_rank];
  // fmt::print("my_rank: {}, target_rank: {}, target_disp: {}\n", comm.rank(),
  //            target_rank, target_disp.native());

  sw_milli.reset();
  // warm up
  for (size_t ofs = 0; ofs < block_size; ofs += transfer_size) {
    win.get(xfer_buffer, target_rank, static_cast<MPI_Aint>(target_disp + ofs));
  }
  win.flush(target_rank);

  mpi::run_on_rank0([&] {
    std::cout << "warm up: " << utils::make_inspector(sw_milli.get_and_reset())
              << std::endl;
  });

  comm.barrier();
  sw.reset();
  comm.barrier();

  for (size_t ofs = 0; ofs < block_size; ofs += transfer_size) {
    // MPI_Get(&xfer_buffer[0], xfer_buffer.size(), MPI_BYTE, target_rank,
    //         target_disp + mpi::aint{ofs}, xfer_buffer.size(), MPI_BYTE, win);
    // MPI_Win_flush(target_rank, win);
    win.get(xfer_buffer, target_rank, static_cast<MPI_Aint>(target_disp + ofs));
    win.flush(target_rank);
    // wf_read.add(sw.lap_time().count());
  }
  // win.flush(target_rank);

  comm.barrier();
  auto read_elapsed_time = sw.get();
  // auto read_elapsed_time = sw.get();

  if (comm.rank() == 0) {
    auto total_nops = comm.size() * block_size / transfer_size;
    std::cout << utils::make_inspector(bench_stats{
                     read_elapsed_time,
                     total_nops,
                     comm.size() * block_size,
                 })
              << std::endl;
  }

  // verify xfer_buffer == neighbor's random_data_buffer
  auto neighbor_random_data_buffer = std::vector<std::byte>(transfer_size);
  comm.send_receive(random_data_buffer,
                    (comm.rank() + (comm.size() - shift_unit)) % comm.size(), 0,
                    neighbor_random_data_buffer);
  if (!std::equal(xfer_buffer.begin(), xfer_buffer.end(),
                  neighbor_random_data_buffer.begin(),
                  neighbor_random_data_buffer.end())) {
    fmt::print(stderr,
               "Error: xfer_buffer != neighbor_random_data_buffer on rank {}\n",
               comm.rank());
    fmt::print(stderr, "{}: write={}, read={}, expected_read={}\n", comm.rank(),
               random_data_buffer, to_string(xfer_buffer),
               to_string(neighbor_random_data_buffer));
  }

  comm.barrier();

  if (comm.rank() == 9999) {
    {
      auto total_num_of_ops = wf_write.n() * comm.size();
      auto ops_per_sec = total_num_of_ops / write_elapsed_time_sec;
      auto bytes_per_sec = ops_per_sec * transfer_size;
      bench_result["write"] = {
          {"elapsed_time_sec", write_elapsed_time_sec},
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
      //auto total_num_of_ops = wf_read.n() * comm.size();
      auto total_num_of_ops = (block_size / transfer_size) * comm.size();
      auto ops_per_sec = total_num_of_ops / read_elapsed_time.count();
      auto bytes_per_sec = ops_per_sec * transfer_size;
      bench_result["read"] = {
          {"elapsed_time_sec", read_elapsed_time.count()},
          {"ops_per_sec", ops_per_sec},
          {"bytes_per_sec", bytes_per_sec},
          {"gbytes_per_sec", bytes_per_sec / (1 << 30)},
          // {"nops_per_proc", wf_read.n()},
          // {"mean", wf_read.mean()},
          // {"var", wf_read.var()},
          // {"std", wf_read.std()},
      };
    }

    if (parsed.count("prettify") != 0U) {
      std::cout << std::setw(4);
    }
    std::cout << bench_result << std::endl;
  }

  sw_milli.reset();
  lock.unlock();
  mpi::run_on_rank0([&] {
    std::cout << "win_unlock_all: "
              << utils::make_inspector(sw_milli.get_and_reset()) << std::endl;
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
