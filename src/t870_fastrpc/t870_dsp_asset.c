#define _GNU_SOURCE

#include "t870_dsp_asset.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define T870_APP_FILES_PREFIX \
    "/data/user/0/dev.busung.s25uroot/files/"
#define T870_DSP_FILENAME "libt870_map_collision_skel.so"

extern const unsigned char t870_dsp_blob_start[];
extern const unsigned char t870_dsp_blob_end[];

static int write_full(int fd, const unsigned char *data, size_t size)
{
    while (size != 0U) {
        ssize_t written = write(fd, data, size);

        if (written < 0 && errno == EINTR)
            continue;
        if (written <= 0)
            return -1;
        data += written;
        size -= (size_t)written;
    }
    return 0;
}

int t870_prepare_dsp_asset(char *directory, size_t directory_size)
{
    Dl_info information;
    const char *slash;
    char library_path[PATH_MAX] = {0};
    size_t parent_length;
    size_t blob_size = (size_t)(t870_dsp_blob_end - t870_dsp_blob_start);
    int fd = -1;

    if (directory == NULL || directory_size == 0U || blob_size == 0U) {
        errno = EINVAL;
        return -1;
    }
    memset(&information, 0, sizeof(information));
    if (dladdr((const void *)&t870_prepare_dsp_asset, &information) == 0 ||
        information.dli_fname == NULL) {
        errno = ENOENT;
        return -1;
    }
    if (strncmp(information.dli_fname, T870_APP_FILES_PREFIX,
                sizeof(T870_APP_FILES_PREFIX) - 1U) != 0) {
        fprintf(stderr, "[-] refusing DSP extraction outside app files: "
                "%s\n", information.dli_fname);
        errno = EPERM;
        return -1;
    }
    slash = strrchr(information.dli_fname, '/');
    if (slash == NULL) {
        errno = EINVAL;
        return -1;
    }
    parent_length = (size_t)(slash - information.dli_fname);
    if (snprintf(directory, directory_size, "%.*s/t870-dsp-%d",
                 (int)parent_length, information.dli_fname,
                 getpid()) >= (int)directory_size) {
        errno = ENAMETOOLONG;
        return -1;
    }
    if (mkdir(directory, 0700) != 0)
        return -1;
    if (snprintf(library_path, sizeof(library_path), "%s/%s", directory,
                 T870_DSP_FILENAME) >= (int)sizeof(library_path)) {
        errno = ENAMETOOLONG;
        goto fail;
    }
    fd = open(library_path,
              O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
              0600);
    if (fd < 0)
        goto fail;
    if (write_full(fd, t870_dsp_blob_start, blob_size) != 0 ||
        fsync(fd) != 0 || close(fd) != 0) {
        int saved_errno = errno;

        if (fd >= 0)
            (void)close(fd);
        fd = -1;
        errno = saved_errno;
        goto fail;
    }
    fd = -1;
    printf("[+] embedded T870 DSP asset ready dir=%s size=%zu\n",
           directory, blob_size);
    return 0;

fail:
    {
        int saved_errno = errno;

        if (fd >= 0)
            (void)close(fd);
        if (library_path[0] != '\0')
            (void)unlink(library_path);
        (void)rmdir(directory);
        errno = saved_errno;
    }
    return -1;
}
