#ifndef T870_FASTRPC_BRIDGE_ADAPTER_H
#define T870_FASTRPC_BRIDGE_ADAPTER_H

#include "t870_bridge.h"

typedef int (*t870_original_route_fn)(void *opaque);

struct t870_fastrpc_bridge_adapter {
    const char *dsp_library_directory;
    struct t870_bridge_inputs inputs;
    int heap_rw_checkpoint;

    /* Production supplies a thin wrapper around the existing try_cfi_stage. */
    t870_original_route_fn verify_original_route;
    void *original_route_opaque;

    /* Optional payload/supervisor state marker; no retry after stale-map arm. */
    void (*mark_nonretryable)(void *opaque);
    void *nonretryable_opaque;
};

/* Intended as the worker passed to t870_spawn_detached_owner(). */
int t870_fastrpc_bridge_adapter_worker(void *opaque);

#endif
