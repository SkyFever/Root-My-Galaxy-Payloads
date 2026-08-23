#ifndef T870_PIPE_WAVE_H
#define T870_PIPE_WAVE_H

#include <stddef.h>

#define T870_PIPE_WAVE_COUNT 256U
#define T870_PIPE_WAVE_CAPACITY (4U * 4096U)
#define T870_PIPE_WAVE_PAGE_SIZE 4096U

struct t870_pipe_wave {
    int fds[T870_PIPE_WAVE_COUNT][2];
};

int t870_pipe_wave_prepare(struct t870_pipe_wave *wave, int packetized);
unsigned int t870_pipe_wave_populate(struct t870_pipe_wave *wave,
                                     unsigned int buffer_count);
unsigned int t870_pipe_wave_write_all(struct t870_pipe_wave *wave,
                                      const void *data, size_t size);
unsigned int t870_pipe_wave_probe_packet_decrement(
    struct t870_pipe_wave *wave);
void t870_pipe_wave_release(struct t870_pipe_wave *wave);

#endif
