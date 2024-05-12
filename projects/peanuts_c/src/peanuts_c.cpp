#include "peanuts_c.h"
#include "peanuts/bb.hpp"

#include <mpi.h>
#include <sys/types.h>

#include <cstddef>
#include <cstdlib>

extern "C" {

peanuts_store_t peanuts_store_create(MPI_Comm comm,
                                     const char* pmem_path,
                                     size_t pmem_size) try {
  auto topo =
      std::make_unique<peanuts::topology>(peanuts::mpi::comm{comm, false});
  auto rpm =
      std::make_unique<peanuts::rpm>(std::cref(*topo), pmem_path, pmem_size);
  auto store = std::make_unique<peanuts::bb_store>(*rpm);
  auto c_store =
      static_cast<peanuts_store_t>(std::malloc(sizeof(peanuts_store)));
  c_store->topo = topo.release();
  c_store->rpm = rpm.release();
  c_store->store = store.release();
  return c_store;
} catch (...) {
  return nullptr;
}

int peanuts_store_free(peanuts_store_t store) {
  delete reinterpret_cast<peanuts::bb_store*>(store->store);
  delete reinterpret_cast<peanuts::rpm*>(store->rpm);
  delete reinterpret_cast<peanuts::topology*>(store->topo);
  free(store);
  return 0;
}

int peanuts_store_save(peanuts_store_t store) try {
  auto cpp_store = reinterpret_cast<peanuts::bb_store*>(store->store);
  cpp_store->save();
  return 0;
} catch (...) {
  return -1;
}

int peanuts_store_load(peanuts_store_t store) try {
  auto cpp_store = reinterpret_cast<peanuts::bb_store*>(store->store);
  cpp_store->load();
  return 0;
} catch (...) {
  return -1;
}

peanuts_handler_t peanuts_store_open(peanuts_store_t store,
                                     MPI_Comm comm,
                                     const char* pathname,
                                     int flags,
                                     mode_t mode) try {
  auto cpp_store = reinterpret_cast<peanuts::bb_store*>(store->store);
  auto cpp_handler =
      cpp_store->open(peanuts::mpi::comm{comm, false}, pathname, flags, mode);
  auto handler =
      static_cast<peanuts_handler_t>(std::malloc(sizeof(peanuts_handler)));
  handler->handler = cpp_handler.release();
  return handler;
} catch (...) {
  return nullptr;
}

int peanuts_store_unlink(peanuts_store_t store, const char* pathname) try {
  auto cpp_store = reinterpret_cast<peanuts::bb_store*>(store->store);
  cpp_store->unlink(pathname);
  return 0;
} catch (...) {
  return -1;
}

int peanuts_bb_close(peanuts_handler_t handler) {
  delete reinterpret_cast<peanuts::bb_handler*>(handler->handler);
  free(handler);
  return 0;
}

ssize_t peanuts_bb_pwrite(peanuts_handler_t handler,
                          const void* buf,
                          size_t count,
                          off_t offset) try {
  auto cpp_handler = reinterpret_cast<peanuts::bb_handler*>(handler->handler);
  return cpp_handler->pwrite(
      std::span<const std::byte>(static_cast<const std::byte*>(buf), count),
      offset);
} catch (...) {
  return -1;
}

ssize_t peanuts_bb_pread(peanuts_handler_t handler,
                         void* buf,
                         size_t count,
                         off_t offset) try {
  auto cpp_handler = reinterpret_cast<peanuts::bb_handler*>(handler->handler);
  return cpp_handler->pread(
      std::span<std::byte>(static_cast<std::byte*>(buf), count), offset);
} catch (...) {
  return -1;
}

ssize_t peanuts_bb_pread_aggregate(peanuts_handler_t handler,
                                   void* buf,
                                   size_t count,
                                   off_t offset) try {
  auto cpp_handler = reinterpret_cast<peanuts::bb_handler*>(handler->handler);
  return cpp_handler->pread_noflush(
      std::span<std::byte>(static_cast<std::byte*>(buf), count), offset);
} catch (...) {
  return -1;
}

int peanuts_bb_wait(peanuts_handler_t handler) try {
  auto cpp_handler = reinterpret_cast<peanuts::bb_handler*>(handler->handler);
  cpp_handler->flush();
  return 0;
} catch (...) {
  return -1;
}

int peanuts_bb_sync(peanuts_handler_t handler) try {
  auto cpp_handler = reinterpret_cast<peanuts::bb_handler*>(handler->handler);
  cpp_handler->sync();
  return 0;
} catch (const std::exception& e) {
#ifndef NDEBUG
  fprintf(stderr, "peanuts_bb_sync: %s\n", e.what());
#endif
  return -1;
} catch (...) {
  return -1;
}

int peanuts_bb_size(peanuts_handler_t handler, size_t* size) try {
  auto cpp_handler = reinterpret_cast<peanuts::bb_handler*>(handler->handler);
  *size = cpp_handler->size();
  return 0;
} catch (...) {
  return -1;
}

int peanuts_bb_truncate(peanuts_handler_t handler, size_t size) try {
  auto cpp_handler = reinterpret_cast<peanuts::bb_handler*>(handler->handler);
  cpp_handler->truncate(size);
  return 0;
} catch (...) {
  return -1;
}

}  // extern "C"
