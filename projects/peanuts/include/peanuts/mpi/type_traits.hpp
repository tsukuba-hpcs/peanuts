#pragma once

#include <span>
#include <type_traits>

namespace peanuts::mpi {

template <typename T, typename = void>
struct is_convertible_to_span : std::false_type {};

template <typename T>
struct is_convertible_to_span<
    T,
    std::void_t<decltype(std::span{std::declval<T&>()})>> : std::true_type {};

}  // namespace peanuts::mpi
