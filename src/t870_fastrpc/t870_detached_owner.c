#define _GNU_SOURCE

#include "t870_detached_owner.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define T870_OWNER_RESULT_MAGIC 0x5438374fU

struct t870_owner_result {
    uint32_t magic;
    int32_t status;
    int32_t pid;
    uint32_t detached;
};

static int current_thread_count(void)
{
    DIR *tasks = opendir("/proc/self/task");
    struct dirent *entry;
    int count = 0;

    if (tasks == NULL)
        return -1;
    errno = 0;
    while ((entry = readdir(tasks)) != NULL) {
        const char *name = entry->d_name;
        int numeric = *name != '\0';

        while (*name != '\0') {
            if (*name < '0' || *name > '9') {
                numeric = 0;
                break;
            }
            ++name;
        }
        if (numeric)
            ++count;
    }
    if (errno != 0) {
        int saved_errno = errno;

        closedir(tasks);
        errno = saved_errno;
        return -1;
    }
    if (closedir(tasks) != 0)
        return -1;
    return count;
}

static int write_full(int fd, const void *data, size_t size)
{
    const unsigned char *cursor = data;

    while (size != 0U) {
        ssize_t written = write(fd, cursor, size);

        if (written < 0 && errno == EINTR)
            continue;
        if (written <= 0)
            return -1;
        cursor += written;
        size -= (size_t)written;
    }
    return 0;
}

static int read_full(int fd, void *data, size_t size)
{
    unsigned char *cursor = data;

    while (size != 0U) {
        ssize_t got = read(fd, cursor, size);

        if (got < 0 && errno == EINTR)
            continue;
        if (got <= 0)
            return -1;
        cursor += got;
        size -= (size_t)got;
    }
    return 0;
}

static int detach_stdio(void)
{
    int null_fd = open("/dev/null", O_RDWR | O_CLOEXEC);
    int fd;

    if (null_fd < 0)
        return -1;
    for (fd = STDIN_FILENO; fd <= STDERR_FILENO; ++fd) {
        if (null_fd != fd && dup3(null_fd, fd, 0) < 0) {
            int saved_errno = errno;

            if (null_fd > STDERR_FILENO)
                close(null_fd);
            errno = saved_errno;
            return -1;
        }
    }
    if (null_fd > STDERR_FILENO)
        close(null_fd);
    return 0;
}

static void child_main(int result_fd, pid_t expected_parent,
                       t870_owner_worker_fn worker, void *opaque)
{
    struct t870_owner_result result = {
        .magic = T870_OWNER_RESULT_MAGIC,
        .status = -1,
        .pid = (int32_t)getpid(),
        .detached = 0U,
    };
    int status;

    if (result_fd <= STDERR_FILENO) {
        int moved_fd = fcntl(result_fd, F_DUPFD_CLOEXEC,
                             STDERR_FILENO + 1);

        if (moved_fd < 0)
            _exit(1);
        close(result_fd);
        result_fd = moved_fd;
    }

    if (prctl(PR_SET_PDEATHSIG, SIGKILL) != 0 ||
        getppid() != expected_parent) {
        result.status = errno != 0 ? errno : ECHILD;
        (void)write_full(result_fd, &result, sizeof(result));
        _exit(result.status);
    }

    status = worker(opaque);
    result.status = status;
    if (status != 0 && status != T870_OWNER_DIRTY_FAILURE) {
        (void)write_full(result_fd, &result, sizeof(result));
        _exit(status > 0 && status < 256 ? status : 1);
    }

    fflush(NULL);
    if (prctl(PR_SET_PDEATHSIG, 0) != 0 || setsid() < 0 ||
        detach_stdio() != 0) {
        result.status = errno != 0 ? errno : EPERM;
        (void)write_full(result_fd, &result, sizeof(result));
        _exit(1);
    }
    result.detached = 1U;
    if (write_full(result_fd, &result, sizeof(result)) != 0)
        _exit(1);
    close(result_fd);

    for (;;)
        pause();
}

int t870_spawn_detached_owner(t870_owner_worker_fn worker, void *opaque,
                              unsigned int timeout_ms, pid_t *owner_pid)
{
    struct t870_owner_result result;
    struct pollfd poll_fd;
    int result_pipe[2];
    pid_t parent = getpid();
    pid_t child;
    int status;
    int wait_status;

    if (worker == NULL || owner_pid == NULL || timeout_ms == 0U ||
        timeout_ms > (unsigned int)INT_MAX) {
        errno = EINVAL;
        return -1;
    }
    *owner_pid = -1;
    status = current_thread_count();
    if (status < 0)
        return -1;
    if (status != 1) {
        errno = EBUSY;
        return -1;
    }
    if (pipe2(result_pipe, O_CLOEXEC) != 0)
        return -1;
    child = fork();
    if (child < 0) {
        int saved_errno = errno;

        close(result_pipe[0]);
        close(result_pipe[1]);
        errno = saved_errno;
        return -1;
    }
    if (child == 0) {
        close(result_pipe[0]);
        child_main(result_pipe[1], parent, worker, opaque);
    }

    close(result_pipe[1]);
    poll_fd.fd = result_pipe[0];
    poll_fd.events = POLLIN | POLLHUP;
    poll_fd.revents = 0;
    do {
        status = poll(&poll_fd, 1, (int)timeout_ms);
    } while (status < 0 && errno == EINTR);
    if (status <= 0 || read_full(result_pipe[0], &result, sizeof(result)) != 0) {
        int saved_errno = status == 0 ? ETIMEDOUT : errno;

        close(result_pipe[0]);
        (void)kill(child, SIGKILL);
        do {
            status = waitpid(child, &wait_status, 0);
        } while (status < 0 && errno == EINTR);
        errno = saved_errno != 0 ? saved_errno : EPROTO;
        return -1;
    }
    close(result_pipe[0]);
    if (result.magic != T870_OWNER_RESULT_MAGIC || result.pid != child) {
        (void)kill(child, SIGKILL);
        do {
            status = waitpid(child, &wait_status, 0);
        } while (status < 0 && errno == EINTR);
        errno = EPROTO;
        return -1;
    }
    if (result.detached != 1U) {
        do {
            status = waitpid(child, &wait_status, 0);
        } while (status < 0 && errno == EINTR);
        return result.status != 0 ? result.status : -1;
    }
    *owner_pid = child;
    return result.status;
}
