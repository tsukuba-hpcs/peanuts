#pragma once

#include "rpmbb/mpi/raii.hpp"

#include <mpi.h>

namespace rpmbb::mpi {
class group {
  raii::unique_group group_{MPI_GROUP_NULL};

 public:
  group() = default;
  explicit group(const MPI_Group native, const bool managed = false)
      : group_{native, managed} {}

  operator MPI_Group() const { return native(); }
  auto native() const -> MPI_Group { return group_.get().native; }

  int rank() const {
    int result;
    MPI_CHECK_ERROR_CODE(MPI_Group_rank(native(), &result));
    return result;
  }

  int size() const {
    int result;
    MPI_CHECK_ERROR_CODE(MPI_Group_size(native(), &result));
    return result;
  }

  auto include(const std::span<const int> ranks) const -> group {
    MPI_Group group;
    MPI_CHECK_ERROR_CODE(
        MPI_Group_incl(native(), ranks.size(), ranks.data(), &group));
    return mpi::group{group, true};
  }

  auto exclude(const std::span<const int> ranks) const -> group {
    MPI_Group group;
    MPI_CHECK_ERROR_CODE(
        MPI_Group_excl(native(), ranks.size(), ranks.data(), &group));
    return mpi::group{group, true};
  }

  auto translate_ranks(const std::span<const int> ranks,
                       const group& target) const -> std::vector<int> {
    std::vector<int> result(ranks.size());
    MPI_CHECK_ERROR_CODE(MPI_Group_translate_ranks(
        native(), ranks.size(), ranks.data(), target.native(), result.data()));
    return result;
  }
};
}  // namespace rpmbb::mpi
