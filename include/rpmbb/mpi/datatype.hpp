#pragma once

#include "rpmbb/mpi/raii.hpp"

#include <mpi.h>

namespace rpmbb::mpi {

// clang-format off
template <typename T>
MPI_Datatype basic_datatype();

template<> inline MPI_Datatype basic_datatype<int>()                 { return MPI_INT; }
template<> inline MPI_Datatype basic_datatype<unsigned int>()        { return MPI_UNSIGNED; }
template<> inline MPI_Datatype basic_datatype<long>()                { return MPI_LONG; }
template<> inline MPI_Datatype basic_datatype<unsigned long>()       { return MPI_UNSIGNED_LONG; }
template<> inline MPI_Datatype basic_datatype<long long>()           { return MPI_LONG_LONG_INT; }
template<> inline MPI_Datatype basic_datatype<unsigned long long>()  { return MPI_UNSIGNED_LONG_LONG; }
template<> inline MPI_Datatype basic_datatype<short>()               { return MPI_SHORT; }
template<> inline MPI_Datatype basic_datatype<unsigned short>()      { return MPI_UNSIGNED_SHORT; }
template<> inline MPI_Datatype basic_datatype<char>()                { return MPI_CHAR; }
template<> inline MPI_Datatype basic_datatype<unsigned char>()       { return MPI_UNSIGNED_CHAR; }
template<> inline MPI_Datatype basic_datatype<float>()               { return MPI_FLOAT; }
template<> inline MPI_Datatype basic_datatype<double>()              { return MPI_DOUBLE; }
template<> inline MPI_Datatype basic_datatype<long double>()         { return MPI_LONG_DOUBLE; }
// template<> inline MPI_Datatype basic_datatype<std::int8_t>()         { return MPI_INT8_T; }
// template<> inline MPI_Datatype basic_datatype<std::int16_t>()        { return MPI_INT16_T; }
// template<> inline MPI_Datatype basic_datatype<std::int32_t>()        { return MPI_INT32_T; }
// template<> inline MPI_Datatype basic_datatype<std::int64_t>()        { return MPI_INT64_T; }
// template<> inline MPI_Datatype basic_datatype<std::uint8_t>()        { return MPI_UINT8_T; }
// template<> inline MPI_Datatype basic_datatype<std::uint16_t>()       { return MPI_UINT16_T; }
// template<> inline MPI_Datatype basic_datatype<std::uint32_t>()       { return MPI_UINT32_T; }
// template<> inline MPI_Datatype basic_datatype<std::uint64_t>()       { return MPI_UINT64_T; }
template<> inline MPI_Datatype basic_datatype<bool>()                { return MPI_CXX_BOOL; }
template<> inline MPI_Datatype basic_datatype<void*>()               { return basic_datatype<uintptr_t>(); }
template<> inline MPI_Datatype basic_datatype<std::byte>()           { return MPI_BYTE; }
// clang-format on

class datatype {
  raii::unique_datatype datatype_{MPI_DATATYPE_NULL};

 public:
  template <typename T>
  static auto basic() -> const datatype& {
    static datatype instance{basic_datatype<T>(), false};
    return instance;
  }

  explicit datatype(const MPI_Datatype native, const bool managed = true)
      : datatype_(native, managed) {}
  datatype(const datatype&) = delete;
  auto operator=(const datatype&) -> datatype& = delete;
  datatype(datatype&&) = default;
  auto operator=(datatype&&) -> datatype& = default;
  auto operator==(const datatype& other) const -> bool {
    return datatype_ == other.datatype_;
  }
  auto operator!=(const datatype& other) const -> bool {
    return !(*this == other);
  }
  friend bool operator==(const datatype& lhs, MPI_Datatype rhs) {
    return lhs.native() == rhs;
  }
  friend bool operator==(MPI_Datatype lhs, const datatype& rhs) {
    return lhs == rhs.native();
  }
  auto native() const -> MPI_Datatype { return datatype_.get().native; }
  operator MPI_Datatype() const { return native(); }
};

}  // namespace rpmbb::mpi
