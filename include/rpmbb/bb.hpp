#pragma once

#include "rpmbb/extent_tree.hpp"
#include "rpmbb/raii/fd.hpp"
#include "rpmbb/rpm.hpp"
#include "rpmbb/utils/fs.hpp"

#include <sys/types.h>
#include <functional>
#include <memory>
#include <unordered_map>

namespace rpmbb {

class bb {
  extent_tree global_tree_{};
  extent_tree local_tree_{};
};

class bb_handler {
 public:
  bb_handler(std::shared_ptr<bb> bb,
             int fd,
             std::reference_wrapper<rpm> rpm_ref)
      : bb_(std::move(bb)), fd_(fd), rpm_ref_(rpm_ref) {}

 private:
  std::shared_ptr<bb> bb_;
  int fd_;
  std::reference_wrapper<rpm> rpm_ref_;
};

class bb_store {
 public:
  bb_store(std::reference_wrapper<rpm> rpm_ref) : rpm_ref_(rpm_ref) {}

  std::unique_ptr<bb_handler> open(ino_t ino, int fd) {
    auto it = bb_store_.find(ino);
    if (it == bb_store_.end()) {
      it = bb_store_.emplace(ino, std::make_shared<bb>()).first;
    }
    return std::make_unique<bb_handler>(it->second, fd, rpm_ref_);
  }

  std::unique_ptr<bb_handler> open(int fd) {
    return open(utils::get_ino(fd), fd);
  }

  void unlink(ino_t ino) { bb_store_.erase(ino); }
  void unlink(int fd) { unlink(utils::get_ino(fd)); }

 private:
  std::unordered_map<ino_t, std::shared_ptr<bb>> bb_store_;
  std::reference_wrapper<rpm> rpm_ref_;
};

}  // namespace rpmbb
