/* Copyright (C) 2026, LibreDarwin */
/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef TAILSPIN_CAPTURE_H
#define TAILSPIN_CAPTURE_H

#include <stdint.h>

int tailspin_capture_stackshot(const char *path, int pid, uint64_t delta_timestamp,
                               uint64_t *size_out);

#endif
