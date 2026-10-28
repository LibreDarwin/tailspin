/* Copyright (C) 2026, LibreDarwin */
/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef TAILSPIN_LOG_H
#define TAILSPIN_LOG_H

#include <sys/types.h>

int tailspin_append_os_logs(int output_fd, const char *archive_path);

#endif
