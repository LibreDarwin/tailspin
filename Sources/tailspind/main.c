/* Copyright (C) 2026, LibreDarwin */
/* SPDX-License-Identifier: BSD-3-Clause */

#include <dispatch/dispatch.h>
#include <xpc/xpc.h>

static void handle_message(xpc_object_t object) {
    xpc_object_t reply;
    const char *command;
    if (xpc_get_type(object) != XPC_TYPE_DICTIONARY)
        return;
    command = xpc_dictionary_get_string(object, "command");
    reply = xpc_dictionary_create_reply(object);
    if (reply == NULL)
        return;
    if (command == NULL) {
        xpc_dictionary_set_int64(reply, "error", 22);
        xpc_dictionary_set_string(reply, "error-description", "missing command");
    } else {
        xpc_dictionary_set_int64(reply, "error", 95);
        xpc_dictionary_set_string(reply, "error-description", "unsupported command");
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
