/*
 * Copyright (c) 2026 Darling Project.
 * Distributed under the Apple Public Source License, version 2.0.
 */
#include <CoreFoundation/CFFileDescriptor.h>
#include <CoreFoundation/CFRuntime.h>
#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

/* The dispatch queue owns readiness state. The recursive lock serializes client
 * callouts with invalidation (including invalidation from inside a callout).
 * Sources retain the descriptor, never the reverse. Dispatch handlers use a
 * borrowed pointer and are synchronously drained before storage is reclaimed.
 */
struct __CFFileDescriptor {
    CFRuntimeBase base;
    pthread_mutex_t calloutLock;
    dispatch_queue_t queue;
    dispatch_group_t cancellation;
    dispatch_source_t reader, writer;
    mach_port_t port;
    int fd;
    Boolean valid, closeOnInvalidate;
    CFOptionFlags enabled, pending;
    unsigned calloutDepth;
    CFFileDescriptorCallBack callout;
    CFFileDescriptorContext context;
};

static void __CFFDReady(CFFileDescriptorRef f, CFOptionFlags bit) {
    if (!f->valid || !(f->enabled & bit)) return;
    dispatch_suspend(bit == kCFFileDescriptorReadCallBack ? f->reader : f->writer);
    f->enabled &= ~bit;
    f->pending |= bit;
    mach_msg_header_t message = {0};
    message.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    message.msgh_size = sizeof(message);
    message.msgh_remote_port = f->port;
    /* A full port already contains a wakeup; readiness remains in pending. */
    (void)mach_msg(&message, MACH_SEND_MSG | MACH_SEND_TIMEOUT,
                  sizeof(message), 0, MACH_PORT_NULL, 0, MACH_PORT_NULL);
}

void CFFileDescriptorInvalidate(CFFileDescriptorRef f) {
    if (!f) return;
    pthread_mutex_lock(&f->calloutLock);
    __block Boolean wasValid = false;
    dispatch_sync(f->queue, ^{
        if (!f->valid) return;
        wasValid = true;
        f->valid = false;
        f->pending = 0;
        if (f->reader) {
            dispatch_source_cancel(f->reader);
            if (!(f->enabled & kCFFileDescriptorReadCallBack)) dispatch_resume(f->reader);
        }
        if (f->writer) {
            dispatch_source_cancel(f->writer);
            if (!(f->enabled & kCFFileDescriptorWriteCallBack)) dispatch_resume(f->writer);
        }
        f->enabled = 0;
    });
    if (wasValid) {
        /* Cancel handlers run only after the backend has stopped using fd. */
        dispatch_group_wait(f->cancellation, DISPATCH_TIME_FOREVER);
        if (f->reader) dispatch_release(f->reader);
        if (f->writer) dispatch_release(f->writer);
        f->reader = f->writer = NULL;
        if (f->closeOnInvalidate) close(f->fd);
        f->fd = -1;
        if (!f->calloutDepth) {
            void *info = f->context.info;
            void (*release)(void *) = f->context.release;
            memset(&f->context, 0, sizeof(f->context));
            if (release) release(info);
        }
    }
    pthread_mutex_unlock(&f->calloutLock);
}

static void __CFFDDeallocate(CFTypeRef value) {
    CFFileDescriptorRef f = (CFFileDescriptorRef)value;
    CFFileDescriptorInvalidate(f);
    if (f->port != MACH_PORT_NULL) mach_port_destroy(mach_task_self(), f->port);
    dispatch_release(f->cancellation);
    dispatch_release(f->queue);
    pthread_mutex_destroy(&f->calloutLock);
}

CFTypeID CFFileDescriptorGetTypeID(void) {
    static CFTypeID type;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        static const CFRuntimeClass cls = {
            0, "CFFileDescriptor", NULL, NULL, __CFFDDeallocate,
            NULL, NULL, NULL, NULL
        };
        type = _CFRuntimeRegisterClass(&cls);
    });
    return type;
}

CFFileDescriptorRef CFFileDescriptorCreate(CFAllocatorRef allocator, int fd,
        Boolean closeOnInvalidate, CFFileDescriptorCallBack callout,
        const CFFileDescriptorContext *context) {
    if (fd < 0 || fcntl(fd, F_GETFD) < 0 || !callout || (context && context->version != 0)) return NULL;
    CFFileDescriptorRef f = (CFFileDescriptorRef)_CFRuntimeCreateInstance(allocator,
        CFFileDescriptorGetTypeID(), sizeof(*f) - sizeof(CFRuntimeBase), NULL);
    if (!f) return NULL;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&f->calloutLock, &attr);
    pthread_mutexattr_destroy(&attr);
    f->queue = dispatch_queue_create("org.darling.CFFileDescriptor", NULL);
    f->cancellation = dispatch_group_create();
    f->fd = -1;
    f->port = MACH_PORT_NULL;
    if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &f->port) != KERN_SUCCESS ||
        mach_port_insert_right(mach_task_self(), f->port, f->port, MACH_MSG_TYPE_MAKE_SEND) != KERN_SUCCESS) {
        CFRelease(f);
        return NULL;
    }
    f->reader = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, fd, 0, f->queue);
    f->writer = dispatch_source_create(DISPATCH_SOURCE_TYPE_WRITE, fd, 0, f->queue);
    f->valid = true;
    /* Ownership of the caller's fd is not taken on a failed creation. */
    f->fd = fd;
    if (f->reader) {
        dispatch_group_enter(f->cancellation);
        dispatch_source_set_event_handler(f->reader, ^{ __CFFDReady(f, kCFFileDescriptorReadCallBack); });
        dispatch_source_set_cancel_handler(f->reader, ^{ dispatch_group_leave(f->cancellation); });
    }
    if (f->writer) {
        dispatch_group_enter(f->cancellation);
        dispatch_source_set_event_handler(f->writer, ^{ __CFFDReady(f, kCFFileDescriptorWriteCallBack); });
        dispatch_source_set_cancel_handler(f->writer, ^{ dispatch_group_leave(f->cancellation); });
    }
    if (!f->reader || !f->writer) {
        CFRelease(f);
        return NULL;
    }
    f->closeOnInvalidate = closeOnInvalidate;
    f->callout = callout;
    if (context) {
        f->context = *context;
        if (context->retain) f->context.info = context->retain(context->info);
    }
    return f;
}

int CFFileDescriptorGetNativeDescriptor(CFFileDescriptorRef f) {
    if (!f) return -1;
    pthread_mutex_lock(&f->calloutLock);
    int fd = f->fd;
    pthread_mutex_unlock(&f->calloutLock);
    return fd;
}

Boolean CFFileDescriptorIsValid(CFFileDescriptorRef f) {
    return CFFileDescriptorGetNativeDescriptor(f) >= 0;
}

void CFFileDescriptorGetContext(CFFileDescriptorRef f, CFFileDescriptorContext *context) {
    if (!f || !context) return;
    pthread_mutex_lock(&f->calloutLock);
    *context = f->context;
    pthread_mutex_unlock(&f->calloutLock);
}

static void __CFFDChange(CFFileDescriptorRef f, CFOptionFlags bits, Boolean enable) {
    if (!f) return;
    dispatch_sync(f->queue, ^{
        if (!f->valid) return;
        for (CFOptionFlags bit = 1; bit <= 2; bit <<= 1) {
            if (!(bits & bit)) continue;
            dispatch_source_t source = bit == 1 ? f->reader : f->writer;
            f->pending &= ~bit;
            if (enable && !(f->enabled & bit)) {
                f->enabled |= bit;
                dispatch_resume(source);
            } else if (!enable && (f->enabled & bit)) {
                f->enabled &= ~bit;
                dispatch_suspend(source);
            }
        }
    });
}

void CFFileDescriptorEnableCallBacks(CFFileDescriptorRef f, CFOptionFlags bits) {
    __CFFDChange(f, bits, true);
}

void CFFileDescriptorDisableCallBacks(CFFileDescriptorRef f, CFOptionFlags bits) {
    __CFFDChange(f, bits, false);
}

static mach_port_t __CFFDGetPort(void *info) {
    return ((CFFileDescriptorRef)info)->port;
}

static void *__CFFDPerform(void *message, CFIndex size, CFAllocatorRef allocator, void *info) {
    CFFileDescriptorRef f = info;
    pthread_mutex_lock(&f->calloutLock);
    __block CFOptionFlags pending;
    dispatch_sync(f->queue, ^{
        pending = f->valid ? f->pending : 0;
        f->pending = 0;
    });
    if (pending) {
        f->calloutDepth++;
        f->callout(f, pending, f->context.info);
        f->calloutDepth--;
        if (!f->calloutDepth && f->fd < 0) {
            void *contextInfo = f->context.info;
            void (*release)(void *) = f->context.release;
            memset(&f->context, 0, sizeof(f->context));
            if (release) release(contextInfo);
        }
    }
    pthread_mutex_unlock(&f->calloutLock);
    return NULL;
}

CFRunLoopSourceRef CFFileDescriptorCreateRunLoopSource(CFAllocatorRef allocator,
        CFFileDescriptorRef f, CFIndex order) {
    if (!f) return NULL;
    pthread_mutex_lock(&f->calloutLock);
    CFRunLoopSourceRef source = NULL;
    if (f->fd >= 0) {
        CFRunLoopSourceContext1 context = {0};
        context.version = 1;
        context.info = f;
        context.retain = (const void *(*)(const void *))CFRetain;
        context.release = (void (*)(const void *))CFRelease;
        context.getPort = __CFFDGetPort;
        context.perform = __CFFDPerform;
        source = CFRunLoopSourceCreate(allocator, order, (CFRunLoopSourceContext *)&context);
    }
    pthread_mutex_unlock(&f->calloutLock);
    return source;
}
