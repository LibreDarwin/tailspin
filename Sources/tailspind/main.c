/* Copyright (C) 2026, LibreDarwin */
/* SPDX-License-Identifier: BSD-3-Clause */

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

int main(void) {
    signal(SIGTERM, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    for (;;) pause();
    return 0;
}
