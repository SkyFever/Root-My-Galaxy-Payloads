#include "t870_fastrpc_bridge_adapter.h"

#include "t870_detached_owner.h"
#include "t870_fastrpc_session.h"

#include <errno.h>
#include <stdio.h>

struct adapter_state {
    const struct t870_fastrpc_bridge_adapter *configuration;
    struct t870_fastrpc_session *session;
    int nonretryable;
};

static int arm_stale_map(void *opaque)
{
    struct adapter_state *state = opaque;
    int result = t870_fastrpc_session_arm_collision(state->session);

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

    if (t870_fastrpc_session_release_stale_map(
            state->session, &context2_result) != 0)
        return -1;
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
