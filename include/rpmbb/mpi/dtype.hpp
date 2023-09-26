#pragma once

#include "rpmbb/mpi/error.hpp"
#include "rpmbb/mpi/raii.hpp"
#include "rpmbb/util/singleton.hpp"

#include <mpi.h>
#include <tuple>
#include <type_traits>

namespace rpmbb::mpi {

class dtype {
  raii::unique_dtype dtype_{MPI_DATATYPE_NULL, false};

 public:
  dtype(const MPI_Datatype native, bool managed)
      : dtype_(native, managed) {}
  explicit dtype(const dtype& base, const int count) {
    MPI_Datatype new_dtype;
    MPI_CHECK_ERROR_CODE(MPI_Type_contiguous(count, base, &new_dtype));
    dtype_ = raii::unique_dtype{new_dtype, true};
  }

  dtype(const dtype&) = delete;
  auto operator=(const dtype&) -> dtype& = delete;
  dtype(dtype&&) = default;
  auto operator=(dtype&&) -> dtype& = default;
  auto operator==(const dtype& other) const -> bool {
    return dtype_ == other.dtype_;
  }
  auto operator!=(const dtype& other) const -> bool {
    return !(*this == other);
  }
  friend bool operator==(const dtype& lhs, MPI_Datatype rhs) {
    return lhs.native() == rhs;
  }
  friend bool operator==(MPI_Datatype lhs, const dtype& rhs) {
    return lhs == rhs.native();
  }
  auto native() const -> MPI_Datatype { return dtype_.get().native; }
  operator MPI_Datatype() const { return native(); }

  auto commit() -> void {
    MPI_Datatype dtype = native();
    MPI_CHECK_ERROR_CODE(MPI_Type_commit(&dtype));
    assert(dtype == native());
  }

  auto get_envelope() const -> std::tuple<int, int, int, int> {
    int num_integers, num_addresses, num_dtypes, combiner;
    MPI_CHECK_ERROR_CODE(MPI_Type_get_envelope(
        native(), &num_integers, &num_addresses, &num_dtypes, &combiner));
    return {num_integers, num_addresses, num_dtypes, combiner};
  }

  auto is_basic() const -> bool {
    auto [num_integers, num_addresses, num_dtypes, combiner] =
        get_envelope();
    return num_integers == 0 && num_addresses == 0 && num_dtypes == 0 &&
           combiner == MPI_COMBINER_NAMED;
  }
};

template <typename T>
struct dtype_converter {
  static dtype to_dtype();
};

template <typename T>
inline dtype to_dtype();

// clang-format off
template <> inline dtype to_dtype<int>()                { return {MPI_INT, false}; }
template <> inline dtype to_dtype<unsigned int>()       { return {MPI_UNSIGNED, false}; }
template <> inline dtype to_dtype<long>()               { return {MPI_LONG, false}; }
template <> inline dtype to_dtype<unsigned long>()      { return {MPI_UNSIGNED_LONG, false}; }
template <> inline dtype to_dtype<long long>()          { return {MPI_LONG_LONG_INT, false}; }
template <> inline dtype to_dtype<unsigned long long>() { return {MPI_UNSIGNED_LONG_LONG, false}; }
template <> inline dtype to_dtype<short>()              { return {MPI_SHORT, false}; }
template <> inline dtype to_dtype<unsigned short>()     { return {MPI_UNSIGNED_SHORT, false}; }
template <> inline dtype to_dtype<char>()               { return {MPI_CHAR, false}; }
template <> inline dtype to_dtype<unsigned char>()      { return {MPI_UNSIGNED_CHAR, false}; }
template <> inline dtype to_dtype<float>()              { return {MPI_FLOAT, false}; }
template <> inline dtype to_dtype<double>()             { return {MPI_DOUBLE, false}; }
template <> inline dtype to_dtype<long double>()        { return {MPI_LONG_DOUBLE, false}; }
template <> inline dtype to_dtype<bool>()               { return {MPI_CXX_BOOL, false}; }
template <> inline dtype to_dtype<std::byte>()          { return {MPI_BYTE, false}; }
template <> inline dtype to_dtype<void*>()              { return to_dtype<uintptr_t>(); }
// clang-format on

namespace detail {

template <typename T, typename = void>
struct has_func_to_dtype : std::false_type {};

template <typename T>
struct has_func_to_dtype<T, std::void_t<decltype(to_dtype<T>())>>
    : std::true_type {};

template <typename T>
constexpr auto select_to_dtype() -> dtype {
  if constexpr (has_func_to_dtype<T>::value) {
    return to_dtype<T>();
  } else {
    return dtype_converter<T>::to_dtype();
  }
}

}  // namespace detail

}  // namespace rpmbb::mpi
