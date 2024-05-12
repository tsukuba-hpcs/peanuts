#pragma once

#include <zpp/file.h>
#include <optional>
#include <string>

namespace peanuts {

class deferred_file {
 public:
  deferred_file() = default;
  deferred_file(std::string pathname, int flags, mode_t mode)
      : pathname_{std::move(pathname)}, flags_{flags}, mode_{mode} {}

  deferred_file(const deferred_file&) = delete;
  auto operator=(const deferred_file&) -> deferred_file& = delete;
  deferred_file(deferred_file&&) = default;
  auto operator=(deferred_file&&) -> deferred_file& = default;

  void open() {
    int fd = ::open(pathname_.c_str(), flags_, mode_);
    if (fd < 0) {
      throw std::system_error{errno, std::system_category(),
                              "Failed to open file"};
    }
    file_.emplace(zpp::filesystem::file_handle{fd});
  }

  void close() {
    file_->close();
    file_.reset();
  }

  bool is_open() const { return file_.has_value(); }

  auto fd() -> int {
    if (!is_open()) {
      open();
    }
    return file_->get();
  }

  auto ino() -> ino_t {
    if (!is_open()) {
      open();
    }
    struct stat stat_buf;
    if (fstat(fd(), &stat_buf) == 0) {
      return stat_buf.st_ino;
    }
    throw std::system_error(errno, std::system_category(),
                            "Failed to get inode number");
  }

  auto size() -> size_t {
    if (!is_open()) {
      open();
    }
    return file_->size();
  }

  auto truncate(size_t size) -> void {
    if (!is_open()) {
      open();
    }
    file_->truncate(size);
  }

  auto pread(zpp::byte_view buf, off_t offset) -> ssize_t {
    if (!is_open()) {
      open();
    }
    file_->seek(offset, zpp::filesystem::seek_mode::set);
    return file_->read(buf);
  }

  auto pwrite(zpp::cbyte_view buf, off_t offset) -> ssize_t {
    if (!is_open()) {
      open();
    }
    file_->seek(offset, zpp::filesystem::seek_mode::set);
    return file_->write(buf);
  }

 private:
  std::optional<zpp::filesystem::file> file_;
  std::string pathname_;
  int flags_ = 0;
  mode_t mode_ = 0;
};

}  // namespace peanuts
