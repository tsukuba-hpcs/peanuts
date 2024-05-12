#pragma once

#include "peanuts/mpi/dtype.hpp"
#include "peanuts/mpi/type_traits.hpp"

#include <span>
#include <type_traits>

namespace peanuts::mpi::detail {

template <typename T, typename = void>
class container_adapter;

template <typename T>
class container_adapter<T,
                        std::enable_if_t<is_convertible_to_span<
                            std::remove_reference_t<T>>::value>> {
 public:
  using container_type = std::remove_reference_t<T>;
  using value_type =
      decltype(std::span(std::declval<container_type&>()))::value_type;

  static auto to_span(container_type& container) -> std::span<value_type> {
    return std::span<value_type>(container);
  }

  static auto to_cspan(const container_type& container)
      -> std::span<const value_type> {
    return std::span<const value_type>(container);
  }

  static auto to_dtype() -> mpi::dtype {
    return mpi::to_dtype<std::remove_cv_t<value_type>>();
  }
};

template <typename T>
class container_adapter<
    T,
    std::enable_if_t<
        std::negation_v<is_convertible_to_span<std::remove_reference_t<T>>>>> {
 public:
  using container_type = std::remove_reference_t<T>;
  using value_type = container_type;

  static auto to_span(container_type& container) -> std::span<value_type> {
    return std::span<value_type>{&container, 1};
  }

  static auto to_cspan(const container_type& container)
      -> std::span<const value_type> {
    return std::span<const value_type>{&container, 1};
  }

  static auto to_dtype() -> mpi::dtype {
    return mpi::to_dtype<std::remove_cv_t<value_type>>();
  }
};

}  // namespace peanuts::mpi::detail
