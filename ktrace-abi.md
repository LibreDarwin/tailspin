Copyright (C) 2026, LibreDarwin
SPDX-License-Identifier: BSD-3-Clause

Reconstructed ktrace ABI
========================

This is a clean-room ABI note for the private `ktrace` framework used by the
system `/usr/bin/ktrace` client. It is based on exported symbols in the
Command Line Tools `libktrace.tbd` and AArch64 call sites in `/usr/bin/ktrace`.
No private header or implementation source was available in the local SDK.

Opaque types
------------

The call sites treat both values as opaque pointers:

    typedef struct ktrace_session *ktrace_session_t;
    typedef struct ktrace_recording *ktrace_recording_t;

Confirmed function signatures
-----------------------------

    ktrace_session_t ktrace_session_create(void);
    void ktrace_session_destroy(ktrace_session_t session);
    int ktrace_start(ktrace_session_t session);
    int ktrace_end(ktrace_session_t session, int flush);

`ktrace_session_create` is called with no argument and its return value is
tested for null. `ktrace_start` and `ktrace_end` return an integer error code.
The second `ktrace_end` argument is loaded as either zero or one at all
observed call sites and is therefore represented as an integer rather than a
 guessed C++ or enum type.

Output functions
----------------

    int ktrace_start_writing_path(ktrace_session_t session,
                                  const char *path,
                                  mode_t mode);

The `ktrace` trace command loads the session into `x0`, the output path into
`x1`, and the literal `0120644` into `w2` before calling this symbol. The
return value is checked as a POSIX-style error code.

The raw-output command supplies the following five arguments:

    int ktrace_start_writing_fd(ktrace_session_t session,
                                int close_fd,
                                int fd,
                                uint64_t flags,
                                uint64_t options);

    int ktrace_start_writing_file(ktrace_session_t session,
                                  FILE *file,
                                  int fd_or_mode,
                                  uint64_t flags,
                                  uint64_t options);

The first call site loads `x1 = 1`, `x2 = fd`, and zeroes `x3`/`x4`. The second
loads `x1 = FILE *`, `x2` from the same local value used for the FD form, and
also zeroes `x3`/`x4`. The exact semantic names of the final arguments are not
established; they must remain opaque until private headers or additional
callers are available.

Recording helpers
-----------------

    ktrace_recording_t ktrace_recording_create(const char *name,
                                               int flags);
    void ktrace_recording_set_streams(ktrace_recording_t recording,
                                      FILE *stdout_stream,
                                      FILE *stderr_stream);

The system client passes the string `"ktrace record"` and `1` to the
constructor, then passes the returned object and its stdout/stderr streams to
`ktrace_recording_set_streams`. The constructor result is stored as an opaque
pointer. The stream setter's return value is not consumed at that call site,
so its return type is not asserted here; `void` is the safest source-level
declaration for callers that do not need diagnostics.

Additional observed symbols
---------------------------

The client imports `ktrace_configure`, `ktrace_set_command_argv`,
`ktrace_set_completion_handler`, and `ktrace_set_file`. Their call sites show
that they accept a session as their first argument, but callback, file, and
configuration object types require the unavailable private headers. They are
intentionally not declared by this project yet.

tailspind/libtailspin boundary
------------------------------

The installed `/usr/libexec/tailspind` imports these additional symbols from
`libtailspin.dylib`:

    id tailspin_make_ondisk_config(const char *path, bool create);
    void tailspin_config_free(void *config);

The first call is made with `/var/db/tailspin_config.plist` and either zero or
one as its second argument. Its result is retained as an Objective-C object,
so the concrete config type is an Objective-C/CF object rather than a plain C
struct. The free routine is called with that object after use.

The default getters are called without arguments and return their values in
the normal scalar/pointer return register:

    bool tailspin_enabled_get_default(void);
    uint64_t tailspin_buffer_size_get_default(void);
    const char *tailspin_kdbg_filter_get_default(void);
    uint64_t tailspin_full_sampling_period_get_default(void);
    uint64_t tailspin_oncore_sampling_period_get_default(void);
    const char *tailspin_sampling_options_get_default(void);
    bool tailspin_cputrace_enabled_get_default(void);

These names are private implementation symbols, not a supported public API.
The `TSPDumpOptions_*` imports are data accessors or exported option objects;
their exact storage type is not inferred from the daemon call sites and should
not be represented as C functions.

`tailspin_save_trace_with_standard_chunks` is called with at least nine
arguments: six register arguments followed by three stack arguments. The
daemon passes a file descriptor, timestamps, an options/config object, a
completion block, and processing state, but the call site alone does not
establish stable source-level types for all positions. Its ABI should be
wrapped only after matching the same call convention on the target OS.

ABI cautions
------------

* Do not define the internal session or recording layout. The client accesses
  fields only in its own wrapper objects, not in the framework objects.
* The framework is a private system framework and is not present as a normal
  SDK framework for portable linking.
* `ktrace_start_writing_path` uses a `mode_t` mode argument on the observed
  AArch64 path; this is stronger evidence than treating the third argument as
  an arbitrary flags word.
* Before adding a runtime loader, validate every function pointer and preserve
  the framework's integer error returns. Never call an inferred symbol through
  a guessed callback or struct layout.

tailspind request values
------------------------

Disassembly of `/usr/libexec/tailspind` resolves the numeric values stored in
`tailspin_request_type`:

    0x00000000       TAILSPIN_APPLY_CONFIG
    0x00000001       TAILSPIN_DUMP_OUTPUT
    0xdeadcaff       TAILSPIN_POKE
    0xdeadcb00       TAILSPIN_APPLY_CONFIG (legacy/private form)
    0xdeadcb01       TAILSPIN_GET_CPUTRACE_SESSION

The daemon first handles the ordinary values `0` and `1`, then compares the
low 32 bits against the `0xdead....` values. This is why the values appear as
negative signed integers in the optimized AArch64 comparison sequence. The
request dictionary also uses `tailspin_config` as an XPC data object for the
native apply-config path; the observed data length is `0x2118` bytes. A
dictionary-shaped configuration is a compatibility convenience in this
reimplementation, not the native Apple wire representation.

Native config buffer observations
---------------------------------

The daemon's apply routine receives a source configuration pointer in `rdi`
and a destination buffer in `rsi`. The tasking-config branch calls
`get_tailspin_tasking_config()`, then copies exactly `0x2118` bytes from the
returned object into the destination before calling `tailspin_config_free`.
The source object is inspected at byte offset `0x08` as a 32-bit flags field.
Later, the resulting buffer is inspected at offset `0x20f0` as a byte state
or enable field. No other field offsets are asserted here; the middle of the
buffer contains additional profile, tasking, filter, and sampling data that
still requires field-by-field tracing.

Evidence commands
-----------------

    otool -arch arm64 -Iv /usr/bin/ktrace
    otool -arch arm64 -tvV /usr/bin/ktrace
