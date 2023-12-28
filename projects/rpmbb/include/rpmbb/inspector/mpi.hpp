#pragma once

#include "rpmbb/mpi/info.hpp"

namespace rpmbb::mpi {
std::ostream& inspect(std::ostream& os, const info& info) {
  os << "{";
  for (const auto& [key, value] : info) {
    os << key << ": " << value << ", ";
  }
  os << "}";
  return os;
}
}  // namespace rpmbb::mpi
