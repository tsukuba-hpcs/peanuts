#ifndef RPMBB_H
#define RPMBB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <mpi.h>
#include <sys/types.h>

struct rpmbb_store {
  void* topo;
  void* rpm;
  void* store;
};

struct rpmbb_handler {
  void* handler;
};

typedef struct rpmbb_store* rpmbb_store_t;
typedef struct rpmbb_handler* rpmbb_handler_t;

rpmbb_store_t rpmbb_store_create(MPI_Comm comm,
                                 const char* pmem_path,
                                 size_t pmem_size);
int rpmbb_store_free(rpmbb_store_t store);
int rpmbb_store_save(rpmbb_store_t store);
int rpmbb_store_load(rpmbb_store_t store);

rpmbb_handler_t rpmbb_store_open_attach(rpmbb_store_t store,
                                        MPI_Comm comm,
                                        int fd);
int rpmbb_store_unlink(rpmbb_store_t store, int fd);

int rpmbb_bb_close(rpmbb_handler_t handler);
ssize_t rpmbb_bb_pwrite(rpmbb_handler_t handler,
                        const void* buf,
                        size_t count,
                        off_t offset);
ssize_t rpmbb_bb_pread(rpmbb_handler_t handler,
                       void* buf,
                       size_t count,
                       off_t offset);

ssize_t rpmbb_bb_pread_aggregate(rpmbb_handler_t handler,
                                 void* buf,
                                 size_t count,
                                 off_t offset);
int rpmbb_bb_wait(rpmbb_handler_t handler);
int rpmbb_bb_sync(rpmbb_handler_t handler);
int rpmbb_bb_size(rpmbb_handler_t handler, size_t* size);

#ifdef __cplusplus
}
#endif

#endif  // RPMBB_H
