/* Copyright (C) 2026, LibreDarwin */
/* SPDX-License-Identifier: BSD-3-Clause */

#include "../common/tailspin_state.h"
#include "../common/tailspin_capture.h"

#include <errno.h>
#include <getopt.h>
#include <mach/mach_time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

static int parse_period(const char *value, unsigned long long minimum,
                        unsigned long long *result) {
    char *end;
    unsigned long long parsed;
    errno = 0;
    parsed = strtoull(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed < minimum) {
        fprintf(stderr, "tailspin: sampling period must be at least %llu ns\n", minimum);
        return 64;
    }
    *result = parsed;
    return 0;
}

static int update_list(char **list, const char *modifier) {
    const char *value = modifier;
    int remove = 0;
    char *copy;
    char *cursor;
    char *token;
    char *result;
    size_t result_length = 0;
    if (strncmp(modifier, "add:", 4) == 0) {
        value += 4;
    } else if (strncmp(modifier, "remove:", 7) == 0) {
        remove = 1;
        value += 7;
    }
    if (*value == '\0')
        return 64;
    copy = strdup(*list);
    if (copy == NULL)
        return 1;
    result = calloc(1, strlen(copy) + strlen(value) + 2);
    if (result == NULL) {
        free(copy);
        return 1;
    }
    cursor = copy;
    while ((token = strsep(&cursor, ",")) != NULL) {
        if (strcmp(token, value) == 0) {
            if (remove)
                continue;
            free(result);
            free(copy);
            return 0;
        }
        if (*token == '\0')
            continue;
        if (result_length != 0)
            result[result_length++] = ',';
        strcpy(result + result_length, token);
        result_length += strlen(token);
    }
    if (!remove) {
        if (result_length != 0)
            result[result_length++] = ',';
        strcpy(result + result_length, value);
    }
    free(copy);
    free(*list);
    *list = result;
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
        char *end;
        errno = 0;
        state->buffer_size_mb = strtoull(argv[1], &end, 10);
        if (errno != 0 || end == argv[1] || *end != '\0' || state->buffer_size_mb == 0)
            return 64;
    } else if (strcmp(argv[0], "oncore-sampling-period") == 0) {
        if (strcmp(argv[1], "disabled") == 0) state->oncore_period_ns = 0;
        else if (parse_period(argv[1], 1000000, &state->oncore_period_ns) != 0) return 64;
    } else if (strcmp(argv[0], "full-system-sampling-period") == 0) {
        if (strcmp(argv[1], "disabled") == 0) state->full_period_ns = 0;
        else if (parse_period(argv[1], 10000000, &state->full_period_ns) != 0) return 64;
    } else if (strcmp(argv[0], "cputrace") == 0) {
        state->cputrace = atoi(argv[1]) != 0;
    } else if (strcmp(argv[0], "ktrace-filter-descriptor") == 0) {
        if (strncmp(argv[1], "add:", 4) == 0 || strncmp(argv[1], "remove:", 7) == 0) {
            if (update_list(&state->filter, argv[1]) != 0) return 64;
        } else {
            free(state->filter);
            state->filter = strdup(argv[1]);
        }
    } else if (strcmp(argv[0], "sampling-option") == 0) {
        if (strncmp(argv[1], "add:", 4) == 0 || strncmp(argv[1], "remove:", 7) == 0) {
            if (update_list(&state->sampling, argv[1]) != 0) return 64;
        } else {
            free(state->sampling);
            state->sampling = strdup(argv[1]);
        }
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
    int target_pid = -1;
    int cputrace = 0;
    unsigned long long last_seconds = 0;
    static const struct option options[] = {
        {"reason", required_argument, NULL, 'r'},
        {"for-last-seconds", required_argument, NULL, 'l'},
        {"no-symbolicate", no_argument, NULL, 'n'},
        {"dsymForUUID", no_argument, NULL, 'd'},
        {"gather-os-signposts", no_argument, NULL, 's'},
        {"gather-os-logs", no_argument, NULL, 'o'},
        {"scrub-output", no_argument, NULL, 'S'},
        {"pid", required_argument, NULL, 'p'},
        {"cputrace", no_argument, NULL, 'C'},
        {NULL, 0, NULL, 0}
    };
    optind = 1;
    while ((option = getopt_long(argc, argv, "r:l:ndosSp:c:C", options, NULL)) != -1) {
        if (option == 'r') reason = optarg;
        else if (option == 'l') {
            char *end;
            errno = 0;
            last_seconds = strtoull(optarg, &end, 10);
            if (errno != 0 || end == optarg || *end != '\0')
                return 64;
        }
        else if (option == 'p') target_pid = atoi(optarg);
        else if (option == 'C') cputrace = 1;
        else if (option == 'n' || option == 'd' || option == 'o' || option == 's' ||
                 option == 'S' || option == 'c') continue;
        else return 64;
    }
    if (optind < argc) path = argv[optind];
    if (path == NULL) {
        fprintf(stderr, "tailspin: save requires a path\n");
        return 64;
    }
    uint64_t size = 0;
    uint64_t delta_timestamp = 0;
    if (last_seconds != 0) {
        mach_timebase_info_data_t timebase = {0, 0};
        uint64_t now = mach_absolute_time();
        mach_timebase_info(&timebase);
        long double ticks_per_second = 1000000000.0L * timebase.denom / timebase.numer;
        uint64_t duration = (uint64_t)(last_seconds * ticks_per_second);
        delta_timestamp = now > duration ? now - duration : 0;
    }
    int result = tailspin_capture_stackshot(path, target_pid, delta_timestamp, &size);
    if (result != 0) {
        fprintf(stderr, "tailspin: unable to save %s: %s\n", path, strerror(result));
        return 1;
    }
    (void)reason;
    (void)cputrace;
    return size == 0 ? 1 : 0;
}

static int command_stat(int argc, char **argv) {
    struct kcdata_item_header {
        unsigned int type;
        unsigned int size;
        unsigned long long flags;
    };
    struct stat file_stat;
    unsigned char *buffer;
    size_t offset = 0;
    size_t item_count = 0;
    struct type_count { unsigned int type; size_t count; } type_counts[64];
    size_t type_count_length = 0;
    int sort_by_frequency = 0;
    int ended = 0;
    unsigned int marker;
    FILE *file;
    int verbose = 0;
    int option;
    const char *path;
    optind = 1;
    while ((option = getopt(argc, argv, "vs")) != -1) {
        if (option == 'v') verbose = 1;
        else if (option == 's') sort_by_frequency = 1;
        else return 64;
    }
    if (optind >= argc)
        return 64;
    path = argv[optind];
    if (stat(path, &file_stat) != 0 || file_stat.st_size == 0) {
        fprintf(stderr, "tailspin: unable to inspect %s: %s\n", path,
                errno == 0 ? "empty file" : strerror(errno));
        return 1;
    }
    buffer = malloc((size_t)file_stat.st_size);
    file = fopen(path, "rb");
    if (buffer == NULL || file == NULL || fread(buffer, 1, (size_t)file_stat.st_size, file) != (size_t)file_stat.st_size) {
        if (file != NULL) fclose(file);
        free(buffer);
        fprintf(stderr, "tailspin: unable to read %s\n", path);
        return 1;
    }
    fclose(file);
    marker = *(unsigned int *)buffer;
    while (offset + sizeof(struct kcdata_item_header) <= (size_t)file_stat.st_size) {
        struct kcdata_item_header *item = (struct kcdata_item_header *)(buffer + offset);
        size_t item_size = sizeof(*item) + item->size;
        size_t aligned_size = (item_size + 15) & ~((size_t)15);
        if (item->type == 0xf19158edU) {
            ended = item->size == 0;
            break;
        }
        if (aligned_size < item_size || offset + aligned_size > (size_t)file_stat.st_size) {
            free(buffer);
            fprintf(stderr, "tailspin: malformed KCData item in %s\n", path);
            return 1;
        }
        item_count++;
        size_t type_index;
        for (type_index = 0; type_index < type_count_length; type_index++)
            if (type_counts[type_index].type == item->type)
                break;
        if (type_index == type_count_length && type_count_length < 64) {
            type_counts[type_index].type = item->type;
            type_counts[type_index].count = 0;
            type_count_length++;
        }
        if (type_index < type_count_length)
            type_counts[type_index].count++;
        offset += aligned_size;
    }
    free(buffer);
    printf("File: %s\nSize: %lld bytes\nFormat: %s\n",
           path, (long long)file_stat.st_size,
           marker == 0x59a25807U ? "KCData stackshot" : "unknown");
    if (verbose)
        printf("Header marker: 0x%08x\nItems: %zu\nEnd marker: %s\n", marker,
               item_count, ended ? "present" : "missing");
    if (sort_by_frequency) {
        size_t i;
        for (i = 0; i < type_count_length; i++) {
            size_t j;
            for (j = i + 1; j < type_count_length; j++) {
                if (type_counts[j].count > type_counts[i].count) {
                    struct type_count temporary = type_counts[i];
                    type_counts[i] = type_counts[j];
                    type_counts[j] = temporary;
                }
            }
        }
        for (i = 0; i < type_count_length; i++)
            printf("0x%08x %zu\n", type_counts[i].type, type_counts[i].count);
    }
    return marker == 0x59a25807U && ended ? 0 : 1;
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
    } else if (strcmp(argv[1], "stat") == 0) {
        result = command_stat(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "augment") == 0) {
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
