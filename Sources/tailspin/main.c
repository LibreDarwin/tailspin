/* Copyright (C) 2026, LibreDarwin */
/* SPDX-License-Identifier: BSD-3-Clause */

#include "../common/tailspin_state.h"
#include "../common/tailspin_capture.h"
#include "ktrace.h"

#include <CoreFoundation/CoreFoundation.h>
#include <errno.h>
#include <getopt.h>
#include <mach/mach_time.h>
#include <stdint.h>
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
        if (strcmp(argv[1], "0") == 0)
            state->cputrace = 0;
        else if (strcmp(argv[1], "1") == 0)
            state->cputrace = 1;
        else
            return 64;
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
        if ((long double)last_seconds * ticks_per_second > 18446744073709551615.0L)
            return 64;
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

static int command_augment(int argc, char **argv) {
    int option;
    int requested = 0;
    static const struct option options[] = {
        {"with-symbols", no_argument, NULL, 's'},
        {"dsymForUUID", no_argument, NULL, 'd'},
        {"collect-ariadne-plists", no_argument, NULL, 'a'},
        {"collect-trials", no_argument, NULL, 't'},
        {"with-os-signposts-logs", no_argument, NULL, 'l'},
        {"log-archive-path", required_argument, NULL, 'L'},
        {NULL, 0, NULL, 0}
    };

    optind = 1;
    while ((option = getopt_long(argc, argv, "sdatlL:", options, NULL)) != -1) {
        if (option == 's' || option == 'd' || option == 'a' || option == 't' ||
            option == 'l' || option == 'L')
            requested = 1;
        else
            return 64;
    }
    if (!requested || optind >= argc)
        return 64;
    for (; optind < argc; optind++) {
        struct stat file_stat;
        if (stat(argv[optind], &file_stat) != 0 || !S_ISREG(file_stat.st_mode)) {
            fprintf(stderr, "tailspin: unable to open %s for augmenting: %s\n",
                    argv[optind], errno == 0 ? "not a regular file" : strerror(errno));
            return 1;
        }
    }
    fprintf(stderr, "tailspin: augmentation is not supported in this build\n");
    return 1;
}

struct tailspin_chunk_header {
    uint32_t tag;
    uint32_t version;
    uint64_t payload_length;
};

static uint32_t read_u32(const unsigned char *bytes) {
    uint32_t value;
    memcpy(&value, bytes, sizeof(value));
    return value;
}

static uint64_t read_u64(const unsigned char *bytes) {
    uint64_t value;
    memcpy(&value, bytes, sizeof(value));
    return value;
}

static int command_stat_tailspin(const char *path, const unsigned char *buffer,
                                 size_t length, int verbose, int sort_by_frequency) {
    struct type_count {
        uint32_t tag;
        uint64_t size;
        size_t count;
    } chunks[128];
    uint32_t version = read_u32(buffer + 4);
    uint64_t header_length = read_u64(buffer + 8);
    size_t offset;
    size_t header_offset;
    size_t header_end;
    size_t chunk_count = 0;
    size_t header_chunk_count = 0;
    size_t i;
    uint64_t metadata_size = 0;
    size_t metadata_offset = 0;
    uint64_t *event_frequencies = NULL;
    __block uint64_t event_total = 0;
    struct type_count header_chunks[32];
    (void)sort_by_frequency;
    if (header_length > length - 16 || header_length > SIZE_MAX - 16) {
        fprintf(stderr, "tailspin: malformed tailspin header in %s\n", path);
        return 1;
    }
    header_offset = 16;
    header_end = 16 + (size_t)header_length;
    while (header_offset + 24 <= header_end) {
        uint32_t tag;
        uint64_t payload_length;
        size_t index;
        tag = read_u32(buffer + header_offset + 8);
        payload_length = read_u64(buffer + header_offset + 16);
        if ((tag != 0x8c00 && tag != 0x8c01 && tag != 0x1c00) ||
            payload_length > header_end - header_offset - 24) {
            header_offset += 8;
            continue;
        }
        for (index = 0; index < header_chunk_count; index++)
            if (header_chunks[index].tag == tag)
                break;
        if (index == header_chunk_count && header_chunk_count < 32) {
            header_chunks[index].tag = tag;
            header_chunks[index].size = 0;
            header_chunks[index].count = 0;
            header_chunk_count++;
        }
        if (index < header_chunk_count) {
            header_chunks[index].size += payload_length;
            header_chunks[index].count++;
        }
        header_offset += 8;
    }
    offset = 16 + (size_t)header_length;
    while (offset < length) {
        uint32_t tag;
        uint64_t payload_length;
        size_t index;
        if (length - offset < sizeof(struct tailspin_chunk_header)) {
            fprintf(stderr, "tailspin: truncated tailspin chunk in %s\n", path);
            return 1;
        }
        tag = read_u32(buffer + offset);
        payload_length = read_u64(buffer + offset + 8);
        if (payload_length > length - offset - sizeof(struct tailspin_chunk_header)) {
            fprintf(stderr, "tailspin: malformed tailspin chunk in %s\n", path);
            return 1;
        }
        if (tag == 0x9003) {
            metadata_size += payload_length;
            metadata_offset = offset + sizeof(struct tailspin_chunk_header);
            goto next_chunk;
        }
        for (index = 0; index < chunk_count; index++)
            if (chunks[index].tag == tag)
                break;
        if (index == chunk_count && chunk_count < sizeof(chunks) / sizeof(chunks[0])) {
            chunks[index].tag = tag;
            chunks[index].size = 0;
            chunks[index].count = 0;
            chunk_count++;
        }
        if (index < chunk_count) {
            chunks[index].size += payload_length;
            chunks[index].count++;
        }
next_chunk:
        offset += sizeof(struct tailspin_chunk_header) + (size_t)payload_length;
        if (offset > SIZE_MAX - 7)
            return 1;
        offset = (offset + 7) & ~(size_t)7;
    }
    printf("File: %s\nSize: %zu bytes\nFormat: Apple tailspin\n", path, length);
    if (verbose)
        printf("Header magic: 0x%08x\nHeader version: %u\nHeader bytes: %llu\n",
               read_u32(buffer), version, (unsigned long long)header_length);
    if (verbose) {
        printf("Header chunks:\n");
        for (i = 0; i < header_chunk_count; i++)
            printf("0x%08x %zu chunks %llu bytes\n", header_chunks[i].tag,
                   header_chunks[i].count,
                   (unsigned long long)header_chunks[i].size);
    }
    printf("Regular chunks: %zu\n", chunk_count);
    for (i = 0; i < chunk_count; i++)
        printf("0x%08x %zu chunks %llu bytes\n", chunks[i].tag, chunks[i].count,
               (unsigned long long)chunks[i].size);
    if (metadata_size != 0)
        printf("metadata chunk: %llu bytes\n", (unsigned long long)metadata_size);
    if (sort_by_frequency) {
        ktrace_file_t trace_file = ktrace_file_open(path, 0);
        if (trace_file != NULL) {
            event_frequencies = calloc(256 * 256, sizeof(*event_frequencies));
            if (event_frequencies != NULL &&
                ktrace_file_iterate_ktrace_events(trace_file, ^int(uint32_t debugid) {
                    event_frequencies[((debugid >> 24) & 0xff) * 256 +
                                      ((debugid >> 16) & 0xff)]++;
                    event_total++;
                    return 1;
                }) == 0) {
                printf("Frequencies:\n");
                for (i = 0; i < 256 * 256; i++) {
                    if (event_frequencies[i] != 0) {
                        double percentage = event_total == 0 ? 0.0 :
                            100.0 * event_frequencies[i] / event_total;
                        printf("C0x%02x,S0x%02x: %llu events (%5.2f%%)\n",
                               (unsigned)(i / 256), (unsigned)(i % 256),
                               (unsigned long long)event_frequencies[i],
                               percentage);
                    }
                }
            }
            free(event_frequencies);
            ktrace_file_close(trace_file);
        }
    }
    if (verbose) {
        ktrace_file_t trace_file = ktrace_file_open(path, 0);
        uint64_t earliest;
        uint64_t latest;
        uint32_t numerator;
        uint32_t denominator;
        if (trace_file != NULL &&
            ktrace_file_earliest_timestamp(trace_file, &earliest) == 0 &&
            ktrace_file_latest_timestamp(trace_file, &latest) == 0 &&
            ktrace_file_timebase(trace_file, &numerator, &denominator) == 0 &&
            latest >= earliest && denominator != 0) {
            long double seconds = ((long double)(latest - earliest) * numerator) /
                                  ((long double)denominator * 1000000000.0L);
            printf("Amount of ktrace history collected: %.2Lf s\n", seconds);
        }
        if (trace_file != NULL)
            ktrace_file_close(trace_file);
    }
    if (verbose) {
        ktrace_file_t trace_file = ktrace_file_open(path, 0);
        int64_t event_count = trace_file == NULL ? -1 :
            ktrace_file_ktrace_event_count(trace_file);
        if (trace_file != NULL)
            ktrace_file_close(trace_file);
        if (event_count >= 0)
            printf("Total tailspin buffer size : %.2f MB\n",
                   (double)event_count * 64.0 / (1024.0 * 1024.0));
    }
    if (metadata_size != 0 && metadata_offset <= length &&
        metadata_size <= length - metadata_offset) {
        CFDataRef data = CFDataCreate(kCFAllocatorDefault,
                                      buffer + metadata_offset,
                                      (CFIndex)metadata_size);
        CFPropertyListRef metadata = CFPropertyListCreateWithData(
            kCFAllocatorDefault, data, kCFPropertyListImmutable, NULL, NULL);
        if (metadata != NULL) {
            printf("tailspin metadata:\n");
            CFShow(metadata);
        }
        if (metadata != NULL)
            CFRelease(metadata);
        if (data != NULL)
            CFRelease(data);
    }
    return 0;
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
    if (marker == 0x55aa0300U) {
        int result = command_stat_tailspin(path, buffer, (size_t)file_stat.st_size,
                                           verbose, sort_by_frequency);
        free(buffer);
        return result;
    }
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
        result = command_augment(argc - 1, argv + 1);
    } else {
        usage(stderr);
        result = 64;
    }
out:
    tailspin_state_free(&state);
    return result;
}
