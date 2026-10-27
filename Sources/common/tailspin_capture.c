/* Copyright (C) 2026, LibreDarwin */
/* SPDX-License-Identifier: BSD-3-Clause */

#include "tailspin_capture.h"
#include "../../../ktrace/Sources/ktrace.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct tailspin_stackshot_config tailspin_stackshot_config_t;

extern tailspin_stackshot_config_t *stackshot_config_create(void);
extern int stackshot_config_set_pid(tailspin_stackshot_config_t *, int);
extern int stackshot_config_set_flags(tailspin_stackshot_config_t *, uint64_t);
extern int stackshot_config_set_delta_timestamp(tailspin_stackshot_config_t *, uint64_t);
extern int stackshot_capture_with_config(tailspin_stackshot_config_t *);
extern void *stackshot_config_get_stackshot_buffer(tailspin_stackshot_config_t *);
extern uint32_t stackshot_config_get_stackshot_size(tailspin_stackshot_config_t *);
extern int stackshot_config_dealloc_buffer(tailspin_stackshot_config_t *);
extern int stackshot_config_dealloc(tailspin_stackshot_config_t *);

enum {
    tailspin_stackshot_kcdata_format = 0x10000,
    tailspin_stackshot_loadinfo = 0x02,
    tailspin_stackshot_global_mem_stats = 0x04,
    tailspin_stackshot_sharedcache_layout = 0x80000,
};

int tailspin_capture_stackshot(const char *path, int pid, uint64_t delta_timestamp,
                               uint64_t *size_out) {
    tailspin_stackshot_config_t *config;
    void *buffer;
    uint32_t size;
    int result;

    config = stackshot_config_create();
    if (config == NULL)
        return ENOMEM;
    result = stackshot_config_set_pid(config, pid);
    if (result == 0)
        result = stackshot_config_set_flags(config, tailspin_stackshot_kcdata_format |
                                            tailspin_stackshot_loadinfo |
                                            tailspin_stackshot_global_mem_stats |
                                            tailspin_stackshot_sharedcache_layout);
    if (result == 0 && delta_timestamp != 0)
        result = stackshot_config_set_delta_timestamp(config, delta_timestamp);
    if (result == 0)
        result = stackshot_capture_with_config(config);
    if (result != 0) {
        stackshot_config_dealloc(config);
        return result;
    }
    buffer = stackshot_config_get_stackshot_buffer(config);
    size = stackshot_config_get_stackshot_size(config);
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        result = errno;
    else if (size == 0 || write(fd, buffer, size) != (ssize_t)size || close(fd) != 0)
        result = errno != 0 ? errno : EIO;
    else {
        result = 0;
        if (size_out != NULL)
            *size_out = size;
    }
    if (result != 0)
        unlink(path);
    stackshot_config_dealloc_buffer(config);
    stackshot_config_dealloc(config);
    return result;
}

int tailspin_capture_stackshot_fd(int fd, int pid, uint64_t delta_timestamp,
                                  uint64_t *size_out) {
    ktrace_session_t session;
    int result;
    char path[64];
    if (fd < 0)
        return EBADF;
    session = ktrace_session_create();
    if (session == NULL)
        return ENOMEM;
    result = ktrace_start_writing_fd(session, 0, fd, 0, 0);
    if (result == 0)
        result = ktrace_start(session);
    if (result != 0) {
        ktrace_session_destroy(session);
        return result;
    }
    snprintf(path, sizeof(path), "/dev/fd/%d", fd);
    result = tailspin_capture_stackshot(path, pid, delta_timestamp, size_out);
    {
        int end_result = ktrace_end(session, 1);
        if (result == 0)
            result = end_result;
    }
    ktrace_session_destroy(session);
    return result;
}
