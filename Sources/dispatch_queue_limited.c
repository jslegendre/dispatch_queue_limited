//
//  dispatch_queue_limited.c
//  IconChamp
//
//  Created by Jeremy on 1/24/21.
//

#if __has_include(<objc/objc.h>)
#define HAVE_OBJC 1
#else
#define HAVE_OBJC 0
#define DQL_USE_OBJC 0
#endif

#if !defined(DQL_USE_OBJC) && HAVE_OBJC
#define DQL_USE_OBJC 1
#else
#define OS_OBJECT_USE_OBJC 0
#endif

#include <Block.h>
#include <assert.h>
#include <pthread/pthread.h>
#include <stdio.h>
#include <mach/mach.h>

#if DQL_USE_OBJC
#import <objc/objc-runtime.h>
extern void *objc_autoreleasePoolPush(void);
extern void objc_autoreleasePoolPop(void *ctxt);
#define OS_OBJECT_CLASS_SYMBOL(name) OS_##name##_class
#define OS_OBJC_CLASS_RAW_SYMBOL_NAME(name) "_OBJC_CLASS_$_" OS_STRINGIFY(name)
#define OS_OBJECT_CLASS_DECL(name) \
        extern void *OS_OBJECT_CLASS_SYMBOL(name) \
                __asm__(OS_OBJC_CLASS_RAW_SYMBOL_NAME(OS_##name))
#define OS_OBJECT_CLASS(name) &OS_OBJECT_CLASS_SYMBOL(name)
#else // !DQL_USE_OBJC
#define OS_OBJECT_CLASS_DECL(name)
#define OS_OBJECT_CLASS(x) NULL
#endif // DQL_USE_OBJC

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

// This is mostly from libdispatch (with slight modifications).
// atomic_private emits a lot more barriers and prefers
// a stronger memory model.
#if __has_include(<os/atomic_private.h>)
#include <os/atomic_private.h>
#define _os_atomic_c11_atomic(p) ((__typeof__(*(p)) _Atomic *)(p))
#ifndef _os_atomic_basetypeof
#define _os_atomic_basetypeof(p) \
        __typeof__(atomic_load_explicit(_os_atomic_c11_atomic(p), memory_order_relaxed))
#endif
#define os_atomic_cmpxchgvw(p, e, v, g, m) \
        ({ _os_atomic_basetypeof(p) _r = (e); _Bool _b = \
        atomic_compare_exchange_weak_explicit(_os_atomic_c11_atomic(p), \
        &_r, v, memory_order_##m, memory_order_relaxed); *(g) = _r;  _b; })
#else
#include <stdatomic.h>
#define _os_atomic_c11_atomic(p) ((__typeof__(*(p)) _Atomic *)(p))
#ifndef _os_atomic_basetypeof
#define _os_atomic_basetypeof(p) \
        __typeof__(atomic_load_explicit(_os_atomic_c11_atomic(p), memory_order_relaxed))
#endif
#define os_atomic_load(p, m) \
        atomic_load_explicit(_os_atomic_c11_atomic(p), memory_order_##m)
#define os_atomic_store(p, v, m) \
        atomic_store_explicit(_os_atomic_c11_atomic(p), v, memory_order_##m)
#define os_atomic_xchg(p, v, m) \
        atomic_exchange_explicit(_os_atomic_c11_atomic(p), v, memory_order_##m)
#define os_atomic_cmpxchg(p, e, v, m) \
        ({ _os_atomic_basetypeof(p) _r = (e); \
        atomic_compare_exchange_strong_explicit(_os_atomic_c11_atomic(p), \
        &_r, v, memory_order_##m, memory_order_relaxed); })
#define os_atomic_cmpxchgv(p, e, v, g, m) \
        ({ _os_atomic_basetypeof(p) _r = (e); _Bool _b = \
        atomic_compare_exchange_strong_explicit(_os_atomic_c11_atomic(p), \
        &_r, v, memory_order_##m, memory_order_relaxed); *(g) = _r; _b; })
#define os_atomic_cmpxchgvw(p, e, v, g, m) \
        ({ _os_atomic_basetypeof(p) _r = (e); _Bool _b = \
        atomic_compare_exchange_weak_explicit(_os_atomic_c11_atomic(p), \
        &_r, v, memory_order_##m, memory_order_relaxed); *(g) = _r;  _b; })
#define os_atomic_thread_fence(m)  atomic_thread_fence(memory_order_##m)
#define os_atomic_rmw_loop_give_up_with_fence(m, expr) \
        ({ os_atomic_thread_fence(m); expr; __builtin_unreachable(); })
#define os_atomic_rmw_loop_give_up(expr) \
        os_atomic_rmw_loop_give_up_with_fence(relaxed, expr)
#define os_atomic_rmw_loop(p, ov, nv, m, ...)  ({ \
        bool _result = false; \
        __typeof__(p) _p = (p); \
        ov = os_atomic_load(_p, relaxed); \
        do { \
            __VA_ARGS__; \
            _result = os_atomic_cmpxchgvw(_p, ov, nv, &ov, m); \
        } while (unlikely(!_result)); \
        _result;})
#define _os_atomic_c11_op(p, v, m, o, op) \
        ({ _os_atomic_basetypeof(p) _v = (v), _r = \
        atomic_fetch_##o##_explicit(_os_atomic_c11_atomic(p), _v, \
        memory_order_##m); (__typeof__(_r))(_r op _v); })
#define _os_atomic_c11_op_orig(p, v, m, o, op) \
        atomic_fetch_##o##_explicit(_os_atomic_c11_atomic(p), v, \
        memory_order_##m)
#define os_atomic_add(p, v, m) \
        _os_atomic_c11_op((p), (v), m, add, +)
#define os_atomic_sub(p, v, m) \
        _os_atomic_c11_op((p), (v), m, sub, -)
#define os_atomic_sub_orig(p, v, m) \
        _os_atomic_c11_op_orig((p), (v), m, sub, -)
#endif

#if __has_include(<ptrauth.h>)
#include <ptrauth.h>
#endif
#ifndef __ptrauth_objc_isa_pointer
#define __ptrauth_objc_isa_pointer
#endif

#ifndef DQL_RESUME_MSG_ID
#define DQL_RESUME_MSG_ID 7575
#endif

#if DISPATCH_QUEUE_LIMITED_AWAIT_BLOCKING
extern mach_port_t mig_get_reply_port(void);
extern void mig_put_reply_port(mach_port_t reply_port);
#define _dispatch_queue_limited_handle_await(x) \
    _dispatch_queue_limited_hold_for_await(x)
#define dispatch_queue_limited_handle_resume(x) \
    _dispatch_queue_limited_unblock_await(x)
#else
#define dispatch_queue_limited_handle_resume(x) \
    _dispatch_queue_limited_reenqueue_drain(x)

#define _dispatch_queue_limited_handle_await(x) \
    return;
#endif

static const void * const dql_queue_key = (const void*)&dql_queue_key;

#include "include/dispatch_queue_limited.h"

// Unused, here for reference
struct Block_layout {
    void *isa;
    volatile int32_t flags; // contains ref count
    int32_t reserved;
    void (*invoke)(void *, ...);
};

typedef struct dql_work_item_s {
    bool shouldAwait;
    void *context;
    dispatch_function_t invoke;
    struct dql_work_item_s *next;
} dql_work_item, *dql_work_item_t;

// mach port list node
// Used for holding threads
typedef struct mpl_node_s {
    mach_port_t port;
    struct mpl_node_s *next;
} mpl_node, *mpl_node_t;


OS_OBJECT_CLASS_DECL(dispatch_queue_limited);
struct dispatch_queue_limited_s {
    #if DQL_USE_OBJC
    const void *__ptrauth_objc_isa_pointer obj_isa;
    volatile _Atomic int ref_cnt;
    #endif
    volatile _Atomic int xref_cnt;
    uint32_t limit;
    volatile _Atomic int32_t thread_count;
    volatile _Atomic int32_t await_count;
    dispatch_queue_t queue;
    volatile _Atomic dql_work_item_t dql_head;
    volatile _Atomic dql_work_item_t dql_tail;
    volatile _Atomic mpl_node_t wait_head;
    volatile _Atomic mpl_node_t wait_tail;
} __attribute__((aligned(8)));

static inline __attribute__((always_inline))
void dql_work_item_free(dql_work_item_t item) {
    free(item);
}

static inline __attribute__((always_inline))
dql_work_item_t dql_work_item_create(dispatch_function_t work, void *ctxt, bool shouldAwait) {
    // Todo: create cache/arena for work items
    dql_work_item_t b = malloc(sizeof(dql_work_item));
    b->shouldAwait = shouldAwait;
    b->invoke = work;
    b->context = ctxt;
    b->next = NULL;
    return b;
}

static inline __attribute__((always_inline))
mpl_node_t mp_list_pop_head(dispatch_queue_limited_t dq) {
    mpl_node_t old_head, new_head = NULL;
    (void)os_atomic_rmw_loop(&dq->wait_head, old_head, new_head, release, {
        if (unlikely(!old_head)) os_atomic_rmw_loop_give_up(return NULL);
        new_head = os_atomic_load(&old_head->next, acquire);
        if (unlikely(!new_head)) {
            os_atomic_cmpxchg(&dq->wait_tail, dq->wait_head, NULL, release);
            os_atomic_store(&dq->wait_head, NULL, release);
            os_atomic_rmw_loop_give_up(break);
        }
    });
    
    return old_head;
}

static inline __attribute__((always_inline))
void mp_list_add_port(dispatch_queue_limited_t dq, mpl_node_t node) {
    mpl_node_t prev = os_atomic_xchg(&dq->wait_tail, node, release);
    if (likely(prev)) {
        os_atomic_store(&prev->next, node, release);
        return;
    }
    os_atomic_store(&dq->wait_head, node, release);
}

static inline __attribute__((always_inline))
void * dispatch_queue_limited_alloc(void *isa, size_t size) {
#if !DQL_USE_OBJC
    return calloc(1u, size);
#else
    size -= 8;
    return class_createInstance(isa, size);
#endif
}

static inline __attribute__((always_inline))
void dispatch_queue_limited_dealloc(dispatch_queue_limited_t dq) {
    assert(dq->xref_cnt == -1);
    
#if !DQL_USE_OBJC
    free(dq);
#else
    // Have objc runtime handle deallocation for us.
    // Bump xref_cnt to not cause an 'over-release'
    // error.
    dq->xref_cnt += 1;
    os_release(dq);
#endif
}

static
void dispatch_queue_limited_dispose(dispatch_queue_limited_t dq) {
    dispatch_release(dq->queue);
}

static
void dql_block_invoke(void *block) {
    void *pool = objc_autoreleasePoolPush();
    void (^b)(void) = block;
    b();
    Block_release(b);
    objc_autoreleasePoolPop(pool);
}

static inline __attribute__((always_inline))
void _dispatch_queue_limited_retain(dispatch_queue_limited_t dq) {
    os_atomic_add(&dq->xref_cnt, 1, relaxed);
}

static inline __attribute__((always_inline))
void _dispatch_queue_limited_release(dispatch_queue_limited_t dq) {
    int xrefs = os_atomic_sub(&dq->xref_cnt, 1, relaxed);
    if (unlikely(xrefs == -1)) {
        dispatch_queue_limited_dispose(dq);
        dispatch_queue_limited_dealloc(dq);
    }
}

void
dispatch_queue_limited_debug_desc(dispatch_queue_limited_t dq, char* buf, size_t bufsiz) {
    snprintf(buf, bufsiz, "thread_count: %d\nthread_limit: %d\nawait_count: %d",
             dq->thread_count, dq->limit, dq->await_count);
}

void dispatch_queue_limited_retain(dispatch_queue_limited_t dq) {
    _dispatch_queue_limited_retain(dq);
}

void dispatch_queue_limited_release(dispatch_queue_limited_t dq) {
    _dispatch_queue_limited_release(dq);
}

static inline __attribute__((always_inline))
void dispatch_queue_limited_dec_await(dispatch_queue_limited_t dq) {
    int32_t r = os_atomic_sub(&dq->await_count, 1, relaxed);
    assert(r >= 0);
}

static inline __attribute__((used,always_inline))
void dispatch_queue_limited_inc_await(dispatch_queue_limited_t dq) {
    (void)os_atomic_add(&dq->await_count, 1, relaxed);
}

static inline __attribute__((used))
int32_t dispatch_queue_limited_thread_count(dispatch_queue_limited_t dq) {
    return os_atomic_load(&dq->thread_count, relaxed);
}

static inline __attribute__((always_inline))
void dispatch_queue_limited_dec_count(dispatch_queue_limited_t dq) {
    int32_t r = os_atomic_sub(&dq->thread_count, 1, relaxed);
    assert(r >= 0);
    _dispatch_queue_limited_release(dq);
}

static inline __attribute__((used,always_inline))
void dispatch_queue_limited_inc_count(dispatch_queue_limited_t dq) {
    _dispatch_queue_limited_retain(dq);
    int32_t r = os_atomic_add(&dq->thread_count, 1, relaxed);
    assert(r <= dq->limit);
}

static inline __attribute__((always_inline))
bool dispatch_queue_limited_maybe_inc_count(dispatch_queue_limited_t dq) {
    int32_t old_count, new_count = 0;
    (void)os_atomic_rmw_loop(&dq->thread_count, old_count, new_count, relaxed, {
        if (old_count >= dq->limit) {
            os_atomic_rmw_loop_give_up(return false);
        }

        new_count = old_count + 1;
    });
    
    _dispatch_queue_limited_retain(dq);
    return true;
}

static inline __attribute__((always_inline))
void dispatch_queue_limited_add_item(dispatch_queue_limited_t dq, dql_work_item_t item) {

    dql_work_item_t prev = os_atomic_xchg(&dq->dql_tail, item, release);
    if (likely(prev)) {
        os_atomic_store(&prev->next, item, relaxed);
        return;
    }
    os_atomic_store(&dq->dql_head, item, relaxed);
}

static inline __attribute__((always_inline))
dql_work_item_t dispatch_queue_limited_pop_item(dispatch_queue_limited_t dq) {
    if (unlikely(!dq->dql_tail)) NULL;
    
    dql_work_item_t old_head, new_head = NULL;
    (void)os_atomic_rmw_loop(&dq->dql_head, old_head, new_head, acquire, {
        if (unlikely(!old_head)) os_atomic_rmw_loop_give_up(return NULL);
        
        new_head = os_atomic_load(&old_head->next, acquire);
        if (unlikely(!new_head)) {
            os_atomic_cmpxchg(&dq->dql_tail, dq->dql_head, NULL, release);
            os_atomic_store(&dq->dql_head, NULL, release);
            os_atomic_rmw_loop_give_up(break);
        }
    });

    return old_head;
}

static inline __attribute__((always_inline))
void _dispatch_queue_limited_hold_for_await(dispatch_queue_limited_t dq) {
    mach_port_t port = mig_get_reply_port();
    mpl_node node = { port, NULL };
    // We can get away with a stack pointer because mach_msg will block
    mp_list_add_port(dq, &node);
    mach_msg_header_t header = {0};
    mach_msg((mach_msg_header_t*)&header, MACH_RCV_MSG, 0, sizeof(header),
                        port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    if (header.msgh_id != DQL_RESUME_MSG_ID) {
        /* Not sure what to do about this */
    }
    mach_msg_destroy(&header);
    mig_put_reply_port(port);

}

static inline __attribute__((always_inline))
void _dispatch_queue_limited_unblock_await(dispatch_queue_limited_t dq) {
    mpl_node_t node = mp_list_pop_head(dq);
    if (!node) return;
    mach_port_t mp = node->port;
    mach_port_t send_port = MACH_PORT_NULL;
    mach_msg_type_name_t type;
    mach_port_extract_right(mach_task_self(), mp, MACH_MSG_TYPE_MAKE_SEND_ONCE, &send_port, &type);
    if (!mp) return;
    mach_msg_header_t header = {
        .msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE, MACH_MSG_TYPE_MAKE_SEND_ONCE),
        .msgh_remote_port = send_port,
        .msgh_size = sizeof(header),
        .msgh_id = DQL_RESUME_MSG_ID,
    };
    mach_msg((mach_msg_header_t*)&header, MACH_SEND_MSG, sizeof(header), 0, send_port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
}

static
void dispatch_queue_limited_drain(dispatch_queue_limited_t dq) {
    if (unlikely(!dq->dql_tail)) {
        return dispatch_queue_limited_dec_count(dq);
    }
    
    bool shouldAwait = false;
    dql_work_item_t head = NULL;
    while ((head = dispatch_queue_limited_pop_item(dq))) {
        shouldAwait = head->shouldAwait;
        head->invoke(head->context);
        dql_work_item_free(head);
        
        if (shouldAwait) {
            // If DISPATCH_QUEUE_LIMITED_AWAIT_BLOCKING:
            // Calls _dispatch_queue_limited_hold_for_await,
            // blocking until _dispatch_queue_limited_unblock_await.
            // If !DISPATCH_QUEUE_LIMITED_AWAIT_BLOCKING:
            // Calls 'return', which skips call to
            // dispatch_queue_limited_dec_count and returns
            // this thread to the backing queue.
            dispatch_queue_limited_inc_await(dq);
            _dispatch_queue_limited_handle_await(dq);
        }
    }
    
    dispatch_queue_limited_dec_count(dq);
}

static inline __attribute__((always_inline,used))
void _dispatch_queue_limited_reenqueue_drain(dispatch_queue_limited_t dq) {
    if (unlikely(!dq->dql_tail)) {
        // There are no more items for now so decrease thread count and return.
        return dispatch_queue_limited_dec_count(dq);
    }
    
    // Even though _dispatch_queue_limited_handle_await does 'return' out of the thread,
    // it does not decrease 'thread_count' so we never go OVER the thread limit. Now
    // re-enqueue the drain without increasing the thread count.
    dispatch_async_f(dq->queue, dq, (dispatch_function_t)dispatch_queue_limited_drain);
}

void dispatch_queue_limited_dequeue_await(dispatch_queue_limited_t dq) {
    // If compiled with DISPATCH_QUEUE_LIMITED_AWAIT_BLOCKING, calls _dispatch_queue_limited_unblock_await.
    // Otherwise calls _dispatch_queue_limited_reenqueue_drain
    dispatch_queue_limited_dec_await(dq);
    dispatch_queue_limited_handle_resume(dq);
}

__attribute__((minsize))
void dispatch_queue_limited_set_target_queue(dispatch_queue_limited_t dq, dispatch_queue_t queue) {
    dispatch_retain(queue);
    dispatch_queue_t old_queue = NULL;
    os_atomic_cmpxchgvw(&dq->queue, dq->queue, queue, &old_queue, seq_cst);
    if (old_queue) {
        dispatch_release(old_queue);
    }
}

/*
 * If we are below the thread limit, spin up another thread and start draining.
 * !!! Do not send blocks directly to the backing queue !!!
 * Calling dispatch_(a)sync on the queue will cause blocks to be copied agin
 */
static inline __attribute__((always_inline,used))
void _dispatch_queue_limited_maybe_invoke(dispatch_queue_limited_t dq, dql_work_item_t work) {
    dispatch_queue_limited_add_item(dq, work);
    if (dispatch_queue_limited_maybe_inc_count(dq)) {
        dispatch_async_f(dq->queue, dq, (dispatch_function_t)dispatch_queue_limited_drain);
    }
}

static inline __attribute__((always_inline,used))
void _dispatch_queue_limited_enqueue_f(dispatch_queue_limited_t dq, void *_Nullable context, dispatch_function_t work) {
    dql_work_item_t item = dql_work_item_create(work, context, false);
    _dispatch_queue_limited_maybe_invoke(dq, item);
}

static inline __attribute__((always_inline,used))
void _dispatch_queue_limited_enqueue_await_f(dispatch_queue_limited_t dq, void *_Nullable context, dispatch_function_t work) {
    dql_work_item_t item = dql_work_item_create(work, context, true);
    _dispatch_queue_limited_maybe_invoke(dq, item);
}

void dispatch_queue_limited_enqueue_await_f(dispatch_queue_limited_t dq, void *_Nullable context, dispatch_function_t work) {
    _dispatch_queue_limited_enqueue_await_f(dq, context, work);
}

void dispatch_queue_limited_enqueue_f(dispatch_queue_limited_t dq, void *_Nullable context, dispatch_function_t work) {
    _dispatch_queue_limited_enqueue_f(dq, context, work);
}

/*
 * We do not have ARC here so blocks need to be copied now or risk getting released before
 * we get to them. The dispatch_queue_limited_enqueue* non "_f" variants do this with
 * 'dispatch_block_create'. Copied blocks are forwarded as 'contexts' to their "_f" counterparts
 * with a 'work' function (dql_block_invoke) to handle calling and releaseing them.
 */
void dispatch_queue_limited_enqueue_await(dispatch_queue_limited_t dq, void (^op)(void)) {
    _dispatch_queue_limited_enqueue_await_f(dq, (void*)dispatch_block_create(0, op), dql_block_invoke);
}

void dispatch_queue_limited_enqueue(dispatch_queue_limited_t dq, void (^op)(void)) {
    _dispatch_queue_limited_enqueue_f(dq, (void*)dispatch_block_create(0, op), dql_block_invoke);
}

dispatch_queue_limited_t dispatch_queue_limited_create(uint32_t limit, dispatch_qos_class_t qos) {
    dispatch_queue_limited_t dq;
    dq = dispatch_queue_limited_alloc(OS_OBJECT_CLASS(dispatch_queue_limited), sizeof(struct dispatch_queue_limited_s));
    dispatch_queue_attr_t attr = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_CONCURRENT, qos, -1);
    dq->queue = dispatch_queue_create("dispatch_queue_limited_queue", attr);
    dispatch_queue_set_specific(dq->queue, dql_queue_key, dq->queue, NULL);
    dq->limit = limit;
    dq->thread_count = 0;
    dq->await_count = 0;
    dq->ref_cnt = 1;
    dq->xref_cnt = 1;
    return dq;
}
