#ifndef RPMBB_H
#define RPMBB_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bb_handler_t {
    void* internal;
} bb_handler_t;


bb_handler_t* bb_handler_create();
void bb_handler_destroy(bb_handler_t* handler);
ssize_t bb_handler_pwrite(bb_handler_t* handler);
ssize_t bb_handler_pread(bb_handler_t* handler);

#ifdef __cplusplus
}
#endif


#endif // RPMBB_H
