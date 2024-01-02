#include "rpmbb_c.h"
#include "rpmbb/bb.hpp"

#include <mpi.h>
#include <sys/types.h>

#include <cstddef>

extern "C" {

rpmbb_store_t rpmbb_store_create(MPI_Comm comm,
                                 const char* pmem_path,
                                 size_t pmem_size) {
  // try {
  //   auto topo =
  //       std::make_unique<rpmbb::topology>(rpmbb::mpi::comm{comm, false});
  //   auto rpm =
  //       std::make_unique<rpmbb::rpm>(std::cref(*topo), pmem_path, pmem_size);

  //   rpmbb_store_t store =
  //       static_cast<rpmbb_store_t>(std::malloc(sizeof(rpmbb::bb_store)));
  //   store->topo = new rpmbb::topology{rpmbb::mpi::comm{comm, false}};
  //   store->rpm = new rpmbb::rpm{
  //       std::cref(*reinterpret_cast<rpmbb::topology*>(store->topo)), pmem_path,
  //       pmem_size};
  //   auto store = new rpmbb::bb_store(comm, pmem_path, pmem_size);
  //   return reinterpret_cast<rpmbb_store_t>(store);
  // } catch (...) {
  //   return nullptr;
  // }
  return nullptr;
}

int rpmbb_store_free(rpmbb_store_t store) {
  // if (!store)
  //   return -1;
  // delete reinterpret_cast<rpmbb::bb_store*>(store);
  // return 0;  // 成功
  return -1;
}

int rpmbb_store_save(rpmbb_store_t store) {
  // if (!store)
  //   return -1;
  // try {
  //   auto cpp_store = reinterpret_cast<rpmbb::bb_store*>(store);
  //   cpp_store->save();
  //   return 0;  // 成功
  // } catch (...) {
  //   return -1;  // 失敗時は-1を返す
  // }
  return -1;
}

int rpmbb_store_load(rpmbb_store_t store) {
  // if (!store)
  //   return -1;
  // try {
  //   auto cpp_store = reinterpret_cast<rpmbb::bb_store*>(store);
  //   cpp_store->load();
  //   return 0;  // 成功
  // } catch (...) {
  //   return -1;  // 失敗時は-1を返す
  // }
  return -1;
}

rpmbb_handler_t rpmbb_store_open_attach(rpmbb_store_t store, int fd) {
  // if (!store)
  //   return nullptr;
  // try {
  //   auto cpp_store = reinterpret_cast<rpmbb::bb_store*>(store);
  //   auto handler = cpp_store->open(fd);
  //   return reinterpret_cast<rpmbb_handler_t>(handler.release());
  // } catch (...) {
  //   return nullptr;  // 失敗時はnullptrを返す
  // }
  return nullptr;
}

int rpmbb_bb_close(rpmbb_handler_t handler) {
  // if (!handler)
  //   return -1;
  // delete reinterpret_cast<rpmbb::bb_handler*>(handler);
  // return 0;  // 成功
  return -1;
}

ssize_t rpmbb_bb_pwrite(rpmbb_handler_t handler,
                        const void* buf,
                        size_t count,
                        off_t offset) {
  // if (!handler)
  //   return -1;
  // try {
  //   auto cpp_handler = reinterpret_cast<rpmbb::bb_handler*>(handler);
  //   return cpp_handler->pwrite(
  //       std::span<const std::byte>(static_cast<const std::byte*>(buf), count),
  //       offset);
  // } catch (...) {
  //   return -1;  // 失敗時は-1を返す
  // }
  return -1;
}

ssize_t rpmbb_bb_pread(rpmbb_handler_t handler,
                       void* buf,
                       size_t count,
                       off_t offset) {
  // if (!handler)
  //   return -1;
  // try {
  //   auto cpp_handler = reinterpret_cast<rpmbb::bb_handler*>(handler);
  //   return cpp_handler->pread(
  //       std::span<std::byte>(static_cast<std::byte*>(buf), count), offset);
  // } catch (...) {
  //   return -1;  // 失敗時は-1を返す
  // }
  return -1;
}

int rpmbb_bb_sync(rpmbb_handler_t handler) {
  // if (!handler)
  //   return -1;
  // try {
  //   auto cpp_handler = reinterpret_cast<rpmbb::bb_handler*>(handler);
  //   cpp_handler->sync();
  //   return 0;  // 成功
  // } catch (...) {
  //   return -1;  // 失敗時は-1を返す
  // }
  return -1;
}
}
