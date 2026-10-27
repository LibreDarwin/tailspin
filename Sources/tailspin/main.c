/* Copyright (C) 2026, LibreDarwin */
/* SPDX-License-Identifier: BSD-3-Clause */

#include "../common/tailspin_state.h"

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static void usage(FILE *stream) {
    fprintf(stream, "usage: tailspin info|enable|disable|set|reset|save|augment|stat ...\n");
}

static int save_state(tailspin_state_t *state) {
    if (tailspin_state_save(state) != 0) {
        fprintf(stderr, "tailspin: unable to persist configuration: %s\n", strerror(errno));
        return 1;
    }
    return 0;
}

static int command_info(const tailspin_state_t *state) {
    printf("Tailspin: %s\nBuffer size: %llu MB\nOn-core sampling period: %llu ns\n"
           "Full-system sampling period: %llu ns\nCPU trace: %s\n"
           "Ktrace filter: %s\nSampling options: %s\n",
           state->enabled ? "enabled" : "disabled", state->buffer_size_mb,
           state->oncore_period_ns, state->full_period_ns,
           state->cputrace ? "enabled" : "disabled", state->filter, state->sampling);
    return 0;
}

static int command_set(tailspin_state_t *state, int argc, char **argv) {
    if (argc < 2) {
        usage(stderr);
        return 64;
    }
    if (strcmp(argv[0], "buffer-size") == 0) {
        state->buffer_size_mb = strtoull(argv[1], NULL, 10);
    } else if (strcmp(argv[0], "oncore-sampling-period") == 0) {
        state->oncore_period_ns = strcmp(argv[1], "disabled") == 0 ? 0 : strtoull(argv[1], NULL, 10);
    } else if (strcmp(argv[0], "full-system-sampling-period") == 0) {
        state->full_period_ns = strcmp(argv[1], "disabled") == 0 ? 0 : strtoull(argv[1], NULL, 10);
    } else if (strcmp(argv[0], "cputrace") == 0) {
        state->cputrace = atoi(argv[1]) != 0;
    } else if (strcmp(argv[0], "ktrace-filter-descriptor") == 0) {
        free(state->filter);
        state->filter = strdup(argv[1]);
    } else if (strcmp(argv[0], "sampling-option") == 0) {
        free(state->sampling);
        state->sampling = strdup(argv[1]);
    } else {
        fprintf(stderr, "tailspin: unknown setting: %s\n", argv[0]);
        return 64;
    }
    return save_state(state);
}

static int command_save(int argc, char **argv) {
    int option;
    const char *reason = NULL;
    const char *path = NULL;
    while ((option = getopt(argc, argv, "r:l:n")) != -1) {
        if (option == 'r') reason = optarg;
        else if (option == 'l') (void)strtoull(optarg, NULL, 10);
        else if (option == 'n') continue;
        else return 64;
    }
    if (optind < argc) path = argv[optind];
    if (path == NULL) {
        fprintf(stderr, "tailspin: save requires a path\n");
        return 64;
    }
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        fprintf(stderr, "tailspin: unable to save %s: %s\n", path, strerror(errno));
        return 1;
    }
    fprintf(file, "tailspin\nreason=%s\ntimestamp=%lld\n", reason ? reason : "", (long long)time(NULL));
    fclose(file);
    return 0;
}

int main(int argc, char **argv) {
    tailspin_state_t state;
    int result = 0;
    tailspin_state_init(&state);
    if (tailspin_state_load(&state) != 0) {
        fprintf(stderr, "tailspin: unable to read configuration\n");
        result = 1;
        goto out;
    }
    if (argc < 2) {
        usage(stderr);
        result = 64;
    } else if (strcmp(argv[1], "info") == 0) {
        result = command_info(&state);
    } else if (strcmp(argv[1], "enable") == 0 || strcmp(argv[1], "disable") == 0) {
        state.enabled = strcmp(argv[1], "enable") == 0;
        result = save_state(&state);
    } else if (strcmp(argv[1], "set") == 0) {
        result = command_set(&state, argc - 2, argv + 2);
    } else if (strcmp(argv[1], "reset") == 0) {
        tailspin_state_reset(&state, argc > 2 ? argv[2] : NULL);
        result = save_state(&state);
    } else if (strcmp(argv[1], "save") == 0) {
        result = command_save(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "augment") == 0 || strcmp(argv[1], "stat") == 0) {
        if (argc < 3) result = 64;
        else result = 0;
    } else {
        usage(stderr);
        result = 64;
    }
out:
    tailspin_state_free(&state);
    return result;
}
