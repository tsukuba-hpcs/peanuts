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

struct bb {
  ino_t ino;
  extent_tree global_tree_{};
  extent_tree local_tree_{};
};

namespace detail {
struct bb_hash {
  std::size_t operator()(const std::shared_ptr<bb>& bb) const {
    return std::hash<ino_t>()(bb->ino);
  }
};

struct bb_equal {
  bool operator()(const std::shared_ptr<bb>& lhs,
                  const std::shared_ptr<bb>& rhs) const {
    return lhs->ino == rhs->ino;
  }
};
}  // namespace detail

class bb_handler {
 public:
  bb_handler(std::shared_ptr<bb> bb,
             int fd,
             std::reference_wrapper<rpm> rpm_ref)
      : bb_(std::move(bb)), fd_(fd), rpm_ref_(rpm_ref) {}

  auto bb_ref() -> rpmbb::bb& { return *bb_; }

 private:
  std::shared_ptr<bb> bb_;
  int fd_;
  std::reference_wrapper<rpm> rpm_ref_;
};

class bb_store {
 public:
  bb_store(std::reference_wrapper<rpm> rpm_ref) : rpm_ref_(rpm_ref) {}

  std::unique_ptr<bb_handler> open(ino_t ino, int fd) {
    auto bb_obj = std::make_shared<bb>(bb{ino});
    auto [it, inserted] = bb_store_.insert(bb_obj);
    return std::make_unique<bb_handler>(*it, fd, rpm_ref_);
  }

  std::unique_ptr<bb_handler> open(int fd) {
    return open(utils::get_ino(fd), fd);
  }

  void unlink(ino_t ino) { bb_store_.erase(std::make_shared<bb>(bb{ino})); }
  void unlink(int fd) { unlink(utils::get_ino(fd)); }

 private:
  std::unordered_set<std::shared_ptr<bb>, detail::bb_hash, detail::bb_equal>
      bb_store_;
  std::reference_wrapper<rpm> rpm_ref_;
};

}  // namespace rpmbb
