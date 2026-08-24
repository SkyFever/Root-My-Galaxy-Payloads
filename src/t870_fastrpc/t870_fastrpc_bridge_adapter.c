#define _GNU_SOURCE

#include "t870_fastrpc_bridge_adapter.h"

#include "t870_detached_owner.h"
#include "t870_fastrpc_session.h"

#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>

struct adapter_state {
    const struct t870_fastrpc_bridge_adapter *configuration;
    struct t870_fastrpc_session *session;
    int nonretryable;
};

static int pin_worker_to_lowest_allowed_cpu(void)
{
    cpu_set_t allowed;
    cpu_set_t selected_mask;
    int allowed_count = 0;
    int previous_cpu;
    int selected = -1;
    int cpu;

    CPU_ZERO(&allowed);
    if (sched_getaffinity(0, sizeof(allowed), &allowed) != 0) {
        fprintf(stderr, "[-] bridge sched_getaffinity: %s\n",
                strerror(errno));
        return -1;
    }
    previous_cpu = sched_getcpu();
    for (cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
        if (!CPU_ISSET(cpu, &allowed))
            continue;
        ++allowed_count;
        if (selected < 0)
            selected = cpu;
    }
    if (selected < 0) {
        errno = EINVAL;
        fputs("[-] bridge affinity mask contains no allowed CPU\n", stderr);
        return -1;
    }

    CPU_ZERO(&selected_mask);
    CPU_SET(selected, &selected_mask);
    if (sched_setaffinity(0, sizeof(selected_mask), &selected_mask) != 0) {
        fprintf(stderr, "[-] bridge sched_setaffinity cpu=%d: %s\n",
                selected, strerror(errno));
        return -1;
    }
    printf("[+] bridge CPU-local reclaim pinned lowest_allowed_cpu=%d "
           "previous_cpu=%d allowed_count=%d\n", selected, previous_cpu,
           allowed_count);
    return selected;
}

static int arm_stale_map(void *opaque)
{
    struct adapter_state *state = opaque;
    int result;

    printf("[*] bridge CPU-local stage=collision-arm-enter cpu=%d\n",
           sched_getcpu());
    result = t870_fastrpc_session_arm_collision(state->session);
    printf("[*] bridge CPU-local stage=collision-arm-return cpu=%d\n",
           sched_getcpu());

    if (result != 0 && t870_fastrpc_session_is_dirty(state->session)) {
        fputs("[-] FastRPC arm failed after dirty transition; retaining "
              "owner\n", stderr);
        state->nonretryable = 1;
        return T870_OWNER_DIRTY_FAILURE;
    }
    return result;
}

static int release_stale_map(void *opaque)
{
    struct adapter_state *state = opaque;
    int context2_result = -1;

    printf("[*] bridge CPU-local stage=context2-release-enter cpu=%d\n",
           sched_getcpu());
    if (t870_fastrpc_session_release_stale_map(
            state->session, &context2_result) != 0)
        return -1;
    printf("[*] bridge CPU-local stage=context2-release-return cpu=%d\n",
           sched_getcpu());
    if (context2_result != 0) {
        fprintf(stderr, "[-] FastRPC context2 returned ret=%d\n",
                context2_result);
        errno = EIO;
        return -1;
    }
    return 0;
}

static int verify_original_route(void *opaque)
{
    struct adapter_state *state = opaque;

    return state->configuration->verify_original_route(
        state->configuration->original_route_opaque);
}

static void mark_nonretryable(void *opaque)
{
    struct adapter_state *state = opaque;

    state->nonretryable = 1;
    if (state->configuration->mark_nonretryable != NULL) {
        state->configuration->mark_nonretryable(
            state->configuration->nonretryable_opaque);
    }
}

int t870_fastrpc_bridge_adapter_worker(void *opaque)
{
    const struct t870_fastrpc_bridge_adapter *configuration = opaque;
    struct adapter_state state = {0};
    struct t870_bridge_context bridge = {0};
    int result;

    if (configuration == NULL ||
        configuration->dsp_library_directory == NULL ||
        configuration->dsp_library_directory[0] == '\0' ||
        configuration->verify_original_route == NULL) {
        return EINVAL;
    }
    if (pin_worker_to_lowest_allowed_cpu() < 0)
        return errno != 0 && errno < T870_OWNER_DIRTY_FAILURE ? errno : EIO;
    state.configuration = configuration;
    state.session = t870_fastrpc_session_open(
        configuration->dsp_library_directory);
    if (state.session == NULL)
        return errno != 0 && errno < T870_OWNER_DIRTY_FAILURE ? errno : EIO;

    bridge.inputs = configuration->inputs;
    bridge.hooks.arm_stale_map = arm_stale_map;
    bridge.hooks.release_stale_map = release_stale_map;
    bridge.hooks.verify_original_route = verify_original_route;
    bridge.hooks.mark_nonretryable = mark_nonretryable;
    bridge.hooks.opaque = &state;
    result = t870_bridge_worker(&bridge);

    if (result == 0 || result == T870_OWNER_DIRTY_FAILURE ||
        state.nonretryable) {
        /* The detached child owns the session and all bridge allocations. */
        return result == 0 ? 0 : T870_OWNER_DIRTY_FAILURE;
    }
    if (t870_fastrpc_session_close(state.session) != 0) {
        fputs("[-] clean bridge failure left a dirty FastRPC session\n",
              stderr);
        return T870_OWNER_DIRTY_FAILURE;
    }
    return result;
}
