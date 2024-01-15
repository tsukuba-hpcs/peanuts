#include "rpmbb_c.h"
#include "rpmbb/bb.hpp"

#include <mpi.h>
#include <sys/types.h>

#include <cstddef>
#include <cstdlib>

extern "C" {

rpmbb_store_t rpmbb_store_create(MPI_Comm comm,
                                 const char* pmem_path,
                                 size_t pmem_size) try {
  auto topo = std::make_unique<rpmbb::topology>(rpmbb::mpi::comm{comm, false});
  auto rpm =
      std::make_unique<rpmbb::rpm>(std::cref(*topo), pmem_path, pmem_size);
  auto store = std::make_unique<rpmbb::bb_store>(*rpm);
  auto c_store = static_cast<rpmbb_store_t>(std::malloc(sizeof(rpmbb_store)));
  c_store->topo = topo.release();
  c_store->rpm = rpm.release();
  c_store->store = store.release();
  return c_store;
} catch (...) {
  return nullptr;
}

int rpmbb_store_free(rpmbb_store_t store) {
  delete reinterpret_cast<rpmbb::bb_store*>(store->store);
  delete reinterpret_cast<rpmbb::rpm*>(store->rpm);
  delete reinterpret_cast<rpmbb::topology*>(store->topo);
  free(store);
  return 0;
}

int rpmbb_store_save(rpmbb_store_t store) try {
  auto cpp_store = reinterpret_cast<rpmbb::bb_store*>(store->store);
  cpp_store->save();
  return 0;
} catch (...) {
  return -1;
}

int rpmbb_store_load(rpmbb_store_t store) try {
  auto cpp_store = reinterpret_cast<rpmbb::bb_store*>(store->store);
  cpp_store->load();
  return 0;
} catch (...) {
  return -1;
}

rpmbb_handler_t rpmbb_store_open(rpmbb_store_t store,
                                 MPI_Comm comm,
                                 const char* pathname,
                                 int flags,
                                 mode_t mode) try {
  auto cpp_store = reinterpret_cast<rpmbb::bb_store*>(store->store);
  auto cpp_handler =
      cpp_store->open(rpmbb::mpi::comm{comm, false}, pathname, flags, mode);
  auto handler =
      static_cast<rpmbb_handler_t>(std::malloc(sizeof(rpmbb_handler)));
  handler->handler = cpp_handler.release();
  return handler;
} catch (...) {
  return nullptr;
}

int rpmbb_store_unlink(rpmbb_store_t store, const char* pathname) try {
  auto cpp_store = reinterpret_cast<rpmbb::bb_store*>(store->store);
  cpp_store->unlink(pathname);
  return 0;
} catch (...) {
  return -1;
}

int rpmbb_bb_close(rpmbb_handler_t handler) {
  delete reinterpret_cast<rpmbb::bb_handler*>(handler->handler);
  free(handler);
  return 0;
}

ssize_t rpmbb_bb_pwrite(rpmbb_handler_t handler,
                        const void* buf,
                        size_t count,
                        off_t offset) try {
  auto cpp_handler = reinterpret_cast<rpmbb::bb_handler*>(handler->handler);
  return cpp_handler->pwrite(
      std::span<const std::byte>(static_cast<const std::byte*>(buf), count),
      offset);
} catch (...) {
  return -1;
}

ssize_t rpmbb_bb_pread(rpmbb_handler_t handler,
                       void* buf,
                       size_t count,
                       off_t offset) try {
  auto cpp_handler = reinterpret_cast<rpmbb::bb_handler*>(handler->handler);
  return cpp_handler->pread(
      std::span<std::byte>(static_cast<std::byte*>(buf), count), offset);
} catch (...) {
  return -1;
}

ssize_t rpmbb_bb_pread_aggregate(rpmbb_handler_t handler,
                                 void* buf,
                                 size_t count,
                                 off_t offset) try {
  auto cpp_handler = reinterpret_cast<rpmbb::bb_handler*>(handler->handler);
  return cpp_handler->pread_noflush(
      std::span<std::byte>(static_cast<std::byte*>(buf), count), offset);
} catch (...) {
  return -1;
}

int rpmbb_bb_wait(rpmbb_handler_t handler) try {
  auto cpp_handler = reinterpret_cast<rpmbb::bb_handler*>(handler->handler);
  cpp_handler->flush();
  return 0;
} catch (...) {
  return -1;
}

int rpmbb_bb_sync(rpmbb_handler_t handler) try {
  auto cpp_handler = reinterpret_cast<rpmbb::bb_handler*>(handler->handler);
  cpp_handler->sync();
  return 0;
} catch (const std::exception& e) {
#ifndef NDEBUG
  fprintf(stderr, "rpmbb_bb_sync: %s\n", e.what());
#endif
  return -1;
} catch (...) {
  return -1;
}

int rpmbb_bb_size(rpmbb_handler_t handler, size_t* size) try {
  auto cpp_handler = reinterpret_cast<rpmbb::bb_handler*>(handler->handler);
  *size = cpp_handler->size();
  return 0;
} catch (...) {
  return -1;
}

int rpmbb_bb_truncate(rpmbb_handler_t handler, size_t size) try {
  auto cpp_handler = reinterpret_cast<rpmbb::bb_handler*>(handler->handler);
  cpp_handler->truncate(size);
  return 0;
} catch (...) {
  return -1;
}

}  // extern "C"
