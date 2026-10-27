/* Copyright (C) 2026, LibreDarwin */
/* SPDX-License-Identifier: BSD-3-Clause */

#include "tailspin_state.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const unsigned long long default_buffer = 32;
static const unsigned long long default_oncore = 1000000;
static const unsigned long long default_full = 10000000;

static char *copy_string(const char *value) {
    return value == NULL ? NULL : strdup(value);
}

static void replace_string(char **slot, const char *value) {
    free(*slot);
    *slot = copy_string(value);
}

static int valid_state(const tailspin_state_t *state) {
    return state->buffer_size_mb != 0 &&
           (state->oncore_period_ns == 0 || state->oncore_period_ns >= 1000000) &&
           (state->full_period_ns == 0 || state->full_period_ns >= 10000000) &&
           (state->cputrace == 0 || state->cputrace == 1);
}

const char *tailspin_state_path(void) {
    const char *path = getenv("TAILSPIN_CONFIG_PATH");
    return path != NULL && path[0] != '\0' ? path : "/var/db/tailspin/config";
}

void tailspin_state_init(tailspin_state_t *state) {
    memset(state, 0, sizeof(*state));
    state->buffer_size_mb = default_buffer;
    state->oncore_period_ns = default_oncore;
    state->full_period_ns = default_full;
    state->filter = copy_string("ALL");
    state->sampling = copy_string("cswitch-sampling,syscall-sampling,vmfault-sampling");
}

void tailspin_state_free(tailspin_state_t *state) {
    free(state->filter);
    free(state->sampling);
    memset(state, 0, sizeof(*state));
}

static int parse_line(tailspin_state_t *state, char *line) {
    char *equals = strchr(line, '=');
    char *value;
    if (equals == NULL)
        return 0;
    *equals = '\0';
    value = equals + 1;
    if (strcmp(line, "enabled") == 0)
        state->enabled = atoi(value) != 0;
    else if (strcmp(line, "buffer-size-mb") == 0)
        state->buffer_size_mb = strtoull(value, NULL, 10);
    else if (strcmp(line, "oncore-sampling-period") == 0)
        state->oncore_period_ns = strtoull(value, NULL, 10);
    else if (strcmp(line, "full-system-sampling-period") == 0)
        state->full_period_ns = strtoull(value, NULL, 10);
    else if (strcmp(line, "cputrace") == 0) {
        if (strcmp(value, "0") == 0)
            state->cputrace = 0;
        else if (strcmp(value, "1") == 0)
            state->cputrace = 1;
        else
            return -1;
    }
    else if (strcmp(line, "ktrace-filter-descriptor") == 0)
        replace_string(&state->filter, value);
    else if (strcmp(line, "sampling-option") == 0)
        replace_string(&state->sampling, value);
    return 0;
}

int tailspin_state_load_path(tailspin_state_t *state, const char *path) {
    FILE *file = fopen(path, "r");
    char line[4096];
    if (file == NULL)
        return errno == ENOENT ? 0 : -1;
    while (fgets(line, sizeof(line), file) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        if (parse_line(state, line) != 0) {
            fclose(file);
            errno = EINVAL;
            return -1;
        }
    }
    fclose(file);
    if (!valid_state(state)) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

int tailspin_state_load(tailspin_state_t *state) {
    return tailspin_state_load_path(state, tailspin_state_path());
}

int tailspin_state_save(const tailspin_state_t *state) {
    const char *path = tailspin_state_path();
    char *copy = strdup(path);
    char *slash;
    FILE *file;
    if (copy == NULL)
        return -1;
    slash = strrchr(copy, '/');
    if (slash != NULL) {
        *slash = '\0';
        if (mkdir(copy, 0755) != 0 && errno != EEXIST) {
            free(copy);
            return -1;
        }
    }
    free(copy);
    file = fopen(path, "w");
    if (file == NULL)
        return -1;
    fprintf(file, "enabled=%d\nbuffer-size-mb=%llu\noncore-sampling-period=%llu\n"
                  "full-system-sampling-period=%llu\ncputrace=%d\n"
                  "ktrace-filter-descriptor=%s\nsampling-option=%s\n",
            state->enabled, state->buffer_size_mb, state->oncore_period_ns,
            state->full_period_ns, state->cputrace, state->filter, state->sampling);
    if (fclose(file) != 0)
        return -1;
    return 0;
}

void tailspin_state_reset(tailspin_state_t *state, const char *field) {
    if (field == NULL) {
        int enabled = state->enabled;
        tailspin_state_free(state);
        tailspin_state_init(state);
        state->enabled = enabled;
    } else if (strcmp(field, "buffer-size") == 0) {
        state->buffer_size_mb = default_buffer;
    } else if (strcmp(field, "ktrace-filter-descriptor") == 0) {
        replace_string(&state->filter, "ALL");
    } else if (strcmp(field, "oncore-sampling-period") == 0) {
        state->oncore_period_ns = default_oncore;
    } else if (strcmp(field, "full-system-sampling-period") == 0) {
        state->full_period_ns = default_full;
    } else if (strcmp(field, "sampling-option") == 0) {
        replace_string(&state->sampling, "cswitch-sampling,syscall-sampling,vmfault-sampling");
    }
}
