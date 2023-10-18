#pragma once

#include "rpmbb/mpi.hpp"
#include "rpmbb/utils/enumerate.hpp"

#include <map>
#include <ostream>
#include <span>
#include <tuple>
#include <vector>

namespace rpmbb {

struct rank_pair {
  int inter;
  int intra;

  bool operator==(const rank_pair& other) const {
    return std::tie(inter, intra) == std::tie(other.inter, other.intra);
  }

  bool operator<(const rank_pair& other) const {
    return std::tie(inter, intra) < std::tie(other.inter, other.intra);
  }

  std::ostream& inspect(std::ostream& os) const {
    return os << "[" << inter << ", " << intra << "]";
  }
};

class topology {
 public:
  explicit topology(mpi::comm comm = mpi::comm{MPI_COMM_WORLD})
      : comm_(std::move(comm)),
        intra_comm_{comm_, mpi::split_type::shared},
        inter_comm_{comm_, intra_comm_.rank()},
        rank_map_{create_rank_map()},
        inverted_rank_map_{create_inverted_rank_map()} {}

  auto comm() const -> const mpi::comm& { return comm_; }
  auto intra_comm() const -> const mpi::comm& { return intra_comm_; }
  auto inter_comm() const -> const mpi::comm& { return inter_comm_; }

  auto rank() const -> int { return comm_.rank(); }
  auto size() const -> int { return comm_.size(); }
  auto intra_rank() const -> int { return intra_comm_.rank(); }
  auto intra_size() const -> int { return intra_comm_.size(); }
  auto inter_rank() const -> int { return inter_comm_.rank(); }
  auto inter_size() const -> int { return inter_comm_.size(); }
  auto np() const -> int { return size(); }

  auto global2inter_rank(const int global_rank) const -> int {
    return rank_map_[global_rank].inter;
  }

  auto global2intra_rank(const int global_rank) const -> int {
    return rank_map_[global_rank].intra;
  }

  auto global2rank_pair(const int global_rank) const -> rank_pair {
    return rank_map_[global_rank];
  }

  auto inter2global_rank(const int inter_rank) const -> int {
    return inverted_rank_map_.at({inter_rank, 0});
  }

  auto intra2global_rank(const int intra_rank) const -> int {
    return inverted_rank_map_.at({inter_rank(), intra_rank});
  }

  auto rank_pair2global_rank(const rank_pair& pair) const -> int {
    return inverted_rank_map_.at(pair);
  }

  auto rank_pair2global_rank(const int inter_rank, const int intra_rank) const
      -> int {
    return inverted_rank_map_.at({inter_rank, intra_rank});
  }

 private:
  auto create_rank_map() const -> std::vector<rank_pair> {
    std::vector<rank_pair> rank_map(size());
    mpi::dtype rank_pair_dtype = mpi::dtype{mpi::to_dtype<int>(), 2};
    rank_pair_dtype.commit();
    const auto pair = rank_pair{inter_rank(), intra_rank()};
    comm().all_gather(pair, rank_pair_dtype, std::span{rank_map},
                      rank_pair_dtype);
    return rank_map;
  }

  auto create_inverted_rank_map() const -> std::map<rank_pair, int> {
    std::map<rank_pair, int> inverted_rank_map;
    for (const auto& [global_rank, pair] : utils::cenumerate(rank_map_)) {
      inverted_rank_map[pair] = global_rank;
    }
    return inverted_rank_map;
  }

 private:
  mpi::comm comm_;
  mpi::comm intra_comm_;
  mpi::comm inter_comm_;
  std::vector<rank_pair> rank_map_;
  std::map<rank_pair, int> inverted_rank_map_;
};

}  // namespace rpmbb
