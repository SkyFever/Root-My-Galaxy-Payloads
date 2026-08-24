#ifndef T870_BRIDGE_H
#define T870_BRIDGE_H

#include <stdint.h>

struct t870_bridge_inputs {
    uint64_t payload_base;
    uint64_t fake_fops;
    uint64_t misc_fops_direct;
    uint64_t misc_fops_page;
    uint64_t anon_pipe_buf_ops;
};

struct t870_bridge_hooks {
    int (*arm_stale_map)(void *opaque);
    int (*release_stale_map)(void *opaque);
    int (*verify_original_route)(void *opaque);
    void (*mark_nonretryable)(void *opaque);
    void *opaque;
};

struct t870_bridge_context {
    struct t870_bridge_inputs inputs;
    struct t870_bridge_hooks hooks;
    int heap_rw_checkpoint;
};

/* Intended as the worker passed to t870_spawn_detached_owner(). */
int t870_bridge_worker(void *opaque);

#endif
