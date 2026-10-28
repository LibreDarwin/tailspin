/* Copyright (C) 2026, LibreDarwin */
/* SPDX-License-Identifier: BSD-3-Clause */

#import "tailspin_log.h"

#import "ktrace.h"

#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>
#import <dlfcn.h>
#import <errno.h>
#import <objc/message.h>
#import <objc/runtime.h>
#import <sys/time.h>

typedef id (*msg_id_id_t)(id, SEL, id);
typedef id (*serializer_init_t)(id, SEL, id, id, id);
typedef void (*prepare_t)(id, SEL, id);
typedef void (*serialize_dates_t)(id, SEL, id, id);

struct tailspin_log_context {
    ktrace_file_t file;
    int result;
};

static int append_data(struct tailspin_log_context *context, uint32_t tag,
                       NSData *data) {
    ktrace_chunk_t chunk;
    int result;

    if (context->result != 0)
        return context->result;
    chunk = ktrace_file_append_start(context->file, tag, 0, 0);
    if (chunk == NULL) {
        context->result = EIO;
        return context->result;
    }
    result = ktrace_file_append_data(context->file, chunk, data.bytes, data.length);
    if (result == 0)
        result = ktrace_file_append_finish(context->file, chunk);
    context->result = result;
    return result;
}

int tailspin_append_os_logs(int output_fd, const char *archive_path) {
    void *framework;
    ktrace_file_t file;
    struct timeval start_walltime;
    struct timeval end_walltime;
    NSURL *archive_url;
    Class store_class;
    Class serializer_class;
    id store;
    __block id source = nil;
    id serializer;
    dispatch_semaphore_t prepared;
    dispatch_semaphore_t completed;
    __block int prepare_result = 0;
    __block int completion_result = 0;
    __block struct tailspin_log_context context;

    if (archive_path == NULL)
        return EINVAL;
    framework = dlopen("/System/Library/PrivateFrameworks/LoggingSupport.framework/LoggingSupport",
                       RTLD_NOW);
    if (framework == NULL)
        return ENOTSUP;
    file = ktrace_file_open_fd(output_fd, 1);
    if (file == NULL)
        return errno != 0 ? errno : EIO;
    if (ktrace_file_earliest_walltime(file, &start_walltime) != 0 ||
        ktrace_file_latest_walltime(file, &end_walltime) != 0) {
        ktrace_file_close(file);
        return ENODATA;
    }

    @autoreleasepool {
        archive_url = [NSURL fileURLWithPath:@(archive_path) isDirectory:YES];
        store_class = NSClassFromString(@"OSLogEventStore");
        serializer_class = NSClassFromString(@"OSLogEventSerializer");
        if (store_class == Nil || serializer_class == Nil) {
            ktrace_file_close(file);
            return ENOTSUP;
        }
        store = ((msg_id_id_t)objc_msgSend)(store_class,
                                             @selector(storeWithArchiveURL:), archive_url);
        if (store == nil) {
            ktrace_file_close(file);
            return ENOENT;
        }
        prepared = dispatch_semaphore_create(0);
        void (^prepare_block)(id, id) = ^(id result, id error) {
            source = result;
            if (error != nil)
                prepare_result = EIO;
            dispatch_semaphore_signal(prepared);
        };
        ((prepare_t)objc_msgSend)(store, @selector(prepareWithCompletionHandler:),
                                   prepare_block);
        if (dispatch_semaphore_wait(prepared,
                                     dispatch_time(DISPATCH_TIME_NOW, 120 * NSEC_PER_SEC)) != 0)
            prepare_result = ETIMEDOUT;
        if (prepare_result != 0 || source == nil) {
            ktrace_file_close(file);
            return prepare_result != 0 ? prepare_result : EIO;
        }

        context.file = file;
        context.result = 0;
        completed = dispatch_semaphore_create(0);
        void (^data_block)(id) = ^(id data) {
            @autoreleasepool {
                if ([data isKindOfClass:[NSData class]])
                    (void)append_data(&context, 0x8011, data);
                else
                    context.result = EPROTO;
            }
        };
        void (^completion_block)(id, unsigned long long) = ^(id data,
                                                              unsigned long long count) {
            @autoreleasepool {
                (void)count;
                if (context.result == 0 && [data isKindOfClass:[NSData class]])
                    (void)append_data(&context, 0x8012, data);
                else if (context.result == 0)
                    context.result = EPROTO;
                completion_result = context.result;
                dispatch_semaphore_signal(completed);
            }
        };
        serializer = ((serializer_init_t)objc_msgSend)([serializer_class alloc],
                                                         @selector(initWithSource:dataProcessingBlock:completionBlock:),
                                                         source, data_block, completion_block);
        if (serializer == nil) {
            ktrace_file_close(file);
            return EIO;
        }
        NSDate *start = [NSDate dateWithTimeIntervalSince1970:
                         (NSTimeInterval)start_walltime.tv_sec +
                         (NSTimeInterval)start_walltime.tv_usec / 1000000.0];
        NSDate *end = [NSDate dateWithTimeIntervalSince1970:
                       (NSTimeInterval)end_walltime.tv_sec +
                       (NSTimeInterval)end_walltime.tv_usec / 1000000.0];
        ((serialize_dates_t)objc_msgSend)(serializer,
                                           @selector(serializeFromDate:toDate:), start, end);
        if (dispatch_semaphore_wait(completed,
                                     dispatch_time(DISPATCH_TIME_NOW, 120 * NSEC_PER_SEC)) != 0)
            completion_result = ETIMEDOUT;
    }
    ktrace_file_close(file);
    return completion_result;
}
