/* Copyright (C) 2026, LibreDarwin */
/* SPDX-License-Identifier: BSD-3-Clause */

#include "../common/tailspin_private_abi.h"
#include "../common/tailspin_capture.h"
#include "../common/tailspin_state.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct tailspin_config {
    tailspin_state_t state;
};

void tailspin_config_free(void *opaque_config);

static const unsigned long long default_buffer_size = 32;
static const unsigned long long default_oncore_period = 1000000;
static const unsigned long long default_full_period = 10000000;

static struct tailspin_config *config_create(int use_current_state) {
    struct tailspin_config *config = calloc(1, sizeof(*config));
    if (config == NULL)
        return NULL;
    tailspin_state_init(&config->state);
    if (use_current_state && tailspin_state_load(&config->state) != 0) {
        tailspin_state_free(&config->state);
        free(config);
        return NULL;
    }
    return config;
}

void *tailspin_config_create_new(void) {
    return config_create(0);
}

void *tailspin_config_create_with_current_state(void) {
    return config_create(1);
}

void *tailspin_config_create_with_default_config(void) {
    return config_create(0);
}

void *tailspin_make_ondisk_config(const char *path, bool create) {
    struct tailspin_config *config;
    if (path == NULL)
        return NULL;
    config = config_create(0);
    if (config == NULL)
        return NULL;
    if (tailspin_state_load_path(&config->state, path) != 0) {
        if (!create || errno != ENOENT) {
            tailspin_config_free(config);
            return NULL;
        }
    }
    return config;
}

void tailspin_config_free(void *opaque_config) {
    struct tailspin_config *config = opaque_config;
    if (config == NULL)
        return;
    tailspin_state_free(&config->state);
    free(config);
}

static int config_apply_state(struct tailspin_config *config) {
    if (config == NULL || config->state.buffer_size_mb == 0 ||
        (config->state.oncore_period_ns != 0 &&
         config->state.oncore_period_ns < default_oncore_period) ||
        (config->state.full_period_ns != 0 &&
         config->state.full_period_ns < default_full_period)) {
        errno = EINVAL;
        return -1;
    }
    return tailspin_state_save(&config->state);
}

void tailspin_config_apply_sync(void *opaque_config) {
    (void)config_apply_state(opaque_config);
}

void tailspin_config_apply(void *opaque_config, dispatch_queue_t queue,
                           tailspin_private_completion_t completion) {
    struct tailspin_config *config = opaque_config;
    if (queue == NULL) {
        if (completion != NULL)
            completion(config_apply_state(config) == 0);
        return;
    }
    dispatch_async(queue, ^{
        bool success = config_apply_state(config) == 0;
        if (completion != NULL)
            completion(success);
    });
}

void *tailspin_config_copy_description(void *opaque_config) {
    struct tailspin_config *config = opaque_config;
    char *description;
    if (config == NULL)
        return NULL;
    if (asprintf(&description,
                 "enabled=%d buffer-size-mb=%llu oncore-sampling-period=%llu "
                 "full-system-sampling-period=%llu cputrace=%d",
                 config->state.enabled, config->state.buffer_size_mb,
                 config->state.oncore_period_ns,
                 config->state.full_period_ns, config->state.cputrace) < 0)
        return NULL;
    return description;
}

bool tailspin_enabled_get(void *opaque_config) {
    struct tailspin_config *config = opaque_config;
    return config != NULL && config->state.enabled;
}

bool tailspin_enabled_get_default(void) { return false; }
bool tailspin_enabled_is_default(void *opaque_config) {
    return !tailspin_enabled_get(opaque_config);
}
bool tailspin_enabled_set(void *opaque_config, bool enabled) {
    struct tailspin_config *config = opaque_config;
    if (config == NULL)
        return false;
    config->state.enabled = enabled;
    return true;
}
void tailspin_enabled_reset(void *opaque_config) {
    (void)tailspin_enabled_set(opaque_config, false);
}

unsigned long long tailspin_buffer_size_get(void *opaque_config) {
    struct tailspin_config *config = opaque_config;
    return config == NULL ? 0 : config->state.buffer_size_mb;
}
unsigned long long tailspin_buffer_size_get_default(void) { return default_buffer_size; }
bool tailspin_buffer_size_is_default(void *opaque_config) {
    return tailspin_buffer_size_get(opaque_config) == default_buffer_size;
}
bool tailspin_buffer_size_set(void *opaque_config, unsigned long long value) {
    struct tailspin_config *config = opaque_config;
    if (config == NULL || value == 0)
        return false;
    config->state.buffer_size_mb = value;
    return true;
}
void tailspin_buffer_size_reset(void *opaque_config) {
    (void)tailspin_buffer_size_set(opaque_config, default_buffer_size);
}

unsigned long long tailspin_oncore_sampling_period_get(void *opaque_config) {
    struct tailspin_config *config = opaque_config;
    return config == NULL ? 0 : config->state.oncore_period_ns;
}
unsigned long long tailspin_oncore_sampling_period_get_default(void) {
    return default_oncore_period;
}
unsigned long long tailspin_min_oncore_sampling_period_allowed(void) {
    return default_oncore_period;
}
bool tailspin_oncore_sampling_period_is_default(void *config) {
    return tailspin_oncore_sampling_period_get(config) == default_oncore_period;
}
bool tailspin_oncore_sampling_period_set(void *opaque_config,
                                         unsigned long long value) {
    struct tailspin_config *config = opaque_config;
    if (config == NULL || (value != 0 && value < default_oncore_period))
        return false;
    config->state.oncore_period_ns = value;
    return true;
}
void tailspin_oncore_sampling_period_reset(void *opaque_config) {
    (void)tailspin_oncore_sampling_period_set(opaque_config, default_oncore_period);
}

unsigned long long tailspin_full_sampling_period_get(void *opaque_config) {
    struct tailspin_config *config = opaque_config;
    return config == NULL ? 0 : config->state.full_period_ns;
}
unsigned long long tailspin_full_sampling_period_get_default(void) {
    return default_full_period;
}
unsigned long long tailspin_min_full_sampling_period_allowed(void) {
    return default_full_period;
}
bool tailspin_full_sampling_period_is_default(void *config) {
    return tailspin_full_sampling_period_get(config) == default_full_period;
}
bool tailspin_full_sampling_period_set(void *opaque_config,
                                       unsigned long long value) {
    struct tailspin_config *config = opaque_config;
    if (config == NULL || (value != 0 && value < default_full_period))
        return false;
    config->state.full_period_ns = value;
    return true;
}
void tailspin_full_sampling_period_reset(void *opaque_config) {
    (void)tailspin_full_sampling_period_set(opaque_config, default_full_period);
}

bool tailspin_cputrace_enabled_get(void *opaque_config) {
    struct tailspin_config *config = opaque_config;
    return config != NULL && config->state.cputrace;
}
bool tailspin_cputrace_enabled_get_default(void) { return false; }
bool tailspin_cputrace_enabled_is_default(void *config) {
    return !tailspin_cputrace_enabled_get(config);
}
void tailspin_cputrace_enabled_reset(void *opaque_config) {
    struct tailspin_config *config = opaque_config;
    if (config != NULL)
        config->state.cputrace = 0;
}
bool tailspin_cputrace_enabled_set_with_options(void *opaque_config, bool enabled,
                                                unsigned long long options) {
    struct tailspin_config *config = opaque_config;
    (void)options;
    if (config == NULL)
        return false;
    config->state.cputrace = enabled;
    return true;
}

const char *tailspin_kdbg_filter_get_default(void) { return "ALL"; }
const char *tailspin_kdbg_filter_copy_description(void *opaque_config) {
    struct tailspin_config *config = opaque_config;
    return config == NULL || config->state.filter == NULL ? NULL : strdup(config->state.filter);
}
bool tailspin_kdbg_filter_is_default(void *opaque_config) {
    struct tailspin_config *config = opaque_config;
    return config != NULL && config->state.filter != NULL &&
           strcmp(config->state.filter, "ALL") == 0;
}
void tailspin_kdbg_filter_reset(void *opaque_config) {
    struct tailspin_config *config = opaque_config;
    if (config == NULL)
        return;
    free(config->state.filter);
    config->state.filter = strdup("ALL");
}
bool tailspin_kdbg_filter_clear(void *opaque_config) {
    struct tailspin_config *config = opaque_config;
    if (config == NULL)
        return false;
    free(config->state.filter);
    config->state.filter = strdup("");
    return config->state.filter != NULL;
}
bool tailspin_kdbg_filter_subclass_set(void *opaque_config, const char *value,
                                       bool enabled) {
    struct tailspin_config *config = opaque_config;
    if (config == NULL || value == NULL || value[0] == '\0')
        return false;
    if (!enabled)
        return true;
    free(config->state.filter);
    config->state.filter = strdup(value);
    return config->state.filter != NULL;
}
bool tailspin_kdbg_filter_subclass_get(void *opaque_config, const char *value) {
    struct tailspin_config *config = opaque_config;
    return config != NULL && value != NULL && config->state.filter != NULL &&
           strcmp(config->state.filter, value) == 0;
}
const char *tailspin_kdbg_filter_class_get(void *config, const char *value) {
    (void)config;
    return value;
}
const char *tailspin_kdbg_filter_class_get_partial(void *config, const char *value) {
    (void)config;
    return value;
}
bool tailspin_kdbg_filter_class_set(void *config, const char *value, bool enabled) {
    return tailspin_kdbg_filter_subclass_set(config, value, enabled);
}

const char *tailspin_sampling_options_get_default(void) {
    return "cswitch-sampling,syscall-sampling,vmfault-sampling";
}
const char *tailspin_sampling_option_get(void *opaque_config, const char *value) {
    struct tailspin_config *config = opaque_config;
    if (config == NULL)
        return NULL;
    return value == NULL ? config->state.sampling :
           strstr(config->state.sampling, value) != NULL ? value : NULL;
}
bool tailspin_sampling_options_is_default(void *opaque_config) {
    struct tailspin_config *config = opaque_config;
    return config != NULL && config->state.sampling != NULL &&
           strcmp(config->state.sampling, tailspin_sampling_options_get_default()) == 0;
}
void tailspin_sampling_options_reset(void *opaque_config) {
    struct tailspin_config *config = opaque_config;
    if (config == NULL)
        return;
    free(config->state.sampling);
    config->state.sampling = strdup(tailspin_sampling_options_get_default());
}
bool tailspin_sampling_options_clear(void *opaque_config) {
    struct tailspin_config *config = opaque_config;
    if (config == NULL)
        return false;
    free(config->state.sampling);
    config->state.sampling = strdup("");
    return config->state.sampling != NULL;
}
bool tailspin_sampling_option_set(void *opaque_config, const char *value,
                                  bool enabled) {
    struct tailspin_config *config = opaque_config;
    if (config == NULL || value == NULL || value[0] == '\0')
        return false;
    if (!enabled)
        return true;
    free(config->state.sampling);
    config->state.sampling = strdup(value);
    return config->state.sampling != NULL;
}

bool tailspin_currently_running(void) {
    tailspin_state_t state;
    bool running;
    tailspin_state_init(&state);
    if (tailspin_state_load(&state) != 0) {
        tailspin_state_free(&state);
        return false;
    }
    running = state.enabled != 0;
    tailspin_state_free(&state);
    return running;
}

bool tailspin_dump_output_with_options_sync(int output_fd,
                                            tailspin_private_options_t options) {
    uint64_t size = 0;
    (void)options;
    return tailspin_capture_stackshot_fd(output_fd, -1, 0, &size) == 0 && size != 0;
}

bool tailspin_dump_output_sync(int output_fd) {
    return tailspin_dump_output_with_options_sync(output_fd, NULL);
}

bool tailspin_dump_output_with_options(int output_fd,
                                       tailspin_private_options_t options,
                                       dispatch_queue_t queue,
                                       tailspin_private_completion_t completion) {
    bool success = tailspin_dump_output_with_options_sync(output_fd, options);
    if (queue == NULL) {
        if (completion != NULL)
            completion(success);
    } else {
        dispatch_async(queue, ^{
            if (completion != NULL)
                completion(success);
        });
    }
    return success;
}

bool tailspin_dump_output(int output_fd, dispatch_queue_t queue,
                          tailspin_private_completion_t completion) {
    return tailspin_dump_output_with_options(output_fd, NULL, queue, completion);
}

void tailspin_augment_output(int output_fd, const char *path, dispatch_queue_t queue,
                             tailspin_private_completion_t completion) {
    (void)output_fd;
    (void)path;
    if (queue == NULL) {
        if (completion != NULL)
            completion(false);
        return;
    }
    dispatch_async(queue, ^{
        if (completion != NULL)
            completion(false);
    });
}
