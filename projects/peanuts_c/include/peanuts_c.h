#ifndef PEANUTS_C_H
#define PEANUTS_C_H

#ifdef __cplusplus
extern "C" {
#endif

#include <mpi.h>
#include <sys/types.h>

struct peanuts_store {
  void* topo;
  void* rpm;
  void* store;
};

struct peanuts_handler {
  void* handler;
};

typedef struct peanuts_store* peanuts_store_t;
typedef struct peanuts_handler* peanuts_handler_t;

peanuts_store_t peanuts_store_create(MPI_Comm comm,
                                     const char* pmem_path,
                                     size_t pmem_size);
int peanuts_store_free(peanuts_store_t store);
int peanuts_store_save(peanuts_store_t store);
int peanuts_store_load(peanuts_store_t store);

peanuts_handler_t peanuts_store_open(peanuts_store_t store,
                                     MPI_Comm comm,
                                     const char* pathname,
                                     int flags,
                                     mode_t mode);
int peanuts_store_unlink(peanuts_store_t store, const char* pathname);

int peanuts_bb_close(peanuts_handler_t handler);
ssize_t peanuts_bb_pwrite(peanuts_handler_t handler,
                          const void* buf,
                          size_t count,
                          off_t offset);
ssize_t peanuts_bb_pread(peanuts_handler_t handler,
                         void* buf,
                         size_t count,
                         off_t offset);

ssize_t peanuts_bb_pread_aggregate(peanuts_handler_t handler,
                                   void* buf,
                                   size_t count,
                                   off_t offset);
int peanuts_bb_wait(peanuts_handler_t handler);
int peanuts_bb_sync(peanuts_handler_t handler);
int peanuts_bb_size(peanuts_handler_t handler, size_t* size);
int peanuts_bb_truncate(peanuts_handler_t handler, size_t size);

#ifdef __cplusplus
}
#endif

#endif  // PEANUTS_C_H
