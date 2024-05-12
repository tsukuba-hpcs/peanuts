#pragma once

#include "peanuts/mpi/info.hpp"

namespace peanuts::mpi {
std::ostream& inspect(std::ostream& os, const info& info) {
  os << "{";
  for (const auto& [key, value] : info) {
    os << key << ": " << value << ", ";
  }
  os << "}";
  return os;
}
}  // namespace peanuts::mpi
