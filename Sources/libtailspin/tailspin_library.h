/* Copyright (C) 2026, LibreDarwin */
/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef TAILSPIN_LIBRARY_H
#define TAILSPIN_LIBRARY_H

#include <stdbool.h>

void *tailspin_config_create_new(void);
void *tailspin_config_create_with_current_state(void);
void *tailspin_config_create_with_default_config(void);
void tailspin_config_free(void *config);
void tailspin_config_apply_sync(void *config);

#endif
