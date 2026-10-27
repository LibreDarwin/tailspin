/* Copyright (C) 2026, LibreDarwin */
/* SPDX-License-Identifier: BSD-3-Clause */

#include <errno.h>
#include <stddef.h>

#define TAILSPIN_KEY(symbol, value) const char *symbol = value
#define TAILSPIN_UNIMPLEMENTED(symbol) \
    void *symbol(void) { errno = ENOTSUP; return NULL; }

TAILSPIN_KEY(TSPConfiguration_CPUTraceEnabledKey, "cputrace");
TAILSPIN_KEY(TSPConfiguration_CPUTraceEnabledModifierKey, "cputrace-modifier");
TAILSPIN_KEY(TSPConfiguration_CPUTraceKey, "cputrace");
TAILSPIN_KEY(TSPConfiguration_EnabledKey, "enabled");
TAILSPIN_KEY(TSPConfiguration_EnabledModifierKey, "enabled-modifier");
TAILSPIN_KEY(TSPConfiguration_EnforcedByManualConfigurationKey, "manual");
TAILSPIN_KEY(TSPConfiguration_EnforcedByProfileKey, "profile");
TAILSPIN_KEY(TSPConfiguration_EnforcedByTaskingKey, "tasking");
TAILSPIN_KEY(TSPConfiguration_FullSystemSamplingPeriodKey, "full-system-sampling-period");
TAILSPIN_KEY(TSPConfiguration_FullSystemSamplingPeriodModifierKey, "full-system-sampling-period-modifier");
TAILSPIN_KEY(TSPConfiguration_IsCswitchSamplingEnabledKey, "cswitch-sampling");
TAILSPIN_KEY(TSPConfiguration_IsSyscallSamplingEnabledKey, "syscall-sampling");
TAILSPIN_KEY(TSPConfiguration_IsVMFaultSamplingEnabledKey, "vmfault-sampling");
TAILSPIN_KEY(TSPConfiguration_KdebugBufferSizeKey, "buffer-size-mb");
TAILSPIN_KEY(TSPConfiguration_KdebugBufferSizeModifierKey, "buffer-size-mb-modifier");
TAILSPIN_KEY(TSPConfiguration_KdebugFilterDescKey, "ktrace-filter-descriptor");
TAILSPIN_KEY(TSPConfiguration_KdebugFilterDescModifierKey, "ktrace-filter-descriptor-modifier");
TAILSPIN_KEY(TSPConfiguration_KdebugKey, "kdebug");
TAILSPIN_KEY(TSPConfiguration_OncoreSamplingPeriodKey, "oncore-sampling-period");
TAILSPIN_KEY(TSPConfiguration_OncoreSamplingPeriodModifierKey, "oncore-sampling-period-modifier");
TAILSPIN_KEY(TSPConfiguration_SamplingKey, "sampling");
TAILSPIN_KEY(TSPConfiguration_SamplingOptionsModifierKey, "sampling-option-modifier");

TAILSPIN_KEY(TSPDumpOptions_CollectAriadnePlists, "collect-ariadne-plists");
TAILSPIN_KEY(TSPDumpOptions_CollectOsLogs, "collect-os-logs");
TAILSPIN_KEY(TSPDumpOptions_CollectOsSignposts, "collect-os-signposts");
TAILSPIN_KEY(TSPDumpOptions_CollectTrials, "collect-trials");
TAILSPIN_KEY(TSPDumpOptions_EndTimestamp, "end-timestamp");
TAILSPIN_KEY(TSPDumpOptions_ExistingTailspinMode, "existing-tailspin-mode");
TAILSPIN_KEY(TSPDumpOptions_FilePath, "file-path");
TAILSPIN_KEY(TSPDumpOptions_FullFSyncAfterEvents, "full-fsync-after-events");
TAILSPIN_KEY(TSPDumpOptions_KtraceCompressionLevel, "ktrace-compression-level");
TAILSPIN_KEY(TSPDumpOptions_MaxTimestamp, "max-timestamp");
TAILSPIN_KEY(TSPDumpOptions_MinTimestamp, "min-timestamp");
TAILSPIN_KEY(TSPDumpOptions_MinTraceBufferDurationSec, "min-trace-buffer-duration-sec");
TAILSPIN_KEY(TSPDumpOptions_NoSymbolicate, "no-symbolicate");
TAILSPIN_KEY(TSPDumpOptions_OsLogArchive, "os-log-archive");
TAILSPIN_KEY(TSPDumpOptions_ReasonString, "reason");
TAILSPIN_KEY(TSPDumpOptions_ResampleTruncatedStacks, "resample-truncated-stacks");
TAILSPIN_KEY(TSPDumpOptions_ScrubOutput, "scrub-output");
TAILSPIN_KEY(TSPDumpOptions_ShouldCollectCPUTrace, "cputrace");
TAILSPIN_KEY(TSPDumpOptions_Symbolicate, "symbolicate");
TAILSPIN_KEY(TSPDumpOptions_TargetPID, "target-pid");
TAILSPIN_KEY(TSPDumpOptions_UseDsymForUUID, "dsym-for-uuid");

TAILSPIN_KEY(TSPMetadata_TailspinDumpReasonKey, "tailspin-dump-reason");
TAILSPIN_KEY(TSPMetadata_TailspinConfigurationKey, "tailspin-configuration");
TAILSPIN_KEY(TSPMetadata_TargetPID, "target-pid");
TAILSPIN_KEY(TSPProcessInfo_PidKey, "pid");
TAILSPIN_KEY(TSPProcessInfo_LoadInfosKey, "load-infos");
TAILSPIN_KEY(TSPSymbols_UUIDKey, "uuid");
TAILSPIN_KEY(TSPSymbols_BinaryNameKey, "binary-name");
TAILSPIN_KEY(TSPSymbols_BinaryPathKey, "binary-path");
TAILSPIN_KEY(TSPSymbols_SymbolNameKey, "symbol-name");
TAILSPIN_KEY(TSPSymbols_SymbolOffsetKey, "symbol-offset");
TAILSPIN_KEY(TSPTaskingConfiguration_EnabledKey, "enabled");
TAILSPIN_KEY(TSPTaskingConfiguration_CPUTraceEnabledKey, "cputrace");
TAILSPIN_KEY(TSPTaskingConfiguration_KdebugBufferSizeKey, "buffer-size-mb");
TAILSPIN_KEY(TSPTaskingConfiguration_KdebugFilterDescKey, "ktrace-filter-descriptor");

TAILSPIN_UNIMPLEMENTED(add_binary_info_to_binary_dictionary);
TAILSPIN_UNIMPLEMENTED(cputrace_session_data_from_disk);
TAILSPIN_UNIMPLEMENTED(create_and_start_cputrace_live_recording);
TAILSPIN_UNIMPLEMENTED(get_responsible_pid_and_name);
TAILSPIN_UNIMPLEMENTED(get_tailspin_ondisk_config);
TAILSPIN_UNIMPLEMENTED(get_tailspin_profile_config);
TAILSPIN_UNIMPLEMENTED(get_tailspin_tasking_config);
TAILSPIN_UNIMPLEMENTED(is_apple_internal_setting);
TAILSPIN_UNIMPLEMENTED(main_binary_for_pid_in_ktrace);
TAILSPIN_UNIMPLEMENTED(reinit_cputrace_live_recording_from_existing_session);
TAILSPIN_UNIMPLEMENTED(serialize_cputrace_session);
TAILSPIN_UNIMPLEMENTED(set_config_field_modifier);

TAILSPIN_UNIMPLEMENTED(tailspin_augment_output);
TAILSPIN_UNIMPLEMENTED(tailspin_augment_output_with_request_id);
TAILSPIN_UNIMPLEMENTED(tailspin_compress_file);
TAILSPIN_UNIMPLEMENTED(tailspin_config_create_with_config);
TAILSPIN_UNIMPLEMENTED(tailspin_cputrace_handle_deinit);
TAILSPIN_UNIMPLEMENTED(tailspin_currently_running);
TAILSPIN_UNIMPLEMENTED(tailspin_get_cputrace_handle);
TAILSPIN_UNIMPLEMENTED(tailspin_kdbg_filter_class_get);
TAILSPIN_UNIMPLEMENTED(tailspin_kdbg_filter_class_get_partial);
TAILSPIN_UNIMPLEMENTED(tailspin_kdbg_filter_class_set);
TAILSPIN_UNIMPLEMENTED(tailspin_kdbg_filter_clear);
TAILSPIN_UNIMPLEMENTED(tailspin_kdbg_filter_copy_description);
TAILSPIN_UNIMPLEMENTED(tailspin_kdbg_filter_get_default);
TAILSPIN_UNIMPLEMENTED(tailspin_kdbg_filter_is_default);
TAILSPIN_UNIMPLEMENTED(tailspin_kdbg_filter_reset);
TAILSPIN_UNIMPLEMENTED(tailspin_kdbg_filter_subclass_get);
TAILSPIN_UNIMPLEMENTED(tailspin_kdbg_filter_subclass_set);
TAILSPIN_UNIMPLEMENTED(tailspin_make_ondisk_config);
TAILSPIN_UNIMPLEMENTED(tailspin_parse_stackshot_chunk);
TAILSPIN_UNIMPLEMENTED(tailspin_pause_cputrace);
TAILSPIN_UNIMPLEMENTED(tailspin_poke);
TAILSPIN_UNIMPLEMENTED(tailspin_resolve_configs);
TAILSPIN_UNIMPLEMENTED(tailspin_sampling_option_get);
TAILSPIN_UNIMPLEMENTED(tailspin_sampling_option_set);
TAILSPIN_UNIMPLEMENTED(tailspin_sampling_options_clear);
TAILSPIN_UNIMPLEMENTED(tailspin_sampling_options_get_default);
TAILSPIN_UNIMPLEMENTED(tailspin_sampling_options_is_default);
TAILSPIN_UNIMPLEMENTED(tailspin_sampling_options_reset);
TAILSPIN_UNIMPLEMENTED(tailspin_save_trace_with_standard_chunks);
TAILSPIN_UNIMPLEMENTED(tailspin_symbolicate_file_async);
TAILSPIN_UNIMPLEMENTED(tailspin_symbolicator_copy_symbol_data_using_uuid_pid_path);
TAILSPIN_UNIMPLEMENTED(tailspin_write_metadata_chunk);
TAILSPIN_UNIMPLEMENTED(tailspin_write_os_signpost_support_chunks);
TAILSPIN_UNIMPLEMENTED(tailspin_write_symbolless_binary_info_chunk);
