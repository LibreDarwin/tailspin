/* Copyright (C) 2026, LibreDarwin */
/* SPDX-License-Identifier: BSD-3-Clause */

#include <dispatch/dispatch.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xpc/xpc.h>

#include "../common/tailspin_capture.h"
#include "../common/tailspin_state.h"

static int apply_config(xpc_object_t object) {
    tailspin_state_t state;
    xpc_object_t config = xpc_dictionary_get_value(object, "tailspin_config");
    if (config == NULL)
        config = object;
    if (xpc_get_type(config) != XPC_TYPE_DICTIONARY)
        return EINVAL;
    tailspin_state_init(&state);
    if (tailspin_state_load(&state) != 0) {
        tailspin_state_free(&state);
        return EIO;
    }
    if (xpc_dictionary_get_value(config, "enabled") != NULL)
        state.enabled = xpc_dictionary_get_bool(config, "enabled");
    if (xpc_dictionary_get_value(config, "buffer-size-mb") != NULL)
        state.buffer_size_mb = xpc_dictionary_get_uint64(config, "buffer-size-mb");
    if (xpc_dictionary_get_value(config, "oncore-sampling-period") != NULL)
        state.oncore_period_ns = xpc_dictionary_get_uint64(config, "oncore-sampling-period");
    if (xpc_dictionary_get_value(config, "full-system-sampling-period") != NULL)
        state.full_period_ns = xpc_dictionary_get_uint64(config, "full-system-sampling-period");
    if (xpc_dictionary_get_value(config, "cputrace") != NULL)
        state.cputrace = xpc_dictionary_get_bool(config, "cputrace");
    const char *filter = xpc_dictionary_get_string(config, "ktrace-filter-descriptor");
    if (filter != NULL) {
        free(state.filter);
        state.filter = strdup(filter);
    }
    const char *sampling = xpc_dictionary_get_string(config, "sampling-option");
    if (sampling != NULL) {
        free(state.sampling);
        state.sampling = strdup(sampling);
    }
    int result = tailspin_state_save(&state) == 0 ? 0 : EIO;
    tailspin_state_free(&state);
    return result;
}

static void handle_message(xpc_object_t object) {
    xpc_object_t reply;
    const char *command = NULL;
    int64_t request_type;
    int64_t dump_fd;
    uint64_t request_timestamp;
    uint64_t request_id;
    int result = ENOTSUP;
    if (xpc_get_type(object) != XPC_TYPE_DICTIONARY)
        return;
    request_type = xpc_dictionary_get_int64(object, "tailspin_request_type");
    command = xpc_dictionary_get_string(object, "tailspin_request_type");
    if (command == NULL)
        command = xpc_dictionary_get_string(object, "command");
    if (command == NULL && request_type > 0)
        command = request_type == 1 ? "TAILSPIN_DUMP_OUTPUT" :
                  request_type == 2 ? "TAILSPIN_APPLY_CONFIG" :
                  request_type == 3 ? "TAILSPIN_GET_CPUTRACE_SESSION" : "UNKNOWN";
    reply = xpc_dictionary_create_reply(object);
    if (reply == NULL)
        return;
    if (command == NULL) {
        xpc_dictionary_set_int64(reply, "error", 22);
        xpc_dictionary_set_string(reply, "error-description", "missing command");
    } else {
        if (strcmp(command, "TAILSPIN_DUMP_OUTPUT") == 0 || request_type == 1) {
            dump_fd = xpc_dictionary_get_int64(object, "tailspin_dump_fd");
            request_timestamp = xpc_dictionary_get_uint64(object, "tailspin_dump_request_timestamp");
            request_id = xpc_dictionary_get_uint64(object, "request_id");
            if (request_id != 0)
                xpc_dictionary_set_uint64(reply, "request_id", request_id);
            if (dump_fd >= 0) {
                char path[64];
                uint64_t size = 0;
                snprintf(path, sizeof(path), "/dev/fd/%lld", (long long)dump_fd);
                result = tailspin_capture_stackshot(path, -1, request_timestamp, &size);
                xpc_dictionary_set_uint64(reply, "fileSizeBytes", size);
            } else {
                result = EBADF;
            }
        } else if (strcmp(command, "TAILSPIN_APPLY_CONFIG") == 0 || request_type == 2) {
            result = apply_config(object);
        } else if (strcmp(command, "TAILSPIN_GET_CPUTRACE_SESSION") == 0 || request_type == 3) {
            result = ENOTSUP;
        }
        xpc_dictionary_set_int64(reply, "error", result);
        xpc_dictionary_set_bool(reply, "tailspin_succeeded", result == 0);
        if (result != 0)
            xpc_dictionary_set_string(reply, "error-description", strerror(result));
        xpc_dictionary_set_string(reply, "command", command);
    }
    xpc_connection_t connection = xpc_dictionary_get_remote_connection(object);
    if (connection != NULL)
        xpc_connection_send_message(connection, reply);
    xpc_release(reply);
}

static void handle_connection(xpc_connection_t connection) {
    xpc_connection_set_event_handler(connection, ^(xpc_object_t object) {
        if (xpc_get_type(object) == XPC_TYPE_ERROR) {
            if (object == XPC_ERROR_CONNECTION_INVALID || object == XPC_ERROR_TERMINATION_IMMINENT)
                return;
        }
        handle_message(object);
    });
    xpc_connection_resume(connection);
}

int main(void) {
    xpc_connection_t listener = xpc_connection_create_mach_service(
        "com.apple.tailspind", dispatch_get_main_queue(), XPC_CONNECTION_MACH_SERVICE_LISTENER);
    if (listener == NULL)
        return 1;
    xpc_connection_set_event_handler(listener, ^(xpc_object_t object) {
        handle_connection((xpc_connection_t)object);
    });
    xpc_connection_resume(listener);
    dispatch_main();
}
