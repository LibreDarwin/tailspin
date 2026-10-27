/* Copyright (C) 2026, LibreDarwin */
/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef TAILSPIN_PRIVATE_ABI_H
#define TAILSPIN_PRIVATE_ABI_H

#include <stdbool.h>
#include <dispatch/dispatch.h>

/* Opaque objects owned by the optional system libtailspin bridge. */
typedef void *tailspin_private_config_t;
typedef void *tailspin_private_options_t;

typedef void (^tailspin_private_completion_t)(bool success);

typedef tailspin_private_config_t (*tailspin_private_config_create_fn)(void);
typedef void (*tailspin_private_config_free_fn)(tailspin_private_config_t config);
typedef void (*tailspin_private_config_apply_fn)(
    tailspin_private_config_t config,
    dispatch_queue_t queue,
    tailspin_private_completion_t completion);

typedef bool (*tailspin_private_dump_output_fn)(
    int output_fd,
    tailspin_private_options_t options);
typedef void (*tailspin_private_augment_output_fn)(
    int output_fd,
    const char *path,
    dispatch_queue_t queue,
    tailspin_private_completion_t completion);

#endif
