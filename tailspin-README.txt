Copyright (C) 2026, LibreDarwin
SPDX-License-Identifier: BSD-3-Clause

tailspin
========

tailspin continuously collects system sampling and selected kdebug data so a
recent system history can be saved for diagnosis. The command-line interface
is compatible with the Darwin tailspin tool:

The build also produces `libtailspin.dylib`. Its configuration and dump
entry points are implemented locally; symbols whose private on-disk or
callback ABI has not yet been reconstructed are exported with explicit
`ENOTSUP` behavior rather than silently producing invalid data.

FD-based dump capture now uses the sibling LibreDarwin ktrace session layer
for output setup, lifecycle, and flushing. Set `KTRACE_ROOT` when the ktrace
checkout is not at `../ktrace`.

    tailspin info
    tailspin enable
    tailspin disable
    tailspin set <setting> <value>
    tailspin reset [setting]
    tailspin save [options] <path>
    tailspin augment [options] <path>
    tailspin stat [options] <path>

Configuration settings
----------------------

    buffer-size <megabytes>
    ktrace-filter-descriptor <descriptor>
    oncore-sampling-period <nanoseconds>|disabled
    full-system-sampling-period <nanoseconds>|disabled
    sampling-option <options>
    cputrace 0|1

On-core sampling periods must be at least 1000000 ns. Full-system sampling
periods must be at least 10000000 ns. Filter and sampling option values may be
modified with add:<value> and remove:<value>.

Output
------

save requests a kernel KCData-format stackshot. The resulting file begins with
the KCData stackshot marker and contains aligned KCData records terminated by
the KCData end marker. Privileged kernel access is required for capture.

stat validates the KCData framing and can print record-type frequencies with
-s. Full Apple tailspin files may contain additional ktrace, metadata, log,
signpost, trial, CPU trace, and symbolication chunks; those chunks are not yet
reconstructed by this clean-room implementation.

Configuration storage
---------------------

The default configuration path is /var/db/tailspin/config. Tests and staged
systems can override it with TAILSPIN_CONFIG_PATH.

tailspind
---------

tailspind is a service endpoint reserved for use by tailspin. The production
implementation communicates with clients through the com.apple.tailspind Mach
service and coordinates kernel tracing. The current LibreDarwin daemon keeps
the service process alive while that IPC and tracing integration is rebuilt.

References
----------

    tailspin(1)
    tailspind(8)
    XNU bsd/sys/stackshot.h
    XNU libkdd/kcdata.h
    ktrace-abi.md

The recovered high-confidence libtailspin function-pointer declarations are
kept in `Sources/common/tailspin_private_abi.h`. The header intentionally does
not declare the uncertain standard-chunk save routine or any private object
layouts.
