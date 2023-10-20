#pragma once

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <system_error>

namespace rpmbb::utils {

inline ino_t get_ino(const std::string& file_path) {
  struct stat stat_buf;
  if (stat(file_path.c_str(), &stat_buf) == 0) {
    return stat_buf.st_ino;
  }
  throw std::system_error(errno, std::system_category(),
                          "Failed to get inode number");
}

inline ino_t get_ino(int fd) {
  struct stat stat_buf;
  if (fstat(fd, &stat_buf) == 0) {
    return stat_buf.st_ino;
  }
  throw std::system_error(errno, std::system_category(),
                          "Failed to get inode number");
}

}  // namespace rpmbb::utils
