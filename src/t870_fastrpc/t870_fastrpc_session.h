#ifndef T870_FASTRPC_SESSION_H
#define T870_FASTRPC_SESSION_H

struct t870_fastrpc_session;

/*
 * Opens the same unsigned-CDSP object used by the standalone reproducer and
 * resets its state.  The collision itself is not armed until arm_collision().
 */
struct t870_fastrpc_session *
t870_fastrpc_session_open(const char *dsp_library_directory);

/*
 * Runs the documented map-A/context1/map-B/context2/map-B-unmap sequence.
 * A successful return means context2 is still pending and the stale map is
 * live.  From that point onward the session is dirty and must not be closed.
 */
int t870_fastrpc_session_arm_collision(struct t870_fastrpc_session *session);

/*
 * Performs only SC_RELEASE_CONTEXT2 and joins the retained invoke.  The DSP
 * invoke result is returned through context2_result.
 */
int t870_fastrpc_session_release_stale_map(
    struct t870_fastrpc_session *session, int *context2_result);

int t870_fastrpc_session_is_dirty(
    const struct t870_fastrpc_session *session);

/*
 * Releases a clean session.  It refuses with EBUSY after map B was unmapped
 * unless release_stale_map() completed and joined context2.
 */
int t870_fastrpc_session_close(struct t870_fastrpc_session *session);

#endif
