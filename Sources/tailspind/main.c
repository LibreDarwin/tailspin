/* Copyright (C) 2026, LibreDarwin */
/* SPDX-License-Identifier: BSD-3-Clause */

#include <dispatch/dispatch.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <xpc/xpc.h>

#include "../common/tailspin_capture.h"

static void handle_message(xpc_object_t object) {
    xpc_object_t reply;
    const char *command = NULL;
    int64_t request_type;
    int64_t dump_fd;
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
            if (dump_fd >= 0) {
                char path[64];
                uint64_t size = 0;
                snprintf(path, sizeof(path), "/dev/fd/%lld", (long long)dump_fd);
                result = tailspin_capture_stackshot(path, -1, 0, &size);
                xpc_dictionary_set_uint64(reply, "fileSizeBytes", size);
            } else {
                result = EBADF;
            }
        } else if (strcmp(command, "TAILSPIN_APPLY_CONFIG") == 0 || request_type == 2) {
            result = ENOTSUP;
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
