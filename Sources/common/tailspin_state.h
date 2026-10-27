/* Copyright (C) 2026, LibreDarwin */
/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef TAILSPIN_STATE_H
#define TAILSPIN_STATE_H

#include <stddef.h>

typedef struct {
    int enabled;
    unsigned long long buffer_size_mb;
    unsigned long long oncore_period_ns;
    unsigned long long full_period_ns;
    int cputrace;
    char *filter;
    char *sampling;
} tailspin_state_t;

void tailspin_state_init(tailspin_state_t *state);
void tailspin_state_free(tailspin_state_t *state);
int tailspin_state_load(tailspin_state_t *state);
int tailspin_state_save(const tailspin_state_t *state);
void tailspin_state_reset(tailspin_state_t *state, const char *field);
const char *tailspin_state_path(void);

#endif
