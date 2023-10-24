#pragma once

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <libpmem2.h>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <system_error>
#include <vector>

#include "raii/fd.hpp"
#include "raii/pmem2.hpp"

namespace rpmbb::pmem2 {

class pmem2_error_category : public std::error_category {
 public:
  auto name() const noexcept -> const char* override { return "pmem2"; }

  auto message(int ev) const -> std::string override {
    std::error_condition posix_cond =
        std::system_category().default_error_condition(ev);
    if (posix_cond.category() == std::generic_category()) {
      return std::generic_category().message(ev);
    }
    return pmem2_errormsg();
  }

  auto default_error_condition(int ev) const noexcept
      -> std::error_condition override {
    std::error_condition posix_cond =
        std::system_category().default_error_condition(ev);
    if (posix_cond.category() == std::generic_category()) {
      return posix_cond;
    }
    return {ev, *this};
  }
};

auto pmem2_category() -> const std::error_category& {
  static pmem2_error_category instance;
  return instance;
}

auto make_error_code(int ev) -> std::error_code {
  return {ev, pmem2_category()};
}

class pmem2_error : public std::system_error {
 public:
  explicit pmem2_error(int ev, const std::string& what_arg = "")
      : std::system_error(make_error_code(ev), what_arg) {}
};

class source;

class device {
  raii::file_descriptor fd_{};

 public:
  device() = default;
  explicit device(std::string_view path, bool create_if_does_not_exist = true) {
    fd_ = raii::file_descriptor(::open(path.data(), O_RDWR));
    if (!fd_ && create_if_does_not_exist) {
      fd_ = raii::file_descriptor(
          ::open(path.data(), O_RDWR | O_CREAT | O_EXCL | O_TRUNC, 0600));
      if (!fd_) {
        throw pmem2_error(errno, "pmem2::device::open failed");
      }
    }
  }
  device(const device&) = delete;
  auto operator=(const device&) -> device& = delete;
  device(device&&) = default;
  auto operator=(device&&) -> device& = default;
  ~device() = default;

  auto fd() const noexcept -> int { return fd_.get(); }

  auto is_devdax() const -> bool {
    struct stat st {};
    if (fstat(fd(), &st) < 0) {
      throw pmem2_error(errno, "pmem2::device::is_devdax failed");
    }
    return S_ISCHR(st.st_mode);
  }

  auto truncate(off_t size) const -> void {
    if (ftruncate(fd(), size) < 0) {
      throw pmem2_error(errno, "pmem2::device::truncate failed");
    }
  }

  auto close() -> void {
    if (::close(fd_.release()) < 0) {
      throw pmem2_error(errno, "pmem2::device::close failed");
    }
  }
};

class source {
  raii::pmem2_source source_{};

 public:
  source() = default;
  explicit source(const device& device) {
    ::pmem2_source* src = nullptr;
    if (int ret = ::pmem2_source_from_fd(&src, device.fd()); ret < 0) {
      throw pmem2_error(-ret, "pmem2::device::from_device failed");
    }
    source_ = raii::pmem2_source(src);
  }
  source(const source&) = delete;
  auto operator=(const source&) -> source& = delete;
  source(source&&) = default;
  auto operator=(source&&) -> source& = default;
  ~source() = default;

  auto get() const noexcept -> ::pmem2_source* { return source_.get(); }

  auto alignment() const -> size_t {
    size_t alignment = 0;
    if (int ret = ::pmem2_source_alignment(source_.get(), &alignment);
        ret < 0) {
      throw pmem2_error(-ret, "pmem2::source::alignment failed");
    }
    return alignment;
  }
};

class config {
  raii::pmem2_config config_{};

 public:
  config() {
    ::pmem2_config* cfg_raw = nullptr;
    if (int ret = ::pmem2_config_new(&cfg_raw); ret < 0) {
      throw pmem2_error(-ret, "pmem2::config::create failed");
    }
    config_ = raii::pmem2_config(cfg_raw);
  };
  config(pmem2_granularity required) : config() {
    set_required_store_granularity(required);
  }
  config(const config&) = delete;
  auto operator=(const config&) -> config& = delete;
  config(config&&) = default;
  auto operator=(config&&) -> config& = default;
  ~config() = default;

  auto get() const noexcept -> ::pmem2_config* { return config_.get(); }

  auto set_required_store_granularity(pmem2_granularity granularity)
      -> config& {
    if (int ret = ::pmem2_config_set_required_store_granularity(config_.get(),
                                                                granularity);
        ret < 0) {
      throw pmem2_error(-ret,
                        "pmem2::config::set_required_store_granularity "
                        "failed");
    }
    return *this;
  }
};

class map {
  raii::pmem2_map map_{};

 public:
  map() = default;
  map(const source& source, const config& config) {
    ::pmem2_map* map_raw = nullptr;
    if (int ret = ::pmem2_map_new(&map_raw, config.get(), source.get());
        ret < 0) {
      throw pmem2_error(-ret, "pmem2::map::create failed");
    }
    map_ = raii::pmem2_map(map_raw);
  }
  map(const map&) = delete;
  auto operator=(const map&) -> map& = delete;
  map(map&&) = default;
  auto operator=(map&&) -> map& = default;
  ~map() = default;

  auto address() const noexcept -> void* {
    return ::pmem2_map_get_address(map_.get());
  }

  auto size() const noexcept -> size_t {
    return ::pmem2_map_get_size(map_.get());
  }

  auto as_span() const noexcept -> std::span<std::byte> {
    return {static_cast<std::byte*>(address()), size()};
  }

  auto as_iovec() const noexcept -> ::iovec { return {address(), size()}; }

  auto store_granularity() const noexcept -> pmem2_granularity {
    return ::pmem2_map_get_store_granularity(map_.get());
  }

  auto get_drain_fn() const noexcept -> ::pmem2_drain_fn {
    return ::pmem2_get_drain_fn(map_.get());
  }

  auto get_flush_fn() const noexcept -> ::pmem2_flush_fn {
    return ::pmem2_get_flush_fn(map_.get());
  }

  auto get_memmove_fn() const noexcept -> ::pmem2_memmove_fn {
    return ::pmem2_get_memmove_fn(map_.get());
  }

  auto get_memset_fn() const noexcept -> ::pmem2_memset_fn {
    return ::pmem2_get_memset_fn(map_.get());
  }

  auto get_memcpy_fn() const noexcept -> ::pmem2_memcpy_fn {
    return ::pmem2_get_memcpy_fn(map_.get());
  }

  auto get_persist_fn() const noexcept -> ::pmem2_persist_fn {
    return ::pmem2_get_persist_fn(map_.get());
  }
};

class memory_operations {
  ::pmem2_drain_fn drain_fn_{};
  ::pmem2_flush_fn flush_fn_{};
  ::pmem2_memmove_fn memmove_fn_{};
  ::pmem2_memset_fn memset_fn_{};
  ::pmem2_memcpy_fn memcpy_fn_{};
  ::pmem2_persist_fn persist_fn_{};

 public:
  memory_operations() = default;
  explicit memory_operations(const map& map) { associate_with(map); }
  auto associate_with(const map& map) -> void {
    drain_fn_ = map.get_drain_fn();
    flush_fn_ = map.get_flush_fn();
    memmove_fn_ = map.get_memmove_fn();
    memset_fn_ = map.get_memset_fn();
    memcpy_fn_ = map.get_memcpy_fn();
    persist_fn_ = map.get_persist_fn();
  }
  memory_operations(const memory_operations&) = default;
  auto operator=(const memory_operations&) -> memory_operations& = default;
  memory_operations(memory_operations&&) = default;
  auto operator=(memory_operations&&) -> memory_operations& = default;
  ~memory_operations() = default;

  auto drain() const -> void { drain_fn_(); }
  auto flush(const void* ptr, size_t size) const -> void {
    flush_fn_(ptr, size);
  }
  auto persist(const void* ptr, size_t size) const -> void {
    persist_fn_(ptr, size);
  }
  auto memmove(void* pmemdest,
               const void* src,
               size_t len,
               unsigned flags) const -> void* {
    return memmove_fn_(pmemdest, src, len, flags);
  }
  auto memmove_nt(void* pmemdest, const void* src, size_t len) const -> void* {
    return memmove(pmemdest, src, len, PMEM2_F_MEM_NONTEMPORAL);
  }
  auto memmove_t(void* pmemdest,
                 const void* src,
                 size_t len,
                 unsigned flags = 0) const -> void* {
    return memmove(pmemdest, src, len, flags | PMEM2_F_MEM_TEMPORAL);
  }
  auto memmove_noflush(void* pmemdest, const void* src, size_t len) const
      -> void* {
    return memmove_t(pmemdest, src, len, PMEM2_F_MEM_NOFLUSH);
  }
  auto memset(void* pmemdest, int val, size_t len, unsigned flags) const
      -> void* {
    return memset_fn_(pmemdest, val, len, flags);
  }
  auto memset_nt(void* pmemdest, int val, size_t len) const -> void* {
    return memset(pmemdest, val, len, PMEM2_F_MEM_NONTEMPORAL);
  }
  auto memset_t(void* pmemdest, int val, size_t len, unsigned flags) const
      -> void* {
    return memset(pmemdest, val, len, flags | PMEM2_F_MEM_TEMPORAL);
  }
  auto memset_noflush(void* pmemdest, int val, size_t len) const -> void* {
    return memset_t(pmemdest, val, len, PMEM2_F_MEM_NOFLUSH);
  }
  auto memcpy(void* pmemdest, const void* src, size_t len, unsigned flags) const
      -> void* {
    return memcpy_fn_(pmemdest, src, len, flags);
  }
  auto memcpy_nt(void* pmemdest, const void* src, size_t len) const -> void* {
    return memcpy(pmemdest, src, len, PMEM2_F_MEM_NONTEMPORAL);
  }
  auto memcpy_t(void* pmemdest,
                const void* src,
                size_t len,
                unsigned flags = 0) const -> void* {
    return memcpy(pmemdest, src, len, flags | PMEM2_F_MEM_TEMPORAL);
  }
  auto memcpy_noflush(void* pmemdest, const void* src, size_t len) const
      -> void* {
    return memcpy_t(pmemdest, src, len, PMEM2_F_MEM_NOFLUSH);
  }
};

class file_operations {
  memory_operations ops_{};
  void* address_{};
  size_t size_{};

 public:
  file_operations() = default;
  explicit file_operations(const map& map) { associate_with(map); }
  auto associate_with(const map& map) -> void {
    ops_.associate_with(map);
    address_ = map.address();
    size_ = map.size();
  }
  file_operations(const file_operations&) = default;
  auto operator=(const file_operations&) -> file_operations& = default;
  file_operations(file_operations&&) = default;
  auto operator=(file_operations&&) -> file_operations& = default;
  ~file_operations() = default;

  auto mem_ops() const noexcept -> const memory_operations& { return ops_; }

  auto address() const noexcept -> void* { return address_; }
  auto size() const noexcept -> size_t { return size_; }

  auto to_addr(off_t ofs) const noexcept -> void* {
    return static_cast<char*>(address_) + ofs;
  }

  auto drain() const -> void { ops_.drain(); }
  auto flush(off_t ofs, size_t size) const -> void {
    ops_.flush(to_addr(ofs), size);
  }
  auto persist(off_t ofs, size_t size) const -> void {
    ops_.persist(to_addr(ofs), size);
  }

  auto pread(std::span<std::byte> buf, off_t ofs) const -> void {
    std::memcpy(buf.data(), to_addr(ofs), buf.size());
  }

  auto pread(size_t count, off_t ofs) const -> std::vector<std::byte> {
    std::vector<std::byte> buf(count);
    std::memcpy(buf.data(), to_addr(ofs), count);
    return buf;
  }

  auto pwrite(std::span<const std::byte> buf, off_t ofs, unsigned flags) const
      -> void {
    ops_.memcpy(to_addr(ofs), buf.data(), buf.size(), flags);
  }

  auto pwrite_nt(std::span<const std::byte> buf, off_t ofs) const -> void {
    pwrite(buf, ofs, PMEM2_F_MEM_NONTEMPORAL);
  }

  auto pwrite_t(std::span<const std::byte> buf,
                off_t ofs,
                unsigned flags = 0) const -> void {
    pwrite(buf, ofs, flags | PMEM2_F_MEM_TEMPORAL);
  }

  auto pwrite_noflush(std::span<const std::byte> buf, off_t ofs) const -> void {
    pwrite_t(buf, ofs, PMEM2_F_MEM_NOFLUSH);
  }
};

}  // namespace rpmbb::pmem2
