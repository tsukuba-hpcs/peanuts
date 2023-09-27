#pragma once

#include "rpmbb/mpi/dtype.hpp"
#include "rpmbb/mpi/type_traits.hpp"

#include <type_traits>

namespace rpmbb::mpi::detail {

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

  explicit container_adapter(container_type& container)
      : container_(container) {}

  auto to_span() -> std::span<value_type> {
    return std::span<value_type>(container_);
  }

  auto to_cspan() -> std::span<const value_type> {
    return std::span<const value_type>(container_);
  }

  auto to_dtype() -> mpi::dtype {
    return mpi::to_dtype<std::remove_cv_t<value_type>>();
  }

 private:
  container_type& container_;
};

template <typename T>
class container_adapter<
    T,
    std::enable_if_t<
        std::negation_v<is_convertible_to_span<std::remove_reference_t<T>>>>> {
 public:
  using container_type = std::remove_reference_t<T>;
  using value_type = container_type;

  explicit container_adapter(T& container) : container_(container) {}

  auto to_span() -> std::span<value_type> {
    return std::span<value_type>{&container_, 1};
  }

  auto to_cspan() -> std::span<const value_type> {
    return std::span<const value_type>{&container_, 1};
  }

  auto to_dtype() -> mpi::dtype {
    return mpi::to_dtype<std::remove_cv_t<value_type>>();
  }

 private:
  container_type& container_;
};

}  // namespace rpmbb::mpi::detail
