#ifndef T870_CMSG_WAVE_H
#define T870_CMSG_WAVE_H

#include "t870_reclaim_layout.h"

#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>

#define T870_CMSG_WAVE_SENDERS 256U
#define T870_CMSG_WAVE_FILLERS 4U

struct t870_cmsg_job {
    int sender_fd;
    struct sockaddr_un receiver_address;
    socklen_t receiver_address_length;
    unsigned char control[T870_CMSG_CONTROL_SIZE];
    pthread_t thread;
    int result;
    int send_errno;
    volatile int entered;
    volatile int done;
};

struct t870_cmsg_wave {
    int receiver_fd;
    int filler_fds[T870_CMSG_WAVE_FILLERS];
    struct sockaddr_un receiver_address;
    socklen_t receiver_address_length;
    struct t870_cmsg_job jobs[T870_CMSG_WAVE_SENDERS];
    unsigned int started;
};

int t870_cmsg_wave_prepare(
    struct t870_cmsg_wave *wave,
    const unsigned char control_template[T870_CMSG_CONTROL_SIZE],
    const char *wave_label);
unsigned int t870_cmsg_wave_start(struct t870_cmsg_wave *wave);
void t870_cmsg_wave_release(struct t870_cmsg_wave *wave);

#endif
