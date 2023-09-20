#pragma once

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <libpmem2.h>
#include <memory>
#include <optional>
#include <string_view>
#include <system_error>

#include "raii/fd.hpp"
#include "raii/pmem2.hpp"

namespace rpmbb::pmem2 {

class pmem2_error_category : public std::error_category {
 public:
  const char* name() const noexcept override { return "pmem2"; }

  std::string message(int ev) const override {
    std::error_condition posix_cond =
        std::system_category().default_error_condition(ev);
    if (posix_cond.category() == std::generic_category()) {
      return std::generic_category().message(ev);
    }
    return pmem2_errormsg();
  }

  std::error_condition default_error_condition(int ev) const noexcept override {
    std::error_condition posix_cond =
        std::system_category().default_error_condition(ev);
    if (posix_cond.category() == std::generic_category()) {
      return posix_cond;
    }
    return std::error_condition(ev, *this);
  }
};

const std::error_category& pmem2_category() {
  static pmem2_error_category instance;
  return instance;
}

std::error_code make_error_code(int e) {
  return {e, pmem2_category()};
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
  static auto open(std::string_view path, bool create_if_does_not_exist = true)
      -> device {
    auto fd = raii::file_descriptor(::open(path.data(), O_RDWR));
    if (!fd && create_if_does_not_exist) {
      fd = raii::file_descriptor(
          ::open(path.data(), O_RDWR | O_CREAT | O_EXCL | O_TRUNC, 0600));
      if (!fd) {
        throw pmem2_error(errno, "pmem2::device::open failed");
      }
    }
    return device(std::move(fd));
  }

  device() = default;
  explicit device(raii::file_descriptor&& fd) : fd_(std::move(fd)) {}
  device(const device&) = delete;
  device& operator=(const device&) = delete;
  device(device&& device) = default;
  device& operator=(device&& device) = default;

  auto fd() const noexcept -> int { return fd_.get(); }

  auto is_devdax() const -> bool {
    struct stat st;
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

  auto close() const -> void {
    if (::close(fd()) < 0) {
      throw pmem2_error(errno, "pmem2::device::close failed");
    }
  }
};

class source {
  raii::pmem2_source source_{};

 public:
  static auto from_device(const device& device) -> source {
    ::pmem2_source* src = nullptr;
    if (int ret = ::pmem2_source_from_fd(&src, device.fd()); ret < 0) {
      throw pmem2_error(-ret, "pmem2::device::from_device failed");
    }
    return source(raii::pmem2_source(src));
  }

  source() = default;
  explicit source(raii::pmem2_source&& source) : source_(std::move(source)) {}
  source(const source&) = delete;
  source& operator=(const source&) = delete;
  source(source&& source) = default;
  source& operator=(source&& source) = default;

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
  static auto create() -> config {
    ::pmem2_config* cfg_raw = nullptr;
    if (int ret = ::pmem2_config_new(&cfg_raw); ret < 0) {
      throw pmem2_error(-ret, "pmem2::config::create failed");
    }
    return config{raii::pmem2_config(cfg_raw)};
  }
  config() = default;
  explicit config(raii::pmem2_config&& config) : config_(std::move(config)) {}
  config(const config&) = delete;
  config& operator=(const config&) = delete;
  config(config&& config) = default;
  config& operator=(config&& config) = default;

  auto get() const noexcept -> ::pmem2_config* { return config_.get(); }

  config& set_required_store_granularity(pmem2_granularity granularity) {
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
  static auto create(const source& source, const config& config) -> map {
    ::pmem2_map* map_raw = nullptr;
    if (int ret = ::pmem2_map_new(&map_raw, config.get(), source.get());
        ret < 0) {
      throw pmem2_error(-ret, "pmem2::map::create failed");
    }
    return map{raii::pmem2_map(map_raw)};
  }

  map() = default;
  explicit map(raii::pmem2_map&& map) : map_(std::move(map)) {}
  map(const map&) = delete;
  map& operator=(const map&) = delete;
  map(map&& map) = default;
  map& operator=(map&& map) = default;

  auto address() const noexcept -> void* {
    return ::pmem2_map_get_address(map_.get());
  }

  auto size() const noexcept -> size_t {
    return ::pmem2_map_get_size(map_.get());
  }

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
  memory_operations& operator=(const memory_operations&) = default;
  memory_operations(memory_operations&&) = default;
  memory_operations& operator=(memory_operations&&) = default;

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
  auto memmove_noflush(void* pmemdest, const void* src, size_t len) const -> void* {
    return memmove_t(pmemdest, src, len, PMEM2_F_MEM_NOFLUSH);
  }
  auto memset(void* pmemdest, int c, size_t len, unsigned flags) const
      -> void* {
    return memset_fn_(pmemdest, c, len, flags);
  }
  auto memset_nt(void* pmemdest, int c, size_t len) const -> void* {
    return memset(pmemdest, c, len, PMEM2_F_MEM_NONTEMPORAL);
  }
  auto memset_t(void* pmemdest, int c, size_t len, unsigned flags) const
      -> void* {
    return memset(pmemdest, c, len, flags | PMEM2_F_MEM_TEMPORAL);
  }
  auto memset_noflush(void* pmemdest, int c, size_t len) const -> void* {
    return memset_t(pmemdest, c, len, PMEM2_F_MEM_NOFLUSH);
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
  auto memcpy_noflush(void* pmemdest, const void* src, size_t len) const -> void* {
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
  file_operations& operator=(const file_operations&) = default;
  file_operations(file_operations&&) = default;
  file_operations& operator=(file_operations&&) = default;

  const memory_operations& mem_ops() const noexcept { return ops_; }

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

  auto pread(void* buf, size_t count, off_t ofs) const -> void {
    ::memcpy(buf, to_addr(ofs), count);
  }
  auto pwrite(const void* buf, size_t count, off_t ofs, unsigned flags) const
      -> void {
    ops_.memcpy(to_addr(ofs), buf, count, flags);
  }
  auto pwrite_nt(const void* buf, size_t count, off_t ofs) const -> void {
    pwrite(buf, count, ofs, PMEM2_F_MEM_NONTEMPORAL);
  }
  auto pwrite_t(const void* buf,
                size_t count,
                off_t ofs,
                unsigned flags = 0) const -> void {
    pwrite(buf, count, ofs, flags | PMEM2_F_MEM_TEMPORAL);
  }
  auto pwrite_noflush(const void* buf, size_t count, off_t ofs) const -> void {
    pwrite_t(buf, count, ofs, PMEM2_F_MEM_NOFLUSH);
  }
};

}  // namespace rpmbb::pmem2
