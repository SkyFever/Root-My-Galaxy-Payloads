#ifndef T870_DETACHED_OWNER_H
#define T870_DETACHED_OWNER_H

#include <sys/types.h>

typedef int (*t870_owner_worker_fn)(void *opaque);

/* Reserved worker result: report failure but retain dirty kernel resources. */
#define T870_OWNER_DIRTY_FAILURE 125

/*
 * Runs worker in a child that remains tied to the caller while worker is in
 * progress. Only a zero worker result clears the parent-death signal,
 * creates a new session, detaches stdio, reports success, and parks the child.
 *
 * Returns zero after a successful detached owner is confirmed. A worker may
 * return T870_OWNER_DIRTY_FAILURE after a non-recoverable kernel transition;
 * that result also detaches and parks the owner, but is returned to the caller
 * as failure so the supervisor will not retry. Both retained outcomes write
 * the owner PID. Other positive worker failures exit and are reaped. Returns
 * -1 for a protocol/system error with errno set.
 */
int t870_spawn_detached_owner(t870_owner_worker_fn worker, void *opaque,
                              unsigned int timeout_ms, pid_t *owner_pid);

#endif
