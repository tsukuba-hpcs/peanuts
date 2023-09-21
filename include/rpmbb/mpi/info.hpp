#pragma once

#include "rpmbb/mpi/error.hpp"

#include <mpi.h>

#include <optional>
#include <string>
#include <unordered_map>

namespace rpmbb::mpi {

class info {
  raii::unique_info info_{MPI_INFO_NULL};

 public:
  static constexpr size_t max_key_length = MPI_MAX_INFO_KEY;
  static constexpr size_t max_value_length = MPI_MAX_INFO_VAL;
  using map_type = std::unordered_map<std::string, std::string>;

 public:
  info() {
    MPI_Info info;
    MPI_CHECK_ERROR_CODE(MPI_Info_create(&info));
    info_.reset(info);
  }
  info(const MPI_Info native) : info_(native) {}
  explicit info(const map_type& map) : info() {
    for (const auto& [key, value] : map) {
      set(key, value);
    }
  }
  info(std::initializer_list<std::pair<std::string, std::string>> init_list)
      : info() {
    for (const auto& [key, value] : init_list) {
      set(key, value);
    }
  }
  info(const info&) = delete;
  auto operator=(const info&) -> info& = delete;
  info(info&&) = default;
  auto operator=(info&&) -> info& = default;

  auto operator==(const info& other) const -> bool {
    return info_ == other.info_;
  }
  auto operator!=(const info& other) const -> bool { return !(*this == other); }

  auto native() const -> MPI_Info { return info_.get().native; }

  auto duplicate() const -> info {
    MPI_Info new_info;
    MPI_CHECK_ERROR_CODE(MPI_Info_dup(native(), &new_info));
    return {new_info};
  }

  auto to_map() const -> map_type {
    map_type result;
    for (int i = 0; i < get_nkeys(); ++i) {
      const auto key = get_nthkey(i);
      const auto value = get(key);
      if (value) {
        result[key] = *value;
      }
    }
    return result;
  }

  auto set(const std::string& key, const std::string& value) -> void {
    MPI_CHECK_ERROR_CODE(MPI_Info_set(native(), key.c_str(), value.c_str()));
  }

  auto get(const std::string& key) const -> std::optional<std::string> {
    int flag;
    char value[max_value_length];
    MPI_CHECK_ERROR_CODE(
        MPI_Info_get(native(), key.c_str(), max_value_length, value, &flag));
    return flag ? std::optional<std::string>{value} : std::nullopt;
  }

  auto remove(const std::string& key) -> void {
    MPI_CHECK_ERROR_CODE(MPI_Info_delete(native(), key.c_str()));
  }

  auto get_nkeys() const -> int {
    int nkeys;
    MPI_CHECK_ERROR_CODE(MPI_Info_get_nkeys(native(), &nkeys));
    return nkeys;
  }

  auto get_nthkey(int n) const -> std::string {
    char key[max_key_length];
    MPI_CHECK_ERROR_CODE(MPI_Info_get_nthkey(native(), n, key));
    return {key};
  }

  auto operator[](int n) const -> std::string { return get_nthkey(n); }

  auto operator[](const std::string& key) const -> std::optional<std::string> {
    return get(key);
  }
};

}  // namespace rpmbb::mpi
