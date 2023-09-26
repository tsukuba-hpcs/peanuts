#pragma once

#include "rpmbb/mpi/error.hpp"
#include "rpmbb/mpi/raii.hpp"
#include "rpmbb/util/singleton.hpp"

#include <mpi.h>
#include <tuple>
#include <type_traits>

namespace rpmbb::mpi {

class datatype {
  raii::unique_datatype datatype_{MPI_DATATYPE_NULL, false};

 public:
  datatype(const MPI_Datatype native, bool managed)
      : datatype_(native, managed) {}
  explicit datatype(const datatype& base, const int count) {
    MPI_Datatype new_dtype;
    MPI_CHECK_ERROR_CODE(MPI_Type_contiguous(count, base, &new_dtype));
    datatype_ = raii::unique_datatype{new_dtype, true};
  }

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

  auto commit() -> void {
    MPI_Datatype dtype = native();
    MPI_CHECK_ERROR_CODE(MPI_Type_commit(&dtype));
    assert(dtype == native());
  }

  auto get_envelope() const -> std::tuple<int, int, int, int> {
    int num_integers, num_addresses, num_datatypes, combiner;
    MPI_CHECK_ERROR_CODE(MPI_Type_get_envelope(
        native(), &num_integers, &num_addresses, &num_datatypes, &combiner));
    return {num_integers, num_addresses, num_datatypes, combiner};
  }

  auto is_basic() const -> bool {
    auto [num_integers, num_addresses, num_datatypes, combiner] =
        get_envelope();
    return num_integers == 0 && num_addresses == 0 && num_datatypes == 0 &&
           combiner == MPI_COMBINER_NAMED;
  }
};

template <typename T>
struct datatype_converter {
  static datatype to_datatype();
};

template <typename T>
inline datatype to_datatype();

// clang-format off
template <> inline datatype to_datatype<int>()                { return {MPI_INT, false}; }
template <> inline datatype to_datatype<unsigned int>()       { return {MPI_UNSIGNED, false}; }
template <> inline datatype to_datatype<long>()               { return {MPI_LONG, false}; }
template <> inline datatype to_datatype<unsigned long>()      { return {MPI_UNSIGNED_LONG, false}; }
template <> inline datatype to_datatype<long long>()          { return {MPI_LONG_LONG_INT, false}; }
template <> inline datatype to_datatype<unsigned long long>() { return {MPI_UNSIGNED_LONG_LONG, false}; }
template <> inline datatype to_datatype<short>()              { return {MPI_SHORT, false}; }
template <> inline datatype to_datatype<unsigned short>()     { return {MPI_UNSIGNED_SHORT, false}; }
template <> inline datatype to_datatype<char>()               { return {MPI_CHAR, false}; }
template <> inline datatype to_datatype<unsigned char>()      { return {MPI_UNSIGNED_CHAR, false}; }
template <> inline datatype to_datatype<float>()              { return {MPI_FLOAT, false}; }
template <> inline datatype to_datatype<double>()             { return {MPI_DOUBLE, false}; }
template <> inline datatype to_datatype<long double>()        { return {MPI_LONG_DOUBLE, false}; }
template <> inline datatype to_datatype<bool>()               { return {MPI_CXX_BOOL, false}; }
template <> inline datatype to_datatype<std::byte>()          { return {MPI_BYTE, false}; }
template <> inline datatype to_datatype<void*>()              { return to_datatype<uintptr_t>(); }
// clang-format on

namespace detail {

template <typename T, typename = void>
struct has_func_to_datatype : std::false_type {};

template <typename T>
struct has_func_to_datatype<T, std::void_t<decltype(to_datatype<T>())>>
    : std::true_type {};

template <typename T>
constexpr auto select_to_datatype() -> datatype {
  if constexpr (has_func_to_datatype<T>::value) {
    return to_datatype<T>();
  } else {
    return datatype_converter<T>::to_datatype();
  }
}

}  // namespace detail

}  // namespace rpmbb::mpi
