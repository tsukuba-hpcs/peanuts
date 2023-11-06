#include "rpmbb.hpp"
#include "rpmbb/inspector/std_chrono.hpp"
#include "rpmbb/inspector/std_containers.hpp"

#include <fmt/chrono.h>
#include <fmt/core.h>
#include <zpp_bits.h>
#include <cxxopts.hpp>
#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <span>

using ordered_json = nlohmann::ordered_json;

using namespace rpmbb;

struct bench_stats {
  utils::stopwatch<>::duration elapsed_time{1};
  size_t total_num_ops{};
  size_t total_transfer_size{};
};

struct bench_params {
  std::string pmem;
  std::string output_dir;
  size_t xfer_size;
  size_t block_size;
  size_t segment_count;
  size_t io_size_per_proc;
  std::optional<std::seed_seq::result_type> verify_seed;
  bool verbose = false;

  bench_params(const cxxopts::ParseResult& parsed)
      : pmem(parsed["pmem"].as<std::string>()),
        output_dir(parsed["outdir"].as<std::string>()),
        xfer_size(
            utils::from_human<size_t>(parsed["transfer"].as<std::string>())),
        block_size(
            utils::from_human<size_t>(parsed["block"].as<std::string>())),
        segment_count(parsed["segment"].as<size_t>()),
        io_size_per_proc(block_size * segment_count),
        verify_seed(parsed.count("verify") != 0U
                        ? std::optional<std::seed_seq::result_type>(
                              parsed["verify"].as<std::seed_seq::result_type>())
                        : std::nullopt),
        verbose(parsed.count("verbose") != 0U)

  {
    if (block_size % xfer_size != 0) {
      throw std::invalid_argument(
          "block_size must be a multiple of transfer_size");
    }
  }

  size_t io_count_per_proc() const { return io_size_per_proc / xfer_size; }
};

namespace nlohmann {

template <>
struct adl_serializer<bench_stats> {
  static void to_json(ordered_json& j, const bench_stats& stats) {
    auto t = stats.elapsed_time.count();
    j.merge_patch({
        {"elapsed_time", t},
        {"ops", stats.total_num_ops / t},
        {"ops_human", utils::to_human<1000>(stats.total_num_ops / t) + "ops/s"},
        {"bw", stats.total_transfer_size / t},
        {"bw_human",
         utils::to_human<1000>(stats.total_transfer_size / t) + "B/s"},
    });
  }
};

template <>
struct adl_serializer<bench_params> {
  static void to_json(ordered_json& j, const bench_params& obj) {
    j.merge_patch({
        {"pmem", obj.pmem},
        {"output_dir", obj.output_dir},
        {"transfer_size", obj.xfer_size},
        {"transfer_size_human", utils::to_human(obj.xfer_size)},
        {"block_size", obj.block_size},
        {"block_size_human", utils::to_human(obj.block_size)},
        {"segment_count", obj.segment_count},
        {"segment_count_human", utils::to_human(obj.segment_count)},
        {"io_size_per_proc", obj.io_size_per_proc},
        {"io_size_per_proc_human", utils::to_human(obj.io_size_per_proc)},
    });
    if (obj.verify_seed) {
      j["verify_seed"] = obj.verify_seed.value();
    } else {
      j["verify_seed"] = nullptr;
    }
  }
};

}  // namespace nlohmann

std::ostream& inspect(std::ostream& os, const bench_params& obj) {
  auto j = ordered_json(obj);
  j.erase("transfer_size");
  j.erase("block_size");
  j.erase("segment_count");
  j.erase("io_size_per_proc");
  return os << j;
}
std::ostream& inspect(std::ostream& os, const bench_stats& obj) {
  auto j = ordered_json(obj);
  j.erase("ops");
  j.erase("bw");
  return os << j;
}

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
  mpi::env env(&argc, &argv);
  cxxopts::Options options("ior_mimic_bench", "IOR-like benchmark");
  // clang-format off
  options.add_options()
    ("h,help", "Print usage")
    ("V,version", "Print version")
    ("verbose", "output verbose information to stderr")
    ("prettify", "Prettify output")
    ("o,outdir", "output directory", cxxopts::value<std::string>()->default_value("."))
    ("pmem", "path to pmem device", cxxopts::value<std::string>()->default_value("/dev/dax0.0"))
    ("t,transfer", "transfer size", cxxopts::value<std::string>()->default_value("4K"))
    ("b,block", "block size - contiguous bytes to write per task", cxxopts::value<std::string>()->default_value("2M"))
    ("s,segment", "segment count", cxxopts::value<size_t>()->default_value("1"))
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

  auto params = bench_params{parsed};

  if (params.verbose) {
    mpi::run_on_rank0([&] {
      std::cerr << std::setw(2) << utils::make_inspector(params) << std::endl;
    });
  }

  auto topo = rpmbb::topology{};
  auto rpm = rpmbb::rpm{topo, params.pmem,
                        params.io_size_per_proc * topo.intra_size()};
  auto ring = rpmbb::ring_buffer{rpm};
  auto store = rpmbb::bb_store{rpm};

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

  auto sw = utils::stopwatch<double>();
  auto result_json = ordered_json{};

  // N-1 file path
  auto test_file_path = params.output_dir + "/test_file";

  sw.reset();
  topo.comm().barrier();

  // FIXME: should not open file by each process
  auto fd = raii::file_descriptor{
      ::open(test_file_path.c_str(), O_RDWR | O_CREAT, 0644)};
  // auto ino = utils::get_ino(fd.get());

  // fmt::print("{}: ino: {}\n", topo.rank(), ino);

  auto handler = store.open(fd.get());

  topo.comm().barrier();
  result_json["time_open"] = sw.get().count();

  // writing
  auto my_seed = params.verify_seed.value_or(0) + topo.rank();
  auto rsg = utils::random_string_generator{my_seed};
  auto random_data_buffer = rsg.generate(params.xfer_size);
  auto xfer_buffer_span = std::as_writable_bytes(std::span{random_data_buffer});

  auto iter_count = params.block_size / params.xfer_size;
  auto segment_size = params.block_size * topo.size();
  topo.comm().barrier();
  sw.reset();
  topo.comm().barrier();
  for (size_t s = 0; s < params.segment_count; ++s) {
    auto segment_ofs = segment_size * s;
    auto block_ofs = segment_ofs + params.block_size * topo.rank();
    auto ofs = block_ofs;
    for (size_t i = 0; i < iter_count; ++i) {
      auto lsn = ring.reserve_nb(params.xfer_size);
      if (!lsn) {
        throw std::runtime_error("ring buffer is full");
      }
      ring.pwrite(xfer_buffer_span, *lsn);
      handler->bb_ref().local_tree_.add(ofs, ofs + params.xfer_size, *lsn,
                                        topo.rank());
      ofs += params.xfer_size;
    }
  }

  topo.comm().barrier();
  result_json["write"] =
      bench_stats{sw.get(), params.io_count_per_proc() * topo.size(),
                  params.io_size_per_proc * topo.size()};
  topo.comm().barrier();
  sw.reset();
  topo.comm().barrier();

  // serialize local extent tree
  auto [send_data, out] = zpp::bits::data_out();
  out(handler->bb_ref().local_tree_).or_throw();

  topo.comm().barrier();
  result_json["time_serialize"] = sw.lap_time().count();
  topo.comm().barrier();

  // allgather serialized extent tree size
  std::vector<int> extent_tree_sizes(topo.size());
  topo.comm().all_gather(static_cast<int>(send_data.size()),
                         std::span{extent_tree_sizes});

  // all_gather_v serialized extent tree
  auto [recv_data, in] = zpp::bits::data_in();
  recv_data.resize(std::accumulate(extent_tree_sizes.begin(),
                                   extent_tree_sizes.end(), 0ULL));
  topo.comm().all_gather_v(std::as_bytes(std::span{send_data}),
                           std::as_writable_bytes(std::span{recv_data}),
                           std::span{extent_tree_sizes});

  topo.comm().barrier();
  result_json["time_allgather"] = sw.lap_time().count();
  topo.comm().barrier();

  // deserialize extent trees
  in(handler->bb_ref().global_tree_).or_throw();

  topo.comm().barrier();
  result_json["time_deserialize"] = sw.lap_time().count();
  topo.comm().barrier();

  // FIXME: inefficient merge
  extent_tree tmp_tree;
  for (size_t i = 1; i < extent_tree_sizes.size(); ++i) {
    in(tmp_tree).or_throw();
    handler->bb_ref().global_tree_.merge(tmp_tree);
  }

  topo.comm().barrier();
  result_json["time_merge"] = sw.lap_time().count();
  result_json["time_sync"] = sw.get().count();
  result_json["memory_usage"] = memory_usage::get_current_rss_kb();

  if (params.verbose) {
    mpi::run_on_rank0([&] {
      std::cerr << utils::to_string(handler->bb_ref().global_tree_)
                << std::endl;
    });
  }

  topo.comm().barrier();
  sw.reset();
  topo.comm().barrier();

  // reading
  auto& global_tree = handler->bb_ref().global_tree_;
  auto target_rank = (topo.rank() + shift_unit) % topo.size();
  for (size_t s = 0; s < params.segment_count; ++s) {
    auto segment_ofs = segment_size * s;
    auto block_ofs = segment_ofs + params.block_size * target_rank;
    auto ofs = block_ofs;
    for (size_t i = 0; i < iter_count; ++i) {
      auto it = global_tree.find(ofs, ofs + params.xfer_size);
      // mpi::run_on_rank0([&] {
      //   if (it == global_tree.end()) {
      //     fmt::print("extent not found: ofs: {}, size: {}\n", ofs,
      //                params.xfer_size);
      //   } else {
      //     fmt::print("ofs: {}, size: {}, extent: {}\n", ofs,
      //     params.xfer_size,
      //                utils::to_string(*it));
      //   }
      // });

      // read extent(s) and fill xfer_buffer_span
      size_t rpos = 0;
      while (rpos < params.xfer_size) {
        if (it == global_tree.end()) {
          throw std::runtime_error(
              fmt::format("extent not found: ofs: {}, size: {}", ofs + rpos,
                          params.xfer_size - rpos));
        }
        if (it->ex.begin > ofs + rpos) {
          throw std::runtime_error(
              fmt::format("io error: ofs: {}", ofs + rpos));
        }

        auto ofs_in_extent = ofs + rpos - it->ex.begin;
        auto rlsn = it->ptr + ofs_in_extent;
        auto rsize =
            std::min(it->ex.end - (ofs + rpos), params.xfer_size - rpos);
        // mpi::run_on_rank0([&] {
        //   fmt::print("{}: pread: buf(ofs{}, size{}), from rank {}, ofs {}\n",
        //              topo.rank(), rpos, rsize, it->client_id,
        //              ring.to_ofs(rlsn));
        // });
        rpm.pread(xfer_buffer_span.subspan(rpos, rsize), it->client_id,
                  ring.to_ofs(rlsn));
        ++it;
        rpos += rsize;
      }

      ofs += params.xfer_size;
    }
  }

  topo.comm().barrier();
  result_json["read"] =
      bench_stats{sw.get(), params.io_count_per_proc() * topo.size(),
                  params.io_size_per_proc * topo.size()};

  // output json
  mpi::run_on_rank0([&] {
    if (parsed.count("prettify") != 0U) {
      std::cout << std::setw(4);
    }
    ordered_json j = {
        {"version", RPMBB_VERSION},
    };
    j.merge_patch(ordered_json(params));
    j.merge_patch(result_json);
    std::cout << j << std::endl;
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
